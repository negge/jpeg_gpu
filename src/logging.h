#if !defined(_logging_H)
# define _logging_H (1)

#include <stdarg.h>

typedef enum {
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
void glj_log_set_level(glj_log_category cat, glj_log_level level);
int glj_logging_active(glj_log_category cat, glj_log_level level);
void glj_log(glj_log_category cat, glj_log_level level, const char *fmt, ...);

#  define GLJ_LOG(a) glj_log a

# else

#  define glj_log_init(logger)
#  define glj_log_set_level(cat, level)
#  define glj_logging_active(cat, level) (0)

#  define GLJ_LOG(a)

# endif

#endif
