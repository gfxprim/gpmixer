CFLAGS?=-W -Wall -Wextra -O2
CFLAGS+=$(shell gfxprim-config --cflags)
LDLIBS=-lgfxprim $(shell gfxprim-config --libs-widgets) -lasound
BIN=gpmixer
DEP=$(BIN:=.dep)

all: $(DEP) $(BIN)

%.dep: %.c
	$(CC) $(CFLAGS) -M $< -o $@

install:
	install -D $(BIN) -t $(DESTDIR)/usr/bin/

-include $(DEP)

clean:
	rm -f $(BIN) *.dep *.o
