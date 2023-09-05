
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



#ifndef DIR_TREE_H__
#define DIR_TREE_H__

#include <string>
#include <unordered_map>

#include <time.h>
#include <unistd.h>
#include <assert.h>

#include "common.h"


struct GDFSNode;
extern std::unordered_multimap <std::string, GDFSNode *> file_id_node;


/****************************************************/
/*            GOOGLE DRIVE FILE METADATA            */
/*                                                  */
/****************************************************/


struct GDFSEntry {
  std::string file_id;
  uint64_t file_size;
  time_t ctime;
  time_t mtime;
  time_t atime;
  time_t cached_time;
  uid_t uid;
  gid_t gid;
  mode_t file_mode;
  dev_t dev;
  bool is_dir;
  int ref_count;
  std::string mime_type;
  bool g_doc;
  bool dirty;
  bool pending_create;
  bool file_open;
  bool write;
  bool pending_get;

  // To store file reference.
  GDFSEntry (const std::string & file_id_,
             uint64_t file_size_,
             bool is_dir_,
             const std::string & atime_,
             const std::string & mtime_,
             uid_t uid_,
             gid_t gid_,
             mode_t file_mode_,
             std::string mime_type_ = "",
             bool g_doc_ = false,
             dev_t dev_ = 0) :
    file_id(file_id_),
    file_size(file_size_),
    uid(uid_),
    gid(gid_),
    file_mode(file_mode_),
    dev(dev_),
    is_dir(is_dir_),
    mime_type(mime_type_),
    g_doc(g_doc_),
    dirty(false),
    pending_create(false),
    file_open(false),
    write(false),
    pending_get(false)
  {
    this->atime = rfc3339_to_sec(atime_);
    this->ctime = this->mtime = rfc3339_to_sec(mtime_);
    this->ref_count   = is_dir_ ? 2 : 1;
    this->cached_time = time(NULL);
  }

  // To store file reference.
  GDFSEntry (const std::string & file_id_,
             uint64_t file_size_,
             bool is_dir_,
             time_t atime_,
             time_t mtime_,
             uid_t uid_,
             gid_t gid_,
             mode_t file_mode_,
             std::string mime_type_ = "",
             bool g_doc_ = false,
             dev_t dev_ = 0) :
    file_id(file_id_),
    file_size(file_size_),
    ctime(mtime_),
    mtime(mtime_),
    atime(atime_),
    uid(uid_),
    gid(gid_),
    file_mode(file_mode_),
    dev(dev_),
    is_dir(is_dir_),
    mime_type(mime_type_),
    g_doc(g_doc_),
    dirty(false),
    pending_create(false),
    file_open(false),
    write(false),
    pending_get(false)
  {
    this->ref_count   = is_dir_ ? 2 : 1;
    this->cached_time = time(NULL);
  }
};


struct GDFSNode {
  char link;
  std::string file_name;
  std::string sym_link;
  GDFSEntry * entry;
  GDFSNode  * parent;
  std::unordered_map <std::string, struct GDFSNode *> children;


  //////////////////////////////////
  //  Constructors & Destructors  //
  //////////////////////////////////

  GDFSNode (void) :
    link(0),
    entry(NULL),
    parent(NULL)
  {
    this->children.clear();
  }


  GDFSNode (const std::string & file_name_,
            struct GDFSEntry * entry_,
            struct GDFSNode * parent_) :
    link(0),
    file_name(file_name_),
    entry(entry_), 
    parent(parent_)
  {
    this->children.clear();
  }


  GDFSNode (const std::string & file_name_,
            struct GDFSEntry * entry_,
            struct GDFSNode * parent_,
            char link_) :
    link(link_),
    file_name(file_name_),
    entry(entry_),
    parent(parent_)
  {
    this->children.clear();
  }


  GDFSNode (const std::string & file_name_,
            struct GDFSEntry * entry_,
            struct GDFSNode * parent_,
            char link_,
            const char * sym_link_) :
    link(link_),
    file_name(file_name_),
    entry(entry_),
    parent(parent_)
  {
    sym_link = std::string(sym_link_, entry_->file_size);
    this->children.clear();
  }


  ~GDFSNode (void)
  {
    --(this->entry->ref_count);
    if (this->entry->ref_count == 0 ||
        (this->entry->is_dir && this->entry->ref_count <= 1)) {
      delete this->entry;
    }
    this->entry = NULL;
    this->parent = NULL;
  }


  ///////////////////////////
  //  Function prototypes  //
  ///////////////////////////


  struct GDFSNode *
  find (const std::string & file_name);

  struct GDFSNode *
  insert (struct GDFSNode * node);

  std::unordered_map <std::string, struct GDFSNode *> &
  get_children (void);

  bool
  is_empty (void);

  void
  remove_child (struct GDFSNode * child);

  void
  rename_child (const std::string & old_file_name,
                const std::string & new_file_name);

};

#endif // DIR_TREE_H__
