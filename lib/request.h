
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



#ifndef REQUEST_H__
#define REQUEST_H__

#include <string>


struct AuthObj {
  char access_token[100];
  char refresh_token[100];
  time_t expires_in;
};


enum requestType {
  GET,
  POST,
  DELETE,
  UPDATE,
  INSERT,
  DOWNLOAD,
  UPLOAD_SESSION,
  UPLOAD,
  GENERATE_ID,
};


class Request {

  private:

    std::string confFile;

    std::string redirectUri;

    std::string clientId;

    std::string clientSecret;

    std::string accessToken;

    std::string refreshToken;

    time_t expiresIn;

  public:

    Request (void);

    Request (const std::string & confFile);

    ~Request (void);

    void
    clear (void);

    std::string
    getAccessToken () const;

    time_t
    getExpiresIn () const;

    std::string
    getRefreshToken () const;

    std::string
    getConfFile () const;

    void
    setTokens (AuthObj & obj);

    void
    getTokens (AuthObj & obj);

    static size_t
    writeCallback (void * contents,
                   size_t size,
                   size_t nmemb,
                   void * userp);

    std::string
    sendRequest (const std::string & url,
                 requestType type,
                 std::string query = "",
                 bool secret = false,
                 std::string headers_ = "");
};

#endif // REQUEST_H__
