
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



#ifndef MAIN_H__
#define MAIN_H__

#include <unistd.h>
#include <getopt.h>

const char * root_dir = NULL;
const char * log_dir = NULL;
char * VERSION_OPT = (char *) "--version";
char * HELP_OPT = (char *) "-h";
char * DEBUG_OPT = (char *) "-d";
char * FOREGROUND_OPT = (char *) "-f";
char * SINGLE_THREADED_OPT = (char *) "-s";

static struct option long_opt[] = {
 {"help", no_argument, NULL, 'h'},
 {"version", no_argument, NULL, 'v'},
 {"debug", no_argument, NULL, 'd'},
 {"single-threadded", no_argument, NULL, 's'},
 {"foreground", no_argument, NULL, 'f'},
 {"log_path", required_argument, NULL, 'l'},
 {"log_level", required_argument, NULL, 'e'},
 {"mount_point", required_argument, NULL, 'm'},
 {"option", required_argument, NULL, 'o'},
};

const char * optstr = ":m:ho:vl:dfe:s";

#endif // MAIN_H__
