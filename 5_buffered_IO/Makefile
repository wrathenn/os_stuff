FLAGS := -std=c99 -pthread
.PHONY: debug
debug: FLAGS += -g3
debug: all

obj_files := 1.out, 1_th.out, 2.out, 2_th.out, 2_th_mutex.out, 3_th.out, 3_th_append.out
SRCS = $(wildcard *.c)
OBJS = $(patsubst %.c,%,$(SRCS))

all: $(OBJS)

%: %.c
	gcc $(FLAGS) -o $@ $<

clean:
	rm $(OBJS)