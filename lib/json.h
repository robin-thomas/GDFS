
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



#ifndef JSON_H__
#define	JSON_H__

#include <string>
#include <vector>
#include <stdlib.h>

namespace json {

  enum jsontok_t {
    TOKEN_OBJECT,
    TOKEN_ARRAY,
    TOKEN_STRING,
    TOKEN_NUMBER,
    TOKEN_TRUE,
    TOKEN_FALSE,
    TOKEN_NULL,
    TOKEN_BOOL
  };

  enum jsonstatus_t {
    STATUS_WAITING,
    STATUS_NUMBER,
    STATUS_STRING,
    STATUS_STRING_SLASH,
    STATUS_STRING_BEGIN,
    STATUS_TRUE,
    STATUS_FALSE,
    STATUS_NULL,
    STATUS_ARRAY_WAITING,
    STATUS_ARRAY_PARSED,
    STATUS_OBJECT_NAME,
    STATUS_OBJECT_WAITING_NAME,
    STATUS_OBJECT_WAITING_COLON,
    STATUS_OBJECT_PARSED
  };

  class Value {
    public:
      Value (void);

      Value (jsontok_t type);

      Value (const Value & obj);

      Value &
      operator= (const Value & obj);

      ~Value (void);

      void
      clear (void);

      Value &
      operator[] (const std::string & str);

      Value *
      find (const std::string & str);

      void
      setType (jsontok_t type);

      Value *
      addObj (const std::string & name);

      Value *
      addObj (void);

      void
      set (const std::string & val);

      std::string
      get (void) const;

      std::vector <Value *>
      getArray (void) const;

      void
      setName (const std::string & name);

      std::string
      getName (void) const;

      void
      parse (const std::string & str);

      bool
      is_json (const std::string & s);

    private:
      std::string name;
      jsontok_t type;
      unsigned long long int pos;
      std::string value;
      std::vector<Value*> children;

      bool
      whiteSpace(const char c);

      void
      parseObject (Value * obj,
                   const std::string & str);

      void
      parseValue (Value * obj,
                  const std::string & str);

      void
      parseArray (Value * obj,
                  const std::string & str);
  };
}


#endif // JSON_H__
