// A small test runner for the DS core on the host (tools/ds/run_tests.sh), in the spirit of the
// remake's TestSuite: expectations count failures, a test may skip when its inputs are missing.
#ifndef T3DS_TESTS_TEST_H
#define T3DS_TESTS_TEST_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    const char *cases_dir;  // synthetic cases (tools/ds/pose_cases.py)
    const char *nitrofs;    // converted DS data (tools/ds_import/convert.py)
    const char *traces;     // golden traces (work/traces)
} TestPaths;

extern TestPaths test_paths;

void test_fail(const char *file, int line, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
void test_skip(const char *reason);

// A whole file into a malloc'ed buffer (NULL when missing); `dir`/`name` joined with '/'.
uint8_t *test_read_file(const char *dir, const char *name, size_t *size);

#define EXPECT(cond, ...)                                     \
    do {                                                      \
        if (!(cond))                                          \
            test_fail(__FILE__, __LINE__, __VA_ARGS__);       \
    } while (0)

#define EXPECT_EQ(actual, expected)                                                         \
    do {                                                                                    \
        long long a_ = (long long)(actual);                                                 \
        long long e_ = (long long)(expected);                                               \
        if (a_ != e_)                                                                       \
            test_fail(__FILE__, __LINE__, "%s == %lld, expected %lld", #actual, a_, e_);    \
    } while (0)

#endif
