
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



#include <algorithm>

#include <string.h>
#include <time.h>
#include <assert.h>

#include "cache.h"
#include "log.h"
#include "request.h"
#include "dir_tree.h"
#include "conf.h"
#include "json.h"
#include "exception.h"


/*
 * Function to delete all the pages of a file from cache.
 */
void
File::delete_pages (void)
{

  Debug("<-- Entering File delete_pages() -->");

  pthread_mutex_lock(&lock);

  // Delete all the pages.
  for (auto p : this->pages) {
    delete p;
  }
  this->pages.clear();

  // Reset file size and mtime in cache.
  this->size = 0;
  this->mtime = time(NULL);

  pthread_mutex_unlock(&lock);

  Debug("<-- Exiting File delete_pages() -->");
}


/*
 * Function to read from the cache, given a start and stop position.
 * If any bytes is not present, a new page shall be created.
 */
size_t
File::get (std::list <struct Page *> & l,
           off_t start,
           off_t stop,
           struct GDFSEntry * entry)
{

  Debug("<-- Entering File get() -->");

  assert (entry != NULL);

  Page * p   = NULL;
  char * buf = NULL;
  size_t len = 0;
  off_t start_ = 0;
  off_t stop_  = 0;
  size_t count  = 0;
  size_t size = 0;
  std::string url;
  std::string resp;

  // If any modifications in the file detected,
  // delete all the pages.
  if (entry->mtime > 0) {
    if (this->mtime == 0) {
      this->mtime = entry->mtime;
    } else if (entry->mtime > this->mtime) {
      this->delete_pages();
      entry->file_size = 0;
    }
  }

  // Check whether a full block is required.
  if (entry->file_size > 0) {
    stop = std::min(stop, (off_t) entry->file_size - 1);
  }

  // Check for Google Docs.
  pthread_mutex_lock(&lock);
  auto f = this->pages.begin();
  if (entry->g_doc) {
    if (f != this->pages.end()) {
      p = *f;
      l.emplace_front(p);
    }
    size = (stop - start + 1);
    if (this->pages.size() == 1) {
      pthread_mutex_unlock(&lock);
      goto out;
    }
  }

  while (f != this->pages.end()) {
    p = *f;
    if (p->start <= start &&
        p->stop >= start) {
      l.emplace_back(p);
      ++f;
      break;
    }
    ++f;
    ++count;
  }
  pthread_mutex_unlock(&lock);

  // Atmost only a single page in the cache.
  if (f == this->pages.end()) {
    size = l.empty() ? 0 : l.front()->size;

    // if no more pages are required
    if (l.size() == 1 &&
        l.front()->stop >= stop) {
      goto out;
    }
  }

  // If page is not found in file cache.
  if (l.empty() == true &&
      f == this->pages.end()) {
    Debug("page not found in cache. downloading it");

    len = stop - start + 1;
    buf = new char[len];
    memset(buf, 0, len);
    assert (buf != NULL);

    this->read_file(entry, buf, start, stop);

    p = this->put(buf, start, stop, entry);
    assert(p != NULL);
    l.emplace_back(p);
    size = p->size;

    goto out;
  }

  // retrieve from file cache.
  pthread_mutex_lock(&lock);
  while (f != this->pages.end() &&
         stop >= (*f)->stop) {
    if (l.back()->stop + 1 != (*f)->start) {
      // missing page between two pages.
      // retrieve the page.
      start_ = l.back()->stop + 1;
      stop_ = (*f)->start - 1;
      len = stop_ - start_ + 1;
      buf = new char[len];
      assert (buf != NULL);
      this->read_file(entry, buf, start_, stop_);

      // Add it into cache.
      Page * p = this->put(buf, start_, stop_, entry);
      assert (p != NULL);
      l.emplace_back(p);
      size += p->size;
    }
    l.emplace_back(*f);
    size += (*f)->size;
    ++f;
  }
  pthread_mutex_unlock(&lock);

  // Check for any more missing pages.
  if (l.back()->stop < stop) {
    start_ = l.back()->stop + 1;
    len = stop - start_ + 1;
    buf = new char[len];
    assert(buf != NULL);
    this->read_file(entry, buf, start_, stop);

    // Add it into cache.
    Page * p = this->put(buf, start, stop, entry);
    assert (p != NULL);
    l.emplace_back(p);
    size += p->size;
  }


out:
  if (entry->g_doc == false) {
    pthread_mutex_lock(&lock);
    entry->file_size = 0;
    for (Page * p : this->pages) {
      entry->file_size += p->size;
    }
    pthread_mutex_unlock(&lock);
  }

  Debug("<-- Exiting File get() -->");
  return size;
}


struct Page *
File::put (char * buf,
           off_t start,
           off_t stop,
           struct GDFSEntry * entry)
{

  Debug("<-- Entering File put() -->");

  off_t start_;
  Page * p = NULL;

  pthread_mutex_lock(&lock);

  auto f = this->pages.begin();
  if (this->pages.empty() == true) {
    p = new Page(buf, start, stop);
    assert (p != NULL);
    this->pages.emplace(p);
    this->size = p->size;
  } else {
    start_ = start;
    while (f != this->pages.end()) {
      p = *f;
      if (p->stop < start_) {
        ++f;
        continue;
      }

      // If the new page can be inserted between two existing pages.
      if (p->start > stop) {
        p = new Page(buf, start, stop);
        assert (p != NULL);
        this->pages.emplace(p);
        this->size = p->size;
        break;
      }

      if (p->stop >= stop) {
        memcpy(p->mem + (start_ - p->start), buf + (start_ - start), stop - start_ + 1);
        break;
      } else {
        memcpy(p->mem + (start_ - p->start), buf + (start_ - start), p->stop - start_ + 1);
        start_ = p->stop + 1;
      }
      ++f;
    }
    if (f == this->pages.end()) {
      p = new Page(buf + (start_ - start), start_, stop);
      assert (p != NULL);
      this->pages.emplace(p);
      this->size += p->size;
    }
  }

  // Update the mtime of the file in the cache.
  this->mtime = entry->mtime;

  pthread_mutex_unlock(&lock);

  Debug("<-- Exiting File put() -->");
  return p;
}


int
File::read_file (struct GDFSEntry * entry,
                 char * buf,
                 off_t & start,
                 off_t & stop)
{

  Debug("<-- Entering File read_file() -->");

  bool ret = false;
  size_t size;
  size_t len = stop - start + 1;
  std::string url;
  std::string query;
  std::string resp;
  json::Value val;


  if (entry->g_doc) {
    return 0;
  }

  // Construct the request.
  url = GDFS_FILE_URL + entry->file_id + "?alt=media";
  query = "Range: bytes=" + std::to_string(start) + "-" + std::to_string(stop);

  // Get the file data.
retry:
  try {
    resp = this->auth.sendRequest(url, DOWNLOAD, query);

    val.clear();
    if (val.is_json(resp) == true) {
      throw GDFSException("read returns JSON string");
    }

    ret = true;
  } catch (GDFSException & err) {
    try {
      val.parse(resp);
      if (val["error"]["code"].get() == "503") {
        sleep(1);
        goto retry;
      }
    } catch (GDFSException & err) {

    }
  }

  if (ret) {
    size = resp.size();
    size = (size > len ? len : size);
    stop = size + start - 1;
    memcpy(buf, resp.c_str(), size);
  }

  Debug("<-- Exiting File read_file() -->");
  return resp.size();
}


void
File::resize (size_t new_size)
{

  Page * p = NULL;
  size_t offset = new_size - 1;
  size_t size_ = 0;

  for (auto it = this->pages.rbegin(); it != this->pages.rend(); ++it) {
    p = *it;
    if (p->start > offset) {
      size_ = p->size;
      delete p;
      this->pages.erase(std::next(it).base());
      this->size -= size_;
    } else if (p->stop == offset) {
      break;
    } else if (p->stop > offset && p->start < offset) {
      size_ = p->size;
      p->stop = offset;
      p->size = (p->stop - p->start + 1);
      p->mem[p->size] = 0;
      this->size -= (size_ - p->size);
      break;
    } else if (p->start == offset) {
      size_ = p->size;
      p->start = offset;
      p->stop = offset;
      p->size = 1;
      this->size -= (size_ - p->size);
      break;
    }
  }

  if (this->pages.empty()) {
    this->pages.clear();
  }
}


LRUCache::~LRUCache (void)
{

  Debug("<-- Entering LRUCache destructor -->");
  
  while (map.empty() == false) {
    auto it = this->map.begin();
    delete it->second->second;
    this->cache.erase(it->second);
    this->map.erase(it);
  }

  Debug("<-- Exiting LRUCache destructor -->");
}


/*
 * Function to make sure that there is atleast
 * size_ bytes free in the cache.
 */
void
LRUCache::free_cache (size_t size_)
{

  Debug("<-- Entering free_cache() -->");

  // Size required is greater than the max size of cache.
  if (size_ > GDFS_CACHE_MAX_SIZE) {
    goto out;
  }

  // Free space available in the cache.
  if (this->size + size_ < GDFS_CACHE_MAX_SIZE) {
    goto out;
  }

  // Need to free some space.
  while (this->cache.empty() == false) {
    auto it = this->cache.rbegin();
    this->size -= it->second->size;
    it->second->delete_pages();

    if (this->size + size_ < GDFS_CACHE_MAX_SIZE) {
      goto out;
    }
  }

out:
  Debug("<-- Exiting free_cache() -->");
}


size_t
LRUCache::get (const std::string & file_id,
               char * buffer,
               off_t offset,
               size_t len,
               struct GDFSNode * node)
{

  Debug("<-- Entering LRUCache get() -->");

  assert(len > 0);
  assert(offset >= 0);
  assert(buffer != NULL);

  Page * p = NULL;
  File * f = NULL;
  off_t start = offset;
  off_t stop = offset + len - 1;
  size_t size_read = 0;
  size_t size_r = 0;
  size_t added_size = 0;
  size_t size_begin = 0;
  std::list <struct Page *> l;
  auto it = this->map.find(file_id);

  memset(buffer, 0, len);

  // Find the file in the cache.
  // Make it the Most Recently Used.
  if (it == map.end()) {
    Debug("File %s not found in cache. Creating new entry", node->file_name.c_str());
    f = new File(this->auth);
    assert (f != NULL);
    this->cache.emplace_front(file_id, f);
    this->map.emplace(file_id, cache.begin());
    size_begin = 0;
  } else {
    f = it->second->second;
    this->cache.splice(cache.begin(), cache, it->second);
    size_begin = f->size;
  }

  // Load the page into the cache, if not in cache.
  size_read = f->get(l, start, stop, node->entry);
  if (l.empty() == true) {
    goto out;
  }
  added_size = f->size - size_begin;

  // Only a single page.
  p = l.front();
  l.pop_front();
  if (l.empty() == true) {
    memcpy(buffer, p->mem + (start - p->start), size_read);
  }

  // Read the rest of the pages.
  else {
    memcpy(buffer, p->mem + (start - p->start), p->stop - start + 1);
    size_r += p->stop - start + 1;
    while (l.size() > 1) {
      p = l.front();
      memcpy(buffer + size_r, p->mem, p->stop - p->start + 1);
      size_r += p->stop - p->start + 1;
      l.pop_front();
    }
    p = l.front();
    l.pop_front();
    memcpy(buffer + size_r, p->mem, size_read - size_r);
  }

  free_cache(added_size);
  this->size += added_size;

out:
  Debug("<-- Exiting LRUCache get() -->");
  return size_read;
}


bool
LRUCache::put (const std::string & file_id,
               char * buffer,
               off_t offset,
               size_t len,
               struct GDFSNode * node,
               bool to_delete)
{

  Debug("<-- Entering LRUCache put() -->");

  bool ret = true;
  char * new_buf = NULL;
  File * f = NULL;
  off_t start = offset;
  off_t stop  = offset + len - 1;

  // Find the file in cache.
  // Make it Most Recently Used.
  auto it = map.find(file_id);
  if (it == map.end()) {
    f = new File(this->auth);
    assert (f != NULL);
    cache.emplace_front(file_id, f);
    map.emplace(file_id, cache.begin());
  } else {
    f = it->second->second;
    cache.splice(cache.begin(), cache, it->second);

    // If the file page has been downloaded from Google Drive,
    // the entire file may have changed. Remove all the pages.
    if (to_delete) {
      this->size -= f->size;
      f->delete_pages();
    }
  }

  // Make sure that cache has enough free space to place the new page.
  if (len > 0) {
    this->free_cache(len);
  }

  // Add the page into the cache.
  if (buffer != NULL &&
      (len > 0 && stop >= start)) {
    new_buf = new char[len];
    assert(new_buf != NULL);
    memcpy(new_buf, buffer, len);
    f->put(new_buf, start, stop, node->entry);
    this->size += len;
  }

  Debug("<-- Exiting LRUCache put() -->");
  return ret;
}


void
LRUCache::remove (const std::string & file_id)
{

  Debug("<-- Entering LRUCache remove() -->");

  File * f = NULL;

  auto it = map.find(file_id);
  if (it != map.end()) {
    f = it->second->second;
    f->delete_pages();
    this->cache.erase(it->second);
    this->map.erase(it);
  }

  Debug("<-- Exiting LRUCache remove() -->");

}


void
LRUCache::change (const std::string & file_id,
                  const std::string & new_file_id)
{

  Debug("<-- Entering LRUCache change() -->");

  assert (file_id != new_file_id);

  auto it = this->map.find(file_id);
  assert (it != this->map.end());

  this->remove(new_file_id);

  this->cache.emplace_front(new_file_id, it->second->second);
  this->map.emplace(new_file_id, this->cache.begin());

  this->cache.erase(it->second);
  this->map.erase(it);

  Debug("<-- Exiting LRUCache change() -->");

}


void
LRUCache::set_time (const std::string & file_id,
                    time_t mtime)
{

  Debug("<-- Entering LRUCache set_time() -->");

  auto it = this->map.find(file_id);
  assert (it != this->map.end());

  it->second->second->mtime = mtime;

  Debug("<-- Exiting LRUCache set_time() -->");
}


void
LRUCache::resize (const std::string & file_id,
                  size_t new_size)
{

  Debug("<-- Entering LRUCache resize() -->");

  size_t size_ = 0;
  File * f = NULL;

  auto it = this->map.find(file_id);
  assert (it != this->map.end());
  f = it->second->second;

  size_ = f->size;
  f->resize(new_size);
  this->size -= (size_ - f->size);

  Debug("<-- Exiting LRUCache resize() -->");

}
