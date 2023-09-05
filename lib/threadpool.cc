
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


#include <unordered_map>


#include "threadpool.h"
#include "dir_tree.h"
#include "common.h"
#include "log.h"
#include "request.h"
#include "json.h"
#include "gdapi.h"
#include "exception.h"


sem_t req_item_sem;
pthread_mutex_t worker_lock;
std::list <struct req_item> req_queue;
std::queue <std::string> file_id_q;


////////////////////////////////////////////////////////////////
//                    THREADPOOL FUNCTIONS                    //                    
////////////////////////////////////////////////////////////////



void *
gdfs_worker (void * arg)
{

  class Threadpool * obj = (class Threadpool *) arg;
  struct req_item new_item;

  while (1) {
    // Wait until there is a request to be sent.
    int ret = sem_wait (&req_item_sem);
    if (ret == -1 && errno == EINTR) {
      pthread_mutex_lock(&worker_lock);
      if (obj->kill_workers == true) {
        pthread_mutex_unlock(&worker_lock);
        break;
      }
      pthread_mutex_unlock(&worker_lock);
      continue;
    }

    // Get the item to be processed.
    struct req_item item;
    pthread_mutex_lock(&worker_lock);
    if (req_queue.empty() == false) {
      item = req_queue.front();
      req_queue.pop_front();
    }
    pthread_mutex_unlock(&worker_lock);

    // Check whether the request has failed.
    // If yes, add it back to the request queue.
    if (obj->send_request(item) == false) {
      new_item.id       = item.id;
      new_item.req_type = item.req_type;
      new_item.url      = item.url;
      new_item.query    = item.query;
      new_item.node     = item.node;
      pthread_mutex_lock(&worker_lock);
      req_queue.emplace_front(new_item);
      sem_post(&req_item_sem);
      pthread_mutex_unlock(&worker_lock);
    }
  }

  return NULL;
}


bool
Threadpool::send_request (struct req_item & item)
{

  Debug("<-- Entering send_request() -->");

  bool ret = false;

  switch (item.req_type) {
    case GET:
      ret = send_get_req(item.url, item.node);
      break;

    case UPDATE:
      ret = send_update_req(item.url, item.query, item.node->entry);
      break;

    case INSERT:
      ret = send_insert_req(item.url, item.query, item.node->entry);
      break;

    case DELETE:
      ret = send_delete_req(item.url, item.node);
      break;

    case GENERATE_ID:
      ret = send_generate_id_req(item.url);
      break;

    case UPLOAD:
      ret = send_upload_req(item.url, item.query, item.headers);
      break;
  }

  Debug("<-- Exiting send_request() -->");
  return ret;
}


bool
Threadpool::send_get_req (const std::string & url,
                          struct GDFSNode * node)
{

  Debug("<-- Entering send_get_req() -->");

  bool ret = false;
  bool json_parsed = true;
  time_t time_;
  json::Value val;
  std::string resp;
  std::string error;
  std::string file_name;
  struct GDFSEntry * entry = node->entry;

retry:
  try {
    resp = this->auth.sendRequest(url, GET);
    val.parse(resp);
  } catch (GDFSException & err) {
    error = err.get();
    json_parsed = false;
    goto out;
  }
  json_parsed = true;

  try {
    time_ = rfc3339_to_sec(val["modifiedTime"].get());
  } catch (GDFSException & err) {
    error  = "Google Drive: Error code = " + val["error"]["code"].get();
    error += ", " + val["error"]["message"].get();
    goto out;
  }

  if (time_ <= entry->mtime) {
    Debug("File %s not modified. Nothing to do", node->file_name.c_str());
    ret = true;
    goto out;
  } else if (entry->g_doc && entry->mtime < time_) {
    gdi->download_file(node);
  }
  entry->mtime = entry->ctime = time_;

  // Get the title of the file.
  // Guaranted that title should exist, if reached this point.
  file_name = val["name"].get();
  std::replace(file_name.begin(), file_name.end(), '/', '_');

  // Check whether its a directory or not.
  // Get the file size, if not a directory.
  if (entry->g_doc == false && entry->is_dir == false) {
    entry->file_size = std::stoull(val["size"].get());
  }

  // Check whether the file name has changed.
  if (file_name != node->file_name) {
    // Name conflict.
    if (node->parent &&
        is_old_name_conflict(file_name, node->file_name) == false) {
      node->file_name = remove_name_conflict(file_name, entry->is_dir, node->parent);
    }
  }
  entry->cached_time = time(NULL);

  ret = true;

out:
  if (ret == false) {
    if (json_parsed && val["error"]["code"].get() == "403") {
      sleep(1);
      goto retry;
    }
    Error("%s", error.c_str());
  }

  Debug("<-- Exiting send_get_req() -->");
  return ret;
}


bool
Threadpool::send_insert_req (std::string & url,
                             std::string & query,
                             struct GDFSEntry * entry)
{

  Debug("<-- Entering send_insert_req() -->");

  bool ret = false;
  bool json_parsed = false;
  json::Value val;
  std::string error;
  time_t mtime;
  std::string resp;
  int count = 0;

retry:
  try {
    val.clear();
    resp = this->auth.sendRequest(url, INSERT, query);
    val.parse(resp);
  } catch (GDFSException & err) {
    error = err.get();
    json_parsed = false;
    goto out;
  }
  json_parsed = true;

  try {
    error  = "Google Drive: Error code = " + val["error"]["code"].get();
    error += ", " + val["error"]["message"].get();
    goto out;
  } catch (GDFSException & err) {
    ret = true;
  }

  mtime = rfc3339_to_sec(val["modifiedTime"].get());
  entry->mtime = entry->ctime = mtime;
  entry->pending_create = false;

out:
  if (ret == false) {
    if (json_parsed) {
      if (val["error"]["code"].get() == "403") {
        sleep(1);
        goto retry;
      } else if (++count < 5 && val["error"]["code"].get() == "404") {
        sleep(1);
        goto retry;
      }
    }
    Error("%s", error.c_str());
  }

  Debug("<-- Exiting send_insert_req() -->");
  return ret;
}


bool
Threadpool::send_delete_req (std::string & url,
                             struct GDFSNode * node)
{

  Debug("<-- Entering send_delete_req() -->");

  bool ret = false;
  bool json_parsed = false;
  json::Value val;
  std::string resp;
  std::string error;
  std::unordered_map <std::string, GDFSNode *>::iterator it_node;

retry:
  try {
    val.clear();
    resp = this->auth.sendRequest(url, DELETE);
    val.parse(resp);
  } catch (GDFSException & err) {
    error = err.get();
    json_parsed = false;
    goto out;
  }
  json_parsed = true;

  try {
    error  = "Google Drive: Error code = " + val["error"]["code"].get();
    error += ", " + val["error"]["message"].get();
    goto out;
  } catch (GDFSException & err) {
    ret = true;
  }

  it_node = file_id_node.find(node->entry->file_id);
  if (it_node != file_id_node.end()) {
    file_id_node.erase(it_node);
    delete node;
  }
  node = NULL;

out:
  if (ret == false) {
    if (json_parsed && (val["error"]["code"].get() == "403" || val["error"]["code"].get() == "404")) {
      sleep(1);
      goto retry;
    }
    Error("%s", error.c_str());
  }

  Debug("<-- Exiting send_delete_req() -->");
  return ret;
}


bool
Threadpool::send_update_req (std::string & url,
                             std::string & query,
                             struct GDFSEntry * entry)
{

  Debug("<-- Entering send_update_req() -->");

  bool ret = false;
  bool json_parsed = false;
  json::Value val;
  std::string error;
  time_t mtime;
  std::string resp;

retry:
  try {
    val.clear();
    resp = this->auth.sendRequest(url, UPDATE, query);
    val.parse(resp);
  } catch (GDFSException & err) {
    error = err.get();
    goto out;
  }
  json_parsed = true;

  try {
    error  = "Google Drive: Error code = " + val["error"]["code"].get();
    error += ", " + val["error"]["message"].get();
    goto out;
  } catch (GDFSException & err) {
    ret = true;
  }

  // Update the file entry.
  mtime = rfc3339_to_sec(val["modifiedTime"].get());
  entry->mtime = entry->ctime = mtime;

out:
  if (ret == false) {
    if (json_parsed && val["error"]["code"].get() == "403") {
      sleep(1);
      goto retry;
    }
    Error("%s", error.c_str());
  }

  Debug("<-- Exiting send_update_req() -->");
  return ret;
}


bool
Threadpool::send_generate_id_req (std::string & url)
{

  Debug("<-- Entering send_generate_id_req() -->");

  bool ret = false;
  bool json_parsed = true;
  json::Value val;
  std::string error;
  std::string resp;
  std::vector <json::Value *> file_ids;

retry:
  try {
    val.clear();
    resp = this->auth.sendRequest(url, GENERATE_ID);
    val.parse(resp);
  } catch (GDFSException & err) {
    error = err.get();
    goto out;
  }
  json_parsed = true;

  try {
    file_ids = val["ids"].getArray();
  } catch (GDFSException & err) {
    error  = "Google Drive: Error code = " + val["error"]["code"].get();
    error += ", " + val["error"]["message"].get();
    goto out;
  }

  for (unsigned i = 0; i < file_ids.size(); ++i) {
    file_id_q.emplace(file_ids[i]->get());
  }
  ret = true;

out:
  if (ret == false) {
    if (json_parsed && val["error"]["code"].get() == "403") {
      sleep(1);
      goto retry;
    }
    Error("%s", error.c_str());
  }

  Debug("<-- Exiting send_generate_id_req() -->");
  return ret;
}


bool
Threadpool::send_upload_req (std::string & url,
                             std::string & query,
                             std::string & headers)
{

  Debug("<-- Entering send_upload_req() -->");

  bool ret = false;
  bool json_parsed = false;
  json::Value val;
  std::string error;
  std::string resp;

retry:
  try {
    val.clear();
    resp = this->auth.sendRequest(url, UPLOAD, query, false, headers);
    val.parse(resp);
  } catch (GDFSException & err) {
    error = err.get();
    goto out;
  }
  json_parsed = true;

  try {
    error  = "Google Drive: Error code = " + val["error"]["code"].get();
    error += ", " + val["error"]["message"].get();
    goto out;
  } catch (GDFSException & err) {
    ret = true;
  }

out:
  if (ret == false) {
    if (json_parsed && val["error"]["code"].get() == "403") {
      sleep(1);
      goto retry;
    }
    Error("%s", error.c_str());
  }

  Debug("<-- Exiting send_upload_req() -->");
  return ret;
}


/*
 * Function to merge the queries of two requests.
 */
std::string
Threadpool::merge_requests (const std::string & a,
                            const std::string & b) const
{

  Debug("<-- Entering merge_requests()-->");

  assert(a.size() > 0);
  assert(b.size() > 0);

  json::Value val1;
  json::Value val2;
  std::string query;
  std::string id;
  std::string title;
  std::string modifiedDate;
  std::string lastViewedByMeDate;
  std::string mimeType;
  std::vector <json::Value *> parents;
  std::queue <std::string> q;

  val1.parse(a);
  val2.parse(b);

  // get id
  try {
    id = val1["id"].get();
  } catch (GDFSException &) {
    id = "";
  }
  if (id.empty() == false) {
    q.emplace("\"id\": \"" + id + "\"");
  }

  // get title.
  try {
    title = val1["name"].get();
  } catch (GDFSException &) {
    try {
      title = val2["name"].get();
    } catch (GDFSException &) {
      title = "";
    }
  }
  if (title.empty() == false) {
    q.emplace("\"name\": \"" + title + "\"");
  }

  // get mimeType.
  try {
    mimeType = val1["mimeType"].get();
  } catch (GDFSException &) {
    try {
      mimeType = val2["mimeType"].get();
    } catch (GDFSException &) {
      mimeType = "";
    }
  }
  if (mimeType.empty() == false) {
    q.emplace("\"mimeType\": \"" + mimeType + "\"");
  }

  // get modifiedDate.
  try {
    modifiedDate = val1["modifiedTime"].get();
  } catch (GDFSException &) {
    try {
      modifiedDate = val2["modifiedTime"].get();
    } catch (GDFSException &) {
      modifiedDate = "";
    }
  }
  if (modifiedDate.empty() == false) {
    q.emplace("\"modifiedTime\": \"" + modifiedDate + "\"");
  }

  // get lastViewedByMeDate.
  try {
    lastViewedByMeDate = val1["viewedByMeTime"].get();
  } catch (GDFSException &) {
    try {
      lastViewedByMeDate = val2["viewedByMeTime"].get();
    } catch (GDFSException & err) {
      lastViewedByMeDate = "";
    }
  }
  if (lastViewedByMeDate.empty() == false) {
    q.emplace("\"viewedByMeTime\": \"" + lastViewedByMeDate + "\"");
  }

  // get parents.
  try {
    parents = val1["parents"].getArray();
  } catch (GDFSException & err) {
    try {
      parents = val2["parents"].getArray();
    } catch (GDFSException & err) {
      parents.clear();
    }
  }
  if (parents.empty() == false) {
    q.emplace("\"parents\": [\"" + parents[0]->get() + "\"]");
  }

  // Build the new request.
  query += "{ ";
  while (q.empty() == false) {
    query += q.front();
    q.pop();
    if (q.empty() == false) {
      query += ", ";
    }
  }
  query += " }";  

  Debug("<-- Exiting merge_requests() -->");
  return query;
}


/*
 * Function to insert a request into the request queue.
 */
void
Threadpool::build_request (const std::string & id,
                           requestType request_type,
                           struct GDFSNode * node,
                           const std::string & url,
                           const std::string query,
                           const std::string headers) const
{

  Debug("<-- Entering build_request() -->");

  struct req_item item;
  std::list <req_item>::iterator it;

  if (node != NULL &&
      node->file_name.at(0) == '.') {
    goto out;
  }

  pthread_mutex_lock(&worker_lock);
  it = std::find_if(req_queue.begin(), req_queue.end(),
                    [&id](const req_item & item)->bool { return item.id == id; });
  if (it != req_queue.end()) {
    if (it->req_type == request_type) {
      switch (it->req_type) {
        case GET:
          // Since there is already a pending request for the same id,
          // there is no need to create the current request.
          break;

        case GENERATE_ID:
          // Since there is already a pending request for the same id,
          // there is no need to create the current request.
          break;

        case INSERT:
          // Since there is already a pending request for the same id,
          // there should not be a new INSERT request for the same id.
          break;

        case UPDATE:
          // Since both the requests are to update the same file,
          // instead of sending two requests, both the requests shall be merged,
          // to create a single request.
          if (query != it->query) {
            it->query = merge_requests(query, it->query);
          }
          break;

        case DELETE:
          // Since there is already a pending request for the same id,
          // there should not be a new INSERT request for the same id.
          break;

        case UPLOAD:
          item.id = id;
          item.req_type = request_type;
          item.node = node;
          item.url = url;
          item.query = query;
          item.headers = headers;
          req_queue.emplace_back(item);
          sem_post(&req_item_sem);
          break;
      }

    } else {
      switch (it->req_type) {
        case DELETE:
          // There is pending DELETE request.
          // There should not be a new request coming in,
          break;

        case INSERT:
          // There is a pending INSERT request.
          // If new request is an UPDATE request, both can be merged.
          // If its a DELETE request, cancel both the requests.
          // if its a GET request, dont add the new request.
          if (request_type == UPDATE) {
            if (query != it->query) {
              it->query = merge_requests(query, it->query);
            }
          } else if (request_type == DELETE) {
            req_queue.erase(it);
          }
          break;

        case GET:
          // There is a pending GET request.
          // If new request is an DELETE request, remove pending GET request.
          // If new request is an UPDATE request, remove pending GET request.
          if (request_type == DELETE || request_type == UPDATE) {
            req_queue.erase(it);
            item.id = id;
            item.req_type = request_type;
            item.node = node;
            item.url = url;
            item.query = query;
            item.headers = headers;
            req_queue.emplace_back(item);
          }
          break;

        case UPDATE:
          // There is a pending UPDATE request.
          // If new request is an DELETE request, remove pending UPDATE request.
          // If new request is an GET request, dont add the new request.
          if (request_type == DELETE) {
            req_queue.erase(it);
            item.id = id;
            item.req_type = request_type;
            item.node = node;
            item.url = url;
            item.query = query;
            item.headers = headers;
            req_queue.emplace_back(item);
          }
          break;

        case UPLOAD:
          if (request_type == DELETE) {
            req_queue.erase(it);
            item.id = id;
            item.req_type = request_type;
            item.node = node;
            item.url = url;
            item.query = query;
            item.headers = headers;
            req_queue.emplace_back(item);
          }
          break;
      }
    }

  } else {
    // Add the new request.
    item.id = id;
    item.req_type = request_type;
    item.node = node;
    item.url = url;
    item.query = query;
    item.headers = headers;
    req_queue.emplace_back(item);
    sem_post(&req_item_sem);
  }
  pthread_mutex_unlock(&worker_lock);

out:
  Debug("<-- Exiting build_request() -->");
}
