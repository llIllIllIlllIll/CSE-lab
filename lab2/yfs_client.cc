// yfs client.  implements FS operations using extent and lock server
#include "yfs_client.h"
#include "extent_client.h"
#include "extent_protocol.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cstring>
#include <string>
yfs_client::yfs_client()
{
    ec = new extent_client();

}

yfs_client::yfs_client(std::string extent_dst, std::string lock_dst)
{
    ec = new extent_client(extent_dst);
    lc = new lock_client(lock_dst);
    if (ec->put(1, "") != extent_protocol::OK)
        printf("error init root dir\n");
}

yfs_client::inum
yfs_client::n2i(std::string n)
{
    std::istringstream ist(n);
    unsigned long long finum;
    ist >> finum;
    return finum;
}

std::string
yfs_client::filename(inum inum)
{
    std::ostringstream ost;
    ost << inum;
    return ost.str();
}

bool
yfs_client::isfile(inum inum)
{
    extent_protocol::attr a;

    if (ec->getattr(inum, a) != extent_protocol::OK) {
        printf("error getting attr\n");
        return false;
    }

    if (a.type == extent_protocol::T_FILE) {
        printf("isfile: %lld is a file\n", inum);
        return true;
    } 
    printf("isfile: %lld is not a file\n", inum);
    return false;
}


bool
yfs_client::isdir(inum inum)
{
    printf("CHECK ISDIR:%d\n",inum);
    extent_protocol::attr a;

   
    if (ec->getattr(inum, a) != extent_protocol::OK) {
	printf("error getting attr\n");
        return false;
    }

    if (a.type == extent_protocol::T_DIR) {
        printf("isdir: %lld is a dir\n", inum);
        return true;
    } 
    printf("isfile: %lld is a not dir\n", inum);
    return false;

}

int
yfs_client::getfile(inum inum, fileinfo &fin)
{
    int r = OK;
    printf("getfile %016llx\n", inum);
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK || a.type == 0) {
        r = IOERR;
        goto release;
    }

    fin.atime = a.atime;
    fin.mtime = a.mtime;
    fin.ctime = a.ctime;
    fin.size = a.size;
    printf("getfile %016llx -> sz %llu\n", inum, fin.size);

release:
    return r;
}

// get dir info
int
yfs_client::getdir(inum inum, dirinfo &din)
{
    int r = OK;
    printf("getdir %016llx\n", inum);
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }
    din.atime = a.atime;
    din.mtime = a.mtime;
    din.ctime = a.ctime;

release:
    return r;
}


#define EXT_RPC(xx) do { \
    if ((xx) != extent_protocol::OK) { \
        printf("EXT_RPC Error: %s:%d \n", __FILE__, __LINE__); \
        r = IOERR; \
        goto release; \
    } \
} while (0)

// Only support set size of attr
int
yfs_client::setattr(inum ino, size_t size)
{
    int n = size;
    std::string content;
    extent_protocol::attr a;
    extent_protocol::status ret;
    if((ret=ec->getattr(ino,a))!=extent_protocol::OK){
	printf("getattr error\n");
	return ret;
    }
    ec->get(ino,content);
    if(a.size<size)
	content+=std::string(size-a.size,'\0');
    else if(a.size>size)
	content=content.substr(0,size);
    ec->put(ino,content);
    return ret;
}

int
yfs_client::create(inum parent, const char *name, mode_t mode, inum &ino_out)
{
    int r = OK;    
    bool found;
    if(strlen(name)>DIRENTSIZE-sizeof(yfs_client::inum)){
        printf("NAME TOO LONG!\n");
        r = IOERR;
        return r;
    }
    lc->acquire(parent);
    if(lookup(parent,name,found,ino_out) == NOENT){
        ec->create(extent_protocol::T_FILE,ino_out);

	lc->acquire(ino_out);
        
        std::string old_content;
        ec->get(parent,old_content);
        dirent new_ent;
        new_ent.inum = ino_out;
        
        strncpy(new_ent.name,name,strlen(name)+1);
        new_ent.name[strlen(name)]='\0';

        std::string new_content;
        new_content.assign((char *)&new_ent,DIRENTSIZE);

        old_content += new_content;
        ec->put(parent,old_content);
	
	lc->release(ino_out);
    }
    else{
        r = EXIST;
    }
    lc->release(parent);
    return r;
}

int
yfs_client::mkdir(inum parent, const char *name, mode_t mode, inum &ino_out)
{
   
    int r = OK;
   

    bool found;
    if(strlen(name)>DIRENTSIZE-sizeof(yfs_client::inum)){
        printf("NAME TOO LONG!\n");
        r = IOERR;
        return r;
    }

    lc -> acquire(parent);

    if(lookup(parent,name,found,ino_out) == NOENT){
        
	ec->create(extent_protocol::T_DIR,ino_out);

	lc->acquire(ino_out);

        std::string old_content;


        ec->get(parent,old_content);
        
        dirent new_ent;
        new_ent.inum = ino_out;
        
        strncpy(new_ent.name,name,strlen(name)+1);
        new_ent.name[strlen(name)]='\0';

        std::string new_content;
        new_content.assign((char *)&new_ent,DIRENTSIZE);

        old_content += new_content;
        ec->put(parent,old_content);

	lc->release(ino_out);
    }
    else{
        r = EXIST;
    }

    lc -> release(parent);

    return r;
}

int
yfs_client::lookup(inum parent, const char *name, bool &found, inum &ino_out)
{
   
    int r = OK;

    
    extent_protocol::attr dir_attr;
    ec->getattr(parent,dir_attr);
    int dir_size = dir_attr.size;
    //printf("\nDIR_SIZE: %d\n",dir_size);
    std::string dir_string;
    ec->get(parent,dir_string);
    dirent * direntp = (dirent *)(dir_string.c_str());
    for(int i = 0; i<dir_size/DIRENTSIZE;i++){
        // printf("\nCurrent [%dth] file name: %s\n",i,(direntp+i)->name);
        if(strncmp((direntp+i)->name,name,MAX(strlen(name),strlen((direntp+i)->name)))==0){
            found = true;
            ino_out = (direntp+i) -> inum;

            //printf("FILE FOUND AT INO: %d\n",ino_out);
            return r;
        }
    }
    found = false;
    r = NOENT;
    return r;
}

int
yfs_client::readdir(inum dir, std::list<dirent> &list)
{
    int r = OK;

    extent_protocol::attr dir_attr;
    ec->getattr(dir,dir_attr);

    if(dir_attr.type == 0 || dir_attr.type == extent_protocol::T_FILE){
        printf("\nERROR: INUM %d IS NOT A DIR!\n",dir);
        r = RPCERR;
        return r;
    }

    int dir_size = dir_attr.size;
    std::string dir_string;
    ec->get(dir,dir_string);
    char buf[dir_size];
    memcpy(buf,dir_string.c_str(),dir_size);
    dirent* direntp = (dirent*) buf;
    for(int i = 0; i<dir_size/DIRENTSIZE;i++){
        dirent* dirp=  new dirent;
	//direntp+i -> name +1
        strncpy(dirp->name, (direntp+i)->name, strlen((direntp+i)->name)+1);
        dirp->inum = (direntp+i)->inum;
        list.push_back(*dirp);

        //printf("DIRENT %d : [%d] %s\n",i,dirp->inum,dirp->name);
    }
    
    return r;
}

int
yfs_client::read(inum ino, size_t size, off_t off, std::string &data)
{
    int r = OK;
    lc->acquire(ino);
    ec->get(ino,data);
    lc->release(ino);

    if(off<=data.size()){
    	data=data.substr(off,size);
    }
    else{
    	data="";
    }
    return r;
}

int
yfs_client::write(inum ino, size_t size, off_t off, const char *data,
        size_t &bytes_written)
{
    int r = OK;
    std::string buf,res;

    lc->acquire(ino);

    ec->get(ino,buf);
    int ori_length = buf.length();
    
    if(off<buf.length()){
    	bytes_written = size;
        res = buf.substr(0,off);
	std::string dd(data,size);
        res.append(dd);

	if(res.length()<ori_length){
		res.append(buf,res.length(),ori_length-res.length());
	}

	ec->put(ino,res);
    }
    else{
        bytes_written = size+off-ori_length;
	buf.append(off-buf.length(),'\0');
	std::string dd(data,size);
        buf.append(dd);
        ec->put(ino,buf);
    }
    //printf("FINAL CONTENT:%s\nSIZE:%d\n",buf.c_str(),buf.length());
	
    lc->release(ino);
    return r;
}

int yfs_client::unlink(inum parent,const char *name)
{

    lc -> acquire(parent);
    int r = OK;
    
    //lock parent since i need to modify dir content

    std::list<dirent> filelist;
    
    this->readdir(parent,filelist);
    std::list<dirent>::iterator it;
    int i2del=INODE_NUM+1;
    std::string n2del;

    for(it = filelist.begin(); it != filelist.end(); it++){
	if(strncmp(it->name,name,strlen(name)+1)==0){
		i2del = it->inum;
                n2del.assign(it->name,strlen(it->name)+1);
		break;
	}	
    }

    if(i2del == INODE_NUM + 1){
	lc->release( parent);
	return NOENT;
    }
    else{
	lc->acquire(i2del);
        if(isdir(i2del)){
		std::list<dirent> sublist;
		std::list<dirent>::iterator it;
		for(it = sublist.begin();it!=sublist.end();it++){
			unlink(i2del,it->name);
		}
	}        
	ec->remove(i2del);

       	std::string new_content;
	char buf[DIRENTSIZE];
	for(it = filelist.begin(); it != filelist.end(); it++){
		if(it->inum != i2del){
			strncpy(((dirent*)buf)->name,it->name,strlen(it->name)+1);
			((dirent*)buf)->inum=it->inum;
			new_content.append(buf,DIRENTSIZE);
			
		}	
	}
	ec->put(parent,new_content);
	
	lc->release(i2del);
        lc->release(parent);	
    }

    return r;
}

int yfs_client::softpudding_symlink(const char * link, yfs_client::inum parent, const char * name, yfs_client::inum & ino){
	char buf[DIRENTSIZE];
	
	bool found = false;
	yfs_client::inum ino_out;
	lookup(parent,name,found,ino_out);
	if(found == true){
		return EXIST;
	}

	extent_protocol::extentid_t strange_ino;
	ec->create(extent_protocol::T_SYMLINK,strange_ino);
	ec->put(strange_ino,std::string(link));
	((dirent*)buf)->inum = strange_ino;
	ino = strange_ino;
	strncpy(((dirent*)buf)->name,name,strlen(name)+1);
	
	
	std::list<dirent> filelist;
	std::list<dirent>::iterator it;
	readdir(parent,filelist);
	filelist.push_back(*((dirent*)buf));
	
	std::string new_content;
	for(it = filelist.begin(); it != filelist.end(); it++){
		if(true){
			strncpy(((dirent*)buf)->name,it->name,strlen(it->name)+1);
			((dirent*)buf)->inum=it->inum;
			new_content+=std::string(buf,DIRENTSIZE);	
		}	
	}
	ec->put(parent,new_content);

	readdir(parent,filelist);

	
	return OK;
}	

void yfs_client::softpudding_readlink(yfs_client::inum strange_inum, std::string & content){
	ec->get(strange_inum,content);
}
