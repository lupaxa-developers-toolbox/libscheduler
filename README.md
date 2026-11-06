<p align="center">
  <a href="https://github.com/lupaxa-developers-toolbox">
    <img src="https://raw.githubusercontent.com/the-lupaxa-project/brand-assets/master/logos/organisations/developers-toolbox/readme-logo.png" alt="Developers Toolbox" />
  </a>
</p>

<h1 align="center">Libscheduler</h1>

C library for running tasks at a chosen time.

A schedule holds named tasks. Each task runs once, when the caller says that
time has arrived, then it is removed. The types are opaque. The public
header is `include/scheduler.h`. It uses C linkage, so a C++ translation
unit can include it.

## Requirements

- GNU make.
- gcc, unless `CC` is set. The C skill uses gcc for syntax checks.
- cppcheck, for `make c-check`.

## Build

`make lib` writes the static archive `build/libscheduler.a`. The archive is
built as C11 with `-Wall -Wextra -Wpedantic -Werror`. The compiler is gcc
unless `CC` is set. `make clean` removes `build/`.

```bash
make lib
cc -std=c11 -Iinclude -o demo demo.c build/libscheduler.a
```

`make lib CC=clang` selects the compiler. `make lib CFLAGS=-g` adds flags to
that C11 build. `make` with no target prints the makefile help.

`make update` refreshes the makefile skills. `make lib` and `make test` do
not need that checkout. `make c-check` and `make version` do.

## Quick Start

Create a schedule, add a task, run it, and destroy the schedule.

```c
#include "scheduler.h"

#include <time.h>

static void started(void *context)
{
    (void)context;
}

int main(void)
{
    scheduler *sched = NULL;
    time_t when = time(NULL) + 60;

    if (scheduler_create(&sched) != SCHEDULER_OK) {
        return 1;
    }

    if (scheduler_add(sched, when, started, NULL, "start", NULL) != SCHEDULER_OK) {
        scheduler_destroy(&sched);
        return 1;
    }

    if (scheduler_run(sched, when) != SCHEDULER_OK) {
        scheduler_destroy(&sched);
        return 1;
    }

    scheduler_destroy(&sched);
    return 0;
}
```

`scheduler_run` calls `started` because `when` is the task time. A real poll
passes `time(NULL)` as that clock.

## Tasks

`scheduler_add` copies the name and stores the function and the context
pointer. The schedule does not free the context. An empty name stores
nothing. A time earlier than `time(NULL)` is `SCHEDULER_PAST`. A time equal
to `time(NULL)` is due on the next run at that timestamp.

`scheduler_run` calls every stored task whose time is `now` or earlier.
Soonest runs first. Two tasks at the same time run in the order they were
added. Each task is taken off the schedule before its callback, then freed
after the callback returns. A task added from a callback waits for a later
`scheduler_run`. Cancelling a later task from a callback stops that task.

`scheduler_next` writes the soonest stored time. An empty schedule leaves
the out-pointer unchanged, so the caller can sleep until that time instead
of polling.

`scheduler_find` returns the soonest task with that function pointer and
name. Name matching folds ASCII `A`–`Z` only. The first spelling of a name
is the one that was stored.

> **Note:** Callers serialise use of a given schedule.

## Return Codes

Every function that can fail returns an `int`.

| Code              | Value | Meaning                                                                              |
| ----------------- | ----- | ------------------------------------------------------------------------------------ |
| `SCHEDULER_OK`    | `0`   | The call succeeded.                                                                  |
| `SCHEDULER_NOMEM` | `-1`  | Allocation failed. The schedule is unchanged. Create sets `*sched` to NULL.          |
| `SCHEDULER_INVAL` | `-2`  | The schedule, task, function, name, or out-pointer is missing, or the name is empty. |
| `SCHEDULER_PAST`  | `-3`  | The task time is earlier than the current time. Nothing was stored.                  |

`scheduler_destroy(NULL)` returns `SCHEDULER_INVAL`. `scheduler_destroy` on a
pointer that is already NULL returns `SCHEDULER_OK` and leaves it NULL.

`scheduler_count(NULL)` returns `0`.

## Functions

```c
typedef struct scheduler scheduler;
typedef struct scheduler_task scheduler_task;
typedef void (*scheduler_fn)(void *context);

int scheduler_create(scheduler **sched);
int scheduler_destroy(scheduler **sched);
int scheduler_add(scheduler *sched, time_t when, scheduler_fn fn,
                  void *context, const char *name, scheduler_task **task);
int scheduler_cancel(scheduler *sched, scheduler_task *task);
int scheduler_clear(scheduler *sched);
int scheduler_run(scheduler *sched, time_t now);
int scheduler_find(const scheduler *sched, scheduler_fn fn, const char *name,
                   scheduler_task **out);
int scheduler_next(const scheduler *sched, time_t *when);
size_t scheduler_count(const scheduler *sched);
```

| Name                | Purpose                               |
| ------------------- | ------------------------------------- |
| `scheduler_create`  | Allocate an empty schedule.           |
| `scheduler_destroy` | Free the schedule and clear `*sched`. |
| `scheduler_add`     | Copy a name and store one task.       |
| `scheduler_cancel`  | Remove one task and free it.          |
| `scheduler_clear`   | Free every task on the schedule.      |
| `scheduler_run`     | Call every due task and remove it.    |
| `scheduler_find`    | Find a task by function and name.     |
| `scheduler_next`    | Read the soonest stored time.         |
| `scheduler_count`   | Return the number of stored tasks.    |

## Empty Schedule

A new schedule holds no tasks. Destroying it returns `SCHEDULER_OK` and sets
the caller's pointer to NULL. Destroying that NULL pointer again returns
`SCHEDULER_OK`.

## Version

The version text is the macro `SCHEDULER_VERSION_STRING` in
`include/scheduler.h`. It is a dotted semantic version. `.bumpversion.toml`
is the version source, and a bump updates that macro.

After `make init`, `make version` prints the version from
`.bumpversion.toml`.

## Development

```bash
make update
make lib
make check
make clean
```

`make c-check` syntax-checks every `.c` and `.h` file, then runs cppcheck.
`make check` runs `c-check` and then the behaviour tests. `make test` runs
only the behaviour tests.

<a href="https://github.com/the-lupaxa-project">
    <img src="https://raw.githubusercontent.com/the-lupaxa-project/brand-assets/master/logos/components/footer-for-child-orgs.svg" alt="The Lupaxa Project Footer" width="100%" />
</a>
