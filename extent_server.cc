// the extent server implementation

#include "extent_server.h"
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "handle.h"

extent_server::extent_server() 
{
  im = new inode_manager();
}

int extent_server::create(std::string cid, uint32_t type, extent_protocol::extentid_t &id)
{
  // alloc a new inode and return inum
  printf("extent_server: create inode\n");
  id = im->alloc_inode(type);
  // owner_map[id] = cid;
  return extent_protocol::OK;
}

int extent_server::put(std::string cid, extent_protocol::extentid_t id, std::string buf, int &)
{
  id &= 0x7fffffff;
  
  const char * cbuf = buf.c_str();
  int size = buf.size();
  im->write_file(id, cbuf, size);
  
  return extent_protocol::OK;
}

int extent_server::get(std::string cid, extent_protocol::extentid_t id, std::string &buf)
{
  printf("extent_server: get %lld\n", id);

  id &= 0x7fffffff;

  int size = 0;
  char *cbuf = NULL;

  int r;
  if (owner_map.find(id) != owner_map.end())
    handle(owner_map[id]).safebind()->call(rextent_protocol::flush, id, r);
  owner_map[id] = cid;
  printf("inode %lld owner is switched to %s", id, cid.c_str());

  im->read_file(id, &cbuf, &size);
  if (size == 0)
    buf = "";
  else {
    buf.assign(cbuf, size);
    free(cbuf);
  }

  return extent_protocol::OK;
}

int extent_server::getattr(std::string cid, extent_protocol::extentid_t id, extent_protocol::attr &a)
{
  printf("extent_server: getattr %lld\n", id);

  id &= 0x7fffffff;
  
  extent_protocol::attr attr;
  if (owner_map.find(id) != owner_map.end()) {
    printf("%s", owner_map[id].c_str());
    handle(owner_map[id]).safebind()->call(rextent_protocol::sync_meta, id, attr);
  }
  else {
    memset(&attr, 0, sizeof(attr));
    im->getattr(id, attr);
  }
  a = attr;
  
  return extent_protocol::OK;
}

int extent_server::remove(std::string cid, extent_protocol::extentid_t id, int &)
{
  printf("extent_server: write %lld\n", id);

  id &= 0x7fffffff;

  int r;
  if (owner_map.find(id) != owner_map.end())
    handle(owner_map[id]).safebind()->call(rextent_protocol::clear, id, r);

  im->remove_file(id);
 
  return extent_protocol::OK;
}

