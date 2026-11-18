/* SPDX-License-Identifier: MIT */
/* Minimal test harness: CHECK() records failures, RUN() runs a test function. */
#ifndef RC_TEST_H
#define RC_TEST_H

#include <stdio.h>

static int rc_test_failures;
static int rc_test_checks;

#define CHECK(cond)                                                                            \
    do {                                                                                       \
        rc_test_checks++;                                                                      \
        if (!(cond)) {                                                                         \
            rc_test_failures++;                                                                \
            fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond);           \
        }                                                                                      \
    } while (0)

#define CHECK_EQ_U32(a, b)                                                                     \
    do {                                                                                       \
        unsigned long rc_a_ = (unsigned long)(a), rc_b_ = (unsigned long)(b);                  \
        rc_test_checks++;                                                                      \
        if (rc_a_ != rc_b_) {                                                                  \
            rc_test_failures++;                                                                \
            fprintf(stderr, "%s:%d: %s == %s failed: 0x%lx != 0x%lx\n", __FILE__, __LINE__,    \
                    #a, #b, rc_a_, rc_b_);                                                     \
        }                                                                                      \
    } while (0)

#define RUN(fn)                                                                                \
    do {                                                                                       \
        int rc_before_ = rc_test_failures;                                                     \
        fn();                                                                                  \
        printf("%-40s %s\n", #fn, rc_test_failures == rc_before_ ? "ok" : "FAILED");           \
    } while (0)

#define TEST_EXIT()                                                                            \
    (printf("%d checks, %d failures\n", rc_test_checks, rc_test_failures),                     \
     rc_test_failures ? 1 : 0)

#endif
