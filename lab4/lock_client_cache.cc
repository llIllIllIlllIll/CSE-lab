// RPC stubs for clients to talk to lock_server, and cache the locks
// see lock_client.cache.h for protocol details.

#include "lock_client_cache.h"
#include "rpc.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include "tprintf.h"

// a tricky problem exists in test5
// because many threads might be stuck at waiting for a lock
// if that lock is revoked..
// threads never get that lock
// so now I add a variable specially for this purpose
// sp_waiters

int lock_client_cache::last_port = 0;

lock_client_cache::lock_client_cache(std::string xdst, 
				     class lock_release_user *_lu)
  : lock_client(xdst), lu(_lu)
{
  srand(time(NULL)^last_port);
  rlock_port = ((rand()%32000) | (0x1 << 10));
  const char *hname;
  // VERIFY(gethostname(hname, 100) == 0);
  hname = "127.0.0.1";
  std::ostringstream host;
  host << hname << ":" << rlock_port;
  id = host.str();
  last_port = rlock_port;
  rpcs *rlsrpc = new rpcs(rlock_port);
  rlsrpc->reg(rlock_protocol::revoke, this, &lock_client_cache::revoke_handler);
  rlsrpc->reg(rlock_protocol::retry, this, &lock_client_cache::retry_handler);

  //initialize mutex and cond
  pthread_mutex_init(&mutex,NULL);
  pthread_mutex_init(&nutex,NULL);
  pthread_mutex_init(&outex,NULL);
  cond = PTHREAD_COND_INITIALIZER;
  dond = PTHREAD_COND_INITIALIZER;
  eond = PTHREAD_COND_INITIALIZER;
}

lock_protocol::status
lock_client_cache::acquire(lock_protocol::lockid_t lid)
{
  int ddxz;
  int ret = lock_protocol::OK;
  

  pthread_mutex_lock(&outex);
  if(this->sp_waiters.count(lid) == 0)
	  this->sp_waiters[lid] = 1;
  else
	  this->sp_waiters[lid] = this->sp_waiters[lid] + 1;
  pthread_mutex_unlock(&outex);
 

  pthread_mutex_lock(&nutex);
  if(this->clt_locks[lid] == lock_client_cache::NONE){
	ret = cl->call(lock_protocol::acquire,lid,id,ddxz);
	// during a rpc call mutex should not be held
	assert(ret == 0);

	pthread_mutex_lock(&mutex);
	this->clt_locks[lid] = lock_client_cache::ACQUIRING;
	if(ddxz == lock_protocol::OK){
		this->clt_locks[lid] = lock_client_cache::LOCKED;
		//tprintf("lcc: %s has acquired lock %d\n",id.c_str(),lid);

		pthread_cond_signal(&dond);
		pthread_mutex_unlock(&mutex);
	}
	// RETRY indicates that clt has been added to waiting list of server and may retry later
	else if(ddxz == lock_protocol::RETRY){
		this->clt_locks[lid] = lock_client_cache::ACQUIRING;
		//tprintf("lcc: %s got RETRY and start retrying...\n",id.c_str());

		//pthread_mutex_unlock(&mutex);

		while(ddxz == lock_protocol::RETRY)
			ret = cl->call(lock_protocol::acquire,lid,id,ddxz);
		if(ddxz == lock_protocol::OK){
			//tprintf("lcc:%s got %d after retrying\n",id.c_str(),lid);
		}
		
		//pthread_mutex_lock(&mutex);
		this->clt_locks[lid] = lock_client_cache::LOCKED;
		pthread_cond_signal(&dond);
		pthread_mutex_unlock(&mutex);
		
	}
	else{
		this->report_error("acquire_1",ret);
	}
  }
  else if(this->clt_locks[lid] == lock_client_cache::LOCKED || this->clt_locks[lid] == lock_client_cache::FREE){
	pthread_mutex_lock(&mutex);
	while(this->clt_locks[lid] == lock_client_cache::LOCKED){
		pthread_cond_wait(&cond,&mutex);
	}
	this->clt_locks[lid] = lock_client_cache::LOCKED;
	pthread_mutex_unlock(&mutex);
	
	//tprintf("lcc: client %s acquired lock %d locally\n",id.c_str(),lid);
  }
  

  pthread_mutex_lock(&outex);
  this->sp_waiters[lid] = this->sp_waiters[lid] - 1;
  pthread_cond_signal(&eond);
  pthread_mutex_unlock(&outex);
  
  pthread_mutex_unlock(&nutex);

  return ret;
}

lock_protocol::status
lock_client_cache::release(lock_protocol::lockid_t lid)
{
  int ret = lock_protocol::OK;
  pthread_mutex_lock(&mutex);
  if(this->clt_locks[lid] != lock_client_cache::LOCKED){
  	report_error("release_1",this->clt_locks[lid]);
  }
  assert(this->clt_locks[lid] == lock_client_cache::LOCKED);
  if(this->clt_locks[lid] == lock_client_cache::LOCKED){
  	this->clt_locks[lid] = lock_client_cache::FREE;
	//tprintf("lcc: client %s released lock %d locally\n",id.c_str(),lid);
	pthread_cond_signal(&cond);
  }
  pthread_mutex_unlock(&mutex);
  //tprintf("lcc: return from release\n");
  return ret;  
}

rlock_protocol::status
lock_client_cache::revoke_handler(lock_protocol::lockid_t lid, 
                                  int & ret)
{
  int ddxz;
  ret = rlock_protocol::OK;

  //pthread_mutex_lock(&nutex);

  pthread_mutex_lock(&outex);
  while(this->sp_waiters[lid]!=0){
	//tprintf("lcc: sp_waiters for lock [%3d] [%3d] remains in %s\n",lid,this->sp_waiters[lid],id.c_str());
  	pthread_cond_wait(&eond,&outex);
  }
  //tprintf("lcc: sp_waiters for lock [%3d] : %d\n",lid,sp_waiters[lid]);
  

  pthread_mutex_lock(&mutex);
  assert(this->sp_waiters[lid] == 0);
  if(this->clt_locks[lid] != lock_client_cache::LOCKED && this->clt_locks[lid] != lock_client_cache::ACQUIRING && this->clt_locks[lid] != lock_client_cache::FREE)
	report_error("revoke_handler_1",this->clt_locks[lid]);
  assert(this->clt_locks[lid] == lock_client_cache::ACQUIRING || this->clt_locks[lid] == lock_client_cache::LOCKED || this->clt_locks[lid] == lock_client_cache::FREE);
  
  if(this->clt_locks[lid] == lock_client_cache::FREE){
	//tprintf("lcc: lock %d free, revoke directly\n",lid);
  	this->clt_locks[lid] = lock_client_cache::NONE;
	//this releases the lock immediately so just use rpc call right now
	//ret = cl->call(lock_protocol::release,lid,id,ddxz);
	assert(ret == 0);
  }
  else if(this->clt_locks[lid] == lock_client_cache::LOCKED || this->clt_locks[lid] == lock_client_cache::ACQUIRING){
  	while(this->clt_locks[lid] == lock_client_cache::ACQUIRING){
		//tprintf("lcc: lock %d ACQUIRING and wait...",lid);
		pthread_cond_wait(&dond,&mutex);
	}
	while(this->clt_locks[lid] == lock_client_cache::LOCKED){
		//tprintf("lcc: lock %d LOCKED and wait...",lid);
		pthread_cond_wait(&cond,&mutex);
	}
	this->clt_locks[lid] = lock_client_cache::NONE;
	//ret = cl->call(lock_protocol::release,lid,id,ddxz);
	assert(ret == 0);
  }
  //tprintf("lcc: %s revoked %d successfully\n",id.c_str(),lid);
  pthread_mutex_unlock(&mutex);
  pthread_mutex_unlock(&outex);

  return 0;
}

rlock_protocol::status
lock_client_cache::retry_handler(lock_protocol::lockid_t lid, 
                                 int & r)
{
  return 0;
}

void lock_client_cache::report_error(char * func,int code){
	tprintf("lock_client_cache: error in %s with an error code %d\n",func,code);
}

