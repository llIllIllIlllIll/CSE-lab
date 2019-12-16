// this is the extent server

#ifndef extent_server_h
#define extent_server_h

#include <string>
#include <map>
#include "extent_protocol.h"
#include "inode_manager.h"

#include <pthread.h>
/* A structure to represent the status of an inode */
typedef struct inode_entry_{
	bool acquired;
	std::string cur_clt;
	std::string wt_clt;
	inode_entry_(std::string s){
		acquired = true;
		cur_clt = s;
	}
	inode_entry_(){}
} inode_entry;

class extent_server {
 private:
  //pthread_mutex_t mutex;
 protected:
#if 0
  typedef struct extent {
    std::string data;
    struct extent_protocol::attr attr;
  } extent_t;
  std::map <extent_protocol::extentid_t, extent_t> extents;
#endif
  inode_manager *im;
  /* Below area defined by YX */
  std::map<extent_protocol::extentid_t,inode_entry> inode_entry_map;
  pthread_mutex_t mutex;
  pthread_cond_t cond;

 public:
  extent_server();

  int create(int, std::string ,extent_protocol::extentid_t &);
  int put(extent_protocol::extentid_t id, std::string, int &);
  int get(extent_protocol::extentid_t id, std::string &);
  int getattr(extent_protocol::extentid_t id, extent_protocol::attr &);
  int remove(extent_protocol::extentid_t id, int &);
  // additional ones
  int setattr(extent_protocol::extentid_t id,int size, int &);
  /* for lab 4, try to get data & metadata */
  int acquire(extent_protocol::extentid_t,std::string,std::string &);
  int renew(extent_protocol::extentid_t,std::string,std::string,int &);
};

#endif 







