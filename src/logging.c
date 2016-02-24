#include "logging.h"

const char *GLJ_LOG_CATEGORY_NAMES[GLJ_LOG_CATEGORY_MAX] = {
  "generic",
  "test"
};

const char *GLJ_LOG_LEVEL_NAMES[GLJ_LOG_LEVEL_MAX] = {
  "INVALID",
  "FATAL",
  "ERROR",
  "WARN",
  "INFO",
  "DEBUG",
};

#if defined(GLJ_ENABLE_LOGGING)
# include <stdlib.h>
# include <stdio.h>

static unsigned int glj_log_levels[GLJ_LOG_CATEGORY_MAX] = { 0 };

static int glj_log_fprintf_stderr(glj_log_category cat, glj_log_level level,
 const char *fmt, va_list ap) {
  fprintf(stderr, "[%s/%s] ", GLJ_LOG_CATEGORY_NAMES[cat],
   GLJ_LOG_LEVEL_NAMES[level]);
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  return EXIT_SUCCESS;
}

static glj_logger_function glj_logger = glj_log_fprintf_stderr;

void glj_log_init(glj_logger_function logger) {
  glj_log_category cat;
  if (logger != NULL) {
    glj_logger = logger;
  }
  for (cat = 0; cat < GLJ_LOG_CATEGORY_MAX; cat++) {
    glj_log_set_level(cat, GLJ_LOG_LEVEL_MAX - 1);
  }
}

glj_log_level glj_log_get_level(glj_log_category cat) {
  return glj_log_levels[cat];
}

void glj_log_set_level(glj_log_category cat, glj_log_level level) {
  glj_log_levels[cat] = level;
}

static void glj_log_impl(glj_log_category cat, glj_log_level level,
 const char *fmt, va_list ap) {
  if (glj_logging_active(cat, level)) {
    glj_logger(cat, level, fmt, ap);
  }
}

void glj_log(glj_log_category cat, glj_log_level level, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  glj_log_impl(cat, level, fmt, ap);
  va_end(ap);
}

int glj_logging_active(glj_log_category cat, glj_log_level level) {
  if (cat >= GLJ_LOG_CATEGORY_MAX) {
    return 0;
  }
  if (glj_log_levels[cat] < level) {
    return 0;
  }
  return 1;
}
#endif
