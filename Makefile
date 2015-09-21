CWARNS  = -Wall -Wextra -Wconversion -Wno-unused-parameter
CFLAGS  = -std=gnu11 -fmessage-length=0 -O2 -g3
CDEFS   = -D_FILE_OFFSET_BITS=64 -DFUSE_USE_VERSION=26 
LDFLAGS = -lfuse

all:
	$(CC) $(CWARNS) $(CFLAGS) $(CDEFS) $(LDFLAGS) *.c -o ehs5fs

clean:
	rm ehs5fs
