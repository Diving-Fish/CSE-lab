// yfs client.  implements FS operations using extent and lock server
#include "yfs_client.h"
#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

yfs_client::yfs_client(std::string extent_dst, std::string lock_dst)
{
  ec = new extent_client(extent_dst);
  lc = new lock_client_cache(lock_dst);
  lc->acquire(1);
  std::string buf;
  if (ec->put(1, "") != extent_protocol::OK)
      printf("error init root dir\n"); // XYB: init root dir
  lc->release(1);
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
    lc->acquire(inum);
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        printf("error getting attr\n");
        return false;
    }
    lc->release(inum);
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
yfs_client::issymlink(inum inum)
{
    extent_protocol::attr a;
    lc->acquire(inum);
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        printf("error getting attr\n");
        return false;
    }
    lc->release(inum);
    if (a.type == extent_protocol::T_SYMLINK) {
        printf("issymlink: %lld is a symlink\n", inum);
        return true;
    } 
    printf("issymlink: %lld is not a symlink\n", inum);
    return false;
}

bool
yfs_client::isdir(inum inum)
{
    extent_protocol::attr a;
    lc->acquire(inum);
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        printf("error getting attr\n");
        return false;
    }
    lc->release(inum);
    if (a.type == extent_protocol::T_DIR) {
        printf("isdir: %lld is a dir\n", inum);
        return true;
    } 
    printf("isdir: %lld is not a dir\n", inum);
    return false;
}

int
yfs_client::getfile(inum inum, fileinfo &fin)
{
    int r = OK;

    printf("getfile %016llx\n", inum);
    extent_protocol::attr a;
    lc->acquire(inum);
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }
    lc->release(inum);
    fin.atime = a.atime;
    fin.mtime = a.mtime;
    fin.ctime = a.ctime;
    fin.size = a.size;
    printf("getfile %016llx -> sz %llu\n", inum, fin.size);

release:
    return r;
}

int
yfs_client::getdir(inum inum, dirinfo &din)
{
    int r = OK;

    printf("getdir %016llx\n", inum);
    extent_protocol::attr a;
    lc->acquire(inum);
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }
    lc->release(inum);
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
    std::string buf;
    lc->acquire(ino);
    r = ec->get(ino, buf);
    if (r != extent_protocol::OK) {
        return r;
    }
    buf.resize(size);
    r = ec->put(ino, buf);
    lc->release(ino);
    if (r != extent_protocol::OK) {
        return r;
    }
    return r;
}

int
yfs_client::create(inum parent, const char *name, mode_t mode, inum &ino_out)
{
    int r = OK;
    
    /*
     * your code goes here.
     * note: lookup is what you need to check if file exist;
     * after create file or dir, you must remember to modify the parent infomation.
     */

    bool found;
    inum ino;
    lc->acquire(parent);
    __lookup(parent, name, found, ino);
    if (found) {
        lc->release(parent);
        return EXIST;
    }
    ec->create(extent_protocol::T_FILE, ino_out);

    std::string buf, str;
    if (ec->get(parent, buf) != extent_protocol::OK) {
        exit(0);
    }
    entry_t entry;
    entry.inum = ino_out;
    entry.name_length = strlen(name);
    //entry.name = (char *) malloc(entry.name_length);
    memcpy(entry.name, name, entry.name_length);
    str.assign((char *) (&entry), sizeof(entry_t));
    buf += str;
    ec->put(parent, buf);
    lc->release(parent);
    return r;
}

int
yfs_client::mkdir(inum parent, const char *name, mode_t mode, inum &ino_out)
{
    int r = OK;

    /*
     * your code goes here.
     * note: lookup is what you need to check if directory exist;
     * after create file or dir, you must remember to modify the parent infomation.
     */
    bool found;
    inum ino;
    lc->acquire(parent);
    __lookup(parent, name, found, ino);
    if (found) {
        lc->release(parent);
        return EXIST;
    }
    ec->create(extent_protocol::T_DIR, ino_out);

    std::string buf, str;
    if (ec->get(parent, buf) != extent_protocol::OK) {
        exit(0);
    }
    entry_t entry;
    entry.inum = ino_out;
    entry.name_length = strlen(name);
    //entry.name = (char *) malloc(entry.name_length);
    memcpy(entry.name, name, entry.name_length);
    str.assign((char *) (&entry), sizeof(entry_t));
    buf += str;
    ec->put(parent, buf);
    lc->release(parent);
    return r;
}

int
yfs_client::lookup(inum parent, const char *name, bool &found, inum &ino_out)
{
    lc->acquire(parent);
    int r = __lookup(parent, name, found, ino_out);
    lc->release(parent);
    return r;
}

int
yfs_client::__lookup(inum parent, const char *name, bool &found, inum &ino_out)
{
    /*
     * your code goes here.
     * note: lookup file from parent dir according to name;
     * you should design the format of directory content.
     */
    
    found = false;
    std::list<entry_t> file_list;
    __readdir(parent, file_list);
    for (std::list<entry_t>::iterator it = file_list.begin(); it != file_list.end(); it++) {
        std::string _name;
        _name.assign(it->name, it->name_length);
        if (_name == name) {
            found = true;
            ino_out = it->inum;
            return EXIST;
        }
    }
    return OK;
}

int
yfs_client::readdir(inum dir, std::list<dirent> &list)
{
    std::list<entry_t> entry_list;
    lc->acquire(dir);
    int r = __readdir(dir, entry_list);
    lc->release(dir);

    for (std::list<entry_t>::iterator it = entry_list.begin(); it != entry_list.end(); it++) {
        struct dirent d;
        d.inum = it->inum;
        d.name.assign(it->name, it->name_length);
        list.push_back(d);
    }
    return r;
}

int
yfs_client::__readdir(inum dir, std::list<entry_t> &list)
{
    int r = OK;

    /*
     * your code goes here.
     * note: you should parse the dirctory content using your defined format,
     * and push the dirents to the list.
     */

    std::string buf;
    if (ec->get(dir, buf) != extent_protocol::OK) {
        exit(0);
        return IOERR;
    }
    extent_protocol::attr attr;
    ec->getattr(dir, attr);
    if (attr.type != extent_protocol::T_DIR) {
        exit(0);
    }
    int len = buf.length();
    const char *entries = buf.c_str();
    for (uint i = 0; i < len / sizeof(entry_t); i++) {
        entry_t entry;
        memcpy(&entry, (entries + i * sizeof(entry_t)), sizeof(entry_t));
        list.push_back(entry);
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
    lc->acquire(ino);
    std::string buf;
    r = ec->get(ino, buf);
    if (r != OK) return r;
    if ( (uint) off >= buf.length()) {
        data.clear();
        return r;
    }
    data = buf.substr(off, size);
    lc->release(ino);
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
    std::string buf;
    lc->acquire(ino);
    ec->get(ino, buf);
    std::string str;
    str.assign(data, size);
    if ((uint) off <= buf.size()) {
        buf.replace(off, size, str);
        bytes_written = size;
    } else {
        size_t old_size = buf.size();
        buf.resize(size + off, '\0');
        buf.replace(off, size, str);
        bytes_written = size + off - old_size;
    }
    ec->put(ino, buf);
    //ec->flush(ino);
    lc->release(ino);
    return r;
}

int yfs_client::unlink(inum parent, const char *name)
{
     /*
     * your code goes here.
     * note: you should remove the file using ec->remove,
     * and update the parent directory content.
     */
    int r = OK;
    bool found = false;
    lc->acquire(parent);
    std::list<entry_t> file_list;
    __readdir(parent, file_list);
    std::list<entry_t>::iterator it = file_list.begin();
    for (; it != file_list.end(); it++) {
        std::string _name;
        _name.assign(it->name, it->name_length);
        if (_name == name) {
            found = true;
            break;
        }
    }
    if (!found) {
        lc->release(parent);
        return NOENT;
    }

    ec->remove(it->inum);
    file_list.erase(it);

    std::string buf, append;
    for (it = file_list.begin(); it != file_list.end(); it++) {
        entry_t entry;
        entry.inum = it->inum;
        entry.name_length = it->name_length;
        //entry.name = (char *) malloc(entry.name_length);
        memcpy(entry.name, it->name, entry.name_length);
        append.assign((char *) (&entry), sizeof(entry_t));
        buf += append;
    }

    ec->put(parent, buf);
    lc->release(parent);
    return r;
}

int
yfs_client::readlink(inum ino, std::string &data) {
    int r = OK;
    std::string buf;
    lc->acquire(ino);
    r = ec->get(ino, buf);
    data = buf;
    lc->release(ino);
    return r;
}

int
yfs_client::symlink(inum parent, const char *name, const char *link, inum &ino_out) {

    int r = OK;

    std::string buf, append;
    lc->acquire(parent);
    r = ec->get(parent, buf);

    bool found;
    inum id;
    __lookup(parent, name, found, id);
    if (found) {
        lc->release(parent);
        return EXIST;
    }
    r = ec->create(extent_protocol::T_SYMLINK, ino_out);
    r = ec->put(ino_out, std::string(link));
    entry_t entry;
    entry.inum = ino_out;
    entry.name_length = (uint) strlen(name);
    //entry.name = (char *) malloc(entry.name_length);
    memcpy(entry.name, name, entry.name_length);
    append.assign((char *) (&entry), sizeof(entry_t));
    buf += append;
    ec->put(parent, buf);
    lc->release(parent);
    return r;
}