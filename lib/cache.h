
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


#ifndef CACHE_H__
#define CACHE_H__

#include <string>
#include <set>
#include <list>
#include <unordered_map>

#include <stdio.h>
#include <pthread.h>

#include "auth.h"


struct Page {
  size_t start;
  size_t stop;
  size_t size;
  char * mem;

  Page (void) :
    start(0),
    stop(0),
    size(0),
    mem(NULL) {};

  Page (char * m,
        size_t start_,
        size_t stop_) :
    start(start_),
    stop(stop_),
    size(stop_ - start_ + 1),
    mem(m) {};

  ~Page (void)
  {
    delete[] mem;
    size = 0;
    mem = NULL;
  }
};


struct page_cmp {
  bool
  operator() (const Page * a,
              const Page * b) const
  {
    return a->start < b->start;
  }
};


struct File {
  Auth & auth;
  time_t mtime;
  size_t size;
  pthread_mutex_t lock;
  std::set <struct Page *, page_cmp> pages;

  File (Auth & auth_) :
    auth(auth_),
    mtime(0),
    size(0)
  {
    pthread_mutex_init(&lock, NULL);
    this->pages.clear();
  }

  ~File() {
    this->delete_pages();
    pthread_mutex_destroy(&lock);
  }

  void
  delete_pages (void);

  size_t
  get (std::list <struct Page *> & l,
       off_t start,
       off_t stop,
       struct GDFSEntry * entry);

  struct Page *
  put (char * buf,
       off_t start,
       off_t stop,
       struct GDFSEntry * entry);

  int
  read_file (struct GDFSEntry * entry,
             char * buf,
             off_t & start,
             off_t & stop);

  void
  resize (size_t new_size);

};


class LRUCache {
  private:
    Auth & auth;
    size_t size;
    std::list <std::pair <std::string, struct File *>> cache;
    std::unordered_map <std::string, decltype(cache.begin())> map;

  public:
    LRUCache (Auth & auth_) :
      auth(auth_),
      size(0) {};

    ~LRUCache (void);

    void
    free_cache (size_t size_);

    size_t
    get (const std::string & file_id,
         char * buf,
         off_t offset,
         size_t len,
         struct GDFSNode * node);

    bool
    put (const std::string & file_id,
         char * buf,
         off_t offset,
         size_t len,
         struct GDFSNode * node,
         bool to_delete = true);

    void
    remove (const std::string & file_id);

    void
    change (const std::string & file_id,
            const std::string & new_file_id);


    void
    set_time (const std::string & file_id,
              time_t mtime);

    void
    resize (const std::string & file_id,
            size_t new_size);

};


#endif // CACHE_H__
