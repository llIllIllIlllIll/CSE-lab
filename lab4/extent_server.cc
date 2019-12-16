// the extent server implementation

#include "extent_server.h"
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

/* add for lab 4*/
#include <cassert>
#include <handle.h>
#include "tprintf.h"
extent_server::extent_server() 
{
  im = new inode_manager();
  pthread_mutex_init(&mutex,NULL);
  cond = PTHREAD_COND_INITIALIZER;
}

int extent_server::create(int type, std::string id, extent_protocol::extentid_t &inodeid)
{
  // alloc a new inode and return inum
  printf("extent_server: create inode\n");
  inodeid = im->alloc_inode(type);
   
  // v0.2 create at the same time may give the entry to client
  pthread_mutex_lock(&mutex);
  this->inode_entry_map.insert(std::make_pair(inodeid,inode_entry(id)));
  pthread_mutex_unlock(&mutex);		
  return extent_protocol::OK;
}

int extent_server::put(extent_protocol::extentid_t id, std::string buf, int & r)
{
  printf("extent_server: put %d %d bytes\n",id,buf.length());
  id &= 0x7fffffff;
  
  const char * cbuf = buf.c_str();
  int size = buf.size();
  printf("write file to im\n");
  im->write_file(id, cbuf, size);

  return extent_protocol::OK;
}

int extent_server::get(extent_protocol::extentid_t id, std::string &buf)
{
  printf("extent_server: get %lld\n", id);

  id &= 0x7fffffff;

  int size = 0;
  char *cbuf = NULL;

  im->read_file(id, &cbuf, &size);
  if (size == 0)
    buf = "";
  else {
    buf.assign(cbuf, size);
    free(cbuf);
  }
 
  return extent_protocol::OK;
}

int extent_server::getattr(extent_protocol::extentid_t id, extent_protocol::attr &a)
{
  printf("extent_server: getattr %lld\n", id);

  id &= 0x7fffffff;
  
  extent_protocol::attr attr;
  memset(&attr, 0, sizeof(attr));
  im->getattr(id, attr);
  a = attr;

  return extent_protocol::OK;
}

int extent_server::remove(extent_protocol::extentid_t id, int & r)
{
  printf("extent_server: write %lld\n", id);

  id &= 0x7fffffff;
  im->remove_file(id);
 
  inode_entry_map[id].acquired = false;
  inode_entry_map.erase(id);

  return extent_protocol::OK;
}
//additional
int extent_server::setattr(extent_protocol::extentid_t id,int size, int & r){
	printf("extent_server: setattr\n");
	im->setattr(id,size);
	return extent_protocol::OK;
}

/* for lab 4 */
int extent_server::acquire(extent_protocol::extentid_t inodeid,std::string id,std::string & content){
	int ddxz;
	extent_protocol::spstatus ret = extent_protocol::SP_OK;
	// don't allow acquire to be called concurrently
	pthread_mutex_lock(&mutex);
	// three kinds of requires can keep going:
	// 1. the inode has never been taken before
	// 2. no one is waiting for current inode
	// 3. the client is the one the should take the inode from current holder
	
	// V0.2 check inode's existence to support acuiqre by create
	//if(im -> get_inode(inodeid) == NULL)
	//	im -> alloc_inode(inodeid);

	//tprintf("extent_server: client %s acquring for inode %d\n",id.c_str(),inodeid);
	while(inode_entry_map.count(inodeid)>0 && !inode_entry_map[inodeid].wt_clt.empty() && inode_entry_map[inodeid].wt_clt != id){
		pthread_cond_wait(&cond,&mutex);
	}
	// case 1	
	if(this->inode_entry_map.count(inodeid)<=0){
		//tprintf("extent_server: empty entry found; response with OK\n");
		this->inode_entry_map.insert(std::make_pair(inodeid,inode_entry(id)));
		
		std::string data;extent_protocol::attr attr;
		
		this->get(inodeid,data);
		this->getattr(inodeid,attr);

		DataAndAttr daa;
		daa.data = data;
		daa.attr = attr;
		content = DataAndAttr::toString(daa);
		
		//tprintf("extent_server: response with content size %d\n",content.size());
		
		pthread_mutex_unlock(&mutex);
	}
	// case 3
	else if(this->inode_entry_map[inodeid].acquired == false){
		//tprintf("extent_server: free entry; response with OK\n");
		
		inode_entry & ie = inode_entry_map[inodeid];
		assert(ie.wt_clt == id);
		ie.acquired = true;
		ie.cur_clt = id;
		ie.wt_clt = "";
                
		std::string data;extent_protocol::attr attr;
		
		this->get(inodeid,data);
		this->getattr(inodeid,attr);
		DataAndAttr daa;
		daa.data = data;
		daa.attr = attr;
		content = DataAndAttr::toString(daa);

		//tprintf("extent_server: response with file size %d\n",content.size());
		
		pthread_cond_signal(&cond);
		pthread_mutex_unlock(&mutex);
	}
	// case 2
	else{
		//tprintf("extent_server: busy entry; response with RETRY\n");

		ret = extent_protocol::SP_RETRY;
		inode_entry & ie = inode_entry_map[inodeid];
		if(ie.cur_clt == id){
			pthread_cond_signal(&cond);
			pthread_mutex_unlock(&mutex);
			return extent_protocol::SP_RED;
		}
		ie.wt_clt = id;

		handle handler(ie.cur_clt);
		rpcc * cl = handler.safebind();
		assert(cl != NULL);
		
		ret = extent_protocol::SP_RETRY;

		//tprintf("extent_server: call client %s to writeback\n",ie.cur_clt.c_str());
		std::string content;
		int r = cl -> call(extent_protocol::writeback_handler,inodeid,content);
		assert(r == 0);
 		DataAndAttr daa = DataAndAttr::toDaa(content);
		this -> put(inodeid,daa.data,ddxz);
		im -> setattr_2(inodeid,daa.attr);

		
  
		//pthread_mutex_lock(&mutex);
		ie.wt_clt = id;
		ie.acquired = false;
		//pthread_mutex_unlock(&mutex);
		
		pthread_cond_signal(&cond);
		pthread_mutex_unlock(&mutex);
	}
	return ret;

}
// renew is given up in Straman V0.3
int extent_server::renew(extent_protocol::extentid_t inodeid,std::string id, std::string content, int & r){
	DataAndAttr daa;
	int ddxz;
	daa = DataAndAttr::toDaa(content);
	this -> put(inodeid,daa.data,ddxz);
	im -> setattr_2(inodeid,daa.attr);
	//tprintf("extent_server: client %s writeback inode %d with size %d and attr size %d\n",id.c_str(),inodeid,content.size(),daa.attr.size);
	return extent_protocol::SP_OK;
}
