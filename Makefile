CFLAGS ?= -std=c11 -Wall -Wextra

MODULES = src/parse.c src/encode.c src/output.c \
          src/register_helper.c src/format_finder.c src/layout.c src/directives.c src/values.c
SOURCES = main.c $(MODULES)
HEADERS = $(wildcard src/*.h)

read_asm: $(SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) $(SOURCES) -o $@

.PHONY: test
test:
	@set -e; dir=$$(mktemp -d); \
	trap 'rm -f "$$dir/unit"; rmdir "$$dir"' 0; \
	$(CC) $(CFLAGS) -Isrc tests/unit.c $(MODULES) -o "$$dir/unit"; \
	"$$dir/unit"
