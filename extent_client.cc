// RPC stubs for clients to talk to extent_server

#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <time.h>

int extent_client::last_port = 666;

extent_client::extent_client(std::string dst)
{
  sockaddr_in dstsock;
  make_sockaddr(dst.c_str(), &dstsock);
  cl = new rpcc(dstsock);
  if (cl->bind() != 0) {
    printf("extent_client: bind failed\n");
  }
  srand(time(NULL) ^ last_port);
  rextent_port = ((rand() % 32000) | (0x1 << 10));
  const char *hname;
  hname = "127.0.0.1";
  std::ostringstream host;
  host << hname << ":" << rextent_port;
  cid = host.str();
  last_port = rextent_port;
  rpcs *resrpc = new rpcs(rextent_port);
  resrpc->reg(rextent_protocol::flush, this, &extent_client::flush_handler);
  resrpc->reg(rextent_protocol::sync_meta, this, &extent_client::sync_meta_handler);
  resrpc->reg(rextent_protocol::clear, this, &extent_client::clear_handler);
}

// a demo to show how to use RPC
extent_protocol::status
extent_client::create(uint32_t type, extent_protocol::extentid_t &id)
{
  extent_protocol::status ret = extent_protocol::OK;
  // Your lab2 part1 code goes here
  ret = cl->call(extent_protocol::create, cid, type, id);
  // printf("ret: %d\n", ret);
  // data_map[id] = new data_cache_t();
  // data_cache_t *cache = data_map[id];
  // cache->stat = OWN;
  return ret;
}

extent_protocol::status
extent_client::get(extent_protocol::extentid_t eid, std::string &buf)
{
  extent_protocol::status ret = extent_protocol::OK;
  // Your lab2 part1 code goes here
  if (data_map.find(eid) == data_map.end()) 
    data_map[eid] = new data_cache_t();
  data_cache_t *cache = data_map[eid];
  if (cache->stat == OWN) {
    if (eid != 1) {
      assert(metadata_map[eid]);
      metadata_cache_t *meta = metadata_map[eid];
      meta->metadata.atime = (uint) time(0); 
    }
    buf = cache->data;
    std::cout << "extent_client_cache: read " << buf << " from inode " << eid << std::endl;
  } else {
    printf("extent_client: fetching inode %lld\n", eid);
    if (metadata_map.find(eid) == metadata_map.end())
      metadata_map[eid] = new metadata_cache_t();
    metadata_cache_t *meta = metadata_map[eid];
    if (meta->stat == NONE) {
      ret = getattr(eid, meta->metadata);
    }
    ret = cl->call(extent_protocol::get, cid, eid, buf) | ret;
    cache->data = buf;
    cache->stat = OWN;
  }
  return ret;
}

extent_protocol::status
extent_client::getattr(extent_protocol::extentid_t eid, 
		       extent_protocol::attr &attr)
{
  extent_protocol::status ret = extent_protocol::OK;
  if (metadata_map.find(eid) == metadata_map.end())
    metadata_map[eid] = new metadata_cache_t();
  metadata_cache_t *cache = metadata_map[eid];
  if (cache->stat != OWN) {
    printf("extent_client: getting inode %lld attr\n", eid);
    ret = cl->call(extent_protocol::getattr, cid, eid, attr);
    printf("got\n");
    cache->metadata = attr;
    cache->stat = OWN;
  } else {
    attr = cache->metadata;
  }
  //printf("ret: %d\n", ret);
  return ret;
}

extent_protocol::status
extent_client::put(extent_protocol::extentid_t eid, std::string buf)
{
  extent_protocol::status ret = extent_protocol::OK;
  if (data_map.find(eid) == data_map.end()) 
    data_map[eid] = new data_cache_t();
  data_cache_t *cache = data_map[eid];
  if (cache->stat != OWN) {
    get(eid, cache->data);
  }
  std::cout << "extent_client_cache: write " << buf << " to inode " << eid << std::endl;
  if (metadata_map.find(eid) == metadata_map.end())
    metadata_map[eid] = new metadata_cache_t();
  metadata_cache_t *meta = metadata_map[eid];
  meta->metadata.size = buf.size();
  meta->metadata.atime = (uint) time(0);
  meta->metadata.ctime = (uint) time(0);
  meta->metadata.mtime = (uint) time(0);
  // cl->call(extent_protocol::put, cid, eid, buf, r);
  cache->data = buf;
  return ret;
}

extent_protocol::status
extent_client::remove(extent_protocol::extentid_t eid)
{
  extent_protocol::status ret = extent_protocol::OK;
  // Your lab2 part1 code goes here
  int r;
  ret = cl->call(extent_protocol::remove, cid, eid, r);
  //printf("ret: %d\n", ret);
  return ret;
}

extent_protocol::status
extent_client::flush(extent_protocol::extentid_t eid) {
  int r;
  data_cache_t *cache = data_map[eid];
  if (cache->stat != OWN) {
    printf("Oops! Flush when not owning?");
    exit(0);
  }
  int ret = cl->call(extent_protocol::put, cid, eid, cache->data, r);
  printf("extent_client: flushing inode %lld\n", eid);
  return ret;
}

rextent_protocol::status
extent_client::flush_handler(extent_protocol::extentid_t eid, int &r) {
  flush(eid);
  return clear_handler(eid, r);
}

rextent_protocol::status
extent_client::clear_handler(extent_protocol::extentid_t eid, int &r) {
  rextent_protocol::status ret = rextent_protocol::OK;
  data_map[eid]->stat = NONE;
  metadata_map[eid]->stat = NONE;
  return ret;
}

rextent_protocol::status
extent_client::sync_meta_handler(extent_protocol::extentid_t eid, extent_protocol::attr &a) {
  rextent_protocol::status ret = rextent_protocol::OK;
  a = metadata_map[eid]->metadata;
  return ret;
}
