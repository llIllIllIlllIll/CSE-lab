// RPC stubs for clients to talk to extent_server

#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <time.h>

extent_client::extent_client(std::string dst)
{
  sockaddr_in dstsock;
  make_sockaddr(dst.c_str(),&dstsock);
  cl = new rpcc(dstsock);
  if(cl->bind()<0){
  	printf("extent_client bind failed\n");
  }
}

extent_client::extent_client(){
  es = new extent_server();
}

extent_protocol::status
extent_client::create(uint32_t type, extent_protocol::extentid_t &id)
{
 
  extent_protocol::status ret = extent_protocol::OK;
  ret = cl->call(extent_protocol::create,type, id);
  printf("\n extent_client: create %d\n",id);

  return ret;
}

extent_protocol::status
extent_client::get(extent_protocol::extentid_t eid, std::string &buf)
{
  extent_protocol::status ret = extent_protocol::OK;
  printf("\n extent_clint: get %d \n");
  ret = cl->call(extent_protocol::get,eid, buf); 
  return ret;
}

extent_protocol::status
extent_client::getattr(extent_protocol::extentid_t eid, 
		       extent_protocol::attr &attr)
{
  extent_protocol::status ret = extent_protocol::OK;
  printf("\n extent_client: getattr %d\n",eid);
  ret = cl->call(extent_protocol::getattr,eid, attr);
  return ret;
}

extent_protocol::status
extent_client::put(extent_protocol::extentid_t eid, std::string buf)
{
  extent_protocol::status ret = extent_protocol::OK;
  printf("\n extent_client: put %d \n",eid);
  int r;
  ret = cl->call(extent_protocol::put,eid, buf, r);
  return ret;
}

extent_protocol::status
extent_client::remove(extent_protocol::extentid_t eid)
{
  extent_protocol::status ret = extent_protocol::OK;
  int r;
  printf("\n extent_client: remove %d \n",eid);
  ret = cl->call(extent_protocol::remove,eid, r);
  return ret;
}

// additional 
extent_protocol::status
 extent_client::setattr(extent_protocol::extentid_t id, int size){
	int r;
	extent_protocol::status ret;
	printf("\n extent_client: setattr %d \n",id);
	ret = cl->call(extent_protocol::setattr,id,size,r);
	return extent_protocol::OK;
}


