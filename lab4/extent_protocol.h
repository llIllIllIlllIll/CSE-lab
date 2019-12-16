// extent wire protocol

#ifndef extent_protocol_h
#define extent_protocol_h

#include "rpc.h"
#include <string>
#include <cstring>

#include "tprintf.h"
class extent_protocol {
 public:
  typedef int status;
  typedef unsigned long long extentid_t;
  enum xxstatus { OK = 0, RPCERR, NOENT, IOERR };
  /* spstatus for lab 4 only */
  enum spstatus { SP_OK = 0, SP_RETRY, SP_RED};
  enum rpc_numbers {
    put = 0x6001,
    get,
    getattr,
    remove,
    create,
    setattr,
    writeback_handler = 0x8001,
    acquire,
    renew
  };

  enum types {
    T_DIR = 1,
    T_FILE,
    T_SYMLINK,
    T_BUSY
  };

  struct attr {
    uint32_t type;
    unsigned int atime;
    unsigned int mtime;
    unsigned int ctime;
    unsigned int size;
  };
};

class DataAndAttr{
	public:
		std::string data;
		extent_protocol::attr attr;	
		static std::string toString(DataAndAttr daa){
			int datasize = daa.data.length();
			char * s = new char[4+datasize+sizeof(extent_protocol::attr)];
			*((int *)s) = datasize;
			memcpy(s+4,daa.data.c_str(),datasize);
			memcpy(s+4+datasize,(char*)&daa.attr,sizeof(extent_protocol::attr));
			return std::string(s,4+datasize+sizeof(extent_protocol::attr));
		}
		static DataAndAttr toDaa(std::string s){
			const char * content = s.c_str();
			int datasize =  *((int *)content);
			std::string data = s.substr(4,datasize);
			extent_protocol::attr attr;
			memcpy((char*)&attr,content+4+datasize,sizeof(extent_protocol::attr));
			//tprintf("bksxdd: %d %d %d\n",attr.size,attr.type,attr.mtime);
			DataAndAttr daa;
			daa.data = data;
			daa.attr = attr;
			return daa;
		}
};




inline unmarshall &
operator>>(unmarshall &u, extent_protocol::attr &a)
{
  u >> a.type;
  u >> a.atime;
  u >> a.mtime;
  u >> a.ctime;
  u >> a.size;
  return u;
}

inline marshall &
operator<<(marshall &m, extent_protocol::attr a)
{
  m << a.type;
  m << a.atime;
  m << a.mtime;
  m << a.ctime;
  m << a.size;
  return m;
}

#endif 

