
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

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "request.h"
#include "conf.h"
#include "auth.h"
#include "json.h"


// Given the file path and authorization code, generate the access_code, refresh_code, expires_in, and
// store in file as well as the request object
void
get_access_token (char * authCode)
{

  char * buf = nullptr;
  const char * pat = "gdfs.mount.user=";
  char buffer[100];
  size_t buf_len;
  size_t ele = 0;
  size_t pat_len = strlen(pat);
  uid_t uid = 0;
  gid_t gid = 0;
  AuthObj obj;
  Request reqObj;
  json::Value val;
  std::string query;
  std::string resp;
  std::string error;
  std::string fpath;
  std::string user_name = "root";
  FILE * fp = nullptr;
  struct passwd * pwd = nullptr;

  // Get the GDFS mount user, from /opt/gdfs/gdfs.conf
  // If not, default to root user.
  fpath = std::string(GDFS_DEST_DIR) + GDFS_CONF_FILE;
  fp = fopen(fpath.c_str(), "r");
  if (fp == NULL) {
    error = "Unable to open configuration file";
    goto out;
  }
  while (fgets(buffer, 100, fp)) {
    buffer[strlen(buffer) - 1] = '\0';
    if (strncmp(buffer, pat, pat_len) == 0) {
      user_name = buffer + pat_len;
      break;
    }
  }
  fclose(fp);

  // Get the UID and GID of the mount user.
  if (user_name != "root") {
    pwd = (struct passwd *) malloc(sizeof(struct passwd));
    if (pwd == nullptr) {
      error = "Unable to allocate memory for struct passwd";
      goto out;
    }

    buf_len = sysconf(_SC_GETPW_R_SIZE_MAX);
    buf = (char *) malloc(buf_len);
    if (buf == nullptr) {
      error = "Unable to allocate memory for buffer";
      goto out;
    }

    getpwnam_r(user_name.c_str(), pwd, buf, buf_len, &pwd);
    if (pwd == nullptr) {
      error = "Failed to retrieve user " + user_name;
      goto out;
    }

    uid = pwd->pw_uid;
    gid = pwd->pw_gid;
  }

  // Get the access_token, refresh_token, and expires_in
  query = "code=" + std::string(authCode) + "&grant_type=authorization_code";
  try {
    resp = reqObj.sendRequest(GDFS_OAUTH_TOKEN_URL, POST, query, /* clientSecret */ true);
    val.parse(resp);
  } catch (const char * err) {
    error = err;
    goto out;
  }

  memset(&obj, 0, sizeof obj);
  try {
    strcpy(obj.access_token, val["access_token"].get().c_str());
    obj.expires_in = time(NULL) + atoi(val["expires_in"].get().c_str());
    strcpy(obj.refresh_token, val["refresh_token"].get().c_str());
  } catch (const char* err) {
    error = val["error"].get() + ": " + val["error_description"].get();
    goto out;
  }

  // Write the parameters to file.
  fpath = std::string(GDFS_DEST_DIR) + GDFS_AUTH_FILE;
  fp = fopen(fpath.c_str(), "wb");
  if (fp == NULL) {
    error = "Unable to open authentication file";
    goto out;
  }

  ele = fwrite((void *) &obj, sizeof obj, 1, fp);
  fclose(fp);
  if (ele != 1) {
    error = "Writing to authentication file failed";
    goto out;
  }

  chown(fpath.c_str(), uid, gid);
  chmod(fpath.c_str(), S_IRUSR | S_IWUSR);

  fprintf(stdout, "\nAccess Token set correctly\n\n");

  // Create the log file.
  fpath = std::string(GDFS_DEST_DIR) + GDFS_LOG_FILE;
  fp = fopen(fpath.c_str(), "w");
  if (fp == NULL) {
    error = "Unable to open log file";
    goto out;
  }
  fclose(fp);

  // Change the owner to gdfs mount user.
  chown(fpath.c_str(), uid, gid);
  chmod(fpath.c_str(), S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

out:
  free(pwd);
  free(buf);
  if (error.empty() == false) {
    throw error.c_str();
  }
}


void
start_OAuth (void)
{

  std::string cmd;

  fprintf(stdout, "\n################################################################");
  fprintf(stdout, "\n#                                                              #");
  fprintf(stdout, "\n#                GOOGLE DRIVE FILE SYSTEM                      #");
  fprintf(stdout, "\n#                                                              #");
  fprintf(stdout, "\n# v%s                                                       #", GDFS_VERSION);
  fprintf(stdout, "\n# Robin Thomas                                                 #");
  fprintf(stdout, "\n# robinthomas2591@gmail.com                                    #");
  fprintf(stdout, "\n#                                                              #");
  fprintf(stdout, "\n################################################################");

  fprintf(stdout, "\n\nTo authorize GDFS to use your Google Drive account, visit the following URL to produce an auth code\n\n");
  fprintf(stdout, "%s\n", GDFS_OAUTH_URL);

  cmd = "xdg-open '" + std::string(GDFS_OAUTH_URL) + "' 1>/dev/null 2>&1";
  system(cmd.c_str());

}


int main (int argc,
          char ** argv)
{

  char auth_token[100];

  // Check that its the root user.
  if (geteuid() != 0) {
    fprintf(stderr, "Run gauth as root user\n");
    return 1;
  }

  umask(S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);

  start_OAuth();

  fprintf(stdout, "\nEnter the auth code: ");
  fgets(auth_token, 100, stdin);
  auth_token[strlen(auth_token) - 1] = '\0';

  try {
    get_access_token(auth_token);
  } catch (const char * err) {
    fprintf(stderr, "\n[ERROR] %s\n\n", err);
  }

  return 0;
}
