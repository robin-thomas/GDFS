
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



#ifndef LOG_H__
#define LOG_H__

#include <string>

#include <stdio.h>
#include <stdarg.h>
#include <pthread.h>
#include <stdint.h>
#include <string.h>


#define LOG_BUF_SIZE 8192
#define TIME_BUF_SIZE 20
#define STACK_TRACE_LIMIT 10

#define LOG_FUNC_SIGNATURE logging_::log_level_t level, \
                           const char * file_name, \
                           uint16_t line, \
                           uint16_t uid

#define Fatal(fmt, ...) \
  do { \
    if (logging_::is_logging_initialized) { \
      logging_::log->log_msg(logging_::FATAL, __FILE__, __LINE__, \
                             logging_::log->get_thread_id(), \
                             fmt, ## __VA_ARGS__); \
    } else { \
      fprintf(stderr, "Should call init_logging() first!\n"); \
    } \
  } while (0)

#define Error(fmt, ...) \
  do { \
    if (logging_::is_logging_initialized) { \
      logging_::log->log_msg(logging_::ERROR, __FILE__, __LINE__, \
                             logging_::log->get_thread_id(), \
                             fmt, ## __VA_ARGS__); \
    } else { \
      fprintf(stderr, "Should call init_logging() first!\n"); \
    } \
  } while (0)

#define Warning(fmt, ...) \
  do { \
    if (logging_::is_logging_initialized) { \
      logging_::log->log_msg(logging_::WARNING, __FILE__, __LINE__, \
                             logging_::log->get_thread_id(), \
                             fmt, ## __VA_ARGS__); \
    } else { \
      fprintf(stderr, "Should call init_logging() first!\n"); \
    } \
  } while (0)

#define Info(fmt, ...) \
  do { \
    if (logging_::is_logging_initialized) { \
      logging_::log->log_msg(logging_::INFO, __FILE__, __LINE__, \
                             logging_::log->get_thread_id(), \
                             fmt, ## __VA_ARGS__); \
    } else { \
      fprintf(stderr, "Should call init_logging() first!\n"); \
    } \
  } while (0)

#define Debug(fmt, ...) \
  do { \
    if (logging_::is_logging_initialized) { \
      logging_::log->log_msg(logging_::DEBUG, __FILE__, __LINE__, \
                             logging_::log->get_thread_id(), \
                             fmt, ## __VA_ARGS__); \
    } else { \
      fprintf(stderr, "Should call init_logging() first!\n"); \
    } \
  } while (0)

#define LOG_IF(type, cond, fmt, ...) \
  do { \
    if (logging_::is_logging_initialized) { \
      if (cond) { \
        logging_::log->log_msg(type, __FILE__, __LINE__, \
                               logging_::log->get_thread_id(), \
                               fmt, ## __VA_ARGS__); \
      } \
    } else { \
      fprintf(stderr, "Should call init_logging() first!\n"); \
    } \
  } while (0)


namespace logging_ {

  typedef enum {
    FATAL,
    ERROR,
    WARNING,
    INFO,
    DEBUG,
    TOTAL_LOG_LEVELS,
  } log_level_t;


  class Log {

    private:
      std::string path;
      FILE * outfp;
      log_level_t log_level;
      pthread_mutex_t mutex;
      char * log_buf;
      size_t log_buf_size;
      pthread_t runner_id;
      bool kill_runner;
      bool fatal_handling;
 
    public:
 
      Log (const std::string & path,
           log_level_t level,
           bool sigsegv_handling,
           bool fatal_handling);
 
      ~Log (void);

      void
      flush_buffer (void);

      void
      write_to_log (char * str,
                    size_t len);

      void
      destroy_runner (void);

      void
      do_cleanup (void);

      friend void
      detect_sigsegv (int sig_no);

      friend void *
      Runner (void * arg);

      friend bool
      is_log_buf_empty (void);

      friend bool
      str_in_log_buf (const char * str);

      void
      set_log_file (const std::string & path);

      void
      log_msg (LOG_FUNC_SIGNATURE,
               const char * fmt,
               ...);

      uint16_t
      get_thread_id (void);

  };

  extern bool is_logging_initialized;
  extern Log * log;
  extern const char * log_level_str[TOTAL_LOG_LEVELS];
  extern log_level_t log_level[TOTAL_LOG_LEVELS];

  void
  init_logging (const std::string & path,
                const std::string & log_level,
                bool sigsegv_handling = true,
                bool fatal_handling = true);

  void
  stop_logging (void);

  void
  detect_sigsegv (int sig_no);

  void *
  Runner (void * arg);

  const char *
  get_level_str (log_level_t level);

  log_level_t
  get_log_level (const std::string & log_level);

  void
  get_current_time (char * time_str);

  char **
  get_stack_trace (size_t * size);

  bool
  is_log_buf_empty (void);

  bool
  str_in_log_buf (const char * str);

  void
  sigusr1_handler (int sig_no);

}


#endif // LOG_H__
