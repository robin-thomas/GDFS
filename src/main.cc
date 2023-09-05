
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

#include <unistd.h>
#include <stdio.h>
#include <pwd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>

#include "mount.h"
#include "log.h"
#include "main.h"
#include "conf.h"
#include "exception.h"


int
main (int argc,
      char ** argv)
{

  bool no_mount = false;
  int ret = 0;
  int arg;
  int len;
  int new_argc = 0;
  int stat = -1;
  char * new_argv[argc];
  std::string error;
  std::string log_dir_path;
  std::string log_level = "ERROR";
  std::string conf_path = GDFS_DEST_DIR;

  // Parse command-line arguments.
  new_argv[new_argc++] = argv[0]; 
  while ((arg = getopt_long(argc, argv, optstr,
                            long_opt, NULL)) != -1) {
    switch (arg) {

      case 'm':
        root_dir = new_argv[new_argc++] = optarg;
        break;

      case 'h':
        new_argv[new_argc++] = HELP_OPT;
        no_mount = true;
        break;

      case 'v':
        new_argv[new_argc++] = VERSION_OPT;
        no_mount = true;
        break;

      case 'd':
        new_argv[new_argc++] = DEBUG_OPT;
        break;

      case 'f':
        new_argv[new_argc++] = FOREGROUND_OPT;
        break;

      case 's':
        new_argv[new_argc++] = SINGLE_THREADED_OPT;

      case 'l':
        log_dir = optarg;
        break;

      case 'e':
        log_level = optarg;
        break;

      case 'o':
        len = strlen(optarg);
        for (int k = strlen(optarg) - 1; k >= 0; k--) {
          optarg[k + 2] = optarg[k];
        }
        optarg[0] = '-';
        optarg[1] = 'o';
        optarg[len + 2] = 0;
        new_argv[new_argc++] = optarg;
        break;

      case '?':
        break;

      case ':':
        break;

      default:
        fprintf(stderr, "Invalid command-line argument\n");
    }
  }

  try {

    if (no_mount == false) {
      // Checking whether mount point is supplied by user.
      if (root_dir == NULL) {
        throw "Missing mount point parameter. Please provide mount directory.";
      }

      // Check whether the conf directory is present.
      // Its created during the installation process.
      stat = mkdir(conf_path.c_str(), S_IRWXU | S_IRGRP | S_IROTH);
      if (!(stat == -1 && errno == EEXIST)) {
        remove(conf_path.c_str());
        throw "GDFS directory does not exist.";
      }

      // Start the logging engine.
      log_dir_path = conf_path;
      if (log_dir != NULL) {
        log_dir_path = log_dir;
      }
      if (log_dir_path.back() != '/') {
        log_dir_path += "/";
      }

      try {
        logging_::init_logging(log_dir_path + GDFS_LOG_FILE, log_level, false, false);
      } catch (GDFSException & err) {
        throw err.get().c_str();
      }

      // Set the mount options.
      mount_::set_mount_opt(new_argc, root_dir, conf_path, new_argv);
    }

    // Mount the file system.
    if (no_mount == true) {
      mount_::mount_fs_lite(new_argc, new_argv);
    } else {
      try {
        if (mount_::mount_fs() == false) {
          error = "Unable to mount GDFS file system";
        }
      } catch (GDFSException & err) {
        error = err.get();
      }

      mount_::unmount_fs();
      logging_::stop_logging();

      if (error.empty() == false) {
        throw error.c_str();
      }
    }

  } catch (const char * err) {
    fprintf(stderr, "\n[ERROR] %s\n\n", err);
    ret = 1;
  }

  return ret;
}
