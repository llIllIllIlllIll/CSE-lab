// the lock server implementation

#include "lock_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>

lock_server::lock_server():
  nacquire (0)
{
  pthread_mutex_init(&mutex,NULL);
  cond = PTHREAD_COND_INITIALIZER;
}

lock_protocol::status
lock_server::stat(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
  printf("stat request from clt %d\n", clt);
  r = nacquire;
  return ret;
}

lock_protocol::status
lock_server::acquire(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
	// Your lab2 part2 code goes here
  	// Here add some protection code later
  printf("clt %d acquiring %d\n",clt,lid);
  pthread_mutex_lock(&mutex);
  if(lock_pool.count(lid)<=0){
    lock_pool.insert(std::make_pair(lid,true));
  }
  else{
    while(lock_pool[lid]){
    	pthread_cond_wait(&cond,&mutex);
    }
  }
  lock_pool[lid] = true;
  pthread_mutex_unlock(&mutex);
  //printf("lock %lld has been successfully acquired by clt %d\n",lid,clt);
  // lock get
  //r = nacquire;
  return ret;
}

lock_protocol::status
lock_server::release(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
	// Your lab2 part2 code goes here
  printf("clt %d releasing %d\n",clt,lid);
  pthread_mutex_lock(&mutex);
  lock_pool[lid] = false;
  pthread_cond_signal(&cond);
  pthread_mutex_unlock(&mutex);
  
  r = nacquire;
  return ret;
}
