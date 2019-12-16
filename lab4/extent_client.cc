// RPC stubs for clients to talk to extent_server

#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include "rpc.h"
#include "tprintf.h"

int extent_client::last_port = 0x1998;

extent_client::extent_client(std::string dst)
{
  sockaddr_in dstsock;
  make_sockaddr(dst.c_str(),&dstsock);
  cl = new rpcc(dstsock);
  if(cl->bind()<0){
  	printf("extent_client bind failed\n");
  }
  printf("extent_client: binded with extent_server successfully\n");

  srand(time(NULL)^last_port);
  ec_port = ((rand()%32000)|(0x1<<10));
  const char *hname;
  hname = "127.0.0.1";
  std::ostringstream host;
  host << hname << ":" << ec_port;
  this->id = host.str();
  std::cout<< "extent_client: addr: "<< this->id<< std::endl;

  last_port = ec_port;
  rpcs * rlsrpc = new rpcs(ec_port);
  rlsrpc->reg(extent_protocol::writeback_handler,this,&extent_client::writeback);
  printf("extent_client: %s initialized successfully\n",id.c_str());
}

extent_client::extent_client(){
  es = new extent_server();
}

/* lab4 : create remains the same, no need to change */
extent_protocol::status
extent_client::create(uint32_t type, extent_protocol::extentid_t &inodeid)
{
 
  extent_protocol::status ret = extent_protocol::OK;
  ret = cl->call(extent_protocol::create,type,this->id ,inodeid);
  assert(ret == 0);
  printf("\n extent_client: create %d\n",inodeid);
  InodeData inodeData;
  inodeData.valid = true;
  inodeData.daa.data = "";
  inodeData.daa.attr.type = type;
  inodeData.daa.attr.atime = clock();
  inodeData.daa.attr.mtime = clock();
  inodeData.daa.attr.ctime = clock();
  inodeData.daa.attr.size = 0;
  ec_cache.insert(std::make_pair(inodeid,inodeData));
  return ret;
}
/* lab4 : get from cache*/
extent_protocol::status
extent_client::get(extent_protocol::extentid_t eid, std::string &buf)
{
  extent_protocol::status ret = extent_protocol::OK;
  printf("\n extent_clint: get %d \n");
  
  DataAndAttr * daap = acquire(eid);
  buf = daap->data;
  tprintf("extent_client: get file size %d\n",buf.size());
  daap->attr.atime = clock();

  return ret;
}

extent_protocol::status
extent_client::getattr(extent_protocol::extentid_t eid, 
		       extent_protocol::attr &attr)
{
  extent_protocol::status ret = extent_protocol::OK;
  printf("\n extent_client: getattr %d\n",eid);

  DataAndAttr * daap = acquire(eid);
  attr = daap->attr;

  return ret;
}

extent_protocol::status
extent_client::put(extent_protocol::extentid_t eid, std::string buf)
{
  extent_protocol::status ret = extent_protocol::OK;
  printf("\n extent_client: put %d \n",eid);
  int r;
  
  DataAndAttr * daap = acquire(eid);
  daap -> data = buf;
  daap -> attr.mtime = clock();
  daap -> attr.ctime = clock();
  daap -> attr.atime = clock();
  daap -> attr.size = buf.size();
  return ret;
}

extent_protocol::status
extent_client::remove(extent_protocol::extentid_t eid)
{
  extent_protocol::status ret = extent_protocol::OK;
  int r;
  printf("\n extent_client: remove %d \n",eid);
  //ret = cl->call(extent_protocol::remove,eid, r);
  //assert(ret == 0);

  DataAndAttr * daap = acquire(eid);
  daap -> attr.type = 0;
  daap -> attr.size = 0;
  daap -> data = "";

  return ret;
}

// additional 
extent_protocol::status
 extent_client::setattr(extent_protocol::extentid_t id, int size){
	int r;
	extent_protocol::status ret;
	printf("\n extent_client: setattr %d \n",id);
	
	DataAndAttr * daap = acquire(id);
	/* this may be a problem, but I don't care right now*/
	daap->attr.size = size;
	daap->attr.ctime = clock();
	daap->attr.mtime = clock();

	return ret;
}

DataAndAttr * extent_client::acquire(extent_protocol::extentid_t inodeid){
	int ret;
	tprintf("extent_client: %s acquires for %d\n",id.c_str(),inodeid);
	// client needs to ask server for daa
	if(ec_cache.count(inodeid)<=0 || ec_cache[inodeid].valid == false){
		InodeData inodedata;
		std::string content;
		ret = cl->call(extent_protocol::acquire,inodeid,this->id,content);
		if(ret == extent_protocol::SP_OK){
			tprintf("extent_client: server responsed with OK, content size %d\n",content.length());
			inodedata.valid = true;
			inodedata.daa = DataAndAttr::toDaa(content);

			ec_cache[inodeid] = inodedata;
			return &(ec_cache[inodeid].daa);
		}
		else if(ret == extent_protocol::SP_RETRY){
			tprintf("extent_client: server responsed with RETRY\n");
			while(ret == extent_protocol::SP_RETRY){
				ret = cl->call(extent_protocol::acquire,inodeid,this->id,content);
			}
			assert(ret == extent_protocol::SP_OK);
			if(ret == extent_protocol::SP_OK){
				inodedata.valid = true;
				inodedata.daa = DataAndAttr::toDaa(content);

				ec_cache[inodeid] = inodedata;
				return &(ec_cache[inodeid].daa);
			}
		}
		else if(ret == extent_protocol::SP_RED){
			tprintf("extent_client: server responsed with RED\n");
			assert(ec_cache.count(inodeid)==1);
			ec_cache[inodeid].valid = true;
			return &(ec_cache[inodeid].daa);
		}
	}
	// just return current daa
	else{
		tprintf("extent_client: found inode in cache; content size: %d\n",ec_cache[inodeid].daa.data.size());
		return &(ec_cache[inodeid].daa);
	}
}

int extent_client::writeback(extent_protocol::extentid_t inodeid, std::string & c){
	int ddxz,ret;
	assert(ec_cache.count(inodeid) == 1);
	//assert(ec_cache[inodeid].valid == true);
	InodeData & inodedata = ec_cache[inodeid];
	inodedata.valid = false;

	std::string content = DataAndAttr::toString(inodedata.daa);
        
	tprintf("extent_client: %s writeback %d to server of size %d and attr size %d\n",id.c_str(),inodeid,content.size(),inodedata.daa.attr.size);
	tprintf("bkggkdd: %d %d %d\n",inodedata.daa.attr.size,inodedata.daa.attr.type,inodedata.daa.attr.mtime);

	c = content;
	return extent_protocol::SP_OK;
	
}
