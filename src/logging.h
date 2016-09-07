/* JPEG GPU project
Copyright (c) 2014-2016 JPEG GPU project contributors.  All rights reserved.

Licensed under the Apache License, Version 2.0 (the "License"); you may not
 use this file except in compliance with the License.
You may obtain a copy of the License at:

  http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software distributed
 under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and limitations
 under the License. */

#if !defined(_logging_H)
# define _logging_H (1)

#include <stdarg.h>

typedef enum {
  GLJ_LOG_UNKNOWN,
  GLJ_LOG_GENERIC,
  GLJ_LOG_TEST,
  GLJ_LOG_CATEGORY_MAX
} glj_log_category;

typedef enum {
  GLJ_LOG_INVALID,
  GLJ_LOG_FATAL,
  GLJ_LOG_ERROR,
  GLJ_LOG_WARN,
  GLJ_LOG_INFO,
  GLJ_LOG_DEBUG,
  GLJ_LOG_LEVEL_MAX
} glj_log_level;

extern const char *GLJ_LOG_CATEGORY_NAMES[GLJ_LOG_CATEGORY_MAX];
extern const char *GLJ_LOG_LEVEL_NAMES[GLJ_LOG_LEVEL_MAX];

typedef int (*glj_logger_function)(glj_log_category cat, glj_log_level level,
 const char *fmt, va_list ap);

# if defined(GLJ_ENABLE_LOGGING)

void glj_log_init(glj_logger_function logger);
glj_log_level glj_log_get_level(glj_log_category cat);
void glj_log_set_level(glj_log_category cat, glj_log_level level);
int glj_logging_active(glj_log_category cat, glj_log_level level);
void glj_log(glj_log_category cat, glj_log_level level, const char *fmt, ...);

#  define GLJ_LOG(a) glj_log a

# else

#  define glj_log_init(logger)
#  define glj_log_get_level(cat) (0)
#  define glj_log_set_level(cat, level)
#  define glj_logging_active(cat, level) (0)

#  define GLJ_LOG(a)

# endif

#endif
