#include "inode_manager.h"
#include <cstring>
#include <stdio.h>
#include "extent_client.h"

// disk layer -----------------------------------------

disk::disk()
{
  bzero(blocks, sizeof(blocks));
}

void
disk::read_block(blockid_t id, char *buf)
{
    memcpy(buf,blocks[id],BLOCK_SIZE);
}

void
disk::write_block(blockid_t id, const char *buf)
{
    memcpy(blocks[id],buf,BLOCK_SIZE);
}

// block layer -----------------------------------------

// Allocate a free disk block.
blockid_t
block_manager::alloc_block()
{
  /*
   * your code goes here.
   * note: you should mark the corresponding bit in block bitmap when alloc.
   * you need to think about which block you can start to be allocated.
   */
  for(blockid_t i = ACT_BLOCKID(0);i < BLOCK_NUM; i++){
      if(using_blocks[i] == 1){
          continue;
      }
      else{
          using_blocks[i] = 1;
          return i;

      }
  }
}

void
block_manager::free_block(uint32_t id)
{
  /* 
   * your code goes here.
   * note: you should unmark the corresponding bit in the block bitmap when free.
   */
  using_blocks[id] = 0;
  return;
}

// The layout of disk should be like this:
// |<-sb->|<-free block bitmap->|<-inode table->|<-data->|
block_manager::block_manager()
{
  d = new disk();

  // format the disk
  sb.size = BLOCK_SIZE * BLOCK_NUM;
  sb.nblocks = BLOCK_NUM;
  sb.ninodes = INODE_NUM;

}

void
block_manager::read_block(uint32_t id, char *buf)
{
  d->read_block(id, buf);
}

void
block_manager::write_block(uint32_t id, const char *buf)
{
  d->write_block(id, buf);
}

// inode layer -----------------------------------------

inode_manager::inode_manager()
{
  bm = new block_manager();
  uint32_t root_dir = alloc_inode(extent_protocol::T_DIR);
  if (root_dir != 1) {
    printf("\tim: error! alloc first inode %d, should be 1\n", root_dir);
    exit(0);
  }
}

/* Create a new file.
 * Return its inum. */
uint32_t
inode_manager::alloc_inode(uint32_t type)
{
  /* 
   * your code goes here.
   * note: the normal inode block should begin from the 2nd inode block.
   * the 1st is used for root_dir, see inode_manager::inode_manager().
   */
  static uint32_t inum = 1;
  char buf[BLOCK_SIZE];
  for( ; inum < INODE_NUM; inum++){
      bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
      if(((inode_t *)(buf))->type == 0){
          ((inode_t *)(buf)) -> type = type;
          bm->write_block(IBLOCK(inum, bm->sb.nblocks),buf);
          //printf("\nACCEPT TYPE: %d INODEID:%d\n",type,i);
          return inum++;
      }
  }
  return 0;
}

void
inode_manager::free_inode(uint32_t inum)
{
  /* 
   * your code goes here.
   * note: you need to check if the inode is already a freed one;
   * if not, clear it, and remember to write back to disk.
   */
  printf("FREE INODE %d\n",inum);
  inode_t * inodep = get_inode(inum);
  if(inodep== NULL){
      printf("ERROR:INODE EMPTY\n");
      return;
  }
  else{
      int ori_size = inodep->size;
      if(ori_size < NDIRECT*BLOCK_SIZE){
          int nblocks = ori_size/BLOCK_SIZE;
          for(int i = 0; i<nblocks;i++){
              bm->free_block(inodep->blocks[i]);
              //printf("FREE BLOCK %d\n",inodep->blocks[i]);
          }
          if(ori_size%BLOCK_SIZE){
              bm->free_block(inodep->blocks[nblocks]);
              //printf("FREE BLOCK %d\n",inodep->blocks[nblocks]);
          }
      }
      else{
          for(int i = 0; i<NDIRECT; i++){
              bm->free_block(inodep->blocks[i]);
          }
          int nblocks = (ori_size - BLOCK_SIZE*NDIRECT)/BLOCK_SIZE;
          int remains = (ori_size - BLOCK_SIZE*NDIRECT)%BLOCK_SIZE;

          uint32_t indirect_block[NINDIRECT];
          bm->read_block(inodep->blocks[NDIRECT],(char *)indirect_block);

          for(int i = 0; i<nblocks; i++){
              bm->free_block(indirect_block[i]);
          }
          if(remains){
              bm->free_block(indirect_block[nblocks]);
          }
      }

      inodep->type = 0;
      inodep->size = 0;
      put_inode(inum,inodep);
      printf("INODE %d FREED SUCCESFULLY\n",inum);
  }

  return;
}


/* Return an inode structure by inum, NULL otherwise.
 * Caller should release the memory. */
// return value is a copy, not the original ptr
struct inode* 
inode_manager::get_inode(uint32_t inum)
{
  struct inode *ino, *ino_disk;
  char buf[BLOCK_SIZE];

  printf("\tim: get_inode %d\n", inum);

  if (inum < 0 || inum >= INODE_NUM) {
    printf("\tim: inum out of range\n");
    return NULL;
  }

  bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
  // printf("%s:%d\n", __FILE__, __LINE__);

  ino_disk = (struct inode*)buf + inum%IPB;
  if (ino_disk->type == 0) {
    printf("\tim: inode not exist\n");
    return NULL;
  }

  ino = (struct inode*)malloc(sizeof(struct inode));
  *ino = *ino_disk;

  return ino;
}

void
inode_manager::put_inode(uint32_t inum, struct inode *ino)
{
  char buf[BLOCK_SIZE];
  struct inode *ino_disk;

  printf("\tim: put_inode %d\n", inum);
  if (ino == NULL)
    return;

  bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
  ino_disk = (struct inode*)buf + inum%IPB;
  *ino_disk = *ino;
  bm->write_block(IBLOCK(inum, bm->sb.nblocks), buf);
}

#define MIN(a,b) ((a)<(b) ? (a) : (b))

/* Get all the data of a file by inum. 
 * Return alloced data, should be freed by caller. */
void
inode_manager::read_file(uint32_t inum, char **buf_out, int *size)
{
  /*
   * your code goes here.
   * note: read blocks related to inode number inum,
   * and copy them to buf_Out
   */
  //if no need of indirect block

  inode_t * inodep = get_inode(inum);

  inodep->atime = clock();

  *size = inodep->size;
  int ssize = inodep->size;

  *buf_out = new char[(ssize/BLOCK_SIZE+1)*BLOCK_SIZE];

  //printf("READFILE INUM:%d SIZE:%d\n",inum,(ssize));

  //printf("\nGET:%d %d\n",inum,ssize);

  if(ssize <= NDIRECT*BLOCK_SIZE){
      int nblocks = ssize/BLOCK_SIZE;
      for(int i = 0; i<nblocks;i++){
          bm->read_block(inodep->blocks[i],*buf_out+i*BLOCK_SIZE);
          /*printf("TARGET READ BLOCK:%d %d\n",inodep->blocks[i],i);

          char buf[512];
          bm->read_block(inodep->blocks[i],buf);
          printf("Content:%d %512s\n",buf[0],buf);*/
      }
      if(ssize%BLOCK_SIZE){
          bm->read_block(inodep->blocks[nblocks],*buf_out+nblocks*BLOCK_SIZE);
          //printf("TARGET READ BLOCK:%d\n",inodep->blocks[nblocks]);
      }
  }
  else{
      for(int i = 0; i<NDIRECT; i++){
          bm->read_block(inodep->blocks[i],*buf_out+i*BLOCK_SIZE);
          //printf("TARGET READ BLOCK:%d\n",inodep->blocks[i]);
      }
      int nblocks = (ssize - BLOCK_SIZE*NDIRECT)/BLOCK_SIZE;
      int remains = (ssize - BLOCK_SIZE*NDIRECT)%BLOCK_SIZE;

      uint32_t indirect_block[NINDIRECT];
      bm->read_block(inodep->blocks[NDIRECT],(char *)indirect_block);

      for(int i = 0; i<nblocks; i++){
          bm->read_block(indirect_block[i],*buf_out+(NDIRECT+i)*BLOCK_SIZE);
          //printf("TARGET READ BLOCK:%d\n",indirect_block[i]);
      }
      if(remains){
          bm->read_block(indirect_block[nblocks],*buf_out+(NDIRECT+nblocks)*BLOCK_SIZE);
          //printf("TARGET READ BLOCK:%d\n",indirect_block[nblocks]);
      }
  }
  delete(inodep);
  //printf("ENDING...\n");
  return;
}

/* alloc/free blocks if needed */
void
inode_manager::write_file(uint32_t inum, const char *buf, int size)
{
  /*
   * your code goes here.
   * note: write buf to blocks of inode inum.
   * you need to consider the situation when the size of buf 
   * is larger or smaller than the size of original inode
   */
  //printf("WRITEFILE INUM:%d SIZE:%d\n",inum,size);

  if(size > MAXFILE*BLOCK_SIZE){
      printf("MAXSIZE EXCEEDED!");
      return;
  }

  inode_t * inodep = get_inode(inum);
  int type = inodep->type;
  // free in advance
  free_inode(inum);
  inodep->size = size;
  inodep->type = type;
  inodep->mtime = clock();
  inodep->atime = clock();
  inodep->ctime = clock();

  if (size <= NDIRECT * BLOCK_SIZE){
      // No need of Indirect Block
      int nblocks = size/BLOCK_SIZE;
      int remains = size%BLOCK_SIZE;
      int bid;
      for(int i = 0;i<nblocks;i++){
          bid = bm->alloc_block();
          inodep->blocks[i]=bid;
          bm->write_block(bid,buf+i*BLOCK_SIZE);

          /*printf("TARGET WRITE BLOCK:%d %d\n",bid,i);
          char buf[512];
          bm->read_block(inodep->blocks[i],buf);
          printf("Content:%512s\n",buf);*/
      }
      if(remains){
          bid=bm->alloc_block();
          inodep->blocks[nblocks]=bid;
          bm->write_block(bid,buf+nblocks*BLOCK_SIZE);
      }
      put_inode(inum,inodep);
      //printf("\nRES:%d %d\n",inum,get_inode(inum)->size);
  }
  else{
      // in need of indirect block
      int bid;
      for(int i = 0;i<NDIRECT;i++){
          bid = bm->alloc_block();
          inodep->blocks[i]=bid;
          bm->write_block(bid,buf+i*BLOCK_SIZE);
      }

      // indirect block
      int indirectBlock = bm->alloc_block();
      inodep->blocks[NDIRECT] = indirectBlock;

      uint32_t indirect_block_buf[NINDIRECT];

      int nblocks = (size-NDIRECT*BLOCK_SIZE)/BLOCK_SIZE;
      int remains = (size-NDIRECT*BLOCK_SIZE)%BLOCK_SIZE;
      for(int i = 0;i<nblocks;i++){
          bid = bm->alloc_block();
          indirect_block_buf[i] = bid;
          bm->write_block(bid,buf+NDIRECT*BLOCK_SIZE+i*BLOCK_SIZE);
      }
      bid=bm->alloc_block();
      indirect_block_buf[nblocks]=bid;
      bm->write_block(bid,buf+NDIRECT*BLOCK_SIZE+nblocks*BLOCK_SIZE);

      bm->write_block(indirectBlock,(char*)indirect_block_buf);

      put_inode(inum,inodep);
  }
  delete(inodep);
  return;
}

void
inode_manager::getattr(uint32_t inum, extent_protocol::attr &a)
{
  /*
   * your code goes here.
   * note: get the attributes of inode inum.
   * you can refer to "struct attr" in extent_protocol.h
   */
  inode_t * inodep = get_inode(inum);
  if(inodep!=NULL&&inodep->type!=0){
      a.atime = inodep->atime;
      a.ctime = inodep->ctime;
      a.mtime = inodep->mtime;
      a.size = inodep->size;
      a.type = inodep->type;
      delete(inodep);
      return;
  }
  else{
      a.type = 0;
      a.size = 0;
      return;
  }
}

void
inode_manager::remove_file(uint32_t inum)
{
  /*
   * your code goes here
   * note: you need to consider about both the data block and inode of the file
   */
  free_inode(inum);
  
  return;
}

// additional ones
void
inode_manager::setattr(uint32_t inum, size_t size){
    inode_t * inodep;
    inodep =get_inode(inum);
    inodep -> size = size;
    inodep -> ctime = clock();
    inodep -> mtime = clock();
    put_inode(inum,inodep);
    
    
}	
