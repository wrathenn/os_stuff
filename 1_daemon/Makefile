#compile options
# -lsubunit -lm
CFLAGS := -pthread

#object files
OBJS_MAIN := out/main_thr.o
.PHONY: release debug
release: CFLAGS += -DNDEBUG -g0
debug: CFLAGS += -g3

release: my_daemon
debug: my_daemon
all: my_daemon

my_daemon: $(OBJS_MAIN)
	gcc $(CFLAGS) -o $@ $^

out/%.o : src/%.c
	gcc $(CFLAGS) -o $@ -c $<

.PHONY: clean
clean:
	rm out/*.o *.out my_daemon
