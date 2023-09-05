
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



#ifndef GDFS_H__
#define GDFS_H__

#define FUSE_USE_VERSION 26

#include <string>

#include "fuse.h"
#include "gdapi.h"

#define GDFS_DATA ((class GDrive *) fuse_get_context()->private_data)

std::string rand_str (void);

int gdfs_getattr(const char * path, struct stat * statbuf);
int gdfs_readlink(const char * path, char * link, size_t size);
int gdfs_mknod(const char* path, mode_t mode, dev_t dev);
int gdfs_mkdir(const char* path, mode_t mode);
int gdfs_unlink(const char* path);
int gdfs_rmdir(const char* path);
int gdfs_symlink(const char* path, const char* link);
int gdfs_rename(const char* path, const char* newpath);
int gdfs_link(const char* path, const char* newpath);
int gdfs_chmod(const char* path, mode_t mode);
int gdfs_chown(const char* path, uid_t uid, gid_t gid);
int gdfs_truncate(const char* path, off_t newsize);
int gdfs_open(const char* path, struct fuse_file_info* fi);
int gdfs_read(const char* path, char* buf, size_t size, off_t offset, struct fuse_file_info* fi);
int gdfs_write(const char* path, const char* buf, size_t size, off_t offset, struct fuse_file_info* fi);
int gdfs_statfs(const char* path, struct statvfs* statv);
int gdfs_flush(const char* path, struct fuse_file_info* fi);
int gdfs_release(const char* path, struct fuse_file_info* fi);
int gdfs_fsync(const char* path, int datasync, struct fuse_file_info* fi);
int gdfs_setxattr(const char* path, const char* name, const char* value, size_t size, int flags);
int gdfs_getxattr(const char* path, const char* name, char* value, size_t size);
int gdfs_listxattr(const char* path, char* list, size_t size);
int gdfs_removexattr(const char* path, const char* name);
int gdfs_opendir(const char* path, struct fuse_file_info* fi);
int gdfs_readdir(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* fi);
int gdfs_releasedir(const char* path, struct fuse_file_info* fi);
int gdfs_fsyncdir(const char* path, int datasync, struct fuse_file_info* fi);
void* gdfs_init(struct fuse_conn_info* conn);
void gdfs_destroy(void* userdata);
int gdfs_access(const char* path, int mask);
int gdfs_create(const char* path, mode_t mode, struct fuse_file_info* fi);
int gdfs_ftruncate(const char* path, off_t offset, struct fuse_file_info* fi);
int gdfs_fgetattr(const char* path, struct stat* statbuf, struct fuse_file_info* fi);
int gdfs_write_buf(const char *, struct fuse_bufvec *buf, off_t off, struct fuse_file_info *);
int gdfs_read_buf(const char* path, struct fuse_bufvec **bufp, size_t size, off_t off, struct fuse_file_info* fi);
int gdfs_fallocate(const char* path, int, off_t offseta, off_t offsetb, struct fuse_file_info* fi);
int gdfs_lock(const char* path, struct fuse_file_info* fi, int cmd, struct flock* lock);
int gdfs_utime(const char * path, struct utimbuf * ubuf);

struct initFUSEoper {
  static void init(struct fuse_operations& gdfs_oper) {
    gdfs_oper.getattr     = gdfs_getattr;
    gdfs_oper.readlink    = gdfs_readlink;
    gdfs_oper.mknod       = gdfs_mknod;
    gdfs_oper.mkdir       = gdfs_mkdir;
    gdfs_oper.unlink      = gdfs_unlink;
    gdfs_oper.rmdir       = gdfs_rmdir;
    gdfs_oper.symlink     = gdfs_symlink;
    gdfs_oper.rename      = gdfs_rename;
    gdfs_oper.link        = gdfs_link;
    gdfs_oper.chmod       = gdfs_chmod;
    gdfs_oper.chown       = gdfs_chown;
    gdfs_oper.truncate    = gdfs_truncate;
    gdfs_oper.utime       = gdfs_utime;
    gdfs_oper.statfs      = gdfs_statfs;
    gdfs_oper.open        = gdfs_open;
    gdfs_oper.read        = gdfs_read;
    gdfs_oper.write       = gdfs_write;
    gdfs_oper.release     = gdfs_release;
    gdfs_oper.readdir     = gdfs_readdir;
    gdfs_oper.access      = gdfs_access;
    gdfs_oper.create      = gdfs_create;
    gdfs_oper.init        = gdfs_init;
    gdfs_oper.destroy     = gdfs_destroy;
    gdfs_oper.flush       = NULL;
    gdfs_oper.getdir      = NULL;
    gdfs_oper.utimens     = NULL;
    gdfs_oper.opendir     = NULL; //gdfs_opendir;
    gdfs_oper.releasedir  = NULL; //gdfs_releasedir;
    gdfs_oper.setxattr    = NULL;
    gdfs_oper.getxattr    = NULL; //gdfs_getxattr;
    gdfs_oper.listxattr   = NULL;
    gdfs_oper.removexattr = NULL;
    gdfs_oper.fsyncdir    = NULL;
    gdfs_oper.fallocate   = NULL;
    gdfs_oper.fsync       = NULL;
    gdfs_oper.ftruncate   = NULL;
    gdfs_oper.fgetattr    = NULL; //gdfs_fgetattr;
    gdfs_oper.write_buf   = NULL;
    gdfs_oper.read_buf    = NULL; //gdfs_read_buf;
    gdfs_oper.lock        = NULL;
    gdfs_oper.poll        = NULL;
  }
};

int initGDFS (const std::string & rootDir,
              const std::string & path,
              int argc,
              char ** argv);

#endif // GDFS_H__
