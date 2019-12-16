// extent client interface.

#ifndef extent_client_h
#define extent_client_h

#include <string>
#include "extent_protocol.h"
#include "extent_server.h"
#include "lock_client.h"
/* add for lab 4*/
#include <map>
#include <pthread.h>

class InodeData{
	public:
	bool valid;
	DataAndAttr daa;	
};

class extent_client {
 private:
  extent_server *es;
  rpcc * cl;
  /* lab4 cache*/
  std::map<extent_protocol::extentid_t,InodeData> ec_cache;
  /* rpc */
  int ec_port;
  std::string hostname;
  std::string id;
  /* status */
  enum ecstatus{OK};

 public:
  extent_client(std::string);
  extent_client();

  extent_protocol::status create(uint32_t type, extent_protocol::extentid_t &eid);
  extent_protocol::status get(extent_protocol::extentid_t eid, 
			                        std::string &buf);
  extent_protocol::status getattr(extent_protocol::extentid_t eid, 
				                          extent_protocol::attr &a);
  extent_protocol::status put(extent_protocol::extentid_t eid, std::string buf);
  extent_protocol::status remove(extent_protocol::extentid_t eid);
  //additional
  extent_protocol::status setattr(extent_protocol::extentid_t,int);
  /* for lab 4*/
  // write corresponding content back to extent server
  int writeback(extent_protocol::extentid_t,std::string &);
  // do nothing if already cached inode; otherwise apply for inode
  DataAndAttr * acquire(extent_protocol::extentid_t);

  static int last_port;
};

#endif 


