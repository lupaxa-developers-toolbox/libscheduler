# Static library build. The c skill syntax-checks and runs cppcheck.
# It does not compile or link.

override CFLAGS += -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude
override CPPCHECK_FLAGS += -Iinclude --std=c11

SCHEDULER_BUILD := build
SCHEDULER_LIB := $(SCHEDULER_BUILD)/libscheduler.a
SCHEDULER_OBJ := $(SCHEDULER_BUILD)/scheduler.o
SCHEDULER_TEST := $(SCHEDULER_BUILD)/test_scheduler

.PHONY: lib test check clean

lib: $(SCHEDULER_LIB)

$(SCHEDULER_OBJ): src/scheduler.c include/scheduler.h
	mkdir -p $(SCHEDULER_BUILD)
	$(CC) $(CFLAGS) -c -o $@ src/scheduler.c

$(SCHEDULER_LIB): $(SCHEDULER_OBJ)
	$(AR) rcs $@ $(SCHEDULER_OBJ)

$(SCHEDULER_TEST): tests/test_scheduler.c include/scheduler.h $(SCHEDULER_LIB)
	$(CC) $(CFLAGS) -o $@ tests/test_scheduler.c $(SCHEDULER_LIB) $(LDFLAGS)

test: $(SCHEDULER_TEST)
	./$(SCHEDULER_TEST)

check: $(if $(filter c,$(SKILLS)),c-check) test

clean:
	rm -rf $(SCHEDULER_BUILD)
