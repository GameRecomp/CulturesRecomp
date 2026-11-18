/* SPDX-License-Identifier: MIT */
/* Logging and fatal errors. The level comes from $RC_LOG (error, warn, info, debug, trace). */
#ifndef RC_LOG_H
#define RC_LOG_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum rc_log_level {
    RC_LOG_ERROR = 0,
    RC_LOG_WARN = 1,
    RC_LOG_INFO = 2,
    RC_LOG_DEBUG = 3,
    RC_LOG_TRACE = 4
} rc_log_level;

#if defined(__GNUC__) || defined(__clang__)
#define RC_PRINTF(f, a) __attribute__((format(printf, f, a)))
#define RC_NORETURN __attribute__((noreturn))
#elif defined(_MSC_VER)
#define RC_PRINTF(f, a)
#define RC_NORETURN __declspec(noreturn)
#else
#define RC_PRINTF(f, a)
#define RC_NORETURN
#endif

void rc_log_set_level(rc_log_level level);
rc_log_level rc_log_get_level(void);
void rc_log(rc_log_level level, const char *fmt, ...) RC_PRINTF(2, 3);

/* Called by rc_fatal() after printing the message. The default handler calls abort().
 * Tests install a handler that longjmps out. The handler must not return. */
typedef void (*rc_fatal_handler)(const char *message);
void rc_set_fatal_handler(rc_fatal_handler handler);
RC_NORETURN void rc_fatal(const char *fmt, ...) RC_PRINTF(1, 2);

#define RC_ERROR(...) rc_log(RC_LOG_ERROR, __VA_ARGS__)
#define RC_WARN(...) rc_log(RC_LOG_WARN, __VA_ARGS__)
#define RC_INFO(...) rc_log(RC_LOG_INFO, __VA_ARGS__)
#define RC_DEBUG(...) rc_log(RC_LOG_DEBUG, __VA_ARGS__)

#ifdef __cplusplus
}
#endif
#endif
