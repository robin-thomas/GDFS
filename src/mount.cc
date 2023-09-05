
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

#include <string>
#include <algorithm>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>


#include "mount.h"
#include "log.h"
#include "gdfs.h"


mount_::mount_opt * mount_::mnt_opt = NULL;


// Constructor.
mount_::mount_opt::mount_opt (int opt_len,
                              const char * mount_point,
                              std::string & conf_path,
                              char ** opt)
{
  this->opt_len = opt_len;
  this->mount_path = (char *) mount_point;
  this->conf_path = conf_path;
  this->opt = opt;
}


void
mount_::set_mount_opt (int opt_len,
                       const char * mount_point,
                       std::string & conf_path,
                       char ** opt)
{

  Debug("<-- Entering set_mount_opt() -->");

  mnt_opt = new mount_::mount_opt(opt_len, mount_point, conf_path, opt);

  Debug("<-- Exiting set_mount_opt() -->");
}


void
mount_::del_mount_opt (void)
{
  delete mnt_opt;
}


bool
mount_::is_fs_running (const char * mount_path)
{

  Debug("<-- Entering is_fs_running() -->");

  bool ret = false;
  char command[100];
  FILE * fp = NULL;

  // Check whether the file system is already mounted.
  if (mount_path) {    
    sprintf(command, "cat /etc/mtab | grep %s | wc -c", mount_path);
    fp = popen(command, "r");
    if (fgetc(fp) != '0') {
      ret = true;
    }
    pclose(fp);
  }

  if (ret) {
    Debug("GDFS is running at %s", mount_path);
  } else {
    Debug("GDFS is not running.");
  }

  Debug("<-- Exiting is_fs_running() -->");
  return ret;
}


void
mount_::mount_fs_lite (int argc,
                       char ** argv)
{
  (void) initGDFS("", "", argc, argv);
}


bool
mount_::mount_fs (void)
{

  Debug("<-- Entering mount_fs() -->");

  int count = 0;
  bool ret = false;
  char command[100];

mount:
  if (is_fs_running(mount_::mnt_opt->mount_path) == false) {
    if (initGDFS(mount_::mnt_opt->mount_path,
                 mount_::mnt_opt->conf_path,
                 mount_::mnt_opt->opt_len,
                 mount_::mnt_opt->opt)) {
      Fatal("Unable to initialize GDFS file system");
      goto out;
    }
    ret = true;
  } else {
    Warning("Trying to mount fs at %s. Already mounted.",
            mount_::mnt_opt->mount_path);
    Warning("Trying to unmount, and mount again");
    sprintf(command, "umount -l %s", mount_::mnt_opt->mount_path);
    system(command);
    if (++count > 3) {
      Fatal("Unable to initialize GDFS file system");
      goto out;
    }
    goto mount;
  }

out:
  Debug("<-- Exiting mount_fs() -->");
  return ret;
}


// Unmount the filesystem.
void
mount_::unmount_fs (void)
{

  Debug("<-- Entering unmount_fs() -->");

  char command[100];
  FILE * fp = NULL;

  if (is_fs_running(mount_::mnt_opt->mount_path)) {
    memset(command, 0, sizeof command);
    sprintf(command, "fusermount -u %s | wc -c", mount_::mnt_opt->mount_path);
    fp = popen(command, "r");
    if (fgetc(fp) != '0') {
      Error("Unable to unmount fs at %s using fsermount. Trying lazy unmount.",
            mount_::mnt_opt->mount_path);
      sprintf(command, "fusermount -uqz %s", mount_::mnt_opt->mount_path);
      system(command);
    }
    pclose(fp);

    Info("GDFS unmounted sucessfully.");
  } else {
    Warning("Trying to unmount fs at %s. Not mounted.", mount_::mnt_opt->mount_path);
  }

  delete mount_::mnt_opt;
  mount_::mnt_opt = NULL;

  Debug("<-- Exiting unmount_fs() -->");
}

