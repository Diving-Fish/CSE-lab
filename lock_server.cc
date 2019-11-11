// the lock server implementation

#include "lock_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>

void Pthread_mutex_lock(pthread_mutex_t *mutex) {
  if (pthread_mutex_lock(mutex) != 0) {
    printf("pthread_mutex_lock error, exit");
    exit(0);
  }
}

void Pthread_mutex_unlock(pthread_mutex_t *mutex) {
  if (pthread_mutex_unlock(mutex) != 0) {
    printf("pthread_mutex_unlock error, exit");
    exit(0);
  }
}

void Pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex) {
  if (pthread_cond_wait(cond, mutex) != 0) {
    printf("pthread_cond_wait error, exit");
    exit(0);
  }
}

void Pthread_cond_signal(pthread_cond_t *cond) {
  if (pthread_cond_signal(cond) != 0) {
    printf("pthread_cond_signal error, exit");
    exit(0);
  }
}

lock_server::lock_server():
  nacquire (0)
{
  if (pthread_mutex_init(&mutex, NULL) != 0) {
    printf("pthread_mutex_init error, exit");
    exit(0);
  }
}

lock_server::~lock_server() {
  for (std::map<lock_protocol::lockid_t, pthread_cond_t *>::iterator it = cond_map.begin(); it != cond_map.end(); it++) {
    delete (it->second);
  }
}

lock_protocol::status
lock_server::stat(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
  printf("stat request from clt %d\n", clt);
  r = nacquire;
  return ret;
}

lock_protocol::status
lock_server::acquire(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
  r = ret;
	// Your lab2 part2 code goes here
  Pthread_mutex_lock(&mutex);
  if (lock_map.find(lid) == lock_map.end()) {
    pthread_cond_t *cond_ptr = new pthread_cond_t();
    if (pthread_cond_init(cond_ptr, NULL) != 0) {
      // printf("pthread_cond_init error, exit\n");
      exit(0);
    }
    cond_map[lid] = cond_ptr;
    lock_map[lid] = 1;
    // printf("lock #%lld granted!\n", lid);
    Pthread_mutex_unlock(&mutex);
    return ret;
  }
  if (lock_map[lid] == 0) {
    lock_map[lid] = 1;
    // printf("lock #%lld granted!\n", lid);
    Pthread_mutex_unlock(&mutex);
    return ret;
  } else {
    // printf("lock #%lld waiting...\n", lid);
    while (lock_map[lid] == 1) Pthread_cond_wait(cond_map[lid], &mutex);
    lock_map[lid] = 1;
    // printf("lock #%lld granted!\n", lid);
  }
  Pthread_mutex_unlock(&mutex);
  return ret;
}

lock_protocol::status
lock_server::release(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
	// Your lab2 part2 code goes here
  Pthread_mutex_lock(&mutex);
  lock_map[lid] = 0;
  // printf("lock #%lld released!\n", lid);
  Pthread_cond_signal(cond_map[lid]);
  Pthread_mutex_unlock(&mutex);
  r = ret;
  return ret;
}
