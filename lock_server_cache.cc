#include "lock_server_cache.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <set>
#include <arpa/inet.h>
#include <sys/time.h>
#include "lang/verify.h"
#include "handle.h"
#include "tprintf.h"

lock_server_cache::lock_server_cache()
{
  VERIFY(pthread_mutex_init(&mutex, NULL) == 0);
}

int lock_server_cache::acquire(lock_protocol::lockid_t lid, std::string clientID,
                               int &)
{
  int r;
  std::map<lock_protocol::lockid_t, lock_state_t *>::iterator it;
  pthread_mutex_lock(&mutex);

  it = lock_map.find(lid);
  if (it == lock_map.end())
  {
    lock_map[lid] = new lock_state_t;
    lock_map[lid]->state = NONE;
  }

  lock_state_t *ls = lock_map[lid];

  switch (ls->state)
  {
  case NONE:
    ls->state = LOCKED;
    ls->owner = clientID;
    pthread_mutex_unlock(&mutex);
    return lock_protocol::OK;

  case LOCKED:
    ls->waiting.insert(clientID);
    ls->state = REVOKING;
    pthread_mutex_unlock(&mutex);
    handle(ls->owner).safebind()->call(rlock_protocol::revoke, lid, r);
    return lock_protocol::RETRY;

  case REVOKING:
    ls->waiting.insert(clientID);
    pthread_mutex_unlock(&mutex);
    return lock_protocol::RETRY;

  case RETRYING:
    if (clientID == ls->retrying)
    {
      assert(!ls->waiting.count(clientID));
      ls->retrying.clear();
      ls->state = LOCKED;
      ls->owner = clientID;
      if (!ls->waiting.empty())
      {
        ls->state = REVOKING;
        pthread_mutex_unlock(&mutex);
        handle(clientID).safebind()->call(rlock_protocol::revoke, lid, r);
        return lock_protocol::OK;
      }
      else
      {
        pthread_mutex_unlock(&mutex);
        return lock_protocol::OK;
      }
    }
    else
    {
      ls->waiting.insert(clientID);
      pthread_mutex_unlock(&mutex);
      return lock_protocol::RETRY;
    }
    default:
      exit(0);
  }
}

int lock_server_cache::release(lock_protocol::lockid_t lid, std::string clientID,
                               int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
  pthread_mutex_lock(&mutex);
  lock_state_t *ls = lock_map[lid];
  std::string nextWaitingClient = *(ls->waiting.begin());
  ls->waiting.erase(ls->waiting.begin());
  ls->retrying = nextWaitingClient;
  ls->owner.clear();
  ls->state = RETRYING;
  pthread_mutex_unlock(&mutex);
  handle(nextWaitingClient).safebind()->call(rlock_protocol::retry, lid, r);
  return ret;
}

lock_protocol::status
lock_server_cache::stat(lock_protocol::lockid_t lid, int &r)
{
  tprintf("stat request\n");
  r = nacquire;
  return lock_protocol::OK;
}

lock_server_cache::~lock_server_cache()
{

  pthread_mutex_lock(&mutex);
  for (std::map<lock_protocol::lockid_t, lock_state_t *>::iterator it = lock_map.begin(); it != lock_map.end(); it++) delete it->second;
  pthread_mutex_unlock(&mutex);
}