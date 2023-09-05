
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



#include <execinfo.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/wait.h>

#include "log.h"
#include "conf.h"
#include "exception.h"


// Declarations
bool logging_::is_logging_initialized = false;
logging_::Log * logging_::log = NULL;

const char * logging_::log_level_str[logging_::TOTAL_LOG_LEVELS] = { 
  "FATAL",
  "ERROR",
  "WARNING",
  "INFO",
  "DEBUG",
};

logging_::log_level_t logging_::log_level[logging_::TOTAL_LOG_LEVELS] = {
  FATAL,
  ERROR,
  WARNING,
  INFO,
  DEBUG,
};


logging_::log_level_t
logging_::get_log_level (const std::string & log_level)
{

  logging_::log_level_t level;

  for (int i = 0; i < logging_::TOTAL_LOG_LEVELS; ++i) {
    if (log_level == logging_::log_level_str[i]) {
      return logging_::log_level[i];
    }
  }

  // Default to ERROR if invalid logging level.
  return logging_::log_level[1];
}


// Initialize the logging library.
void
logging_::init_logging (const std::string & path,
                        const std::string & log_level,
                        bool sigsegv_handling,
                        bool fatal_handling)
{
  if (logging_::is_logging_initialized) {
    fprintf(stderr, "You called init_logging() twice!\n");
    return;
  }

  logging_::log_level_t level = get_log_level(log_level);

  try {
    log = new logging_::Log (path, level, sigsegv_handling, fatal_handling);
  } catch (GDFSException & err) {
    throw err;
  }

  logging_::is_logging_initialized = true;
}


// Stop the logging.
void
logging_::stop_logging (void)
{
  if (!logging_::is_logging_initialized) {
    fprintf(stderr, "You should call init_logging() before stop_logging()!\n");
    return;
  }

  if (logging_::log) {
    delete logging_::log;
    logging_::log = NULL;
  }

  logging_::is_logging_initialized = false;
}


// Constructor
logging_::Log::Log (const std::string & path,
                    logging_::log_level_t level,
                    bool sigsegv_handling,
                    bool fatal_handling)
{
  // Default values.
  this->path = path;
  this->outfp = NULL;
  this->log_level = level;
  this->log_buf_size = 0;
  this->kill_runner = false;
  this->fatal_handling = fatal_handling;

  // Create the log buffer.
  this->log_buf = new char[LOG_BUF_SIZE];
  if (this->log_buf == NULL) {
    throw GDFSException("Unable to create log buffer");
  }

  pthread_mutex_init(&(this->mutex), NULL);

  // sanity check, and set the log output.
  if (path.empty() ||
      path.at(0) != '/') {
    fprintf(stderr, "No valid log path specified. Redirecting to stderr\n");
    outfp = stderr;
  } else {
    try {
      set_log_file(path);
    } catch (GDFSException & err) {
      throw err;
    }
  }

  // Handle SIGSEGV if required.
  if (sigsegv_handling) {
    signal(SIGSEGV, detect_sigsegv);
  }
  signal(SIGUSR1, sigusr1_handler);

  // Runnner thread to flush log buffer every 5 min.
  pthread_create(&(this->runner_id), NULL, Runner, NULL);
}


// Destructor
logging_::Log::~Log (void)
{
  this->destroy_runner();
  sleep(1);
  pthread_kill(this->runner_id, SIGUSR1);
  pthread_join(this->runner_id, NULL);
  this->flush_buffer();
  this->do_cleanup();
  signal(SIGSEGV, SIG_DFL);
  signal(SIGUSR1, SIG_DFL);
}


// Ask the runner thread to destroy.
void
logging_::Log::destroy_runner (void)
{
  pthread_mutex_lock(&(this->mutex));
  this->kill_runner = true;
  pthread_mutex_unlock(&(this->mutex));
}


// Function to flush the log buffer.
void
logging_::Log::flush_buffer (void)
{
  pthread_mutex_lock(&(this->mutex));

  if (this->log_buf_size > 0) {
    this->log_buf[this->log_buf_size] = '\0';
    fputs(this->log_buf, this->outfp);
    this->log_buf_size = 0;
  }

  pthread_mutex_unlock(&(this->mutex));
}


// Flush the log buffer, and write a string to the log file.
void
logging_::Log::write_to_log (char * str,
                             size_t len)
{
  this->flush_buffer();
  str[len] = '\0';
  pthread_mutex_lock(&(this->mutex));
  fputs(str, this->outfp);
  pthread_mutex_unlock(&(this->mutex));
}


// Free all memory and destroy mutex.
void
logging_::Log::do_cleanup (void)
{
  pthread_mutex_lock(&(this->mutex));
  if (this->kill_runner) {
    if (this->outfp != stderr) {
      fclose(this->outfp);
    }
    delete[] this->log_buf;
  }
  pthread_mutex_unlock(&(this->mutex));
  if (this->kill_runner) {
    pthread_mutex_destroy(&(this->mutex));
  }
}


// Set the path to log file.
void
logging_::Log::set_log_file (const std::string & path)
{

  FILE * fp = NULL;
  std::string error;

  fp = fopen(path.c_str(), "a");
  if (fp == NULL) {
    throw GDFSException("Unable to open log file " + path);
  }

  outfp = fp;
  setvbuf(outfp, NULL, _IOLBF, 0);

  fprintf(outfp, "\n################################################################");
  fprintf(outfp, "\n#                                                              #");
  fprintf(outfp, "\n#                GOOGLE DRIVE FILE SYSTEM                      #");
  fprintf(outfp, "\n#                                                              #");
  fprintf(outfp, "\n# v%s                                                       #", GDFS_VERSION);
  fprintf(outfp, "\n# Robin Thomas                                                 #");
  fprintf(outfp, "\n# robinthomas2591@gmail.com                                    #");
  fprintf(outfp, "\n#                                                              #");
  fprintf(outfp, "\n################################################################\n\n");
}


// Get the current time.
void
logging_::get_current_time (char * time_str)
{

  time_t t;
  struct tm * tm = NULL;

  time(&t);
  tm = localtime(&t);
  sprintf(time_str, "%02d-%02d-%04d %02d:%02d:%02d",
                     tm->tm_mday, tm->tm_mon + 1,
                     tm->tm_year + 1900, tm->tm_hour,
                     tm->tm_min, tm->tm_sec);
}


// Function to log a message depending on its severity.
void
logging_::Log::log_msg (LOG_FUNC_SIGNATURE,
                        const char * fmt,
                        ...)
{

  int str_size;
  size_t size = 0;
  char file[20];
  char tmp[LOG_BUF_SIZE];
  char log_str[LOG_BUF_SIZE];
  char time_str[TIME_BUF_SIZE];
  char ** str = NULL;

  // Check whether it should be logged.
  if (level <= this->log_level) {
    memset(tmp, 0, sizeof tmp);
    memset(file, 0, sizeof file);
    memset(log_str, 0, sizeof log_str);
    memset(time_str, 0, sizeof time_str);

    // Get the log message.
    va_list args;
    va_start(args, fmt);
    vsprintf(tmp, fmt, args);
    va_end(args);

    // Get the current time.
    get_current_time(time_str);

    // Generate the complete log message.
    sprintf(file, "%s:%d", file_name, line);
    str_size = sprintf(log_str, "%s, %7s Thread %5d, %20s => %s\n",
                       time_str, get_level_str(level), uid,
                       file, tmp);

    if (level == FATAL) {
      this->write_to_log(log_str, str_size);

      if (this->fatal_handling) {
        pthread_mutex_lock(&(this->mutex));
        fprintf(logging_::log->outfp, "\n*** FATAL Error detected; stack trace: ***\n");
        str = get_stack_trace(&size);
        for (size_t i = 0; i < size; ++i) {
          fprintf(this->outfp, "@\t%s\n", str[i]);
        }
        free(str);
        pthread_mutex_unlock(&(this->mutex));

        logging_::stop_logging();
        exit(0);
      }

    } else if (log_buf_size + str_size >= LOG_BUF_SIZE ||
               level == ERROR) {
      this->write_to_log(log_str, str_size);
    } else {
      // Buffer the log messages if not required instantly.
      pthread_mutex_lock(&(this->mutex));
      memcpy(this->log_buf + this->log_buf_size, log_str, str_size);
      this->log_buf_size += str_size;
      pthread_mutex_unlock(&(this->mutex));
    }
  }
}


/*
 * Runner thread that flushes the log buffer
 * every 5 minutes.
 */
void *
logging_::Runner (void * arg)
{

  while (1) {
    sleep(300);     // Sleep for 5 minutes.

    pthread_mutex_lock(&(logging_::log->mutex));
    if (logging_::log->kill_runner) {
      pthread_mutex_unlock(&(logging_::log->mutex));
      return NULL;
    }
    pthread_mutex_unlock(&(logging_::log->mutex));

    logging_::log->flush_buffer();
  }

  return NULL;
}


// Get the logging severity level in string form.
const char *
logging_::get_level_str (logging_::log_level_t level)
{
  if (level >= 0 && level < TOTAL_LOG_LEVELS) {
    return log_level_str[level];
  }
  return NULL;
}


// Get the current thread id.
uint16_t
logging_::Log::get_thread_id(void)
{
  return pthread_self();
}


// Get the current stack trace.
char **
logging_::get_stack_trace (size_t * size)
{

  char ** str = NULL;
  void *arr[STACK_TRACE_LIMIT];

  *size = backtrace(arr, STACK_TRACE_LIMIT);
  str  = backtrace_symbols(arr, *size);

  return str;
}


// Detect SIGSEGV and log stack trace.
void
logging_::detect_sigsegv (int sig_no)
{
  
  size_t size = 0;
  char ** str = NULL;
  char time_str[TIME_BUF_SIZE];

  memset(time_str, 0, sizeof time_str);

  // Get the current time.
  get_current_time(time_str);

  logging_::log->destroy_runner();
  logging_::log->flush_buffer();

  // Get the stack trace, and log it.
  str = logging_::get_stack_trace(&size);
  pthread_mutex_lock(&(logging_::log->mutex));
  fprintf(logging_::log->outfp, "\n*** Aborted at %s ***\n", time_str);
  fprintf(logging_::log->outfp, "*** SIGSEGV received by PID %d; stack trace: ***\n",
                                logging_::log->get_thread_id());
  for (size_t i = 0; i < size; ++i) {
    fprintf(logging_::log->outfp, "@\t%s\n", str[i]);
  }
  free(str);
  pthread_mutex_unlock(&(logging_::log->mutex));

  logging_::stop_logging();
  exit(1);
}


void
logging_::sigusr1_handler (int sig_no)
{

}


// Check to see whether the log buffer is empty or not.
bool
logging_::is_log_buf_empty (void)
{
  if (logging_::log->log_buf_size > 0) {
    return false;
  } else {
    return true;
  }
}


// Check to see whether a string is in log buffer.
bool
logging_::str_in_log_buf (const char * str)
{
  if (strstr(logging_::log->log_buf, str)) {
    return true;
  } else {
    return false;
  }
}
