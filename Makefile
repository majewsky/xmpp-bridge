all: xmpp-bridge

MODE           := release
CFLAGS_debug   := -O2 -g
CFLAGS_release := -O3 -DRELEASE

CFLAGS   = -std=gnu99 -Wall -Werror -Wextra -pedantic $(CFLAGS_$(MODE))
CFLAGS  += $(shell pkg-config --cflags libstrophe)
LDFLAGS += $(shell pkg-config --libs   libstrophe)

xmpp-bridge: $(patsubst %.c,%.o,$(wildcard *.c))
	$(CC) $(LDFLAGS) -o $@ $^
