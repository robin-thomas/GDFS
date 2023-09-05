
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


#include <assert.h>

#include "dir_tree.h"


std::unordered_multimap <std::string, GDFSNode *> file_id_node;


GDFSNode *
GDFSNode::find (const std::string & file_name)
{
  auto child = this->children.find(file_name);
  if (child != this->children.end()) {
    return child->second;
  }
  return NULL;
}


struct GDFSNode *
GDFSNode::insert (struct GDFSNode * node)
{

  assert(node != NULL);

  this->children.emplace(node->file_name, node);
  return node;
}


std::unordered_map <std::string, struct GDFSNode *> &
GDFSNode::get_children (void)
{
  return this->children;
}


bool
GDFSNode::is_empty (void)
{
  return this->children.empty();
}


void
GDFSNode::remove_child (struct GDFSNode * child)
{

  assert (child != NULL);

  bool reset = false;

  if (this->children.size() == 1) {
    reset = true;
  }

  auto it = this->children.find(child->file_name);
  assert(it != this->children.end());

  this->children.erase(it);
  if (reset) {
    this->children.clear();
  }
}


void
GDFSNode::rename_child (const std::string & old_file_name,
                        const std::string & new_file_name)
{
  
  struct GDFSNode * tmp = NULL;
  auto it = this->children.find(old_file_name);

  assert (it != this->children.end());
  tmp = it->second;
  tmp->file_name = new_file_name;
  this->children.erase(it);
  this->children.emplace(new_file_name, tmp);
}

