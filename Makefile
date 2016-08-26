all: build/xmpp-bridge

MODE           := release
CFLAGS_debug   := -O2 -g
CFLAGS_release := -O3 -DRELEASE

CFLAGS   = -std=gnu99 -Wall -Werror -Wextra -pedantic $(CFLAGS_$(MODE))
CFLAGS  += $(shell pkg-config --cflags libstrophe)
LDFLAGS += $(shell pkg-config --libs   libstrophe)

build/%.o: src/%.c src/*.h
	$(CC) $(CFLAGS) -c -o $@ $<
build/xmpp-bridge: $(patsubst src/%.c,build/%.o,$(wildcard src/*.c))
	$(CC) $(LDFLAGS) -o $@ $^

install: build/xmpp-bridge
	install -D -m 0755 build/xmpp-bridge "$(DESTDIR)/usr/bin/xmpp-bridge"
	install -D -m 0644 xmpp-bridge.1     "$(DESTDIR)/usr/share/man/man1/xmpp-bridge.1"

.PHONY: all install
