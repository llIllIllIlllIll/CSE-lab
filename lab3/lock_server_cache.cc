// the caching lock server implementation

#include "lock_server_cache.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "lang/verify.h"
#include "handle.h"
#include "tprintf.h"


lock_server_cache::lock_server_cache()
{
  pthread_mutex_init(&mutex,NULL);
  cond = PTHREAD_COND_INITIALIZER;
  ready = 1;
}


int lock_server_cache::acquire(lock_protocol::lockid_t lid, std::string id, 
                               int & r)
{
  
  int ddxz;
  lock_protocol::status ret = lock_protocol::OK;

  tprintf("lsc: acquire %d called by %s and stuck\n",lid,id.c_str());

  pthread_mutex_lock(&mutex);

  while(lock_map.count(lid)>0 && !lock_map[lid].wt_clt.empty()  && lock_map[lid].wt_clt != id){
	//tprintf("%s is hanging... waiting for %s...\n",id.c_str(),lock_map[lid].wt_clt.c_str());
	pthread_cond_wait(&cond,&mutex);
  }
  

  tprintf("lsc: %s is continuing...\n",id.c_str());

  if(this->lock_map.count(lid)<=0){
	// the case that lock lid has never been accessed
	this->lock_map.insert(std::make_pair(lid,lock_entry(id)));
	
	tprintf("lsc: empty lock %d acquired by %s\n",lid,id.c_str());

	r = lock_protocol::OK;

	pthread_mutex_unlock(&mutex);
  }
  else if(this->lock_map[lid].locked == false){
  	// the case that lock is free
	lock_entry & le = lock_map[lid];
	// the wt_clts logically cannot be empty at the moment
	assert(id == le.wt_clt);
	le.cur_clt = id;
	le.locked = true;
	le.wt_clt = "";

	tprintf("lsc: free lock %d acquired by %s\n",lid,id.c_str());
	r = lock_protocol::OK;

	pthread_cond_signal(&cond);
	pthread_mutex_unlock(&mutex);
  }
  else{
	// 1. return RETRY to client
	// 2. call revoke function of the cur_clt
	ret = lock_protocol::RETRY;
	lock_entry & le = lock_map[lid];
	
	if(le.cur_clt == id){
		r = lock_protocol::OK;
		pthread_cond_signal(&cond);
		pthread_mutex_unlock(&mutex);
		return 0;
	}

  	le.wt_clt = id;

	pthread_cond_signal(&cond);
	pthread_mutex_unlock(&mutex);

	handle handler(le.cur_clt);
	rpcc * cl = handler.safebind();
	if(cl==NULL){
		report_error("acquire_3",lock_protocol::RPCERR);
		return lock_protocol::RPCERR;
	}

        tprintf("lsc: busy lock %d: %s now waiting and %s being revoked\n",lid,id.c_str(),le.cur_clt.c_str());

	r = lock_protocol::RETRY;

	ret = cl->call(rlock_protocol::revoke,lid,ddxz);
	assert(ret == 0);
	tprintf("lsc: busy lock %s revoked sucessfully; call %s to retry\n",le.cur_clt.c_str(),id.c_str());

	pthread_mutex_lock(&mutex);
	le.wt_clt = id;
	le.locked = false;
	pthread_mutex_unlock(&mutex);

	assert(ret == 0);	

  }
  return 0;
}

int 
lock_server_cache::release(lock_protocol::lockid_t lid, std::string id, 
         int &r)
{
    return 0;
}

lock_protocol::status
lock_server_cache::stat(lock_protocol::lockid_t lid, int &r)
{
  tprintf("stat request\n");
  r = nacquire;
  return lock_protocol::OK;
}

