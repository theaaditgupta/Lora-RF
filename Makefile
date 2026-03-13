CC     = gcc
CFLAGS = -Wall -Wextra -Werror -std=c11 -Iinclude -g
SRCS   = src/sx1276.c src/protocol.c src/main.c
OUT    = build/lora_sim

.PHONY: all run budget clean

all: $(OUT)

$(OUT): $(SRCS) | build
	$(CC) $(CFLAGS) $^ -o $@ -lm

run: $(OUT)
	./$(OUT)

budget:
	python3 scripts/link_budget.py

build:
	mkdir -p build

clean:
	rm -rf build
