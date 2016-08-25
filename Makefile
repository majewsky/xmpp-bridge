all: xmpp-bridge

MODE           := release
CFLAGS_debug   := -O2 -g
CFLAGS_release := -O3 -DRELEASE

CFLAGS   = -std=gnu99 -Wall -Werror -Wextra -pedantic $(CFLAGS_$(MODE))
CFLAGS  += $(shell pkg-config --cflags libstrophe)
LDFLAGS += $(shell pkg-config --libs   libstrophe)

xmpp-bridge: $(patsubst %.c,%.o,$(wildcard *.c))
	$(CC) $(LDFLAGS) -o $@ $^

install: xmpp-bridge
	install -D -m 0755 xmpp-bridge   "$(DESTDIR)/usr/bin/xmpp-bridge"
	install -D -m 0644 xmpp-bridge.1 "$(DESTDIR)/usr/share/man/man1/xmpp-bridge.1"

.PHONY: all install
