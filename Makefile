CC      := clang
CFLAGS  := -O2 -Wall -Wextra -std=c11
INC     := -Iinclude

SRC     := $(wildcard src/*.c)
BIN     := $(patsubst src/%.c,bin/%,$(SRC))

all: $(BIN)

bin/harness: src/harness.c
	@mkdir -p bin
	$(CC) $(CFLAGS) $(INC) -DHARNESS_STANDALONE $^ -o $@

bin/%: src/%.c src/harness.c
	@mkdir -p bin
	$(CC) $(CFLAGS) $(INC) $^ -o $@

run: all
	@chmod +x scripts/run_all.sh
	./scripts/run_all.sh

clean:
	rm -rf bin build