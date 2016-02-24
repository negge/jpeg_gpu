#include <stdlib.h>
#include "logging.h"
#include "test.h"

void glj_test_reset(glj_test *test) {
  test->checks = 0;
  test->errors = 0;
}

glj_test *glj_active_test;

int glj_test_suite_run(glj_test_suite *test_suite, void *ctx) {
  int ret;
  int i;
  if (glj_log_get_level(GLJ_LOG_TEST) < GLJ_LOG_INFO) {
    glj_log_set_level(GLJ_LOG_TEST, GLJ_LOG_INFO);
  }
  ret = EXIT_SUCCESS;
  for (i = 0; i < test_suite->ntests; i++) {
    glj_active_test = &test_suite->tests[i];
    glj_test_reset(glj_active_test);
    if (test_suite->before != NULL) {
      test_suite->before(ctx);
    }
    glj_active_test->func(ctx);
    GLJ_LOG((GLJ_LOG_TEST, GLJ_LOG_INFO, "%-32s %s!  Checks: %2i, Errors: %2i",
     glj_active_test->name, glj_active_test->errors == 0 ? "Passed" : "Failed",
     glj_active_test->checks, glj_active_test->errors));
    if (glj_active_test->errors != 0) {
      ret = EXIT_FAILURE;
    }
    if (test_suite->after != NULL) {
      test_suite->after(ctx);
    }
  }
  return ret;
}

