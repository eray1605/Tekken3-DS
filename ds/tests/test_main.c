// tools/ds/run_tests.sh builds and runs this: t3ds_tests [--cases DIR] [--nitrofs DIR]
// [--traces DIR] [filter]. A filter runs the tests whose name contains it.
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test.h"

#define TESTS(X)                                  \
    X(test_fx_s16_wraps)                          \
    X(test_fx_sat16_saturates)                    \
    X(test_fx_w32_wraps_like_c_int)               \
    X(test_fx_division_and_shifts_round_like_c)   \
    X(test_fx_mul_matrix_identity)                \
    X(test_compose_offset_uses_parent_rotation)   \
    X(test_c_rand_sequence)                       \
    X(test_gte_divide_and_ratan2)                 \
    X(test_pose_tables_file_rejects_bad_input)    \
    X(test_pose_solver_matches_port_cases)        \
    X(test_pose_solver_wide_elbow_differs)        \
    X(test_pose_solver_matches_traces)            \
    X(test_anim_sampling_matches_port_cases)      \
    X(test_anim_sampling_clamps_frames)           \
    X(test_motion_set_resolves_slots)             \
    X(test_fighter_skeleton_matches_port_cases)   \
    X(test_fighter_rotation_xyz_identity_and_quarter_turn)

#define DECLARE(name) void name(void);
TESTS(DECLARE)

#define ENTRY(name) {#name, name},
static const struct {
    const char *name;
    void (*fn)(void);
} ALL[] = {TESTS(ENTRY)};

TestPaths test_paths = {"build/host", "nitrofs", "../work/traces"};

static int failures;
static bool skipped;

void test_fail(const char *file, int line, const char *fmt, ...)
{
    if (failures < 20) {
        va_list args;
        va_start(args, fmt);
        fprintf(stderr, "    %s:%d: ", file, line);
        vfprintf(stderr, fmt, args);
        fputc('\n', stderr);
        va_end(args);
    }
    failures++;
}

void test_skip(const char *reason)
{
    printf("    skipped: %s\n", reason);
    skipped = true;
}

uint8_t *test_read_file(const char *dir, const char *name, size_t *size)
{
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", dir, name);
    FILE *f = fopen(path, "rb");
    if (!f)
        return NULL;
    fseek(f, 0, SEEK_END);
    long length = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *data = malloc(length > 0 ? (size_t)length : 1);
    *size = fread(data, 1, (size_t)length, f);
    fclose(f);
    return data;
}

int main(int argc, char **argv)
{
    const char *filter = NULL;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--cases") == 0 && i + 1 < argc)
            test_paths.cases_dir = argv[++i];
        else if (strcmp(argv[i], "--nitrofs") == 0 && i + 1 < argc)
            test_paths.nitrofs = argv[++i];
        else if (strcmp(argv[i], "--traces") == 0 && i + 1 < argc)
            test_paths.traces = argv[++i];
        else
            filter = argv[i];
    }
    int run = 0;
    int failed = 0;
    int skips = 0;
    for (size_t i = 0; i < sizeof(ALL) / sizeof(ALL[0]); i++) {
        if (filter && !strstr(ALL[i].name, filter))
            continue;
        printf("%s\n", ALL[i].name);
        fflush(stdout);
        failures = 0;
        skipped = false;
        ALL[i].fn();
        run++;
        if (failures) {
            printf("    FAILED (%d)\n", failures);
            failed++;
        } else if (skipped) {
            skips++;
        }
    }
    printf("%d tests, %d failed, %d skipped\n", run, failed, skips);
    return failed ? 1 : 0;
}
