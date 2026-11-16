/**
 * @file dbg.h
 * @brief Defensive programming macros adapted from "Learn C the Hard Way" (Zed
 * Shaw), designed for strict ISO C23 (-pedantic -Werror) and robust error
 * handling.
 */
#ifndef SYSMON_DBG_H
#define SYSMON_DBG_H

#include <errno.h>
#include <stdio.h>
#include <string.h>

/* Retrieve a human-readable representation of errno without returning null */
#define clean_errno() (errno == 0 ? "None" : strerror(errno))

/* Error logging macros emitting diagnostics to stderr */
#define log_err_msg(M)                                                         \
  fprintf(stderr, "[ERROR] (%s:%d: errno: %s) %s\n", __FILE__, __LINE__,       \
          clean_errno(), (M))

#define log_err_fmt(M, ...)                                                    \
  fprintf(stderr, "[ERROR] (%s:%d: errno: %s) " M "\n", __FILE__, __LINE__,    \
          clean_errno(), __VA_ARGS__)

/* Informational logging macro emitting formatted output to stdout */
#define log_info_fmt(M, ...) fprintf(stdout, "[INFO]  " M "\n", __VA_ARGS__)

/* Assertion checks wrapping control flow in do-while(0) blocks to ensure
 * syntactic safety */
#define check_msg(A, M)                                                        \
  do {                                                                         \
    if (!(A)) {                                                                \
      log_err_msg(M);                                                          \
      errno = 0;                                                               \
      goto error;                                                              \
    }                                                                          \
  } while (0)

#define check_fmt(A, M, ...)                                                   \
  do {                                                                         \
    if (!(A)) {                                                                \
      log_err_fmt(M, __VA_ARGS__);                                             \
      errno = 0;                                                               \
      goto error;                                                              \
    }                                                                          \
  } while (0)

/* Unconditional jump triggers for unreachable code paths or unhandled states */
#define sentinel_msg(M)                                                        \
  do {                                                                         \
    log_err_msg(M);                                                            \
    errno = 0;                                                                 \
    goto error;                                                                \
  } while (0)

#define sentinel_fmt(M, ...)                                                   \
  do {                                                                         \
    log_err_fmt(M, __VA_ARGS__);                                               \
    errno = 0;                                                                 \
    goto error;                                                                \
  } while (0)

#endif /* SYSMON_DBG_H */
