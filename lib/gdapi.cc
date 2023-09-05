
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
#include <iostream>
#include <stack>
#include <sstream>

#include <unistd.h>
#include <assert.h>

#include "json.h"
#include "request.h"
#include "gdapi.h"
#include "auth.h"
#include "log.h"
#include "common.h"
#include "conf.h"
#include "exception.h"


uint64_t
GDrive::get_no_files (void)
{

  Debug("<-- Entering get_no_files() -->");

  uint64_t child_count = 0;
  struct GDFSNode * node = this->root;
  std::queue <struct GDFSNode *> list;

  list.emplace(this->root);
  while (list.empty() == false) {
    node = list.front();
    list.pop();

    std::unordered_map <std::string, struct GDFSNode *> & children = node->get_children();
    child_count += children.size();

    for (auto child : children) {
      list.emplace(child.second);
    }
  }

  Debug("<-- Exiting get_no_files() -->");
  return child_count;
}


struct GDFSNode *
GDrive::get_node (const std::string & path,
                  uid_t uid,
                  gid_t gid,
                  bool search)
{

  Debug("<-- Entering get_node() -->");
  Debug("Trying to get file %s", path.c_str());

  int err_num = 0;
  std::string error;
  std::string next_dir;
  std::string tmp = path;
  std::string::size_type pos;
  struct GDFSNode * node = this->root;
  struct GDFSNode * child = NULL;

  if (tmp.back() == '/') {
    if (tmp == "/") {
      this->get_children(node);
      goto out;
    }
    tmp.pop_back();
  }

  while (tmp.empty() == false) {
    if (tmp.front() == '/') {
      tmp.erase(0, 1);
    }
    pos = tmp.find_first_of('/');
    next_dir = tmp.substr(0, pos);
    tmp = (pos == std::string::npos ? "" : tmp.substr(pos));

    // Check if path component is too long.
    if (next_dir.size() > GDFS_NAME_MAX_LEN) {
      err_num = ENAMETOOLONG;
      error = "path component too long";
      goto out;
    }

    // Check if the path component exists.
    child = node->find(next_dir);
    if (child == NULL) {
      search = ((search && tmp.empty() == true) ? true : false);
      try {
        this->get_children(node);
      } catch (GDFSException & err) {
        err_num = errno;
        error = err.get();
        goto out;
      }
      child = node->find(next_dir);
      if (child == NULL) {
        err_num = ENOENT;
        error = "path component " + next_dir + " does not exist";
        goto out;
      }
    }
    node = child;

    if (tmp.empty() == false) {
      // Check for access permissions.
      if (this->file_access(uid, gid, X_OK, node->entry)) {
        err_num = EACCES;
        error = "user doesnt have execute permission on " + node->file_name;
        goto out;
      }

      // Check if a path component is not a directory.
      if (node->entry->is_dir == false) {
        err_num = ENOTDIR;
        error = "path component " + node->file_name + " is not a directory";
        goto out;
      }
    }
  }

  // pending DELETE request in the request queue.
  if (node != NULL &&
      node->entry->dirty == true) {
    err_num = ENOENT;
    error = "path component does not exist";
    goto out;
  }

  if (search &&
      node != NULL &&
      node->entry->mtime > 0 &&
      node->entry->file_id.compare(0, gdfs_name_prefix.size(), gdfs_name_prefix) != 0 &&
      (node->entry->is_dir || node->link == 0) &&
      node->entry->write == false) {
    this->update_node(node);
  } else if (node != NULL &&
             node->entry->is_dir &&
             node->entry->pending_get) {
    get_children(node);
  }

out:
  Debug("<-- Exiting get_node() -->");

  if (err_num) {
    errno = err_num;
    throw GDFSException(error);
  }
  return node;
}


void
GDrive::update_node (struct GDFSNode * node)
{

  assert(node != NULL);

  Debug("<-- Entering update_node() -->");
  Debug("Updating file %s", node->file_name.c_str());

  time_t time_;
  json::Value val;
  struct GDFSEntry * entry = node->entry;
  std::string file_id = entry->file_id;
  std::string file_name;
  std::string url = GDFS_FILE_URL + file_id + "?fields=modifiedTime%2Cname%2Csize";

  // GDFS special file.
  if (file_id.compare(0, gdfs_name_prefix.size(), gdfs_name_prefix) == 0){
    Debug("File is GDFS special file. Nothing to do.");
    goto out;
  }

  // Check whether cache entry is invalidated.
  // If not, do not send the GET request.
  if (time(NULL) - entry->cached_time <= GDFS_CACHE_TIMEOUT) {
    Debug("File is not invalidated in cache. Nothing to do.");
    goto out;
  }

  // Build the GET request and add it to request queue.
  if (entry->g_doc) {
    this->download_file(node);
  } else {
    threadpool.build_request(file_id, GET, node, url);
  }

out:
  Debug("<-- Exiting update_node() -->");
}


int
GDrive::file_access (uid_t uid,
                     gid_t gid,
                     int mask,
                     struct GDFSEntry * entry)
{

  Debug("<-- Entering file_access() -->");

  int ret = 0;
  int mode = entry->file_mode;

  assert(entry != NULL);

  // Check for read permissions.
  if (mask & R_OK) {
    if ((uid == entry->uid || uid == 0) && (mode & S_IRUSR)) {
      ret = 0;
    } else if ((gid == entry->gid || gid == 0) && (mode & S_IRGRP)) {
      ret = 0;
    } else if (mode & S_IROTH) {
      ret = 0;
    } else {
      ret = -EACCES;
      goto out;
    }
  }

  // Check for write permissions.
  if (mask & W_OK) {
    if ((uid == entry->uid || uid == 0) && (mode & S_IWUSR)) {
      ret = 0;
    } else if ((gid == entry->gid || gid == 0) && (mode & S_IWGRP)) {
      ret = 0;
    } else if (mode & S_IWOTH) {
      ret = 0;
    } else {
      ret = -EACCES;
      goto out;
    }
  }

  // Check for execute permissions.
  if (mask & X_OK) {
    // If execute permission is allowed for any user,
    // root user shall get execute permission too.
    if (uid == 0) {
      if ((mode & S_IXUSR) || (mode & S_IXGRP) || (mode & S_IXOTH)) {
        ret = 0;
      } else {
        ret = -EACCES;
      }
      goto out;
    }

    if (uid == entry->uid && (mode & S_IXUSR)) {
      ret = 0;
    } else if (gid == entry->gid && (mode & S_IXGRP)) {
      ret = 0;
    } else if (mode & S_IXOTH) {
      ret = 0;
    } else {
      ret = -EACCES;
      goto out;
    }
  }

out:
  Debug("<-- Exiting file_access() -->");
  return ret;
}


void
GDrive::write_file (struct GDFSNode * node)
{

  Debug("<-- Entering write_file() -->");

  assert(node != NULL);

  int count = 0;
  json::Value val;
  bool upload_complete = false;
  struct GDFSEntry * entry = node->entry;
  std::string url = GDFS_UPLOAD_URL + entry->file_id + "?uploadType=resumable&fields=modifiedTime";
  std::string resp;
  std::string query;
  std::string location;
  std::string headers = "";
  std::string line;
  std::string error;
  std::string start_str;
  std::string prefix = "Location: ";
  std::string complete_prefix = "HTTP/1.1 200 OK";
  std::string range_prefix = "Range: bytes=0-";
  size_t size = 0;
  size_t start_ = 0;
  size_t stop_;
  char * new_buf = NULL;
  std::istringstream m;
  int sval = 0;

  // If its GDFS special file, no writes to Google Drive.
  if (entry->file_id.compare(0, gdfs_name_prefix.size(), gdfs_name_prefix) == 0) {
    return;
  }

  // Construct the request.
  if (entry->mime_type.empty() == false) {
    headers = "X-Upload-Content-Type: " + entry->mime_type;
  }
  query = "{\"modifiedTime\": \"" + to_rfc3339(entry->mtime) + "\"}";

  // Send the request.
  try {
    resp = this->auth.sendRequest(url, UPLOAD_SESSION, query, false, headers);
  } catch (GDFSException & err) {
    error = err.get();
    goto out;
  }

  // Get the location to send the data.
  m.str(resp);
  while (std::getline(m, line)) {
    if (line.compare(0, prefix.size(), prefix) == 0) {
      location = line.substr(prefix.size());
      location.pop_back();
      break;
    }
  }
  m.clear();
  if (location.empty() == true) {
    error = "location url not found in Drive response";
    goto out;
  }

  // Upload the updated file in chunks to Google Drive using threadpool.
  size   = 0;
  start_ = 0;
  stop_  = entry->file_size < GDFS_UPLOAD_CHUNK_SIZE ? entry->file_size - 1 : GDFS_UPLOAD_CHUNK_SIZE - 1;
  new_buf = new char[GDFS_UPLOAD_CHUNK_SIZE];
  while (size < entry->file_size) {
    this->cache.get(entry->file_id, new_buf, start_, stop_ - start_ + 1, node);
    query   = std::string(new_buf, (stop_ - start_ + 1));
    headers = "Content-Range: bytes " + std::to_string(start_) + "-" + std::to_string(stop_) + "/" + std::to_string(entry->file_size);

retry:
    try {
      resp = this->auth.sendRequest(location, UPLOAD, query, false, headers);
    } catch (GDFSException & err) {
      error = err.get();
      goto out;
    }

    try {
      val.clear();
      val.parse(resp);
      if (val["error"]["code"].get() == "404") {
        sleep(1);
        goto retry;
      }
    } catch (GDFSException & err) {

    }

    m.str(resp);
    while (std::getline(m, line)) {
      if (line.compare(0, range_prefix.size(), range_prefix) == 0) {
        start_str = line.substr(range_prefix.size());
        start_str.pop_back();
        break;
      } else if (line.compare(0, complete_prefix.size(), complete_prefix) == 0) {
        upload_complete = true;
        break;
      }
    }
    m.clear();
    if (upload_complete == true) {
      break;
    }

    // Request not processed at Drive.
    // Need to resend it.
    count = 0;

retry_write:
    if (start_str.empty() == true) {
      headers = "Content-Range: bytes */" + std::to_string(entry->file_size);

      try {
        resp = this->auth.sendRequest(location, UPLOAD, query, false, headers);
      } catch (GDFSException & err) {
        error = err.get();
        goto out;
      }

      m.str(resp);
      while (std::getline(m, line)) {
        if (line.compare(0, range_prefix.size(), range_prefix) == 0) {
          start_str = line.substr(range_prefix.size());
          start_str.pop_back();
          break;
        }
      }
      m.clear();
      if (start_str.empty()) {
        if (++count < 10) {
          sleep(1);
          goto retry_write;
        }
        error = "Unable to send the write request";
        goto out;
      }
    }

    sscanf(start_str.c_str(), "%zu", &size);
    start_ = size + 1;
    stop_  = ((entry->file_size - size) < GDFS_UPLOAD_CHUNK_SIZE ? (start_ + entry->file_size - size - 2) : (start_ + GDFS_UPLOAD_CHUNK_SIZE - 1));
  }

out:
  if (new_buf != NULL) {
    delete[] new_buf;
  }

  if (error.empty() == false) {
    Error("write_file(): %s", error.c_str());
    throw GDFSException(error);
  }

  Debug("<-- Exiting write_file() -->");
}


bool
GDrive::get_root (void)
{

  Debug("<-- Entering get_root() -->");

  bool ret = false;
  std::string error;
  std::string resp;
  std::string url = GDFS_ABOUT_URL + std::string("?fields=storageQuota(limit,usageInDrive)");
  json::Value val;
  struct GDFSEntry * entry = NULL;

  try {
    resp = this->auth.sendRequest(url, GET);
    val.parse(resp);
  } catch (GDFSException & err) {
    error = err.get().c_str();
    goto out;
  }

  try {
    this->bytes_used = std::stoull(val["storageQuota"]["usageInDrive"].get());
  } catch (GDFSException & err) {
    error = "Drive: Error " + val["error"]["code"].get() + ", " + val["error"]["message"].get();
    goto out;
  }

  try {
    this->bytes_total = std::stoull(val["storageQuota"]["limit"].get());
    this->bytes_free = this->bytes_total - this->bytes_used;
    Debug("Drive: %llu bytes used out of %llu bytes", this->bytes_used, this->bytes_total);
  } catch (GDFSException & err) {
    this->bytes_total = this->bytes_used;
    this->bytes_free = 0;
  }

  entry = new GDFSEntry("root", 0, true, this->mounting_time, this->mounting_time,
                        this->uid, this->gid, GDFS_ROOT_MODE);
  assert(entry != NULL);
  this->root = new GDFSNode("/", entry, NULL);
  assert(this->root != NULL);
  file_id_node.emplace("root", this->root);
  ret = true;
  Debug("root entry inserted into directory tree");

out:
  if (ret == false) {
    Error("get_root(): %s", error.c_str());
  }
  Debug("<-- Exiting get_root() -->");
  return ret;
}


void
GDrive::download_file (struct GDFSNode * node)
{

  assert (node != NULL);

  bool ret = false;
  json::Value val;
  char * buf = NULL;
  struct GDFSEntry * entry = node->entry;
  std::string resp;
  std::string url = GDFS_FILE_URL + entry->file_id + "/export?mimeType=application%2Fpdf";

retry:
  try {
    resp = this->auth.sendRequest(url, DOWNLOAD, "");

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
    buf = new char[resp.size()];
    assert(buf != NULL);
    memcpy(buf, resp.c_str(), resp.size());
    entry->mtime = entry->ctime = time(NULL);
    this->cache.put(entry->file_id, buf, 0, resp.size(), node, true);
    entry->file_size = resp.size();
  }
}


/*
 * Retrieve the children list,
 * given a pointer to the node of the directory,
 * in the directory tree.
 */
void
GDrive::get_children (struct GDFSNode * parent)
{

  Debug("<-- Entering get_children() -->");

  assert(parent != NULL);

  bool g_doc = false;
  bool is_dir;
  bool dir_modified = false;
  int count = 0;
  int err_num = 0;
  uint64_t file_size;
  time_t mtime;
  time_t atime;
  std::string resp;
  std::string url;
  std::string error;
  std::string file_name;
  std::string file_id;
  std::string parent_file_id = parent->entry->file_id;
  std::string change_id_;
  std::string mime_type;
  json::Value val;
  json::Value * child;
  mode_t file_mode;
  struct GDFSEntry * entry = parent->entry;
  struct GDFSNode * child_node = NULL;
  std::vector <json::Value *> child_items;
  std::unordered_map <std::string, GDFSNode *>::iterator it_node;
  std::set <std::string> s1;
  std::set <std::string> s2;
  std::set <std::string> s3;
  std::queue <struct GDFSNode *> deleted_child;

retry_parent:
  // Check for modification in the directory.
  if (parent->entry->pending_get == true) {
    parent->entry->pending_get = false;
    dir_modified = true;
  } else if (parent->file_name == "/") {
    url = GDFS_CHANGE_URL;

    // Send the request to Drive API.
    try {
      resp = this->auth.sendRequest(url, GET);
    } catch (GDFSException & err) {
      err_num = ECOMM;
      error = resp + "\n" + err.get();
      goto out;
    }

    val.parse(resp);

    // Get the change id of the Drive account.
    try {
      change_id_ = val["startPageToken"].get();
    } catch (GDFSException & err) {
      if (val["error"]["code"].get() == "403") {
        sleep(1);
        goto retry_parent;
      }
      err_num = EAGAIN;
      error  = "Google Drive: Error code = ";
      error += val["error"]["code"].get() + ", " + val["error"]["message"].get();
      goto out;
    }

    // Check for modifications in root directory.
    if (change_id_ != this->change_id) {
      dir_modified = true;
      this->change_id = change_id_;
    }

  } else {
    url = GDFS_FILE_URL + parent_file_id + "?fields=modifiedTime";

    try {
      resp = this->auth.sendRequest(url, GET);
    } catch (GDFSException & err) {
      err_num = ECOMM;
      error = err.get();
      goto out;
    }

    val.clear();
    val.parse(resp);

    try {
      mtime = rfc3339_to_sec(val["modifiedTime"].get());
    } catch (GDFSException & err) {
      // Can only happen if the mkdir request has not reached Drive.
      // Wait until it becomes available in Drive.
      if (val["error"]["code"].get() == "404") {
        if (++count < 5) {
          sleep(1);
          goto retry_parent;
        } else {
          err_num = ENOENT;
          error = "Drive: File " + parent->file_name + " not found when retrieving children";
        }
      } else if (val["error"]["code"].get() == "403") {
        sleep(1);
        goto retry_parent;
      } else {
        err_num = EAGAIN;
        error  = "Drive: Error code = ";
        error += val["error"]["code"].get() + ", " + val["error"]["message"].get();
      }
      goto out; 
    }

    if (mtime > entry->mtime ||
        parent->is_empty() == true) {
      dir_modified = true;
    }
  }

  // Directory is not modified.
  // Get the children list from cache.
  if (dir_modified == false) {
    Debug("Directory %s not modified on Drive. Not updating", parent->file_name.c_str());
    goto out;
  }

  // Construct the URL to send the request.
  url  = GDFS_FILE_URL_ + std::string("?pageSize=1000&q='") + parent_file_id;
  url += "'+in+parents+and+trashed+%3D+false&orderBy=name&spaces=drive";
  url += "&fields=files(id%2CmimeType%2CmodifiedTime%2Cname%2Csize%2CviewedByMeTime)%2CnextPageToken";

  // Retrieve the list of children.
retry_children:
  do {
    val.clear();

    // Send the request to get the children list.
    try {
      resp = this->auth.sendRequest(url, GET);
    } catch (GDFSException & err) {
      err_num = ECOMM;
      error = err.get();
      goto out;
    }

    val.parse(resp);

    // Get the list of child items.
    try {
      child_items = val["files"].getArray();
    } catch (GDFSException & err) {
      if (val["error"]["code"].get() == "403") {
        sleep(1);
        goto retry_children;
      }
      err_num = EAGAIN;
      error  = "Google Drive: Error code = ";
      error += val["error"]["code"].get() + ", " + val["error"]["message"].get();
      goto out;
    }

    // Get the link to next set of child items.
    try {
      url  = GDFS_FILE_URL_ + std::string("?pageSize=1000&q='") + parent_file_id;
      url += "'+in+parents+and+trashed+%3D+false&orderBy=name&spaces=drive&pageToken=" + val["nextPageToken"].get();
      url += "&fields=files(id%2CmimeType%2CmodifiedTime%2Cname%2Csize%2CviewedByMeTime)%2CnextPageToken";
    } catch (GDFSException & err) {
      url = "";
    }

    // Retrieve each of the children.
    for (unsigned i = 0; i < child_items.size(); i++) {
      child = child_items[i];

      // Get the title and id of the file.
      // Guaranted that title and id should exist, if reached this point.
      file_name = child->find("name")->get();
      std::replace(file_name.begin(), file_name.end(), '/', '_');

      file_id = child->find("id")->get();
      mtime   = rfc3339_to_sec(child->find("modifiedTime")->get());

      // If the file has not been viewed by this user before,
      // viewedByMeTime wont exist.
      try {
        atime = rfc3339_to_sec(child->find("viewedByMeTime")->get());
      } catch (GDFSException & err) {
        atime = mtime;
      }

      // Check whether its a directory or not.
      // Get the file size, if not a directory.
      g_doc = false;
      is_dir = false;
      file_size = 0;
      try {
        mime_type = child->find("mimeType")->get();

        if (mime_type == "application/vnd.google-apps.folder") {
          is_dir = true;
        } else {
          // Check for Google Docs.
          if (mime_type == "application/vnd.google-apps.document" ||
              mime_type == "application/vnd.google-apps.spreadsheet" ||
              mime_type == "application/vnd.google-apps.drawing" ||
              mime_type == "application/vnd.google-apps.presentation") {
            g_doc = true;
            file_name += ".pdf";
          } else {
            try {
              file_size = std::stoull(child->find("size")->get());
            } catch (GDFSException & err) {
              file_size = 0;
            }
          }
        }
      } catch (GDFSException & err) {
        try {
          file_size = std::stoull(child->find("size")->get());
        } catch (GDFSException & err) {
          file_size = 0;
        }
      }

      // Check whether the file id exist.
      it_node = file_id_node.find(file_id);
      if (it_node != file_id_node.end()) {
        entry = it_node->second->entry;

        // Pending DELETE or WRITE request in request queue.
        if (entry->dirty == true) {
          continue;
        }

        if (entry->write == true) {
          s1.emplace(entry->file_id);
          continue;
        }

        // Download Google Docs file if there is modification on Drive.
        if (g_doc && entry->mtime < mtime) {
          this->download_file(it_node->second);
        } else if (is_dir && entry->mtime < mtime) {
          entry->pending_get = true;
        }

        if (file_name == it_node->second->file_name) {
          if (g_doc == false && entry->write == false) {
            entry->file_size = file_size;
          }
          entry->atime = atime;
          entry->mtime = mtime;
        } else {
          // Name conflict.
          if (g_doc == false &&
              is_old_name_conflict(file_name, it_node->second->file_name) == false &&
              it_node->second->link == 0) {
            file_name = remove_name_conflict(file_name, is_dir, parent);
            it_node->second->file_name = file_name;
          }

          if (g_doc == false && entry->write == false) {
            entry->file_size = file_size;
          }
          entry->atime = atime;
          entry->mtime = mtime;
        }

      } else {
        file_mode = g_doc ? GDFS_DEF_GDOC_MODE : (is_dir ? GDFS_DEF_DIR_MODE : GDFS_DEF_FILE_MODE);
        file_name = remove_name_conflict(file_name, is_dir, parent);
        entry = new GDFSEntry(file_id, file_size, is_dir,
                              atime, mtime, this->uid, this->gid, file_mode, mime_type, g_doc);
        child_node = parent->insert(new GDFSNode(file_name, entry, parent));
        file_id_node.emplace(file_id, child_node);
        Debug("Created a new entry for %s in directory structure", file_name.c_str());

        if (g_doc) {
          this->download_file(child_node);
        }
      }

      s1.emplace(entry->file_id);
    }

  } while (url.empty() == false);

  // Find the original list of children.
  for (auto child : parent->get_children()) {
    s2.emplace(child.second->entry->file_id);
  }

  // Compare the original list with new list,
  // to find deleted entries/
  std::set_difference(s2.begin(), s2.end(), s1.begin(), s1.end(),
                      std::inserter(s3, s3.end()));
  for (auto s : s3) {
    auto its = file_id_node.equal_range(s);
    for (auto it = its.first; it != its.second; ++it) {
      entry = it->second->entry;
      if (entry->file_id.compare(0, gdfs_name_prefix.size(), gdfs_name_prefix) != 0 &&
          entry->file_open == false &&
          entry->dirty == false &&
          entry->pending_create == false) {
        deleted_child.emplace(it->second);
      }
    }
  }

  // Recursively delete all those children,
  // and their children.
  while (deleted_child.empty() == false) {
    child_node = deleted_child.front();
    deleted_child.pop();
    child_node->parent->remove_child(child_node);
    delete child_node;
    child_node = NULL;
  }

out:
  if (error.empty() == false) {
    errno = err_num;
    throw GDFSException(error);
  }

  Debug("<-- Exiting get_children() -->");
}


void
GDrive::generate_file_id (void)
{

  Debug("<-- Entering generate_file_id() -->");

  std::string url;

  // Replenish list of file ids if required.
  if (file_id_q.empty() || file_id_q.size() <= 100) {
    url = GDFS_FILE_URL + std::string("generateIds");
    url += "?count=1000&space=drive&fields=ids";
    threadpool.build_request(std::string(),
                             GENERATE_ID,
                             NULL,
                             url);

  }

out:
  Debug("<-- Exiting generate_file_id() -->");
}


/*
 * Function to create a new directory.
 */
void
GDrive::make_dir (const std::string & file_name,
                  mode_t file_mode,
                  struct GDFSNode * parent_node,
                  uid_t uid_,
                  gid_t gid_)
{

  assert(parent_node != NULL);

  Debug("<-- Entering make_dir() -->");
  Debug("Creating new directory %s under %s", file_name.c_str(), parent_node->file_name.c_str());

  time_t mtime;
  std::string query;
  std::string file_id;
  std::string url = GDFS_FILE_URL_ + std::string("?fields=modifiedTime");
  std::string parent_id = parent_node->entry->file_id;
  struct GDFSEntry * entry = NULL;
  struct GDFSNode * node = NULL;

  generate_file_id();
  file_id = file_id_q.front();
  file_id_q.pop();

  // Build the query.
  query = "{ \"id\": \"" + file_id + "\", ";
  query += "\"name\": \"" + file_name + "\", \"mimeType\": \"application/vnd.google-apps.folder\"";
  query += ", \"parents\": [\"" + parent_id + "\"] }";

  // mtime shall be updated after the INSERT request has been completed.
  mtime = time(NULL);

  // Add to the directory tree.
  entry = new GDFSEntry(file_id, 0, true, mtime, mtime, uid_, gid_, file_mode);
  node = parent_node->insert(new GDFSNode(file_name, entry, parent_node));
  file_id_node.emplace(file_id, node);

  threadpool.build_request(file_id, INSERT, node, url, query);

  Debug("<-- Exiting make_dir() -->");
}


/*
 * Function to create a new file.
 */
void
GDrive::make_file (const std::string & file_name,
                   mode_t file_mode,
                   struct GDFSNode * parent_node,
                   uid_t uid_,
                   gid_t gid_)
{

  assert (parent_node != NULL);

  Debug("<-- Entering make_file() -->");
  Debug("Creating new file %s under %s", file_name.c_str(), parent_node->file_name.c_str());

  time_t mtime;
  std::string query;
  std::string file_id;
  std::string url = GDFS_FILE_URL_ + std::string("?fields=modifiedTime");
  std::string parent_id = parent_node->entry->file_id;
  struct GDFSEntry * entry = NULL;
  struct GDFSNode * node = NULL;

  if (file_name.at(0) == '.') {
    file_id = gdfs_name_prefix + rand_str();
  } else {
    generate_file_id();
    if (file_id_q.empty()) {
			throw GDFSException("error detected. file ids not retrieved");
    }
    file_id = file_id_q.front();
    file_id_q.pop();
  }

  // Build the query.
  query  = "{ \"id\": \"" + file_id + "\", \"name\": \"" + file_name + "\"";
  query += ", \"parents\": [\"" + parent_id + "\"] }";

  // mtime shall be updated after the INSERT request has been completed.
  mtime = time(NULL);

  // Add to the directory tree.
  entry = new GDFSEntry(file_id, 0, false, mtime, mtime, uid_, gid_, file_mode);
  assert(entry != NULL);
  node = parent_node->insert(new GDFSNode(file_name, entry, parent_node));
  assert(node != NULL);
  file_id_node.emplace(file_id, node);

  if (file_id.compare(0, gdfs_name_prefix.size(), gdfs_name_prefix) != 0) {
    entry->pending_create = true;
    threadpool.build_request(file_id, INSERT, node, url, query);  
  }

  Debug("<-- Exiting make_file() -->");
}


/*
 * Function to delete a file or empty directory.
 */
void
GDrive::delete_file (struct GDFSNode * node,
                     bool delete_req)
{

  assert(node != NULL);

  Debug("<-- Entering delete_file() -->");
  Debug("Deleteing file %s", node->file_name.c_str());

  struct GDFSEntry * entry = node->entry;
  std::string file_id = entry->file_id;
  std::string url = GDFS_FILE_URL + file_id;
  std::queue <struct GDFSNode *> q_nodes;
  std::unordered_map <std::string, struct GDFSNode *>::iterator it_node;

  // Remove it from parent list if not root node.
  if (node->parent) {
    node->parent->remove_child(node);
    node->parent = NULL;
  }

  if (entry->is_dir) {
    for (auto f : node->get_children()) {
      q_nodes.emplace(f.second);
    }
  } else if (entry->ref_count == 1) {
    this->cache.remove(file_id);
  }

  if (file_id.compare(0, gdfs_name_prefix.size(), gdfs_name_prefix) != 0 &&
      delete_req &&
      (entry->ref_count == 1 || (entry->is_dir && entry->ref_count <= 2))) {    
    entry->dirty = true;
    threadpool.build_request(file_id, DELETE, node, url);
  } else {
    // In case of hard links, multiple inodes have the same file id.
    // Make sure to delete the correct node.
    auto its = file_id_node.equal_range(entry->file_id);
    for (auto it = its.first; it != its.second; ++it) {
      if (it->second == node) {
        file_id_node.erase(it);
        break;
      }
    }

    delete node;
    node = NULL;
  }

  // Recursively delete all children.
  while (q_nodes.empty() == false) {
    node = q_nodes.front();
    q_nodes.pop();

    entry = node->entry;
    file_id = entry->file_id;

    auto its = file_id_node.equal_range(file_id);
    for (auto it = its.first; it != its.second; ++it) {
      if (it->second == node) {
        file_id_node.erase(it);
        break;
      }
    }

    if (entry->is_dir) {
      for (auto f : node->get_children()) {
        q_nodes.emplace(f.second);
      }
    } else if (entry->ref_count == 1) {
      this->cache.remove(file_id);
    }

    delete node;
  }

  Debug("<-- Exiting delete_file() -->");
}



/*
 * Function to rename a file/directory.
 */
void
GDrive::rename_file (struct GDFSNode * node,
                     const std::string & new_name)
{

  assert(node != NULL);

  Debug("<-- Entering rename_file -->");
  Debug("Renaming file %s to %s", node->file_name.c_str(), new_name.c_str());

  std::string url;
  std::string query = "{ \"name\": \"" + new_name + "\" }";
  std::string file_id = node->entry->file_id;
  std::string old_file_name = node->file_name;

  // Update the name of the file.
  node->file_name = new_name;

  // Update in parent list.
  node->parent->rename_child(old_file_name, new_name);

  // GDFS special file.
  if (file_id.compare(0, gdfs_name_prefix.size(), gdfs_name_prefix) == 0){
    goto out;
  }

  if (old_file_name.at(0) != '.') {
    // Construct the RENAME request.
    url = GDFS_FILE_URL + file_id + std::string("?fields=modifiedTime");

    // Send the request.
    threadpool.build_request(file_id,
                             UPDATE,
                             node,
                             url,
                             query);
  }

out:
  Debug("<-- Exiting rename_file() -->");
}


/*
 * Function to modify the atime and mtime of a file.
 */
void
GDrive::set_utime (struct GDFSNode * node)
{

  assert (node != NULL);

  Debug("<-- Entering set_utime() -->");
  Debug("Updating mtime and atime of file %s", node->file_name.c_str());

  std::string url;
  std::string query;
  struct GDFSEntry * entry = node->entry;
  std::string file_id = entry->file_id;

  // GDFS special file.
  if (file_id.compare(0, gdfs_name_prefix.size(), gdfs_name_prefix) == 0){
    goto out;
  }

  // Construct the request.
  url = GDFS_FILE_URL + file_id + "?fields=modifiedTime";
  query  = "{ \"modifiedTime\": \"" + to_rfc3339(entry->mtime) + "\",";
  query += " \"viewedByMeTime\": \"" + to_rfc3339(entry->atime) + "\" }";

  // Send the request.
  // threadpool.build_request(file_id, UPDATE, node, url, query);

out:
  Debug("<-- Exiting set_utime() -->");
}

