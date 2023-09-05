
/*
 * Copyright (c) 2016, Robin Thomas.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * The name of Robin Thomas or any other contributors to this software
 * should not be used to endorse or promote products derived from this
 * software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 * Author: Robin Thomas <robinthomas2591@gmail.com>
 *
 */



#ifndef THREADPOOL_H__
#define THREADPOOL_H__


#include <algorithm>
#include <string>
#include <queue>
#include <list>

#include <pthread.h>
#include <semaphore.h>
#include <signal.h>

#include "dir_tree.h"
#include "conf.h"
#include "auth.h"


/***********************************************/
/*            REQUEST QUEUE HANDLING           */
/*                                             */
/***********************************************/ 


struct req_item {
  std::string id;
  requestType req_type;
  std::string url;
  std::string query;
  std::string headers;
  struct GDFSNode * node;
  virtual ~req_item(){};
};


extern sem_t req_item_sem;
extern pthread_mutex_t worker_lock;
extern std::list <struct req_item> req_queue;
extern std::queue <std::string> file_id_q;

void * gdfs_worker (void * arg);

class GDrive;

class Threadpool {
  private:
    int active_threads;
    int threads[GDFS_MAX_WORKER_THREADS];
    pthread_t gdfs_workers[GDFS_MAX_WORKER_THREADS];
    Auth & auth;
    GDrive * gdi;

  public:
    bool kill_workers;

    Threadpool (GDrive * gdi_,
                Auth & auth_) :
      active_threads(0),
      auth(auth_),
      gdi(gdi_),
      kill_workers(false)
    {
      sem_init(&req_item_sem, 0, 0);
      pthread_mutex_init(&worker_lock, NULL);

      for (int i = 0; i < GDFS_MAX_WORKER_THREADS; i++) {
        this->threads[i] = pthread_create(&gdfs_workers[i], NULL, gdfs_worker, this);
        if (this->threads[i] == 0) {
          ++active_threads;
        }
      }
    }


    ~Threadpool (void)
    {
      pthread_mutex_lock(&worker_lock);
      this->kill_workers = true;
      pthread_mutex_unlock(&worker_lock);

      for (int i = 0; i < GDFS_MAX_WORKER_THREADS; i++) {
        if (this->threads[i] == 0 && pthread_tryjoin_np(gdfs_workers[i], NULL) == EBUSY) {
          pthread_cancel(gdfs_workers[i]);
          pthread_join(gdfs_workers[i], NULL);
        }
      }

      pthread_mutex_destroy(&worker_lock);
      sem_destroy(&req_item_sem);
    }

    void
    build_request (const std::string & id,
                   requestType request_type,
                   struct GDFSNode * node,
                   const std::string & url,
                   const std::string query = std::string(),
                   const std::string headers = std::string()) const;

    std::string
    merge_requests (const std::string & a,
                    const std::string & b) const;

    bool
    send_request (struct req_item & item);

    bool
    send_update_req (std::string & url,
                     std::string & query,
                     struct GDFSEntry * entry);

    bool
    send_delete_req (std::string & url,
                     struct GDFSNode * node);

    bool
    send_insert_req (std::string & url,
                     std::string & query, 
                     struct GDFSEntry * entry);

    bool
    send_get_req (const std::string & url,
                  struct GDFSNode * node);

    bool
    send_generate_id_req (std::string & url);

    bool
    send_upload_req (std::string & url,
                     std::string & query,
                     std::string & headers);

};



#endif // THREADPOOL_H__
