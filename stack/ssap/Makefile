# SSAP host-stack port for WS73 NearLink dongle.
#
# Pure userspace, x86-compilable. Layers:
#   codec/    SSAP PDU encode/decode (ssap_codec.c/h, ported from OHOS ssap_pkt.h)
#   transport /dev/hwsle ACB frame adapter (tcid 0x0A)
#   server    SSAP server (service table, request dispatch)
#
CC      ?= cc
CFLAGS  ?= -O2 -Wall -Wextra -Iinclude
CFLAGS  += -std=c11

SRCS := src/ssap_codec.c src/hwsle_transport.c src/ssap_server.c
OBJS := $(SRCS:.c=.o)

all: libssap.a ssap_codec_test

libssap.a: $(OBJS)
	ar rcs $@ $^

ssap_codec_test: test/test_codec.c libssap.a
	$(CC) $(CFLAGS) -o $@ test/test_codec.c libssap.a

ssap_server_test: test/test_server.c libssap.a
	$(CC) $(CFLAGS) -o $@ test/test_server.c libssap.a

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

test: ssap_codec_test ssap_server_test
	./ssap_codec_test
	./ssap_server_test

clean:
	rm -f $(OBJS) libssap.a ssap_codec_test ssap_server_test

.PHONY: all test clean
