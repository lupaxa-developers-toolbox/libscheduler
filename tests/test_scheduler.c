/* Quoted path so a syntax check without -I still finds the public header. */
#include "../include/scheduler.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

static int failures = 0;
static int tests_run = 0;
static int tests_failed = 0;
static const char *current_test = NULL;

static void expect(int condition, const char *file, int line, const char *expr)
{
    if (!condition) {
        printf("FAIL %s %s:%d: %s\n",
               current_test != NULL ? current_test : "?",
               file, line, expr);
        fflush(stdout);
        failures++;
    }
}

#define EXPECT(cond) expect((cond), __FILE__, __LINE__, #cond)

static int semver_major_minor(const char *version)
{
    int parts = 0;

    if (version == NULL) {
        return 0;
    }

    while (parts < 2) {
        if (*version < '0' || *version > '9') {
            return 0;
        }
        while (*version >= '0' && *version <= '9') {
            version++;
        }
        parts++;
        if (parts == 2) {
            return 1;
        }
        if (*version != '.') {
            return 0;
        }
        version++;
    }
    return 0;
}

static int sequence[8];
static int sequence_count;

static void record(void *context)
{
    if (sequence_count < 8) {
        sequence[sequence_count] = *(int *)context;
    }
    sequence_count++;
}

static void test_version_string_is_semver(void)
{
    EXPECT(semver_major_minor(SCHEDULER_VERSION_STRING));
}

static void test_create_and_destroy(void)
{
    scheduler *sched = NULL;
    time_t when = (time_t)42;

    EXPECT(scheduler_create(NULL) == SCHEDULER_INVAL);
    EXPECT(scheduler_destroy(NULL) == SCHEDULER_INVAL);
    EXPECT(scheduler_destroy(&sched) == SCHEDULER_OK);
    EXPECT(scheduler_create(&sched) == SCHEDULER_OK);
    EXPECT(sched != NULL);
    EXPECT(scheduler_count(sched) == 0);
    EXPECT(scheduler_count(NULL) == 0);
    EXPECT(scheduler_next(sched, &when) == SCHEDULER_OK);
    EXPECT(when == (time_t)42);
    EXPECT(scheduler_next(NULL, &when) == SCHEDULER_INVAL);
    EXPECT(scheduler_next(sched, NULL) == SCHEDULER_INVAL);
    EXPECT(scheduler_clear(NULL) == SCHEDULER_INVAL);
    EXPECT(scheduler_clear(sched) == SCHEDULER_OK);
    EXPECT(scheduler_run(NULL, when) == SCHEDULER_INVAL);
    EXPECT(scheduler_destroy(&sched) == SCHEDULER_OK);
    EXPECT(sched == NULL);
    EXPECT(scheduler_destroy(&sched) == SCHEDULER_OK);
}

static void test_add_rejects_invalid_and_past(void)
{
    scheduler *sched = NULL;
    scheduler_task *kept = NULL;
    scheduler_task *task = (scheduler_task *)&kept;
    int marker = 1;
    time_t soon;

    EXPECT(scheduler_create(&sched) == SCHEDULER_OK);
    soon = time(NULL) + 60;
    EXPECT(scheduler_add(NULL, soon, record, &marker, "a", &task) == SCHEDULER_INVAL);
    EXPECT(scheduler_add(sched, soon, NULL, &marker, "a", &task) == SCHEDULER_INVAL);
    EXPECT(scheduler_add(sched, soon, record, &marker, NULL, &task) == SCHEDULER_INVAL);
    EXPECT(scheduler_add(sched, soon, record, &marker, "", &task) == SCHEDULER_INVAL);
    EXPECT(scheduler_add(sched, (time_t)1, record, &marker, "old", &task) == SCHEDULER_PAST);
    EXPECT(scheduler_count(sched) == 0);
    EXPECT(task == (scheduler_task *)&kept);
    EXPECT(scheduler_add(sched, soon, record, NULL, "null-ctx", NULL) == SCHEDULER_OK);
    EXPECT(scheduler_count(sched) == 1);
    EXPECT(scheduler_find(sched, record, "null-ctx", NULL) == SCHEDULER_INVAL);
    EXPECT(scheduler_cancel(NULL, task) == SCHEDULER_INVAL);
    EXPECT(scheduler_cancel(sched, NULL) == SCHEDULER_INVAL);
    EXPECT(scheduler_destroy(&sched) == SCHEDULER_OK);
}

static void test_run_order_and_context(void)
{
    scheduler *sched = NULL;
    time_t base;
    time_t when = (time_t)7;
    int first = 1;
    int second = 2;
    int third = 3;

    sequence_count = 0;
    EXPECT(scheduler_create(&sched) == SCHEDULER_OK);
    base = time(NULL);
    EXPECT(scheduler_add(sched, base + 30, record, &third, "third", NULL) == SCHEDULER_OK);
    EXPECT(scheduler_add(sched, base + 10, record, &first, "first", NULL) == SCHEDULER_OK);
    EXPECT(scheduler_add(sched, base + 10, record, &second, "second", NULL) == SCHEDULER_OK);
    EXPECT(scheduler_count(sched) == 3);
    EXPECT(scheduler_next(sched, &when) == SCHEDULER_OK);
    EXPECT(when == base + 10);
    EXPECT(scheduler_run(sched, base + 9) == SCHEDULER_OK);
    EXPECT(sequence_count == 0);
    EXPECT(scheduler_count(sched) == 3);
    EXPECT(scheduler_run(sched, base + 10) == SCHEDULER_OK);
    EXPECT(sequence_count == 2);
    EXPECT(sequence[0] == 1);
    EXPECT(sequence[1] == 2);
    EXPECT(scheduler_count(sched) == 1);
    EXPECT(scheduler_run(sched, base + 30) == SCHEDULER_OK);
    EXPECT(sequence_count == 3);
    EXPECT(sequence[2] == 3);
    EXPECT(scheduler_count(sched) == 0);
    EXPECT(scheduler_destroy(&sched) == SCHEDULER_OK);
}

static void other_fn(void *context)
{
    (void)context;
}

static void test_find_is_case_insensitive(void)
{
    scheduler *sched = NULL;
    scheduler_task *soon = NULL;
    scheduler_task *later = NULL;
    scheduler_task *kept = NULL;
    scheduler_task *found = (scheduler_task *)&kept;
    time_t base;

    EXPECT(scheduler_create(&sched) == SCHEDULER_OK);
    base = time(NULL);
    EXPECT(scheduler_add(sched, base + 20, record, NULL, "Hello", &soon) == SCHEDULER_OK);
    EXPECT(scheduler_add(sched, base + 40, other_fn, NULL, "hello", &later) == SCHEDULER_OK);
    EXPECT(scheduler_add(sched, base + 50, record, NULL, "Hello", NULL) == SCHEDULER_OK);
    EXPECT(scheduler_find(sched, record, "hello", &found) == SCHEDULER_OK);
    EXPECT(found == soon);
    found = (scheduler_task *)&kept;
    EXPECT(scheduler_find(sched, other_fn, "HELLO", &found) == SCHEDULER_OK);
    EXPECT(found == later);
    found = (scheduler_task *)&kept;
    EXPECT(scheduler_find(sched, record, "missing", &found) == SCHEDULER_OK);
    EXPECT(found == (scheduler_task *)&kept);
    EXPECT(scheduler_find(NULL, record, "Hello", &found) == SCHEDULER_INVAL);
    EXPECT(scheduler_find(sched, record, "", &found) == SCHEDULER_INVAL);
    EXPECT(scheduler_destroy(&sched) == SCHEDULER_OK);
}

typedef struct rig {
    scheduler *sched;
    scheduler_task *other;
    scheduler_task *self;
    time_t when;
    int calls;
    int added;
    int self_rc;
} rig;

static void added_fn(void *context)
{
    rig *state = context;

    state->added++;
}

static void later_fn(void *context)
{
    rig *state = context;

    state->calls += 10;
}

static void earlier_fn(void *context)
{
    rig *state = context;

    state->calls += 1;
    state->self_rc = scheduler_cancel(state->sched, state->self);
    EXPECT(scheduler_cancel(state->sched, state->other) == SCHEDULER_OK);
    EXPECT(scheduler_add(state->sched, state->when, added_fn, state, "added", NULL) ==
           SCHEDULER_OK);
}

static void test_callback_cancel_and_add(void)
{
    scheduler *sched = NULL;
    rig state;
    time_t base;

    memset(&state, 0, sizeof(state));
    EXPECT(scheduler_create(&sched) == SCHEDULER_OK);
    base = time(NULL);
    state.sched = sched;
    state.when = base + 5;
    EXPECT(scheduler_add(sched, base + 5, earlier_fn, &state, "early", &state.self) ==
           SCHEDULER_OK);
    EXPECT(scheduler_add(sched, base + 5, later_fn, &state, "late", &state.other) ==
           SCHEDULER_OK);
    EXPECT(scheduler_run(sched, base + 5) == SCHEDULER_OK);
    EXPECT(state.calls == 1);
    EXPECT(state.added == 0);
    EXPECT(state.self_rc == SCHEDULER_INVAL);
    EXPECT(scheduler_count(sched) == 1);
    EXPECT(scheduler_run(sched, base + 5) == SCHEDULER_OK);
    EXPECT(state.added == 1);
    EXPECT(state.calls == 1);
    EXPECT(scheduler_count(sched) == 0);
    EXPECT(scheduler_destroy(&sched) == SCHEDULER_OK);
}

static void test_schedules_are_independent(void)
{
    scheduler *left = NULL;
    scheduler *right = NULL;
    scheduler_task *task = NULL;
    int marker = 4;
    time_t soon;

    sequence_count = 0;
    EXPECT(scheduler_create(&left) == SCHEDULER_OK);
    EXPECT(scheduler_create(&right) == SCHEDULER_OK);
    soon = time(NULL) + 15;
    EXPECT(scheduler_add(left, soon, record, &marker, "only", &task) == SCHEDULER_OK);
    EXPECT(scheduler_cancel(right, task) == SCHEDULER_INVAL);
    EXPECT(scheduler_count(left) == 1);
    EXPECT(scheduler_count(right) == 0);
    EXPECT(scheduler_clear(left) == SCHEDULER_OK);
    EXPECT(scheduler_count(left) == 0);
    EXPECT(scheduler_run(left, soon) == SCHEDULER_OK);
    EXPECT(sequence_count == 0);
    EXPECT(scheduler_destroy(&left) == SCHEDULER_OK);
    EXPECT(scheduler_destroy(&right) == SCHEDULER_OK);
}

static void run_test(const char *name, void (*fn)(void))
{
    int before = failures;

    current_test = name;
    fn();
    tests_run++;
    if (failures == before) {
        printf("tests/test_scheduler.c::%s PASSED\n", name);
    } else {
        tests_failed++;
        printf("tests/test_scheduler.c::%s FAILED\n", name);
    }
    fflush(stdout);
}

int main(void)
{
    int passed;

    run_test("test_version_string_is_semver", test_version_string_is_semver);
    run_test("test_create_and_destroy", test_create_and_destroy);
    run_test("test_add_rejects_invalid_and_past", test_add_rejects_invalid_and_past);
    run_test("test_run_order_and_context", test_run_order_and_context);
    run_test("test_find_is_case_insensitive", test_find_is_case_insensitive);
    run_test("test_callback_cancel_and_add", test_callback_cancel_and_add);
    run_test("test_schedules_are_independent", test_schedules_are_independent);

    passed = tests_run - tests_failed;
    if (tests_failed != 0) {
        printf("%d failed, %d passed, %d assertion(s) failed\n",
               tests_failed, passed, failures);
        return 1;
    }
    printf("%d passed\n", passed);
    return 0;
}
