
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



#include <fstream>

#include <stdlib.h>
#include <curl/curl.h>
#include <string.h>

#include "request.h"
#include "conf.h"
#include "common.h"
#include "exception.h"


// Set Client ID, Client Secret, Redirect URI
Request::Request (void)
{
  this->clientId     = GDFS_CLIENT_ID;
  this->clientSecret = GDFS_CLIENT_SECRET;
  this->redirectUri  = GDFS_REDIRECT_URI;
}


Request::Request (const std::string & confFile)
{

  this->clientId     = GDFS_CLIENT_ID;
  this->clientSecret = GDFS_CLIENT_SECRET;
  this->redirectUri  = GDFS_REDIRECT_URI;
  this->confFile     = confFile;

  //curl_global_init(CURL_GLOBAL_ALL);
}


Request::~Request (void)
{
  //curl_global_cleanup();
}


void
Request::clear (void)
{
  //curl_global_cleanup();
}


std::string
Request::getAccessToken () const
{
  return this->accessToken;
}


time_t
Request::getExpiresIn () const
{
  return this->expiresIn;
}


std::string
Request::getRefreshToken () const
{
  return this->refreshToken;
}


std::string
Request::getConfFile () const
{
  return this->confFile;
}


void
Request::setTokens (AuthObj & obj)
{
  this->accessToken  = obj.access_token;
  this->expiresIn    = obj.expires_in;
  this->refreshToken = obj.refresh_token;
}


void
Request::getTokens (AuthObj & obj)
{
  strcpy(obj.access_token, this->accessToken.c_str());
  obj.expires_in = this->expiresIn;
  strcpy(obj.refresh_token, this->refreshToken.c_str());
}


size_t
Request::writeCallback (void * contents,
                        size_t size,
                        size_t nmemb,
                        void * userp)
{
  ((std::string *)userp)->append((char *)contents, size * nmemb);
  return size * nmemb;
}


std::string
Request::sendRequest (const std::string & url,
                      requestType reqType,
                      std::string query,
                      bool secret,
                      std::string headers_)
{

  std::string buf;
  std::string new_url;
  std::string header = std::string();
  CURL * curl = NULL;
  CURLcode resp;
  struct curl_slist * headers = NULL;

  // Check to see whether curl is installed,
  // and working fine.
  try {
    if ((curl = curl_easy_init()) == NULL) {
      throw GDFSException("Unable to use curl library");
    }
  } catch (...) {
    throw GDFSException("Install libcurl-devel package");
  }

  if (url.find("?") != std::string::npos) {
    new_url = url + "&quotaUser=" + rand_str();
  } else {
    new_url = url + "?quotaUser=" + rand_str();
  }

  curl_easy_setopt(curl, CURLOPT_URL, new_url.c_str());
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 60L);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);

  // Construct the headers and query, based on request type.
  switch (reqType) {
    case GET:
    case GENERATE_ID:
      header = "Authorization: Bearer " + this->accessToken;
      headers = curl_slist_append(headers, header.c_str());
      curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
      break;

    case POST:
      query += "&client_id=" + this->clientId + "&redirect_uri=" + this->redirectUri;
      if (secret) {
        query += "&client_secret=" + this->clientSecret;
      }
      curl_easy_setopt(curl, CURLOPT_POST, 1L);
      curl_easy_setopt(curl, CURLOPT_POSTFIELDS, query.c_str());
      curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, query.size());
      break;

    case DELETE:
      curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
      header = "Authorization: Bearer " + this->accessToken;
      headers = curl_slist_append(headers, header.c_str());
      curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
      break;

    case DOWNLOAD:
      header = "Authorization: Bearer " + this->accessToken;
      headers = curl_slist_append(headers, header.c_str());
      headers = curl_slist_append(headers, query.c_str());
      curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
      break;

    case UPLOAD_SESSION:
      header = "Authorization: Bearer " + this->accessToken;
      headers = curl_slist_append(headers, header.c_str());
      headers = curl_slist_append(headers, headers_.c_str());
      headers = curl_slist_append(headers, "Content-Type: application/json; charset=UTF-8");
      header = "Content-Length: " + std::to_string(query.size());
      headers = curl_slist_append(headers, header.c_str());
      curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
      curl_easy_setopt(curl, CURLOPT_POSTFIELDS, query.c_str());
      curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, query.size());      
      curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PATCH");
      curl_easy_setopt(curl, CURLOPT_HEADER, 1);
      break;

    case UPLOAD:
      header = "Authorization: Bearer " + this->accessToken;
      headers = curl_slist_append(headers, header.c_str());
      header = "Content-Length: " + std::to_string(query.size());
      headers = curl_slist_append(headers, header.c_str());
      if (headers_.empty() == false) {
        headers = curl_slist_append(headers, headers_.c_str());
      }
      curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
      curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
      if (query.empty() == false) {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, query.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, query.size());
      }
      curl_easy_setopt(curl, CURLOPT_HEADER, 1);
      break;

    case UPDATE:
      header = "Authorization: Bearer " + this->accessToken;
      headers = curl_slist_append(headers, header.c_str());
      headers = curl_slist_append(headers, "Content-Type: application/json; charset=UTF-8");
      header = "Content-Length: " + std::to_string(query.size());
      headers = curl_slist_append(headers, header.c_str());
      curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
      curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PATCH");
      curl_easy_setopt(curl, CURLOPT_POSTFIELDS, query.c_str());
      curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, query.size());
      break;

    default:
      header = "Authorization: Bearer " + this->accessToken;
      headers = curl_slist_append(headers, header.c_str());
      headers = curl_slist_append(headers, "Content-Type: application/json");
      header = "Content-Length: " + std::to_string(query.size());
      headers = curl_slist_append(headers, header.c_str());
      curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
      curl_easy_setopt(curl, CURLOPT_POST, 1L);
      curl_easy_setopt(curl, CURLOPT_POSTFIELDS, query.c_str());
      curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, query.size());
  }

  // Send the CURL request.
  resp = curl_easy_perform(curl);
  if (resp != CURLE_OK) {
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    throw GDFSException("Unable to make a request to " + new_url);
  }
  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);

  return buf;
}
