/**
 * Copyright (c) 2023, Virus.V <virusv@live.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * * Neither the name of SmartOCD nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * Copyright 2023 Virus.V <virusv@live.com>
*/

#ifndef LOG_H
#define LOG_H

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "smartocd.h"

/* 全局log等级 */
extern int g_log_level;
/* 默认日志输出 */
extern int g_log_fd;
void log_print_time(int fd);

enum {
  LOG_FATAL = 0,
  LOG_ERROR,
  LOG_WARN,
  LOG_INFO,
  LOG_DEBUG,
  LOG_TRACE,
};

#define LOG_COLOR_TRACE "\x1b[94m"
#define LOG_COLOR_DEBUG "\x1b[36m"
#define LOG_COLOR_INFO "\x1b[32m"
#define LOG_COLOR_WARN "\x1b[33m"
#define LOG_COLOR_ERROR "\x1b[31m"
#define LOG_COLOR_FATAL "\x1b[35m"

#ifdef LOG_USE_COLOR
#define log_trace(fmt, ...)                                                    \
  if (g_log_level >= LOG_TRACE) {                                              \
    log_print_time(g_log_fd);                                                  \
    dprintf(g_log_fd,                                                          \
            LOG_COLOR_TRACE "TRACE\x1b[0m \x1b[34m%s:%d|\x1b[0m " fmt "\r\n",  \
            __func__, __LINE__, ##__VA_ARGS__);                                \
  }

#define log_debug(fmt, ...)                                                    \
  if (g_log_level >= LOG_DEBUG) {                                              \
    log_print_time(g_log_fd);                                                  \
    dprintf(g_log_fd,                                                          \
            LOG_COLOR_DEBUG "DEBUG\x1b[0m \x1b[34m%s:%d|\x1b[0m " fmt "\r\n",  \
            __func__, __LINE__, ##__VA_ARGS__);                                \
  }

#define log_info(fmt, ...)                                                     \
  if (g_log_level >= LOG_INFO) {                                               \
    log_print_time(g_log_fd);                                                  \
    dprintf(g_log_fd,                                                          \
            LOG_COLOR_INFO "INFO\x1b[0m \x1b[34m%s:%d|\x1b[0m " fmt "\r\n",    \
            __func__, __LINE__, ##__VA_ARGS__);                                \
  }

#define log_warn(fmt, ...)                                                     \
  if (g_log_level >= LOG_WARN) {                                               \
    log_print_time(g_log_fd);                                                  \
    dprintf(g_log_fd,                                                          \
            LOG_COLOR_WARN "WARN\x1b[0m \x1b[34m%s:%d|\x1b[0m " fmt "\r\n",    \
            __func__, __LINE__, ##__VA_ARGS__);                                \
  }

#define log_error(fmt, ...)                                                    \
  if (g_log_level >= LOG_ERROR) {                                              \
    log_print_time(g_log_fd);                                                  \
    dprintf(g_log_fd,                                                          \
            LOG_COLOR_ERROR "ERROR\x1b[0m \x1b[34m%s:%d|\x1b[0m " fmt "\r\n",  \
            __func__, __LINE__, ##__VA_ARGS__);                                \
  }

#define log_fatal(fmt, ...)                                                    \
  do {                                                                         \
    log_print_time(g_log_fd);                                                  \
    dprintf(g_log_fd,                                                          \
            LOG_COLOR_FATAL "FATAL\x1b[0m \x1b[34m%s(%s):%d|\x1b[0m " fmt      \
                            "\r\n",                                            \
            __FILE__, __func__, __LINE__, ##__VA_ARGS__);                      \
    abort();                                                                   \
  } while (0);

#else /* LOG_USE_COLOR */
#define log_trace(fmt, ...)                                                    \
  if (g_log_level >= LOG_TRACE) {                                              \
    log_print_time(g_log_fd);                                                  \
    dprintf(g_log_fd, "TRACE %s:%d| " fmt "\r\n", __func__, __LINE__,          \
            ##__VA_ARGS__);                                                    \
  }

#define log_debug(fmt, ...)                                                    \
  if (g_log_level >= LOG_DEBUG) {                                              \
    log_print_time(g_log_fd);                                                  \
    dprintf(g_log_fd, "DEBUG %s:%d| " fmt "\r\n", __func__, __LINE__,          \
            ##__VA_ARGS__);                                                    \
  }

#define log_info(fmt, ...)                                                     \
  if (g_log_level >= LOG_INFO) {                                               \
    log_print_time(g_log_fd);                                                  \
    dprintf(g_log_fd, "INFO %s:%d| " fmt "\r\n", __func__, __LINE__,           \
            ##__VA_ARGS__);                                                    \
  }

#define log_warn(fmt, ...)                                                     \
  if (g_log_level >= LOG_WARN) {                                               \
    log_print_time(g_log_fd);                                                  \
    dprintf(g_log_fd, "WARN %s:%d| " fmt "\r\n", __func__, __LINE__,           \
            ##__VA_ARGS__);                                                    \
  }

#define log_error(fmt, ...)                                                    \
  if (g_log_level >= LOG_ERROR) {                                              \
    log_print_time(g_log_fd);                                                  \
    dprintf(g_log_fd, "ERROR %s:%d| " fmt "\r\n", __func__, __LINE__,          \
            ##__VA_ARGS__);                                                    \
  }

#define log_fatal(fmt, ...)                                                    \
  do {                                                                         \
    log_print_time(g_log_fd);                                                  \
    dprintf(g_log_fd, "FATAL %s(%s):%d| " fmt "\r\n", __FILE__, __func__,      \
            __LINE__, ##__VA_ARGS__);                                          \
    abort();                                                                   \
  } while (0);

#endif /* LOG_USE_COLOR */

#endif
