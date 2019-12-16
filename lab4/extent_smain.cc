#include "rpc.h"
#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "extent_server.h"
// Main loop of extent server

int
main(int argc, char *argv[])
{
  int count = 0;

  if(argc != 2){
    fprintf(stderr, "Usage: %s port\n", argv[0]);
    exit(1);
  }

  setvbuf(stdout, NULL, _IONBF, 0);

  char *count_env = getenv("RPC_COUNT");
  if(count_env != NULL){
    count = atoi(count_env);
  }
#ifndef SOFTPUDDING
#define SOFTPUDDING
  rpcs server(atoi(argv[1]), count);
  extent_server es;

  server.reg(extent_protocol::get, &es, &extent_server::get);
  server.reg(extent_protocol::getattr, &es, &extent_server::getattr);
  server.reg(extent_protocol::put, &es, &extent_server::put);
  server.reg(extent_protocol::remove, &es, &extent_server::remove);
  server.reg(extent_protocol::setattr,&es, &extent_server::setattr);
  server.reg(extent_protocol::create, &es, &extent_server::create);
  /*below for lab4*/
  server.reg(extent_protocol::acquire, &es, &extent_server::acquire);
  server.reg(extent_protocol::renew, &es, &extent_server::renew);
#endif
  while(1)
    sleep(1000);
}

