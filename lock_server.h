// this is the lock server
// the lock client has a similar interface

#ifndef lock_server_h
#define lock_server_h

#include <string>
#include "lock_protocol.h"
#include "lock_client.h"
#include "rpc.h"
#include <pthread.h>

void Pthread_mutex_lock(pthread_mutex_t *mutex);

void Pthread_mutex_unlock(pthread_mutex_t *mutex);

void Pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex);

void Pthread_cond_signal(pthread_cond_t *cond);

class lock_server {

 protected:
  int nacquire;
  pthread_mutex_t mutex;
  std::map<lock_protocol::lockid_t, pthread_cond_t *> cond_map;
  std::map<lock_protocol::lockid_t, int> lock_map;

 public:
  lock_server();
  ~lock_server();
  lock_protocol::status stat(int clt, lock_protocol::lockid_t lid, int &);
  lock_protocol::status acquire(int clt, lock_protocol::lockid_t lid, int &);
  lock_protocol::status release(int clt, lock_protocol::lockid_t lid, int &);
};

#endif 







