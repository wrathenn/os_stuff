.DEFAULT_GOAL := app.exe

#compile options
CFLAGS := -std=c99 -Wall -pedantic -Wno-gnu-binary-literal

#object files
SRCS = $(shell find ./src ! -name 'main.c' -type f -name *.c)
HEADS = $(shell find ./src -type f -name *.h)
OBJS = $(shell echo $(SRCS) | sed 's/\/src/\/out/g')
OBJS := $(OBJS:.c=.o)

# Tests object files
T_SRCS = $(shell find ./test -type f -name *.c)
T_HEADS = $(shell find ./test -type f -name *.h)
T_OBJS = $(shell echo $(T_SRCS) | sed 's/\/test/\/out\/test/g')
T_OBJS := $(T_OBJS:.c=.o)

release: CFLAGS += -DNDEBUG -g0
debug: CFLAGS += -g3
tests: CFLAGS += -g3 -lcheck -lpthread -lrt -lsubunit -lm
tests: OBJS := $(filter-out ./out/main.o,$(OBJS))

release: app.exe
debug: app.exe
all: app.exe
tests: tests.exe

# Building final app
app.exe: out/main.o $(OBJS)
	clang $(CFLAGS) -o $@ $^

# Building test object files from test files
out/test/%.o : test/%.c
	mkdir -p $(@D)
	clang $(CFLAGS) -o $@ -c $<

# Building object files from src files
out/%.o : src/%.c
	mkdir -p $(@D)
	clang $(CFLAGS) -o $@ -c $<

tests.exe: out/test/main.o $(T_OBJS) $(OBJS)
	echo $(T_OBJS)
	clang -o $@ $^ $(CFLAGS)



.PHONY: clean
clean:
	find out/ -name '*.o' -delete
	rm -f ./app.exe
	rm -f ./tests.exe

all:
	echo $(OBJS)