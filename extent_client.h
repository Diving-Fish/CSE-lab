// extent client interface.

#ifndef extent_client_h
#define extent_client_h

#include <string>
#include "extent_protocol.h"
#include "extent_server.h"

class extent_client {
 private:
  rpcc *cl;
  int rextent_port;
  std::string cid;
  enum status {
    NONE = 0,
    OWN
  };
  typedef struct data_cache {
    extent_client::status stat;
    std::string data;
    data_cache() {
      stat = NONE;
      data.clear();
    }
  } data_cache_t;
  typedef struct metadata_cache {
    extent_client::status stat;
    extent_protocol::attr metadata;
    metadata_cache() {
      stat = NONE;
    }
  } metadata_cache_t;
  std::map<extent_protocol::extentid_t, data_cache_t *> data_map;
  std::map<extent_protocol::extentid_t, metadata_cache_t *> metadata_map;

 public:
  static int last_port;
  extent_client(std::string dst);

  extent_protocol::status create(uint32_t type, extent_protocol::extentid_t &eid);
  extent_protocol::status get(extent_protocol::extentid_t eid, 
			                        std::string &buf);
  extent_protocol::status getattr(extent_protocol::extentid_t eid, 
				                          extent_protocol::attr &a);
  extent_protocol::status put(extent_protocol::extentid_t eid, std::string buf);
  extent_protocol::status remove(extent_protocol::extentid_t eid);
  extent_protocol::status flush(extent_protocol::extentid_t eid);
  rextent_protocol::status flush_handler(extent_protocol::extentid_t eid, int &r);
  rextent_protocol::status clear_handler(extent_protocol::extentid_t eid, int &r);
  rextent_protocol::status sync_meta_handler(extent_protocol::extentid_t eid, extent_protocol::attr &a);
};

#endif 

