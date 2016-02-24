#include "logging.h"

const char *GLJ_LOG_CATEGORY_NAMES[GLJ_LOG_CATEGORY_MAX] = {
  "unknown",
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
# include <string.h>

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

static glj_log_category glj_find_category(const char *str) {
  glj_log_category cat;
  int i;
  cat = GLJ_LOG_UNKNOWN;
  for (i = 0; i < GLJ_LOG_CATEGORY_MAX; i++) {
    if (strcmp(str, GLJ_LOG_CATEGORY_NAMES[i]) == 0) {
      cat = (glj_log_category)i;
      break;
    }
  }
  return cat;
}

static glj_log_level glj_find_level(const char *str) {
  glj_log_level level = GLJ_LOG_INVALID;
  int i;
  for (i = 0; i < GLJ_LOG_LEVEL_MAX; i++) {
    if (strcmp(str, GLJ_LOG_LEVEL_NAMES[i]) == 0) {
      level = (glj_log_level)i;
      break;
    }
  }
  return level;
}

void glj_log_init(glj_logger_function logger) {
  char *env;
  if (logger != NULL) {
    glj_logger = logger;
  }
  env = getenv("GLJ_LOG");
  /* This code clobbers the environment variable and thus glj_log_init() should
      only be run once at the start of the program. */
  if (env) {
    do {
      char *next;
      char *split;
      next = strchr(env, ',');
      if (next) {
        *next = '\0';
        next += 1;
      }
      else {
        next = env + strlen(env);
      }
      split = strchr(env, ':');
      if (split) {
        glj_log_category cat;
        *split = '\0';
        split += 1;
        cat = glj_find_category(env);
        if (cat == GLJ_LOG_UNKNOWN) {
          fprintf(stderr, "Unknown category '%s'\n", env);
        }
        else {
          glj_log_level level;
          level = glj_find_level(split);
          if (level == GLJ_LOG_INVALID) {
            fprintf(stderr, "Invalid level '%s'\n", split);
          }
          else {
            glj_log_set_level(cat, level);
          }
        }
      }
      else {
        fprintf(stderr, "Bad clause '%s'\n", env);
      }
      env = next;
    }
    while (strlen(env) > 0);
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
