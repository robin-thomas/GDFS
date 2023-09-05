
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



#ifndef GDAPI_H__
#define GDAPI_H__

#include <string>
#include <unordered_map>
#include <set>
#include <ctime>
#include <queue>
#include <cstdint>
#include <map>

#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <sys/stat.h>

#include "dir_tree.h"
#include "auth.h"
#include "cache.h"
#include "threadpool.h"


const std::string gdfs_name_prefix = "null";


std::string
remove_name_conflict (std::string & file_name,
                      bool is_dir,
                      struct GDFSNode * parent);
bool
is_old_name_conflict (const std::string & new_file_name,
                      const std::string & old_file_name);



/*********************************************/
/*            GDFS CORE FUNCTIONS            */
/*                                           */
/*********************************************/



class GDrive {
  public:
    uid_t uid;
    gid_t gid;
    time_t mounting_time;
    uint64_t bytes_used;
    uint64_t bytes_free;
    uint64_t bytes_total;
    std::string rootDir;
    std::string change_id;
    Auth auth;
    LRUCache cache;
    Threadpool threadpool;
    struct GDFSNode * root;

    GDrive (const std::string & rootDir_,
            const std::string & path_) :
      rootDir(rootDir_),
      auth(path_ + "gdfs.auth"),
      cache(auth),
      threadpool(this, auth),
      root(NULL)
    {
      mounting_time = time(NULL);
      uid           = getuid();
      gid           = getgid();
    }


    ~GDrive (void)
    {
      if (this->root) {
        this->delete_file(this->root, false);
      }
    }


    uint64_t
    get_no_files (void);

    int
    file_access (uid_t uid,
                 gid_t gid,
                 int mask,
                 struct GDFSEntry * entry);

    struct GDFSNode *
    get_node (const std::string & path,
              uid_t uid,
              gid_t gid,
              bool search = false);

    bool
    get_root (void);

    void
    update_node (struct GDFSNode * node);

    void
    get_children (struct GDFSNode * parent);

    int
    update_file_entry (const std::string & path);

    void
    generate_file_id (void);

    void
    delete_file (struct GDFSNode * node,
                 bool delete_req = true);

    void
    delete_dir (struct GDFSNode * node);

    void
    rename_file (struct GDFSNode * node,
                 const std::string & new_name);

    void
    make_dir (const std::string & file_name,
              mode_t file_mode,
              struct GDFSNode * parent_node,
              uid_t uid_,
              gid_t gid_);

    void
    make_file (const std::string & file_name,
               mode_t file_mode,
               struct GDFSNode * parent_node,
               uid_t uid_,
               gid_t gid_);

    size_t
    read_file (struct GDFSEntry * entry,
               char * buf,
               off_t offset,
               size_t len);

    void
    download_file (struct GDFSNode * node);

    void
    write_file (struct GDFSNode * node);

    void
    set_utime (struct GDFSNode * node);

    void
    empty_file (struct GDFSEntry * entry);

};


struct pArg {
  GDrive& gdi;
  std::string path;
  pArg(GDrive& gdiobj, const std::string& path) : gdi(gdiobj) {
    this->path = path;
  }
};


#endif // GDAPI_H__
