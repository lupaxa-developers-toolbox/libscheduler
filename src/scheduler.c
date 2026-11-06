/* Quoted path so a syntax check without -I still finds the public header. */
#include "../include/scheduler.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct scheduler_task {
    struct scheduler_task *next;
    time_t when;
    scheduler_fn fn;
    void *context;
    char *name;
    struct scheduler *owner;
};

struct scheduler {
    struct scheduler_task *head;
    struct scheduler_task *firing;
    size_t count;
};

static char *dup_text(const char *text)
{
    size_t length = strlen(text);
    char *copy;

    if (length == SIZE_MAX) {
        return NULL;
    }
    copy = malloc(length + 1);
    if (copy == NULL) {
        return NULL;
    }
    memcpy(copy, text, length + 1);
    return copy;
}

static int fold_ascii(unsigned char byte)
{
    if (byte >= 'A' && byte <= 'Z') {
        return byte - 'A' + 'a';
    }
    return byte;
}

static int name_eq(const char *left, const char *right)
{
    while (*left != '\0' && *right != '\0') {
        if (fold_ascii((unsigned char)*left) != fold_ascii((unsigned char)*right)) {
            return 0;
        }
        left++;
        right++;
    }
    return *left == '\0' && *right == '\0';
}

static void insert_task(scheduler *sched, scheduler_task *task)
{
    scheduler_task **link = &sched->head;

    while (*link != NULL && (*link)->when <= task->when) {
        link = &(*link)->next;
    }
    task->next = *link;
    *link = task;
    sched->count++;
}

static void free_list(scheduler_task *task)
{
    while (task != NULL) {
        scheduler_task *next = task->next;

        free(task->name);
        free(task);
        task = next;
    }
}

static int unlink_task(scheduler_task **head, scheduler_task *task)
{
    scheduler_task **link = head;

    while (*link != NULL) {
        if (*link == task) {
            *link = task->next;
            task->next = NULL;
            return 1;
        }
        link = &(*link)->next;
    }
    return 0;
}

static scheduler_task *find_in(scheduler_task *node, scheduler_fn fn,
                               const char *name)
{
    while (node != NULL) {
        if (node->fn == fn && name_eq(node->name, name)) {
            return node;
        }
        node = node->next;
    }
    return NULL;
}

int scheduler_create(scheduler **out)
{
    scheduler *created;

    if (out == NULL) {
        return SCHEDULER_INVAL;
    }
    created = calloc(1, sizeof(*created));
    if (created == NULL) {
        *out = NULL;
        return SCHEDULER_NOMEM;
    }
    *out = created;
    return SCHEDULER_OK;
}

int scheduler_destroy(scheduler **out)
{
    if (out == NULL) {
        return SCHEDULER_INVAL;
    }
    if (*out == NULL) {
        return SCHEDULER_OK;
    }
    scheduler_clear(*out);
    free(*out);
    *out = NULL;
    return SCHEDULER_OK;
}

int scheduler_add(scheduler *sched, time_t when, scheduler_fn fn,
                  void *context, const char *name, scheduler_task **task)
{
    time_t now;
    scheduler_task *created;
    char *copy;

    if (sched == NULL || fn == NULL || name == NULL || name[0] == '\0') {
        return SCHEDULER_INVAL;
    }
    now = time(NULL);
    if (now == (time_t)-1) {
        return SCHEDULER_INVAL;
    }
    if (when < now) {
        return SCHEDULER_PAST;
    }
    created = calloc(1, sizeof(*created));
    if (created == NULL) {
        return SCHEDULER_NOMEM;
    }
    copy = dup_text(name);
    if (copy == NULL) {
        free(created);
        return SCHEDULER_NOMEM;
    }
    created->when = when;
    created->fn = fn;
    created->context = context;
    created->name = copy;
    created->owner = sched;
    insert_task(sched, created);
    if (task != NULL) {
        *task = created;
    }
    return SCHEDULER_OK;
}

int scheduler_cancel(scheduler *sched, scheduler_task *task)
{
    if (sched == NULL || task == NULL || task->owner != sched) {
        return SCHEDULER_INVAL;
    }
    if (!unlink_task(&sched->head, task) && !unlink_task(&sched->firing, task)) {
        return SCHEDULER_INVAL;
    }
    if (sched->count > 0) {
        sched->count--;
    }
    free(task->name);
    task->owner = NULL;
    free(task);
    return SCHEDULER_OK;
}

int scheduler_clear(scheduler *sched)
{
    if (sched == NULL) {
        return SCHEDULER_INVAL;
    }
    free_list(sched->head);
    free_list(sched->firing);
    sched->head = NULL;
    sched->firing = NULL;
    sched->count = 0;
    return SCHEDULER_OK;
}

int scheduler_run(scheduler *sched, time_t now)
{
    scheduler_task **link;
    scheduler_task **tail;

    if (sched == NULL || sched->firing != NULL) {
        return SCHEDULER_INVAL;
    }
    link = &sched->head;
    tail = &sched->firing;
    while (*link != NULL && (*link)->when <= now) {
        scheduler_task *task = *link;

        *link = task->next;
        task->next = NULL;
        *tail = task;
        tail = &task->next;
    }
    while (sched->firing != NULL) {
        scheduler_task *task = sched->firing;

        sched->firing = task->next;
        task->next = NULL;
        task->owner = NULL;
        if (sched->count > 0) {
            sched->count--;
        }
        task->fn(task->context);
        free(task->name);
        task->name = NULL;
        free(task);
    }
    return SCHEDULER_OK;
}

int scheduler_find(const scheduler *sched, scheduler_fn fn, const char *name,
                   scheduler_task **out)
{
    scheduler_task *found;

    if (sched == NULL || fn == NULL || name == NULL || name[0] == '\0' ||
        out == NULL) {
        return SCHEDULER_INVAL;
    }
    found = find_in(sched->firing, fn, name);
    if (found == NULL) {
        found = find_in(sched->head, fn, name);
    }
    if (found == NULL) {
        return SCHEDULER_OK;
    }
    *out = found;
    return SCHEDULER_OK;
}

int scheduler_next(const scheduler *sched, time_t *when)
{
    const scheduler_task *task;

    if (sched == NULL || when == NULL) {
        return SCHEDULER_INVAL;
    }
    task = sched->firing != NULL ? sched->firing : sched->head;
    if (task == NULL) {
        return SCHEDULER_OK;
    }
    *when = task->when;
    return SCHEDULER_OK;
}

size_t scheduler_count(const scheduler *sched)
{
    if (sched == NULL) {
        return 0;
    }
    return sched->count;
}
