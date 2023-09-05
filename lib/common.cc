
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



#include <regex>

#include <string.h>

#include "dir_tree.h"
#include "common.h"
#include "log.h"


time_t
rfc3339_to_sec (const std::string & date)
{
  struct tm tm;
  memset(&tm, 0, sizeof tm);
  strptime(date.c_str(), "%Y-%m-%dT%H:%M:%S", &tm);
  return mktime(&tm);
}


std::string
to_rfc3339(time_t time)
{
  char date[26];
  memset(date, 0, sizeof date);
  strftime(date, sizeof date, "%Y-%m-%dT%H:%M:%S.000Z", localtime(&time));
  return date;
}


std::string
dir_name(const std::string & path)
{
  std::string tmp = path.substr(0, path.find_last_of("/"));
  if (tmp[0] == '/') {
    return tmp;
  } else {
    return '/' + tmp;
  }
}


std::string
base_name(const std::string & path)
{
  return path.substr(path.find_last_of("/") + 1, std::string::npos);
}


/*
 * Function to resolve name conflicts,
 * meaning two files/directories having the same name in parent directory.
 * It happens because Google Drive indexes files based on file id and not file name.
 * As a result, Google Drive allows multiple files with the same name.
 */
std::string
remove_name_conflict (std::string & file_name,
                      bool is_dir,
                      struct GDFSNode * parent)
{

  assert(parent != NULL);

  std::string tmp;
  std::string title = file_name;
  std::string ext = "";
  std::string::size_type idx;

  // Check for conflict.
  if (parent->find(file_name) == NULL) {
    goto out;
  }

  // Split the filename into name and extension, if its a file.
  // If its a directory, dont split.
  if (is_dir == false) {
    idx = title.find_last_of(".");
    if (idx != std::string::npos) {
      ext   = title.substr(idx);
      title = title.substr(0, idx);
    }
  }

  // Create a new file name that doesnt have name conflict.
  tmp = title;
  for (unsigned k = 1; ; k++) {
    tmp = title + "_" + std::to_string(k);
    if (parent->find(tmp + ext) == NULL) {
      break;
    }
  }
  title = tmp;

out:
  return (title + ext);
}


bool
is_old_name_conflict (const std::string & new_file_name,
                      const std::string & old_file_name)
{

  bool ret = false;
  std::string str;

  str = new_file_name + "_[[:digit:]]+";
  try {
    if (std::regex_match(old_file_name,
                         std::regex(str, std::regex_constants::extended))) {
      ret = true;
    }
  } catch (std::regex_error & err) {

  }

  return ret;
}


std::string
rand_str (void)
{

  auto alphanum = []() -> char
  {
    const char set[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    const size_t index = (sizeof set - 1);
    return set[rand() % index];
  };

  size_t len = rand() % 10 + 10;
  std::string s(len, 0);
  std::generate_n(s.begin(), len, alphanum);

  return s;
}
