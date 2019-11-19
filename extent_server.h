// this is the extent server

#ifndef extent_server_h
#define extent_server_h

#include <string>
#include <map>
#include "extent_protocol.h"
#include "inode_manager.h"

class extent_server {
 protected:
#if 0
  typedef struct extent {
    std::string data;
    struct extent_protocol::attr attr;
  } extent_t;
  std::map <extent_protocol::extentid_t, extent_t> extents;
#endif
  inode_manager *im;
  std::map<extent_protocol::extentid_t, std::string> owner_map;

 public:
  extent_server();

  int create(std::string cid, uint32_t type, extent_protocol::extentid_t &id);
  int put(std::string cid, extent_protocol::extentid_t id, std::string, int &);
  int get(std::string cid, extent_protocol::extentid_t id, std::string &);
  int getattr(std::string cid, extent_protocol::extentid_t id, extent_protocol::attr &);
  int remove(std::string cid, extent_protocol::extentid_t id, int &);
};

#endif 







