#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stddef.h>
#include <time.h>

#define SCHEDULER_VERSION_STRING "0.0.0"

/* SCHEDULER_OK is success. SCHEDULER_NOMEM means allocation failed and the
   schedule was left unchanged, except a create which sets *sched to NULL.
   SCHEDULER_INVAL means a missing schedule, task, function, name, or
   out-pointer, or an empty name. SCHEDULER_PAST means the task time is
   earlier than the current time and nothing was stored. */
#define SCHEDULER_OK 0
#define SCHEDULER_NOMEM (-1)
#define SCHEDULER_INVAL (-2)
#define SCHEDULER_PAST (-3)

#ifdef __cplusplus
extern "C" {
#endif

typedef struct scheduler scheduler;
typedef struct scheduler_task scheduler_task;

/* Called with the context pointer given to scheduler_add. The schedule
   does not own that pointer. */
typedef void (*scheduler_fn)(void *context);

/* Allocate an empty schedule. On allocation failure *sched is NULL. */
int scheduler_create(scheduler **sched);

/* Free the schedule and every stored task. *sched is set to NULL.
   Destroying an already NULL schedule is success. */
int scheduler_destroy(scheduler **sched);

/* Copy name and store one task at when. when earlier than the current
   time is SCHEDULER_PAST. A time equal to the current time is due
   immediately. An empty name stores nothing. context may be NULL and is
   not copied. When task is not NULL, *task receives the new task. On
   failure the schedule is unchanged and *task is left unchanged. */
int scheduler_add(scheduler *sched, time_t when, scheduler_fn fn,
                  void *context, const char *name, scheduler_task **task);

/* Remove task from sched and free it. A task that is not on this schedule
   is SCHEDULER_INVAL and is left in place. The pointer is invalid after
   success. */
int scheduler_cancel(scheduler *sched, scheduler_task *task);

/* Free every task on the schedule. An empty schedule is success. */
int scheduler_clear(scheduler *sched);

/* Call every stored task with when <= now, soonest first, then remove it.
   Equal times run in the order they were added. The running task is off
   the schedule before its callback. Tasks added from a callback wait for
   a later run. now is the caller's clock, so a test can fire a future
   task without sleeping. */
int scheduler_run(scheduler *sched, time_t now);

/* Write the soonest task whose function is fn and whose name matches.
   Matching is ASCII case-insensitive, and the first spelling is kept.
   A missing task leaves *out unchanged. */
int scheduler_find(const scheduler *sched, scheduler_fn fn, const char *name,
                   scheduler_task **out);

/* Write the soonest stored time. An empty schedule leaves *when
   unchanged. */
int scheduler_next(const scheduler *sched, time_t *when);

/* Return the number of stored tasks. A NULL schedule has count 0. */
size_t scheduler_count(const scheduler *sched);

#ifdef __cplusplus
}
#endif

#endif /* SCHEDULER_H */
