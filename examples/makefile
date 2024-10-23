SRC  = $(wildcard *.c)
OUT  = $(basename $(SRC))
DEPS = ../miracle.h ../miracle.c

CFLAGS = -g -O2 -std=c99 -Wno-deprecated-declarations -Wall -Wextra -Werror -Wuninitialized \
         -Wundef -pedantic -I../

build: $(OUT)

%: %.c $(DEPS)
	$(CC) $< $(CFLAGS) -o $@

clean:
	rm $(OUT)

all:
	@echo build, clean
