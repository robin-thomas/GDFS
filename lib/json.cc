
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

#include "json.h"
#include "exception.h"


using namespace json;

Value::Value (void)
{
  this->type = TOKEN_OBJECT;
}


Value::Value (jsontok_t type)
{
  this->type = type;
}


Value::Value (const Value & obj)
{
  this->type   = obj.type;
  this->name   = obj.name;
  this->value  = obj.value;
  for (unsigned int i = 0; i < obj.children.size(); i++) {
    this->children.push_back(new Value(*obj.children[i]));
  }
}


Value::~Value()
{
  for (unsigned int i = 0; i < this->children.size(); i++) {
    delete this->children[i];
  }
  this->children.clear();
}


void
Value::clear (void)
{
  for (unsigned int i = 0; i < this->children.size(); i++) {
    delete this->children[i];
  }
  this->children.clear();
}


Value &
Value::operator= (const Value & obj)
{
  if ((unsigned int long)&obj != (unsigned int long)this) {
    this->type   = obj.type;
    this->name   = obj.name;
    this->value  = obj.value;
    for (unsigned int i = 0; i < obj.children.size(); i++) {
      this->children.push_back(new Value(*obj.children[i]));
    }
  }

  return *this;
}


std::string
Value::get (void) const
{
  return this->value;
}


std::vector <Value *>
Value::getArray (void) const
{
  return this->children;
}


Value &
Value::operator[] (const std::string & str)
{
  for (unsigned int i = 0; i < this->children.size(); i++) {
    if (this->children[i]->name == str) {
      return *this->children[i];
    }
  }

  // Value not found in json. Throw exception.
  throw GDFSException("[] => Value " + str + " doesnt exist in json output");
}


Value *
Value::find (const std::string & str)
{
  for (unsigned int i = 0; i < this->children.size(); i++) {
    if (this->children[i]->name == str) {
      return this->children[i];
    }
  }

  // Value not found in json. Throw exception.
  throw GDFSException("find => Value " + str + " doesnt exist in json output");
}


bool
Value::whiteSpace (const char c)
{
  if (c == '\n' ||
      c == '\r' ||
      c == '\t' ||
      c == ' ') {
    return true;
  } else {
    return false;
  }
}


bool
Value::is_json (const std::string & s)
{

  bool ret = false;

  if (s.empty()) {
    goto out;
  }

  this->pos = 0;
  while (this->pos < s.size()) {
    if (s[this->pos] == '{') {
      try {
        this->parseObject(this, s);
      } catch (GDFSException & err) {        
        goto out;
      }
      ret = true;
      goto out;
    } else if (!this->whiteSpace(s[this->pos])) {
      goto out;
    }
    this->pos++;
  }

out:
  return ret;
}


void
Value::parse (const std::string & str)
{

  if (str.empty()) {
    return;
  }

  if (this->children.size() > 0) {
    throw GDFSException("The Node already has children. Will not parse JSON string into it.");
  }

  this->pos = 0;
  while (this->pos < str.size()) {
    if (str[this->pos] == '{') {
      try {
        this->parseObject(this, str);
      } catch (GDFSException & err) {        
        throw err;
      }
      return;
    } else if (!this->whiteSpace(str[this->pos])) {
      throw GDFSException("Cannot parse JSON string");
    }
    this->pos++;
  }
  throw GDFSException("JSON string suddenly ended.");
}


void
Value::parseValue (Value * obj,
                   const std::string & str)
{
  jsonstatus_t status = STATUS_WAITING;
  std::string val;

  while (this->pos < str.size()) {
    char c = str[this->pos];
    switch (status) {
      case STATUS_WAITING: {
        // So we are after : or [ and waiting for a value. We don't know type yet.
        switch(c) {
          case '0':
          case '1':
          case '2':
          case '3':
          case '4':
          case '5':
          case '6':
          case '7':
          case '8':
          case '9':
          case '-':
          case '.': {
            // Value is a number. Accumulating it.
            status = STATUS_NUMBER;
            val += c;
            break;
          }

          case '"': {
            status = STATUS_STRING;
            break;
          }

          case 'T':
          case 't': {
            status = STATUS_TRUE;
            val += 't';
            break;
          }

          case 'F':
          case 'f': {
            status = STATUS_FALSE;
            val += 'f';
            break;
          }

          case 'N':
          case 'n': {
            status = STATUS_NULL;
            val += 'n';
            break;
          }

          case '{': {
            this->parseObject(obj, str);
            // Before parseObject exits it increments "i" value.
            // But after ParseValue exited "i" will be incremented again. Solving it by i--;
            this->pos--;
            return;
          }

          case '[': {
            obj->setType(TOKEN_ARRAY);
            try {
              this->parseArray(obj, str);
            } catch(GDFSException & err) {              
              throw str;
            }
            return;
          }

          default: {
            if (!this->whiteSpace(c)) {
              throw GDFSException("Number, object, string or array expected here");
            }
          }
        }
        break;
      }

      case STATUS_FALSE: {
        // Bool false value detected. Accumulate and confirm it.
        if (c == ',' || c == '}' || c == ']' || this->whiteSpace(c)) {
          // Finished boolean "false" value collecting here.
          if (val == "false") {
            obj->setType(TOKEN_FALSE);
            obj->set("false");
          } else {
            throw GDFSException("Error parsing «false» value");
          }
          // We can be on "}" or "," position. Caller must stay on it.
          // Need to decrement to stay on potentially "}" or "," character.
          this->pos--;
          return;
        } else if (val == "f" && str.substr(this->pos, 4) == "alse") {
          val = "false";
          this->pos += 3;
        } else {
          throw GDFSException("Symbol cannot be in this position in «false» value");
        }
        break;
      }

      case STATUS_TRUE: {
        // Bool true value detected. Accumulate and confirm it.
        if (c == ',' || c == '}' || c == ']' || this->whiteSpace(c)) {
          // Finished boolean "true" value collecting here.
          if (val == "true") {
            obj->setType(TOKEN_TRUE);
            obj->set("true");
          } else {
            throw GDFSException("Error parsing «true» value");
          }
          // We can be on "}" or "," position. Caller must stay on it.
          // Need to decrement to stay on potentially "}" or "," character.
          this->pos--;
          return;
        } else if (val == "t" && str.substr(this->pos, 3) == "rue") {
          val = "true";
          this->pos += 2;
        } else {         
          throw GDFSException("Symbol cannot be in this position in «true» value");
        }
        break;
      }

      case STATUS_NULL: {
        // Null value detected. Accumulate and confirm it.
        if (c == ',' || c == '}' || c == ']' || this->whiteSpace(c)) {
          if (val == "null") {
            obj->setType(TOKEN_NULL);
            obj->set("null");
          } else {
            throw GDFSException("Error parsing «null» value");
          }
          // We can be on "}" or "," position. Caller must stay on it.
          // Need to decrement to stay on potentially "}" or "," character.
          this->pos--;
          return;
        } else if (val == "n" && (str.substr(this->pos, 3) == "ull")) {          
          val = "null";
          this->pos += 2;
        } else {
          throw GDFSException("Symbol cannot be in this position in «null» value");
        }
        break;
      }

      case STATUS_STRING_SLASH: {
        // Collecting string we have found slash. That means second slash, "n", "r" or
        // quotes must be next. If not it is error.
        switch(c) {
          case 'N':
          case 'n': {
            val += '\n';
            status = STATUS_STRING;
            break;
          }

          case 'R':
          case 'r': {
            val += '\r';
            status = STATUS_STRING;
            break;
          }

          case '"': {
            val += '"';
            status = STATUS_STRING;
            break;
          }

          case '\\': {
            val += '\\';
            status = STATUS_STRING;
            break;
          }

          default: {
            throw GDFSException("\\ must be followed by \\, «n», «r» or quotes in string value");
          }
        }
        break;
      }

      case STATUS_STRING: {
        // It was detected that value is string. Collecting it. 
        // Don't foget about slash.
        if (c == '"') {
          // Finished string collecting here.
          obj->setType(TOKEN_STRING);
          obj->set(val);
          return;
        } else if (c == '\\') {
          status = STATUS_STRING_SLASH;
        } else {
          val += c;
        }
        break;
      }

      case STATUS_NUMBER: {
        // We have number as value. Accumulating it.
        switch(c) {
          case '0':
          case '1':
          case '2':
          case '3':
          case '4':
          case '5':
          case '6':
          case '7':
          case '8':
          case '9':
          case '.': {
            // Value is a number and we are collecting it. Note that it cannot contain "-" now.
            if (c == '.' && val.find('.') != std::string::npos) {
              throw GDFSException("Numeric value cannot contain two dots");
            }
            val += c;
            break;
          }

          default: {
            if (c == ',' || c == '}' || c == ']' || this->whiteSpace(c)) {
              // Finished number collecting here.
              obj->setType(TOKEN_NUMBER);
              obj->set(val);
              //obj->setNum(atof(val.c_str()));
              // We can be on "}", "]" or "," position. Caller must stay on it.
              // Need to decrement to stay on potentially "}" or "," character.
              this->pos--;
              return;
            } else {
              throw GDFSException("Numeric value cannot contain this symbol");
            }
          }
        }
        break;
      }
      default:;
    }
    this->pos++;
  }
  // If we are here it means JSON string ended unexpectedly.
  throw GDFSException("JSON string suddenly ended (while parsing value)");
}


void
Value::parseObject (Value * obj,
                    const std::string & str)
{

  char c;
  std::string name;
  jsonstatus_t status = STATUS_OBJECT_WAITING_NAME;

  // Now where are on "{" character so we need to increment i.
  while (++(this->pos) < str.size()) {
    c = str[this->pos];

    switch (status) {
      case STATUS_OBJECT_WAITING_NAME: {
        // We are in object, so expecting name -------------------------
        if (c == '"') { // Quotes is start of the name
          status = STATUS_OBJECT_NAME;
        } else if (c == '}') { // Object is finished
          return;
        } else if (!this->whiteSpace(c)) {
          throw GDFSException("Cannot parse JSON string");
        }
        break;
      }

      case STATUS_OBJECT_NAME: {
        // Quotes found and now we are accumulating name characters ----
        if (c == '"') {
          status = STATUS_OBJECT_WAITING_COLON;
        } else if (isalnum(c) || c == '_' || c == '/' || c == '.' || c == '-') {
          if (name.size() == 0 && isdigit(c)) {
            throw GDFSException("Field name cannot be started with a number");
          } else {
            name += c;
          }
        } else {
          throw GDFSException("Field name cannot contain this character");
        }
        break;
      }

      case STATUS_OBJECT_WAITING_COLON: {        
        // We have just got name. Now colon is needed.
        if (c == ':') { // Color divides name from value
          // Now we are on ":" symbol. Start value parsing after it.
          this->pos++;
          try {
            this->parseValue(obj->addObj(name), str);
          } catch(GDFSException & str) {            
            throw str;
          }
          name.clear();
          status = STATUS_OBJECT_PARSED;
        } else if (!this->whiteSpace(c)) {
          throw GDFSException("Colon expected here");
        }
        break;
      }

      case STATUS_OBJECT_PARSED: {
        // We have just finished value parsing and now are waiting from "," or "}"
        if (c == ',') {
          // After comma must be name in object
          status = STATUS_OBJECT_WAITING_NAME;
        } else if (c == '}') {
          // Increment before return.
          this->pos++;
          return;
        } else if (!this->whiteSpace(c)) {
          throw GDFSException("Colon expected here");
        }
        break;
      }
      default:;
    }
  }
  // If we are here it means JSON string ended unexpectedly.
  throw GDFSException("JSON string suddenly ended (while parsing object)");
}


void
Value::parseArray (Value * obj,
                   const std::string & str)
{

  char c;
  std::string name;
  jsonstatus_t status = STATUS_ARRAY_WAITING;

  // Now we are on "[" character so we need to increment.
  while (++(this->pos) < str.size()) {
    c = str[this->pos];

    switch (status) {
      case STATUS_ARRAY_WAITING: {
        if (c == ']') {
          return;
        } else if (!this->whiteSpace(c)) {
          this->parseValue(obj->addObj(), str);
          status = STATUS_OBJECT_PARSED;
        }
        break;
      }

      case STATUS_OBJECT_PARSED: {
        // We have just finished value parsing and now are waiting for "," or "]"
        if (c == ',') {
          // After comma must be name in object
          status = STATUS_ARRAY_WAITING;
        } else if (c == ']') {
          // We must not increment "i" here. It will be incremeneted in
          // ParseObject function after "while" cycle.
          return;
        } else if (!this->whiteSpace(c)) {
          throw GDFSException("Squared braces are expected here.");
        }
        break;
      }
      default:;
    }
  }
  throw GDFSException("JSON string suddenly ended (while parsing array)");
}


Value *
Value::addObj (const std::string & name)
{
  if (this->type != TOKEN_OBJECT) {
    throw GDFSException("Cannot add object as type is not parent.");
  } else {
    Value * child = new Value(TOKEN_OBJECT);
    child->name = name;
    this->children.push_back(child);
    return child;
  }
}


Value *
Value::addObj (void)
{
  if (this->type != TOKEN_ARRAY) {    
    throw GDFSException("Cannot add object to array as type is not array.");
  } else {
    this->children.push_back(new Value(TOKEN_OBJECT));
    return this->children.back();
  }
}


std::string
Value::getName (void) const
{
  return this->name;
}


void
Value::setType (jsontok_t type)
{
  for (unsigned int i = 0; i < this->children.size(); i++) {
    delete this->children[i];
  }
  this->children.empty();
  this->type = type;
}


void
Value::set (const std::string& val)
{
  this->value = val;
}
