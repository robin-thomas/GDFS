
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
#include <ctime>

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "auth.h"
#include "json.h"
#include "conf.h"
#include "common.h"
#include "log.h"
#include "exception.h"



void
Auth::load_auth_file (void)
{

  Debug("<-- Entering load_auth_file() -->");

  AuthObj obj;
  char auth_token[100];
  std::string url;
  std::string auth_file = this->reqObj.getConfFile();
  std::string fpath;
  FILE * fp = NULL;
  size_t ele = 0;

  // Check that the authentication file exists.
  if (access(auth_file.c_str(), F_OK) == -1) {
    throw GDFSException("Authentication files missing. Run gauth");
  } else {
    fpath = std::string(GDFS_DEST_DIR) + GDFS_AUTH_FILE;
    fp = fopen(fpath.c_str(), "rb");
    if (fp == NULL) {
      throw GDFSException("Unable to open authentication file");
    }

    ele = fread((void *) &obj, sizeof obj, 1, fp);
    fclose(fp);
    if (ele != 1) {
      throw GDFSException("Reading from authentication file failed");
    }

    this->reqObj.setTokens(obj);
  }

  Debug("<-- Exiting load_auth_file() -->");
}


void
Auth::check_access_token (void)
{

  AuthObj obj;
  time_t expires_in;
  time_t cur_time;

  memset(&obj, 0, sizeof obj);
  expires_in = this->reqObj.getExpiresIn();
  cur_time = std::time(0);
  if (expires_in - cur_time <= GDFS_OAUTH_ACCESS_TOKEN_TIMEOUT ||
      cur_time >= expires_in) {
    this->reqObj.getTokens(obj);
    this->renew_access_token(obj);
  }
}


void
Auth::renew_access_token (AuthObj & obj)
{

  size_t ele = 0;
  json::Value val;
  std::string query;
  std::string resp;
  std::string fpath;
  std::string error;
  FILE * fp = NULL;

  query = "refresh_token=" + std::string(obj.refresh_token) + "&grant_type=refresh_token";
  try {  
    resp = this->reqObj.sendRequest(GDFS_OAUTH_TOKEN_URL, POST, query, /* clientSecret */ true);
    val.parse(resp);
  } catch (GDFSException & err) {
    error = err.get();
    goto out;
  }

  // Get the new access_token, refresh_token, expires_in.
  try {
    memset(&obj, 0, sizeof obj);
    strcpy(obj.access_token, val["access_token"].get().c_str());
    obj.expires_in = time(NULL) + atoi(val["expires_in"].get().c_str());
    strcpy(obj.refresh_token, this->reqObj.getRefreshToken().c_str());
  } catch (GDFSException & err) {
    error = val["error"].get() + ": " + val["error_description"].get();
    goto out;
  }
  this->reqObj.setTokens(obj);

  // Write the new parametsrs to auth file.
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

out:
  if (error.empty() == false) {
    throw GDFSException(error);
  }
}


std::string
Auth::sendRequest (const std::string & url,
                   requestType type,
                   std::string query,
                   bool secret,
                   std::string headers)
{

  std::string ret;

  // Update the access token if necessary.
  this->check_access_token();

  // Send the request.
  ret = this->reqObj.sendRequest(url, type, query, secret, headers);

  return ret;
}

