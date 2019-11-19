#ifndef lock_server_cache_h
#define lock_server_cache_h

#include <string>

#include <map>
#include <set>
#include "lock_protocol.h"
#include "rpc.h"
#include "lock_server.h"

class lock_server_cache
{

private:
  int nacquire;
  enum status
  {
    NONE,
    LOCKED,
    REVOKING,
    RETRYING
  };

  typedef struct lock_state
  {
    std::string owner;
    std::string retrying;
    std::set<std::string> waiting;
    status state;
  } lock_state_t;

  std::map<lock_protocol::lockid_t, lock_state_t *> lock_map;
  pthread_mutex_t mutex;

public:
  lock_server_cache();

  ~lock_server_cache();

  lock_protocol::status stat(lock_protocol::lockid_t, int &);

  int acquire(lock_protocol::lockid_t, std::string id, int &);

  int release(lock_protocol::lockid_t, std::string id, int &);
};

#endif