#ifndef lock_server_cache_h
#define lock_server_cache_h

#include <string>

#include <map>
#include "lock_protocol.h"
#include "rpc.h"
#include "lock_server.h"

#include <cstdlib>
#include "tprintf.h"

typedef struct lock_entry_{
	bool locked;
	// if locked, there must be a current client
	std::string cur_clt;
	// waiting clients
	std::string wt_clt;
	lock_entry_(std::string s){
		locked = true;
		cur_clt = s;
	}
	lock_entry_(){}
} lock_entry;

class lock_server_cache {
 private:
  // lock state per lock and a function to find port number
 
  std::map<lock_protocol::lockid_t,lock_entry> lock_map;
  /*inline int id2port(std::string s){
	  int begin = s.find(':');
	  std::string port = s.substr(begin+1);
	  int res = atoi(port.c_str());
	  return res;
  }*/
  inline void report_error(char * func,int code){
  	tprintf("lock_server_cache: failed in function %s with an error code %d\n",func,code);
  }
  // concurrent protection
  pthread_mutex_t mutex;
  pthread_cond_t cond;
  // this is a lock which prevents too many acquires at same time
  int ready;

  int nacquire;
 public:
  lock_server_cache();
  lock_protocol::status stat(lock_protocol::lockid_t, int &);
  int acquire(lock_protocol::lockid_t, std::string id, int &);
  int release(lock_protocol::lockid_t, std::string id, int &);
};

#endif
