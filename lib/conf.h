
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



#ifndef CONF_H__
#define CONF_H__


#define GDFS_DEST_DIR "/opt/gdfs/"
#define GDFS_VERSION "1.0.0"

#define GDFS_ROOT_MODE (S_IRWXU | S_IRWXG | S_IRWXO)
#define GDFS_DEF_FILE_MODE (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)
#define GDFS_DEF_DIR_MODE (S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH)
#define GDFS_DEF_GDOC_MODE (S_IRUSR | S_IRGRP | S_IROTH)

#define GDFS_PATH_MAX_LEN 4096
#define GDFS_NAME_MAX_LEN 255
#define GDFS_BLOCK_SIZE 4096
#define GDFS_FRAGMENT_SIZE 4096

#define GDFS_MAX_WORKER_THREADS 10
#define GDFS_CACHE_MAX_SIZE 104857600
#define GDFS_CACHE_TIMEOUT 60
#define GDFS_UPLOAD_CHUNK_SIZE 10485760

#define GDFS_CLIENT_ID "1226761120-i6c1c1l3aafea2je44ubq3d9g19k48ob.apps.googleusercontent.com"
#define GDFS_CLIENT_SECRET "60wF05CehSS2RmSToMyAzA-N"
#define GDFS_REDIRECT_URI "urn:ietf:wg:oauth:2.0:oob"

#define GDFS_OAUTH_TOKEN_URL "https://www.googleapis.com/oauth2/v3/token"
#define GDFS_OAUTH_ACCESS_TOKEN_TIMEOUT 300

#define GDFS_OAUTH_URL__ "https://accounts.google.com/o/oauth2/auth"
#define GDFS_OAUTH_URL_ GDFS_OAUTH_URL__ "?response_type=code&scope=https://www.googleapis.com/auth/drive&access_type=offline"
#define GDFS_OAUTH_URL GDFS_OAUTH_URL_ "&client_id=" GDFS_CLIENT_ID "&redirect_uri=" GDFS_REDIRECT_URI

#define GDFS_CHANGE_URL "https://www.googleapis.com/drive/v3/changes/startPageToken?fields=startPageToken"
#define GDFS_FILE_URL "https://www.googleapis.com/drive/v3/files/"
#define GDFS_FILE_URL_ "https://www.googleapis.com/drive/v3/files"
#define GDFS_ABOUT_URL "https://www.googleapis.com/drive/v3/about"
#define GDFS_UPLOAD_URL "https://www.googleapis.com/upload/drive/v3/files/"

#define GDFS_AUTH_FILE "gdfs.auth"
#define GDFS_CONF_FILE "gdfs.conf"
#define GDFS_LOG_FILE  "gdfs.log"


#endif // CONF_H__
