// RPC stubs for clients to talk to lock_server, and cache the locks
// see lock_client.cache.h for protocol details.

#include "lock_client_cache.h"
#include "rpc.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <sys/time.h>
#include "tprintf.h"

int lock_client_cache::last_port = 0;

lock_client_cache::lock_client_cache(std::string xdst,
                                     class lock_release_user *_lu)
    : lock_client(xdst), lu(_lu)
{
  srand(time(NULL) ^ last_port);
  rlock_port = ((rand() % 32000) | (0x1 << 10));
  const char *hname;
  // VERIFY(gethostname(hname, 100) == 0);
  hname = "127.0.0.1";
  std::ostringstream host;
  host << hname << ":" << rlock_port;
  id = host.str();
  last_port = rlock_port;
  rpcs *rlsrpc = new rpcs(rlock_port);
  rlsrpc->reg(rlock_protocol::revoke, this, &lock_client_cache::revoke_handler);
  rlsrpc->reg(rlock_protocol::retry, this, &lock_client_cache::retry_handler);
  pthread_mutex_init(&mutex, NULL);
}

lock_protocol::status
lock_client_cache::acquire(lock_protocol::lockid_t lid) {
  pthread_mutex_lock(&mutex);
  if (lock_map.find(lid) == lock_map.end())
    lock_map[lid] = new lock_state_t();
  lock_state_t *ls = lock_map[lid];
  pthread_cond_t *thread = new pthread_cond_t;
  pthread_cond_init(thread, NULL);
  if (ls->threads.empty())
  {
    status s = ls->state;
    ls->threads.push_back(thread);
    if (s == FREE)
    {
      ls->state = LOCKED;
      pthread_mutex_unlock(&mutex);
      return lock_protocol::OK;
    }
    else if (s == NONE)
    {
      return wait(ls, lid, thread);
    }
    else
    {
      pthread_cond_wait(thread, &mutex);
      return wait(ls, lid, thread);
    }
  }
  else
  {
    ls->threads.push_back(thread);
    pthread_cond_wait(thread, &mutex);
    switch (ls->state)
    {
    case FREE:
      ls->state = LOCKED;
    case LOCKED:
      pthread_mutex_unlock(&mutex);
      return lock_protocol::OK;
    case NONE:
      return wait(ls, lid, thread);
    default:
      exit(0);
    }
  }
}

lock_protocol::status
lock_client_cache::release(lock_protocol::lockid_t lid) {
  pthread_mutex_lock(&mutex);
  int r;
  int ret = rlock_protocol::OK;
  lock_state_t *ls = lock_map[lid];
  bool revoking = false;
  if (ls->message == REVOKE)
  {
    revoking = true;
    ls->state = RELEASING;

    pthread_mutex_unlock(&mutex);
    ret = cl->call(lock_protocol::release, lid, id, r);
    pthread_mutex_lock(&mutex);
    ls->message = EMPTY;
    ls->state = NONE;
  }
  else
  {
    ls->state = FREE;
  }
  delete ls->threads.front();
  ls->threads.pop_front();
  if (ls->threads.size() >= 1)
  {
    if (!revoking)
      ls->state = LOCKED;
    pthread_cond_signal(ls->threads.front());
  }
  pthread_mutex_unlock(&mutex);
  return ret;
}

rlock_protocol::status
lock_client_cache::revoke_handler(lock_protocol::lockid_t lid, int &) {
  int r;
  int ret = rlock_protocol::OK;
  pthread_mutex_lock(&mutex);
  lock_state_t *ls = lock_map[lid];
  if (ls->state == FREE)
  {
    ls->state = RELEASING;
    pthread_mutex_unlock(&mutex);
    ret = cl->call(lock_protocol::release, lid, id, r);
    pthread_mutex_lock(&mutex);
    ls->state = NONE;
    if (ls->threads.size() >= 1)
    {
      pthread_cond_signal(ls->threads.front());
    }
  }
  else
  {
    ls->message = REVOKE;
  }
  pthread_mutex_unlock(&mutex);
  return ret;
}

rlock_protocol::status
lock_client_cache::retry_handler(lock_protocol::lockid_t lid, int &) {
  int ret = rlock_protocol::OK;
  pthread_mutex_lock(&mutex);
  lock_state_t *ls = lock_map[lid];
  ls->message = RETRY;
  pthread_cond_signal(ls->threads.front());
  pthread_mutex_unlock(&mutex);
  return ret;
}

lock_protocol::status 
lock_client_cache::wait(lock_client_cache::lock_state_t *ls, lock_protocol::lockid_t lid, pthread_cond_t *thread) {
  int r;
  ls->state = ACQUIRING;
  while (ls->state == ACQUIRING)
  {
    pthread_mutex_unlock(&mutex);
    //printf("I'm acquiring\n");
    int ret = cl->call(lock_protocol::acquire, lid, id, r);
    pthread_mutex_lock(&mutex);
    if (ret == lock_protocol::OK)
    {
      ls->state = LOCKED;
      pthread_mutex_unlock(&mutex);
      //printf("I've acquired\n");
      return lock_protocol::OK;
    }
    else
    {
      if (ls->message == EMPTY)
      {
        pthread_cond_wait(thread, &mutex);
        //printf("I'm waiting\n");
        ls->message = EMPTY;
      }
      else {
        ls->message = EMPTY;
      }
    }
  }
  exit(0);
}

lock_client_cache::~lock_client_cache()
{
  pthread_mutex_lock(&mutex);
  for (std::map<lock_protocol::lockid_t, lock_state_t *>::iterator it = lock_map.begin(); it != lock_map.end(); it++) delete it->second;
  pthread_mutex_unlock(&mutex);
}