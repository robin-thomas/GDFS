
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



#include <iostream>
#include <exception>
#include <string>
#include <set>
#include <unordered_map>
#include <algorithm>
#include <ctime>

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <libgen.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <assert.h>
#include <limits.h>

#include "gdfs.h"
#include "json.h"
#include "log.h"
#include "dir_tree.h"
#include "common.h"
#include "exception.h"


void *
gdfs_init (struct fuse_conn_info * conn)
{
  Info("Mounting GDFS filesytem...");
  return GDFS_DATA;
}


void
gdfs_destroy (void * userdata)
{
  Info("Unmounting GDFS filesytem...");
}



/**********************************/
/*          System Calls          */
/*                                */
/**********************************/


int
gdfs_getattr (const char * path,
              struct stat * statbuf)
{

  Debug("<-- Entering getattr() SYSCALL -->");
  Debug("<-- Trying to get file attributes for %s", path);

  int ret = 0;
  std::string file_name;
  uid_t uid = fuse_get_context()->uid;
  gid_t gid = fuse_get_context()->gid;
  struct GDFSEntry * entry = NULL;
  struct GDFSNode * node = NULL;
  struct GDrive * state = GDFS_DATA;

  // Check for invalid parameters from fuse.
  if (path == NULL || *path == 0 || statbuf == NULL) {
    ret = -EINVAL;
    Error("getattr(): invalid parameters from fuse");
    goto out;
  }

  file_name = base_name(path);
  if (file_name == ".Trash" ||
      file_name == ".Trash-1000" ||
      file_name == ".hidden") {
    ret = -ENOENT;
    goto out;
  }

  memset(statbuf, 0, sizeof (*statbuf));

  // Check whether the path exists.
  try {
    node = state->get_node(path, uid, gid, true);
  } catch (GDFSException & err) {
    ret = -errno;
    Error("getattr(): %s, %s", path, err.get().c_str());
    goto out;
  }
  entry = node->entry;

  // Set the file mode, depending on the file type.
  if (entry->is_dir) {
    statbuf->st_mode = S_IFDIR | entry->file_mode;
  } else {
    switch (node->link) {
      case 's':
        statbuf->st_mode = S_IFLNK | entry->file_mode;
        break;

      case 'f':
        statbuf->st_mode = S_IFIFO | entry->file_mode;
        break;

      case 'c':
        statbuf->st_mode = S_IFCHR | entry->file_mode;
        break;

      case 'b':
        statbuf->st_mode = S_IFBLK | entry->file_mode;
        break;

      case 'k':
        statbuf->st_mode = S_IFSOCK | entry->file_mode;
        break;

      default:
        statbuf->st_mode = S_IFREG | entry->file_mode;
    }
  }

  statbuf->st_rdev   = entry->dev;
  statbuf->st_nlink = entry->ref_count;
  statbuf->st_size  = entry->file_size;
  statbuf->st_ctim  = {entry->ctime, 0};
  statbuf->st_mtim  = {entry->mtime, 0};
  statbuf->st_atim  = {entry->atime, 0};
  statbuf->st_uid   = entry->uid;
  statbuf->st_gid   = entry->gid;

out:
  Debug("<-- Exiting getattr() SYSCALL -->");
  return ret;
}


int
gdfs_mkdir(const char * path,
           mode_t mode)
{

  Debug("<-- Entering mkdir() SYSCALL -->");

  int ret = 0;
  uid_t uid = fuse_get_context()->uid;
  gid_t gid = fuse_get_context()->gid;
  struct GDrive * state = GDFS_DATA;
  struct GDFSNode * node = NULL;  
  struct GDFSNode * parent_node = NULL;
  std::string parent;
  std::string file_name;

  // Check for invalid parameters from fuse.
  if (path == NULL || *path == 0) {
    ret = -EINVAL;
    Error("mkdir(): invalid parameters from fuse");
    goto out;
  }

  // Cannot create root directory.
  if (strcmp(path, "/") == 0) {
    ret = -EPERM;
    Error("mkdir(): invalid operation on root directory");
    goto out;
  }

  parent = dir_name(path);
  file_name = base_name(path);

  // Retrieve the parent node.
  try {
    parent_node = state->get_node(parent, uid, gid);
  } catch (GDFSException & err) {
    ret = -errno;
    Error("mkdir(): %s", err.get().c_str());
    goto out;
  }

  // Check for access permissions.
  // User needs both EXECUTE & WRITE permissions on parent.
  if (state->file_access(uid, gid, (W_OK | X_OK), parent_node->entry) != 0) {
    ret = -EACCES;
    Error("mkdir(): user does not have permissions");
    goto out;
  }

  // Check whether filename is longer than supported.
  if (file_name.size() > GDFS_NAME_MAX_LEN) {
    ret = -ENAMETOOLONG;
    Error("mkdir(): filename %s too long", file_name.c_str());
    goto out;
  }

  // Check whether the pathname is longer than supported.
  if (strlen(path) > GDFS_PATH_MAX_LEN) {
    ret = -ENAMETOOLONG;
    Error("mkdir(): pathname %s too long", path);
    goto out;
  }

  // Check whether a directory of same name already exists.
  node = parent_node->find(file_name);
  if (node != NULL && node->entry->dirty == false) {
    ret = -EEXIST;
    Error("mkdir(): file %s already exists", path);
    goto out;
  }

  // Make the directory in Google Drive.
  state->make_dir(file_name, mode, parent_node, uid, gid);

out:
  Debug("<-- Exiting mkdir() SYSCALL -->");
  return ret;
}


int
gdfs_readdir (const char * path,
              void * buf,
              fuse_fill_dir_t filler,
              off_t offset,
              struct fuse_file_info * fi)
{

  Debug("<-- Entering readdir() SYSYCALL -->");

  int ret = 0;
  uid_t uid = fuse_get_context()->uid;
  gid_t gid = fuse_get_context()->gid;
  struct GDrive * state = GDFS_DATA;
  struct GDFSNode * node = NULL;

  // Check for invalid parameters from fuse.
  if (path == NULL || *path == 0 || buf == NULL) {
    ret = -EINVAL;
    Error("readdir(): invalid parameters from fuse");
    goto out;
  }

  // Check whether directory exist.
  try {
    node = state->get_node(path, uid, gid);
    assert(node != NULL);
    state->get_children(node);
  } catch (GDFSException & err) {
    ret = -errno;
    Error("readdir(): %s, %s", path, err.get().c_str());
    goto out;
  }

  // Check for access permissions.
  if (state->file_access(uid, gid, R_OK, node->entry) != 0) {
    ret = -EACCES;
    Error("readdir(): user does not have read permission for %s", path);
    goto out;
  }

  // Put the list in the filler to be displayed.
  // Load the . and .. entries to be displayed.
  if (filler(buf, ".", NULL, 0) || filler(buf, "..", NULL, 0)) {
    ret = -ENOMEM;
    Error("Filler full! path=%s", path);
    goto out;
  }

  // Retrieve the children list.
  for (auto child : node->get_children()) {
    if (filler(buf, child.second->file_name.c_str(), NULL, 0)) {
      ret = -ENOMEM;
      Error("Filler full! path=%s, child=%s", path, child.second->file_name.c_str());
      goto out;
    }
  }

out:
  Debug("<-- Exiting readdir() SYSCALL -->");
  return ret;
}


int
gdfs_rmdir (const char * path)
{

  Debug("<-- Entering rmdir() SYSYCALL -->");

  int ret = 0;
  uid_t uid = fuse_get_context()->uid;
  gid_t gid = fuse_get_context()->gid;
  struct GDrive * state = GDFS_DATA;
  struct GDFSNode * node = NULL;
  struct GDFSEntry * entry = NULL;

  // Check for invalid parameters from fuse.
  if (path == NULL || *path == 0) {
    ret = -EINVAL;
    Error("rmdir(): invalid parameters from fuse");
    goto out;
  }

  // Should not allow to delete root directory.
  if (strcmp(path, "/") == 0) {
    ret = -EPERM;
    Error("rmdir(): invalid operation on root directory");
    goto out;
  }

  // Check whether the file exists.
  try {
    node = state->get_node(path, uid, gid);
  } catch (GDFSException & err) {
    ret = -errno;
    Error("rmdir(): %s, %s", path, err.get().c_str());
    goto out;
  }
  entry = node->entry;

  // Check to see whether the directory is empty.
  if (node->is_empty() == false) {
    ret = -ENOTEMPTY;
    Error("rmdir() Cannot delete %s. Directory not empty", path);
    goto out;
  }

  // Check access permissions.
  if (state->file_access(uid, gid, W_OK, node->parent->entry) != 0) {
    ret = -EACCES;
    Error("rmdir(): User doesnt have permission in parent directory of %s", path);
    goto out;
  }
  if ((S_ISVTX & entry->file_mode) && uid != 0 && uid != entry->uid) {
    ret = -EACCES;
    Error("rmdir(): sticky bit set; only root/owner have permission to delete %s", path);
    goto out;
  }

  // Remove the directory from Google Drive
  state->delete_file(node);

out:
  Debug("<-- Exiting rmdir() SYSCALL -->");
  return ret;
}


int
gdfs_create (const char * path,
             mode_t mode,
             struct fuse_file_info * fi)
{

  Debug("<-- Entering create() SYSCALL -->");

  int ret = 0;
  uid_t uid = fuse_get_context()->uid;
  gid_t gid = fuse_get_context()->gid;
  std::string parent;
  std::string file_name;
  struct GDrive * state = GDFS_DATA;
  struct GDFSNode * node = NULL;
  struct GDFSNode * parent_node = NULL;

  // Check for invalid parameters from fuse.
  if (path == NULL || *path == 0) {
    ret = -EINVAL;
    Error("create(): invalid parameters from fuse");
    goto out;
  }

  // Invalid operation on root directory.
  if (strcmp(path, "/") == 0) {
    ret = -EPERM;
    Error("create(): invalid operation on root directory");
    goto out;
  }

  parent = dir_name(path);
  file_name = base_name(path);

  // Check whether the path does exist.
  try {
    parent_node = state->get_node(parent, uid, gid);
  } catch (GDFSException & err) {
    ret = -errno;
    Error("create(): %s, %s", path, err.get().c_str());
    goto out;
  }

  // Check access permissions.
  // Needs EXECUTE and WRITE permission on parent.
  if (state->file_access(uid, gid, (W_OK | X_OK), parent_node->entry) != 0) {
    ret = -EACCES;
    Error("create(): User doesnt have permission to create files in %s", parent.c_str());
    goto out;
  }

  // Check whether file name is longer than supported.
  if (file_name.size() > GDFS_NAME_MAX_LEN) {
    ret = -ENAMETOOLONG;
    Error("create(): file name %s too long", file_name.c_str());
    goto out;
  }

  // Check whether file already exists.
  node = parent_node->find(file_name);
  if (node != NULL && node->entry->dirty == false) {
    ret = -EEXIST;
    Error("create(): path %s already exist", path);
    goto out;
  }

  // Create the file in Google Drive
  try {
    state->make_file(file_name, mode, parent_node, uid, gid);
  } catch (GDFSException & err) {
    ret = -EAGAIN;
    Error("create(): path %s, error %s", path, err.get().c_str());
    goto out;
  }

out:
  Debug("<-- Exiting create() SYSCALL -->");
  return ret;
}


int
gdfs_mknod (const char * path,
            mode_t mode,
            dev_t dev)
{

  Debug("<-- Entering mknod() SYSCALL -->");

  int ret = 0;
  char c = 0;
  time_t mtime;
  uid_t uid = fuse_get_context()->uid;
  gid_t gid = fuse_get_context()->gid;
  struct GDrive * state = GDFS_DATA;
  struct GDFSNode * parent_node = NULL;
  struct GDFSNode * node = NULL;
  struct GDFSEntry * entry = NULL;
  std::string parent;
  std::string file_name;
  std::string file_id;

  // Check for invalid parameters from fuse.
  if (path == NULL || *path == 0) {
    ret = -EINVAL;
    Error("mknod(): invalid parameters from fuse");
    goto out;
  }

  // Cannot create root directory.
  if (strcmp(path, "/") == 0) {
    ret = -EPERM;
    Error("mknod(): invalid operation on root directory");
    goto out;
  }

  // Detect the type of file to be created.
  if (mode & S_IFREG) {
    c = 0;
  } else if (mode & S_IFCHR) {
    c = 'c';
  } else if (mode & S_IFBLK) {
    c = 'b';
  } else if (mode & S_IFIFO) {
    c = 'f';
  } else if (mode & S_IFSOCK) {
    c = 'k';
  }

  parent = dir_name(path);
  file_name = base_name(path);

  // Check whether parent exists.
  try {
    parent_node = state->get_node(parent, uid, gid);
  } catch (GDFSException & err) {
    ret = -errno;
    Error("mknod(): %s, %s", path, err.get().c_str());
    goto out;
  }

  // Check for access permissions.
  // User needs both EXECUTE & WRITE permissions on parent.
  if (state->file_access(uid, gid, (W_OK | X_OK), parent_node->entry) != 0) {
    ret = -EACCES;
    Error("mknod(): user does not have permissions");
    goto out;
  }

  // Check whether filename is longer than supported.
  if (file_name.size() > GDFS_NAME_MAX_LEN) {
    ret = -ENAMETOOLONG;
    Error("mknod(): filename %s too long", file_name.c_str());
    goto out;
  }

  // Check whether path exists.
  node = parent_node->find(file_name);
  if (node != NULL && node->entry->dirty == false) {
    ret = -EEXIST;
    Error("mknod(): file %s already exists", path);
    goto out;
  }

  // Create the new node.
  if (mode & S_IFREG) {
    state->make_file(file_name, (GDFS_DEF_FILE_MODE | mode), parent_node, uid, gid);
  } else {
    mtime = time(NULL);
    file_id = gdfs_name_prefix + rand_str();
    entry = new GDFSEntry(file_id, 0, false, mtime, mtime, state->uid,
                          state->gid, (GDFS_DEF_FILE_MODE | mode), "", false, dev);
    assert(entry != NULL);
    node = parent_node->insert(new GDFSNode(file_name, entry, parent_node, c));
    assert(node != NULL);
    file_id_node.emplace(file_id, node);
  }

out:
  Debug("<-- Exiting mknod() SYSCALL -->");
  return ret;
}


int
gdfs_symlink (const char * path,
              const char * link)
{

  Debug("<-- Entering symlink() SYSYCALL -->");

  int ret = 0;
  uid_t uid = fuse_get_context()->uid;
  gid_t gid = fuse_get_context()->gid;
  time_t mtime;
  mode_t file_mode = GDFS_DEF_FILE_MODE;
  std::string parent;
  std::string file_name;
  std::string file_id;
  struct GDrive * state = GDFS_DATA;
  struct GDFSNode * parent_node = NULL;
  struct GDFSNode * node = NULL;
  struct GDFSEntry * entry = NULL;

  // Check for invalid parameters from fuse.
  if (path == NULL || *path == 0 ||
      link == NULL || *link == 0) {
    ret = -EINVAL;
    Error("symlink(): invalid parameters from fuse");
    goto out;
  }

  // Check whether its for root directory.
  if (strcmp(path, "/") == 0) {
    ret = -EPERM;
    Error("symlink(): invalid operation on root directory");
    goto out;
  }

  parent = dir_name(link);
  file_name = base_name(link);

  // Check whether the parent of new link exists.
  try {
    parent_node = state->get_node(parent, uid, gid);
  } catch (GDFSException & err) {
    ret = -errno;
    Error("symlink(): %s, %s", parent.c_str(), err.get().c_str());
    goto out;
  }

  // Check for access permissions.
  if (state->file_access(uid, gid, (W_OK | X_OK), parent_node->entry) != 0) {
    ret = -EACCES;
    Error("symlink(): user does not have permission for %s", parent.c_str());
    goto out;
  }

  // Check whether file name is longer than supported.
  if (file_name.size() > GDFS_NAME_MAX_LEN) {
    ret = -ENAMETOOLONG;
    Error("symlink(): file name %s too long", file_name.c_str());
    goto out;
  }

  // Check whether file with that name already exists.
  node = parent_node->find(file_name);
  if (node != NULL && node->entry->dirty == false) {
    ret = -EEXIST;
    Error("symlink(): link %s already exists", link);
    goto out;
  }

  // Create the new link.
  mtime = time(NULL);
  file_id = gdfs_name_prefix + rand_str();
  entry = new GDFSEntry(file_id, strlen(path) + 1, 0, mtime, mtime, state->uid, state->gid, file_mode);
  assert(entry != NULL);
  node = parent_node->insert(new GDFSNode(file_name, entry, parent_node, 's', path));
  assert(node != NULL);
  file_id_node.emplace(file_id, node);

out:
  Debug("<-- Exiting symlink SYSCALL -->");
  return ret;
}


int
gdfs_readlink (const char * path,
               char * link,
               size_t size)
{

  Debug("<-- Entering readlink() SYSCALL -->");

  int ret = 0;
  uid_t uid = fuse_get_context()->uid;
  gid_t gid = fuse_get_context()->gid;
  struct GDrive * state = GDFS_DATA;
  struct GDFSNode * node = NULL;
  std::string parent;
  std::string file_name;

  // Check for invalid parameters from fuse.
  if (path == NULL || *path == 0) {
    ret = -EINVAL;
    Error("readlink(): invalid parameters from fuse");
    goto out;
  }

  parent    = dir_name(path);
  file_name = base_name(path);

  // Cannot create root directory.
  if (strcmp(path, "/") == 0) {
    ret = -EPERM;
    Error("readlink(): invalid operation on root directory");
    goto out;
  }

  // Check whether path exists.
  try {
    node = state->get_node(path, uid, gid);
  } catch (GDFSException & err) {
    ret = -errno;
    Error("readlink(): %s, %s", path, err.get().c_str());
    goto out;
  }
  assert(node != NULL);

  // Check access permissions.
  if (state->file_access(uid, gid, R_OK, node->entry) != 0) {
    ret = -EACCES;
    Error("readlink(): user cannot access %s", path);
    goto out;
  }

  // Check whether its a symlink.
  if (node->link != 's') {
    ret = -EINVAL;
    Error("readlink(): path %s not a symlink", path);
    goto out;
  }

  // Read the link.
  memcpy(link, node->sym_link.c_str(), node->sym_link.size());

out:
  Debug("<-- Exiting readlink() SYSCALL -->");
  return ret;
}


int
gdfs_link (const char * path,
           const char * newpath)
{

  Debug("<-- Entering link() SYSCALL -->");

  int ret = 0;
  uid_t uid = fuse_get_context()->uid;
  gid_t gid = fuse_get_context()->gid;
  std::string new_parent;
  std::string new_file_name;
  struct GDrive * state = GDFS_DATA;
  struct GDFSNode * node = NULL;
  struct GDFSNode * tmp_node = NULL;
  struct GDFSEntry * entry = NULL;

  // Check for invalid parameters from fuse.
  if (path == NULL || *path == 0 ||
      newpath == NULL || *newpath == 0) {
    ret = -EINVAL;
    Error("link(): invalid parameters from fuse");
    goto out;
  }

  // Check whether its for root directory.
  if (strcmp(path, "/") == 0) {
    ret = -EPERM;
    Error("link(): invalid operation on root directory");
    goto out;
  }

  new_parent = dir_name(newpath);
  new_file_name = base_name(newpath);

  // Check whether the file to which link is to be built exists.
  try {
    node = state->get_node(path, uid, gid);
  } catch (GDFSException & err) {
    ret = -errno;
    Error("link(): %s, %s", path, err.get().c_str());
    goto out;
  }
  entry = node->entry;

  // Check whether the link is to be made to a directory.
  if (entry->is_dir == true) {
    ret = -EPERM;
    Error("link(): hard link not allowed to directory");
    goto out;
  }

  // Check whether the directory where link is to be created exists.
  try {
    node = state->get_node(new_parent, uid, gid);
  } catch (GDFSException & err) {
    ret = -errno;
    Error("link(): %s, %s", new_parent.c_str(), err.get().c_str());
    goto out;
  }

  // Check for access permissions.
  // Need READ & WRITE permisisons on path, and
  // EXECUTE & WRITE permission on link directory.
  if (state->file_access(uid, gid, (R_OK | W_OK), entry) != 0) {
    ret = -EACCES;
    Error("link(): user does not have permission for %s", path);
    goto out;
  }
  if (state->file_access(uid, gid, (W_OK | X_OK), node->entry) != 0) {
    ret = -EACCES;
    Error("link(): user does not have permission for %s", new_parent.c_str());
    goto out;
  }

  // Check if new path exists.
  tmp_node = node->find(new_file_name);
  if (tmp_node != NULL && tmp_node->entry->dirty == false) {
    ret = -EEXIST;
    Error("link(): path %s does exist", newpath);
    goto out;
  }
  tmp_node = NULL;

  ++(entry->ref_count);
  tmp_node = node->insert(new GDFSNode(new_file_name, entry, node, 'h'));
  assert(tmp_node != NULL);
  file_id_node.emplace(entry->file_id, tmp_node);

out:
  Debug("<-- Exiting link() SYSCALL -->");
  return ret;
}


int
gdfs_unlink (const char * path)
{

  Debug("<-- Entering unlink() SYSCALL-->");

  int ret = 0;
  uid_t uid = fuse_get_context()->uid;
  gid_t gid = fuse_get_context()->gid;
  struct GDrive * state = GDFS_DATA;
  struct GDFSNode * node = NULL;
  struct GDFSEntry * entry = NULL;

  // Check for invalid parameters from fuse.
  if (path == NULL || *path == 0) {
    ret = -EINVAL;
    Error("unlink(): invalid parameters from fuse");
    goto out;
  }

  // Should not allow to delete root directory.
  if (strcmp(path, "/") == 0) {
    ret = -EPERM;
    Error("unlink(): invalid operation on root directory");
    goto out;
  }

  // Check whether the file exists.
  try {
    node = state->get_node(path, uid, gid);
  } catch (GDFSException & err) {
    ret = -errno;
    Error("unlink(): %s, %s", path, err.get().c_str());
    goto out;
  }
  entry = node->entry;

  // Check for access permissions on parent directory.
  if (state->file_access(uid, gid, W_OK, node->parent->entry) != 0) {
    ret = -EACCES;
    Error("unlink(): user does not have write permission on parent directory of %s", path);
    goto out;
  }

  // Check access permissions on file.
  /*if ((S_ISVTX & entry->file_mode) && (uid != 0 && uid != entry->uid)) {
    ret = -EACCES;
    Error("unlink(): only root/owner have permission to delete %s", path);
    goto out;
  }*/

  // Update Google Drive.
  Debug("deleting %s", node->file_name.c_str());
  state->delete_file(node);

out:
  Debug("<-- Exiting unlink() SYSCALL -->");
  return ret;
}


int
gdfs_rename (const char * path,
             const char * newpath)
{

  Debug("<-- Entering rename() SYSYCALL -->");

  int ret = 0;
  bool to_write = false;
  uid_t uid = fuse_get_context()->uid;
  gid_t gid = fuse_get_context()->gid;
  std::string new_file_name;
  std::string new_file_id;
  struct GDrive * state = GDFS_DATA;
  struct GDFSNode * node = NULL;
  struct GDFSNode * tmp_node = NULL;
  std::unordered_map <std::string, struct GDFSNode *>::iterator it_node;

  // Check for invalid parameters from fuse.
  if (path == NULL || *path == 0 ||
      newpath == NULL || *newpath == 0) {
    ret = -EINVAL;
    Error("rename(): invalid parameters from fuse");
    goto out;
  }

  // Check whether its for root directory.
  if (strcmp(path, "/") == 0) {
    ret = -EPERM;
    Error("rename(): invalid operation on root directory");
    goto out;
  }

  new_file_name = base_name(newpath);

  // Check whether the file exists.
  try {
    node = state->get_node(path, uid, gid);
  } catch (GDFSException & err) {
    ret = -errno;
    Error("rename(): %s, %s", path, err.get().c_str());
    goto out;
  }

  // Check whether the new name is longer than supported.
  if (new_file_name.size() > GDFS_NAME_MAX_LEN) {
    ret = -ENAMETOOLONG;
    Error("rename(): file name %s too long", new_file_name.c_str());
    goto out;
  }

  // Check access permissions.
  if ((S_ISVTX & node->entry->file_mode) && (uid != 0 && uid != node->entry->uid)) {
    ret = -EACCES;
    Error("rename(): sticky bit set; only the owner/root user can rename a file");
    goto out;
  }

  // Check if the new name already exists.
  tmp_node = node->parent->find(new_file_name);
  if (tmp_node != NULL && tmp_node->entry->dirty == false) {
    if (tmp_node->entry->is_dir && tmp_node->is_empty() == false) {
      ret = -EEXIST;
      Error("rename(): new file name %s already exists", new_file_name.c_str());
      goto out;
    }
    Warning("rename(): new file name %s already exists. Replacing it.", new_file_name.c_str());
    if (node->file_name.at(0) == '.') {
      new_file_id = tmp_node->entry->file_id;
      state->delete_file(tmp_node, false);
      state->cache.change(node->entry->file_id, new_file_id);

      it_node = file_id_node.find(node->entry->file_id);
      file_id_node.erase(it_node);
      file_id_node.emplace(new_file_id, node);

      node->entry->file_id = new_file_id;
      to_write = true;
    } else {
      state->delete_file(tmp_node);
    }
  }

  // Rename file in Google Drive.
  state->rename_file(node, new_file_name);
  if (to_write) {
    state->write_file(node);
  }

out:
  Debug("<-- Exiting rename() SYSCALL -->");
  return ret;
}


int
gdfs_chmod (const char * path,
            mode_t mode)
{

  Debug("<-- Entering chmod() SYSCALL -->");

  int ret = 0;
  char mode_str[10];
  uid_t uid_ = fuse_get_context()->uid;
  gid_t gid_ = fuse_get_context()->gid;
  struct GDrive * state = GDFS_DATA;
  struct GDFSNode * node = NULL;
  struct GDFSEntry * entry = NULL;
  static const char *rwx[] = {"---", "--x", "-w-", "-wx", "r--", "r-x", "rw-", "rwx"};

  // Get the mode in string form.
  strcpy(&mode_str[0], rwx[(mode >> 6) & 7]);
  strcpy(&mode_str[3], rwx[(mode >> 3) & 7]);
  strcpy(&mode_str[6], rwx[(mode & 7)]);
  if (mode & S_ISUID) {
    mode_str[2] = (mode & S_IXUSR) ? 's' : 'S';
  }
  if (mode & S_ISGID) {
    mode_str[5] = (mode & S_IXGRP) ? 's' : 'l';
  }
  if (mode & S_ISVTX) {
    mode_str[8] = (mode & S_IXUSR) ? 't' : 'T';
  }
  mode_str[9] = '\0';
  Debug("Trying to change permissions of %s to %s", path, mode_str);

  // Check for invalid parameters from fuse.
  if (path == NULL || *path == 0) {
    ret = -EINVAL;
    Error("chmod(): invalid parameters from fuse");
    goto out;
  }

  // Do not allow to change permissions of root directory.
  if (strcmp(path, "/") == 0) {
    ret = -EPERM;
    Error("chmod(): invalid operation on root directory");
    goto out;
  }

  // Check if path length is within limits.
  if (strlen(path) > GDFS_PATH_MAX_LEN) {
    ret = -ENAMETOOLONG;
    Error("chmod(): path name exceeded PATH_MAX characters");
    goto out;
  }

  // Check whether the file exists.
  try {
    node = state->get_node(path, uid_, gid_);
  } catch (GDFSException & err) {
    ret = -errno;
    Error("chmod(): %s, %s", path, err.get().c_str());
    goto out;
  }
  entry = node->entry;

  // Check for access permissions.
  if (uid_ != 0 && uid_ != entry->uid) {
    ret = -EPERM;
    Error("chmod(): only owner/root can change file permissions");
    goto out;
  }

  // Change the file permissions.
  entry->file_mode = mode;

  // Change the time.
  entry->ctime = time(NULL);

out:
  Debug("<-- Exiting chmod() SYSCALL -->");
  return ret;
}


int
gdfs_chown (const char * path,
            uid_t uid,
            gid_t gid)
{

  Debug("<-- Entering chown() SYSCALL -->");

  int ret = 0;
  uid_t uid_ = fuse_get_context()->uid;
  gid_t gid_ = fuse_get_context()->gid;
  struct GDrive * state = GDFS_DATA;
  struct GDFSNode * node = NULL;
  struct GDFSEntry * entry = NULL;

  // Check for invalid parameters from fuse.
  if (path == NULL || *path == 0) {
    ret = -EINVAL;
    Error("chown(): invalid parameters from fuse");
    goto out;
  }

  Debug("Trying to change owner:group of %s to %d:%d", path, uid, gid);

  // Should not allow to change owner of root.
  if (strcmp(path, "/") == 0) {
    ret = -EPERM;
    Error("chown(): invalid operation on root directory");
    goto out;
  }

  // Check whether the file exists.
  try {
    node = state->get_node(path, uid_, gid_);
  } catch (GDFSException & err) {
    ret = -errno;
    Error("chown(): %s, %s", path, err.get().c_str());
    goto out;
  }
  entry = node->entry;

  // Check for access permissions.
  if (uid_ != 0 && uid_ != entry->uid) {
    ret = -EPERM;
    Error("chown(): User = %d, Owner = %d, Only owner/root user can change file permissions", uid_, entry->uid);
    goto out;
  }

  // Change the uid and gid
  entry->uid = uid;
  entry->gid = gid;

  // Update the ctime.
  entry->ctime = time(NULL);

out:
  Debug("<-- Exiting chown() SYSYCALL -->");
  return ret;
}


int
gdfs_truncate (const char * path,
               off_t newsize)
{

  Debug("<-- Entering write() SYSCALL -->");

  int ret = 0;
  off_t start = 0;
  size_t size;
  char * buf = NULL;
  time_t mtime;
  std::string url;
  std::string resp;
  uid_t uid = fuse_get_context()->uid;
  gid_t gid = fuse_get_context()->gid;
  struct GDrive * state = GDFS_DATA;
  struct GDFSNode * node = NULL;
  struct GDFSEntry * entry = NULL;

  // Check for invalid parameters from fuse.
  if (path == NULL || *path == 0 || newsize < 0) {
    ret = -EINVAL;
    Error("truncate(): invalid parameters from fuse");
    goto out;
  }

  // Check whether the file exists.
  try {
    node = state->get_node(path, uid, gid);
  } catch (GDFSException & err) {
    ret = -errno;
    Error("truncate(): %s, %s", path, err.get().c_str());
    goto out;
  }
  entry = node->entry;

  // Check for access permissions.
  if (state->file_access(uid, gid, W_OK, entry) != 0) {
    ret = -EACCES;
    Error("truncate(): user does not have write permission for %s", path);
    goto out;
  }

  // Check to see truncate to empty file.
  mtime = time(NULL);
  if (newsize == 0) {
    state->cache.put(entry->file_id, NULL, 0, 0, NULL);
  } else {
    entry->write = true;

    start = newsize > entry->file_size ? entry->file_size : newsize - 1;
    size = newsize > entry->file_size ? (newsize - entry->file_size) : (entry->file_size - newsize);
    if (start > entry->file_size) {
      buf = new char[size];
      assert(buf != NULL);
      memset(buf, 0, size);
    }

    // Update cache with the updated file.
    if (newsize > entry->file_size) {
      if (state->cache.put(entry->file_id, buf, start, size, node, false) == false) {
        ret = -EAGAIN;
        Error("truncate(): unable to write %s to cache", path);
        goto out;
      }
    } else if (newsize < entry->file_size) {
      state->cache.resize(entry->file_id, newsize);
    }
  }

  state->cache.set_time(entry->file_id, mtime);
  entry->mtime = entry->ctime = mtime;
  entry->file_size = newsize;

out:
  Debug("<-- Exiting truncate() SYSCALL -->");
  return ret;
}


int
gdfs_utime (const char * path,
            struct utimbuf * ubuf)
{

  Debug("<-- Entering utime() SYSCALL -->");

  int ret = 0;
  uid_t uid = fuse_get_context()->uid;
  gid_t gid = fuse_get_context()->gid;
  struct GDrive * state = GDFS_DATA;
  time_t mtime = ubuf ? ubuf->modtime : time(NULL);
  time_t atime = ubuf ? ubuf->actime  : time(NULL);
  struct GDFSNode * node = NULL;
  struct GDFSEntry * entry = NULL;

  // Check for invalid parameters from fuse.
  if (path == NULL || *path == 0 || ubuf == NULL) {
    ret = -EINVAL;
    Error("utime(): invalid parameters from fuse");
    goto out;
  }

  Info("checking utime for %s", path);

  // Check whether the file exists.
  try {
    node = state->get_node(path, uid, gid);
  } catch (GDFSException & err) {
    ret = -errno;
    Error("utime(): %s, %s", path, err.get().c_str());
    goto out;
  }
  entry = node->entry;

  // Update the directory listing.
  entry->mtime = mtime;
  entry->atime = atime;

  // Change the times.
  if (strcmp(path, "/") != 0) {
    state->set_utime(node);
  }

out:
  Debug("<-- Exiting utime() SYSCALL -->");
  return ret;
}


int
gdfs_open (const char * path,
           struct fuse_file_info * fi)
{

  Debug("<-- Entering open() SYSCALL -->");

  int ret = 0;
  std::string parent;
  std::string file_name;
  uid_t uid = fuse_get_context()->uid;
  gid_t gid = fuse_get_context()->gid;
  struct GDrive * state = GDFS_DATA;
  struct GDFSNode * node = NULL;
  struct GDFSNode * parent_node = NULL;

  // Check for invalid parameters from fuse.
  if (path == NULL || *path == 0) {
    ret = -EINVAL;
    Error("open(): invalid parameters from fuse");
    goto out;
  }

  parent = dir_name(path);
  file_name = base_name(path);

  // Check whether the parent does exist.
  try {
    parent_node = state->get_node(parent, uid, gid);
  } catch (GDFSException & err) {
    ret = -errno;
    Error("open(): %s, %s", path, err.get().c_str());
    goto out;
  }

  // Check whether the file exists.
  node = parent_node->find(file_name);
  if (node == NULL) {
    gdfs_create(path, GDFS_DEF_FILE_MODE, NULL);
    node = parent_node->find(file_name);
    assert(node != NULL);
  } else {
    // Check for access permissions.
    if (state->file_access(uid, gid, R_OK, node->entry) != 0) {
      ret = -EACCES;
      Error("open(): user does not have read permission for %s", path);
      goto out;
    }
  }
  node->entry->file_open = true;

out:
  Debug("<-- Exiting open() SYSCALL -->");
  return ret;  
}


int
gdfs_read (const char * path,
           char * buf,
           size_t size,
           off_t offset,
           struct fuse_file_info * fi)
{

  Debug("<-- Entering read() SYSCALL -->");

  int ret = 0;
  uid_t uid = fuse_get_context()->uid;
  gid_t gid = fuse_get_context()->gid;
  struct GDrive * state = GDFS_DATA;
  struct GDFSNode * node = NULL;
  struct GDFSEntry * entry = NULL;

  // Check for invalid parameters from fuse.
  if (path == NULL || *path == 0 ||
      buf == NULL) {
    ret = -EINVAL;
    Error("read(): invalid parameters from fuse");
    goto out;
  }

  // Check whether the file exists.
  try {
    node = state->get_node(path, uid, gid);
  } catch (GDFSException & err) {
    ret = -errno;
    Error("read(): %s, %s", path, err.get().c_str());
    goto out;
  }
  entry = node->entry;

  // Check for access permissions.
  if (state->file_access(uid, gid, R_OK, entry) != 0) {
    ret = -EACCES;
    Error("read(): user does not have read permission for %s", path);
    goto out;
  }

  // Check whether file is empty.
  if (entry->file_size == 0) {
    Debug("File %s is empty. Zero READ", node->file_name.c_str());
    goto out;
  }

  // Read the file from cache.
  size = (size > entry->file_size ? entry->file_size : size);
  try {
    ret = state->cache.get(entry->file_id, buf, offset, size, node);
  } catch (GDFSException & err) {
    ret = -EAGAIN;
    Error("read(): %s, %s", path, err.get().c_str());
    goto out;
  }

  // Update the file access time.
  entry->atime = time(NULL);

out:
  Debug("<-- Exiting read() SYSCALL -->");
  return ret;
}


int
gdfs_write (const char * path,
            const char * buf,
            size_t size,
            off_t offset,
            struct fuse_file_info * fi)
{

  Debug("<-- Entering write() SYSCALL -->");
  Debug("writing %d bytes to %s at offset %d", size, path, offset);

  int ret = 0;
  size_t newsize = 0;
  char * new_buf = NULL;
  uid_t uid = fuse_get_context()->uid;
  gid_t gid = fuse_get_context()->gid;
  struct GDrive * state = GDFS_DATA;
  struct GDFSNode * node = NULL;
  struct GDFSEntry * entry = NULL;

  // Check for invalid parameters from fuse.
  if (path == NULL || *path == 0 || buf == NULL) {
    ret = -EINVAL;
    Error("write(): invalid parameters from fuse");
    goto out;
  }

  // Check whether the file exists.
  try {
    node = state->get_node(path, uid, gid);
  } catch (GDFSException & err) {
    ret = -errno;
    Error("write(): %s, %s", path, err.get().c_str());
    goto out;
  }
  entry = node->entry;

  // Check for access permissions.
  if (state->file_access(uid, gid, W_OK, entry) != 0) {
    ret = -EACCES;
    Error("write(): user does not have write permission for %s", path);
    goto out;
  }

  // Put the updated file into cache.
  entry->mtime = time(NULL);
  entry->file_size = entry->file_size > (offset + size) ? entry->file_size : (offset + size);
  try {
    ret = state->cache.put(entry->file_id, const_cast<char*>(buf), offset, size, node, false);
    entry->write = true;
  } catch (GDFSException & err) {
    ret = -EAGAIN;
    Error("write(): %s, %s", path, err.get().c_str());
    goto out;
  }

out:
  Debug("<-- Exiting write() SYSCALL -->");
  return size;
}


int
gdfs_release (const char * path,
              struct fuse_file_info * fi)
{

  Debug("<-- Entering open() SYSCALL -->");

  int ret = 0;
  uid_t uid = fuse_get_context()->uid;
  gid_t gid = fuse_get_context()->gid;
  struct GDrive * state = GDFS_DATA;
  struct GDFSNode * node = NULL;
  struct GDFSEntry * entry = NULL;
  std::string s;

  // Check for invalid parameters from fuse.
  if (path == NULL || *path == 0) {
    ret = -EINVAL;
    Error("release(): invalid parameters from fuse");
    goto out;
  }

  // Check whether the file exists.
  try {
    node = state->get_node(path, uid, gid);
  } catch (GDFSException & err) {
    ret = -errno;
    Error("release(): %s, %s", path, err.get().c_str());
    goto out;
  }
  entry = node->entry;

  // Check for access permissions.
  if (state->file_access(uid, gid, R_OK, entry) != 0) {
    ret = -EACCES;
    Error("release(): user does not have read permission for %s", path);
    goto out;
  }

  // Write the file to Google Drive.
  s = rand_str();
  try {
    if (entry->file_size > 0 && entry->write) {
      state->write_file(node);
    }
  } catch (GDFSException & err) {
    ret = -EAGAIN;
    Error("release(): %s, %s", path, err.get().c_str());
  }
  entry->write = false;
  entry->file_open = false;

out:
  Debug("<-- Exiting release() SYSCALL -->");
  return ret;
}


int
gdfs_statfs (const char * path,
             struct statvfs * statv)
{

  Debug("<-- Entering statfs SYSCALL -->");

  int ret = 0;
  uid_t uid = fuse_get_context()->uid;
  gid_t gid = fuse_get_context()->gid;
  struct GDrive * state = GDFS_DATA;

  // Check for invalid parameters from fuse.
  if (path == NULL || *path == 0 || statv == NULL) {
    ret = -EINVAL;
    Error("statfs(): invalid parameters from fuse");
    goto out;
  }

  // Check whether directory exist.
  try {
    (void) state->get_node(path, uid, gid);
  } catch (GDFSException & err) {
    ret = -errno;
    Error("statfs(): %s, %s", path, err.get().c_str());
    goto out;
  }

  // Set the GDFS paramters.
  statv->f_bsize    = GDFS_BLOCK_SIZE;                             // Filesystem block size
  statv->f_frsize   = GDFS_FRAGMENT_SIZE;                          // Fragment size
  statv->f_blocks   = (state->bytes_total / statv->f_frsize);      // Size of fs in f_frsize units
  statv->f_bfree    = (state->bytes_free / statv->f_frsize);       // Number of free blocks
  statv->f_bavail   = statv->f_bfree;                              // Number of free blocks for unprivileged users
  statv->f_files    = state->get_no_files();                              // Number of inodes
  //statv->f_ffree    = ;                                            // Number of free inodes
  //statv->f_favail   = ;                                            // Number of free inodes for unprivileged users
  //statv->f_fsid     = ;                                            // Filesystem ID
  //statv->f_flag     = ;                                            // Mount flags
  statv->f_namemax  = GDFS_NAME_MAX_LEN;                           // Maximum filename length

out:
  Debug("<-- Exiting statfs SYSCALL -->");  
  return ret;
}


int
gdfs_access (const char * path,
             int mask)
{

  Debug("<-- Entering access() SYSCALL -->");

  int ret = 0;
  uid_t uid = fuse_get_context()->uid;
  gid_t gid = fuse_get_context()->gid;
  struct GDrive * state = GDFS_DATA;
  struct GDFSNode * node = NULL;
  struct GDFSEntry * entry = NULL;

  // Check for invalid parameters from fuse.
  if (path == NULL || *path == 0) {
    ret = -EINVAL;
    Error("access(): invalid parameters from fuse");
    goto out;
  }

  // Check whether the path does exist.
  try {
    node = state->get_node(path, uid, gid);
  } catch (GDFSException & err) {
    ret = -errno;
    Error("access(): %s, %s", path, err.get().c_str());
    goto out;
  }
  entry = node->entry;

  // Check access permissions.
  ret = state->file_access(uid, gid, mask, entry);

out:
  Debug("<-- Exiting access() SYSCALL -->");
  return ret;
}


int
initGDFS (const std::string & rootDir,
          const std::string & path,
          int argc,
          char ** argv)
{

  Debug("<-- Entering initGDFS() -->");

  int ret = -1;
  static fuse_operations gdfs_oper;
  struct GDrive * gdi = NULL;

  // Set the GDFS system calls.
  initFUSEoper::init(gdfs_oper);

  // If no path is provided, then its for getting HELP or VERSION info.
  // Mount GDFS lite version.
  if (path.empty() == true) {
    ret = fuse_main(argc, argv, &gdfs_oper, NULL);
  } else {
    gdi = new GDrive(rootDir, path);
    assert (gdi != NULL);

    // Load the encrypted authentication file.
    try {
      gdi->auth.load_auth_file();
      gdi->auth.check_access_token();
    } catch (GDFSException & err) {
      Error("%s", err.get().c_str());
      goto out;
    }

    // Mount GDFS.
    if (gdi->get_root() == true) {
      gdi->generate_file_id();
      ret = fuse_main(argc, argv, &gdfs_oper, gdi);
    }
  }

out:
  delete gdi;
  gdi = NULL;
  Debug("<-- Exiting initGDFS() -->");
  return ret;
}

