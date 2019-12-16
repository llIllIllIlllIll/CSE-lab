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
    ec = new extent_client();
    if (ec->put(1, "") != extent_protocol::OK)
        printf("error init root dir\n"); // XYB: init root dir
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
    printf("CHECK ISFILE:%d\n",inum);
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
/** Your code here for Lab...
 * You may need to add routines such as
 * readlink, issymlink here to implement symbolic link.
 * 
 * */

bool
yfs_client::isdir(inum inum)
{
    printf("CHECK ISDIR:%d\n",inum);
    // Oops! is this still correct when you implement symlink?
    //printf("CHECK ISFILE:%d\n",inum);
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

// get file info
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
    int r = OK;

    /*
     * your code goes here.
     * note: get the content of inode ino, and modify its content
     * according to the size (<, =, or >) content length.
     */
    
    ec->setattr(ino,size);

    return r;
}

int
yfs_client::create(inum parent, const char *name, mode_t mode, inum &ino_out)
{
    int r = OK;
    printf("\nCREATE FILE NAME:%s\n",name);

    /*
     * your code goes here.
     * note: lookup is what you need to check if file exist;
     * after create file or dir, you must remember to modify the parent infomation.
     */
    bool found;
    if(strlen(name)>DIRENTSIZE-sizeof(yfs_client::inum)){
        printf("NAME TOO LONG!\n");
        r = IOERR;
        return r;
    }
    if(lookup(parent,name,found,ino_out) == NOENT){
        ec->create(extent_protocol::T_FILE,ino_out);
        //printf("INO ALLOCATED: %d\n",ino_out);
        //next modify parent information
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
    }
    else{
        r = EXIST;
    }

    /*printf("CURRENT DIRECTORY CONTENT:");
    extent_protocol::attr a;
    ec->getattr(parent,a);
    printf("\nSIZE:%d TYPE:%d",a.size,a.type);
    std::string content;
    ec->get(parent,content);
    printf("\nACTUAL SIZE:%d\n",content.size());
    */
    


    return r;
}

int
yfs_client::mkdir(inum parent, const char *name, mode_t mode, inum &ino_out)
{
    int r = OK;
    printf("\nMKDIR: DIR: %d NAME:%s\n",parent,name);

    /*
     * your code goes here.
     * note: lookup is what you need to check if directory exist;
     * after create file or dir, you must remember to modify the parent infomation.
     */
    
    bool found;
    if(strlen(name)>DIRENTSIZE-sizeof(yfs_client::inum)){
        printf("NAME TOO LONG!\n");
        r = IOERR;
        return r;
    }

    if(lookup(parent,name,found,ino_out) == NOENT){
        ec->create(extent_protocol::T_DIR,ino_out);
        printf("INO ALLOCATED: %d\n",ino_out);
        //next modify parent information
        std::string old_content;
        ec->get(parent,old_content);
        char new_ent[DIRENTSIZE];
        ((dirent *)new_ent)->inum = ino_out;
        strncpy(((dirent*)new_ent)->name,name,strlen(name)+1);
        std::string new_content;
        new_content.assign(new_ent,DIRENTSIZE);
        old_content += new_content;
        ec->put(parent,old_content);
    }
    else{
        r = EXIST;
    }

    
    return r;
}

int
yfs_client::lookup(inum parent, const char *name, bool &found, inum &ino_out)
{
    int r = OK;

    printf("\nLOOKUP IN DIR: %d FILENAME: %s\n",parent,name);
    /*
     * your code goes here.
     * note: lookup file from parent dir according to name;
     * you should design the format of directory content.
     */
    
    extent_protocol::attr dir_attr;
    ec->getattr(parent,dir_attr);
    int dir_size = dir_attr.size;
    printf("\nDIR_SIZE: %d\n",dir_size);
    std::string dir_string;
    ec->get(parent,dir_string);
    dirent * direntp = (dirent *)(dir_string.c_str());
    for(int i = 0; i<dir_size/DIRENTSIZE;i++){
        // printf("\nCurrent [%dth] file name: %s\n",i,(direntp+i)->name);
        if(strncmp((direntp+i)->name,name,MAX(strlen(name),strlen((direntp+i)->name)))==0){
            found = true;
            ino_out = (direntp+i) -> inum;

            printf("FILE FOUND AT INO: %d\n",ino_out);
            return r;
        }
    }
    /*
    std::list<dirent> filelist;
    readdir(parent,filelist);
    std::list<dirent>::iterator it;
    for(it = filelist.begin(); it != filelist.end(); it++){
	if(strncmp(it->name,name,strlen(name))==0){
		found = true;
		ino_out = it->inum;
		return r;
	}
    }*/

    //printf("FILE NOT FOUND!\n",ino_out);
    found = false;
    r = NOENT;
    return r;
}

int
yfs_client::readdir(inum dir, std::list<dirent> &list)
{
    int r = OK;

    /*
     * your code goes here.
     * note: you should parse the dirctory content using your defined format,
     * and push the dirents to the list.
     */
    printf("\n*****************READDIR*******************\n");

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

        printf("DIRENT %d : [%d] %s\n",i,dirp->inum,dirp->name);
    }
    
    return r;
}

int
yfs_client::read(inum ino, size_t size, off_t off, std::string &data)
{
    int r = OK;

    /*
     * your code goes here.
     * note: read using ec->get().
     */
    std::string buf;
    ec->get(ino,buf);
    data.append(buf,off,size);
    printf("READ INUM: %d SIZE: %d OFF:%d\n CONTENT:%s\nSIZE:%d\n",ino,size,off,data.c_str(),data.length());
    return r;
}

int
yfs_client::write(inum ino, size_t size, off_t off, const char *data,
        size_t &bytes_written)
{
    int r = OK;

    /*
     * your code goes here.
     * note: write using ec->put().
     * when off > length of original file, fill the holes with '\0'.
     */
    printf("WRITE:INO %d SIZE %d OFF %d\n CONTENT:%s\n",ino,size,off,data);
    std::string buf,res;
    ec->get(ino,buf);
    int ori_length = buf.length();
    printf("ORIGINAL CONTENT:%s\n",buf.c_str());
    if(off<buf.length()){
    	bytes_written = size;
        res = buf.substr(0,off);
        res.append(data, size);

	if(res.length()<ori_length){
		res.append(buf,res.length(),ori_length-res.length());
	}

	ec->put(ino,res);
    }
    else{
        bytes_written = size+off-ori_length;
	buf.append(off-buf.length(),'\0');
        buf.append(data,size);
        ec->put(ino,buf);
    }
    printf("FINAL CONTENT:%s\nSIZE:%d\n",buf.c_str(),buf.length());
	
    return r;
}

int yfs_client::unlink(inum parent,const char *name)
{
    int r = OK;
    /*
     * your code goes here.
     * note: you should remove the file using ec->remove,
     * and update the parent directory content.
     */
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
    printf("UNLINK: [%d]:%s \n",i2del,n2del.c_str());
    if(i2del == INODE_NUM + 1){
	printf("ERROR:FILE %s NOT FOUND IN DIR [%d]!\n",name,parent);
	return NOENT;
    }
    else{

        if(isdir(i2del)){
		std::list<dirent> sublist;
		std::list<dirent>::iterator it;
		for(it = sublist.begin();it!=sublist.end();it++){
			unlink(i2del,it->name);
		}
	}        
	ec->remove(i2del);
	printf("INODE %d DELETED!\n",i2del);
	//need implement
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
    }
    
    return r;
}

int yfs_client::softpudding_symlink(const char * link, yfs_client::inum parent, const char * name, yfs_client::inum & ino){
	printf("SYMLINK: %s TO %s\nIN PARENT %d\n",link,name,parent);
	char buf[DIRENTSIZE];
	
	bool found = false;
	yfs_client::inum ino_out;
	lookup(parent,name,found,ino_out);
	if(found == true){
		printf("ERROR:FILE %s ALREADY EXISTS.",name);
		return EXIST;
	}

	extent_protocol::extentid_t strange_ino;
	ec->create(extent_protocol::T_SYMLINK,strange_ino);
	ec->put(strange_ino,std::string(link));
	((dirent*)buf)->inum = strange_ino;
	ino = strange_ino;
	printf("SYMLINK ALLOCATED AT:[%d]%s\n",ino,name);
	strncpy(((dirent*)buf)->name,name,strlen(name)+1);
	
	
	std::list<dirent> filelist;
	std::list<dirent>::iterator it;
	readdir(parent,filelist);
	filelist.push_back(*((dirent*)buf));
	
	std::string new_content;
	//char buf[DIRENTSIZE];
	for(it = filelist.begin(); it != filelist.end(); it++){
		printf("DIRENTINSERTING...%d %s",it->inum,it->name);
		if(true){
			strncpy(((dirent*)buf)->name,it->name,strlen(it->name)+1);
			((dirent*)buf)->inum=it->inum;
			new_content+=std::string(buf,DIRENTSIZE);	
		}	
	}
	ec->put(parent,new_content);

	printf("DIRCONTENT: [Supposed to be length %d]\n",new_content.length());
	readdir(parent,filelist);

	return OK;
}	

void yfs_client::softpudding_readlink(yfs_client::inum strange_inum, std::string & content){
	printf("READLINK %d\n");
	ec->get(strange_inum,content);
	printf("READLINK CONTENT:%s\n",content.c_str());
}
