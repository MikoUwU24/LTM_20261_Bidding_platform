CC ?= gcc
CFLAGS = -std=c11 -Wall -Wextra -Werror -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE -I. -g $(CFLAGS_EXTRA)

COMMON_SRCS = common/stream.c common/parse.c
SERVER_SRCS = server/main.c server/session.c server/account.c server/room.c \
              server/item.c server/auction.c server/handler.c server/storage.c \
              server/logger.c $(COMMON_SRCS)
CLIENT_SRCS = client/main.c client/ui.c $(COMMON_SRCS)

SERVER_OBJS = $(SERVER_SRCS:.c=.o)
CLIENT_OBJS = $(CLIENT_SRCS:.c=.o)

.PHONY: all clean dirs unit test web

all: dirs bin/server bin/client

web:
	$(MAKE) CFLAGS_EXTRA="-DAUCTION_IDLE_SEC=15 -DCOUNTDOWN_INTERVAL_SEC=3" all
	python3 web/gateway.py

dirs:
	@mkdir -p bin data logs

bin/server: $(SERVER_OBJS) | dirs
	$(CC) $(CFLAGS) -o $@ $(SERVER_OBJS)

bin/client: $(CLIENT_OBJS) | dirs
	$(CC) $(CFLAGS) -o $@ $(CLIENT_OBJS)

bin/server_small: $(SERVER_SRCS) | dirs
	$(CC) $(CFLAGS) -DMAX_CLIENTS=2 -o $@ $(SERVER_SRCS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

unit: dirs
	$(CC) $(CFLAGS) -o bin/test_stream tests/unit/test_stream.c common/stream.c
	$(CC) $(CFLAGS) -o bin/test_parse tests/unit/test_parse.c common/parse.c
	$(CC) $(CFLAGS) -o bin/test_account tests/unit/test_account.c server/account.c server/storage.c server/logger.c common/parse.c
	./bin/test_stream
	./bin/test_parse
	./bin/test_account

test:
	$(MAKE) clean
	$(MAKE) CFLAGS_EXTRA="-DAUCTION_IDLE_SEC=3 -DCOUNTDOWN_INTERVAL_SEC=1" all bin/server_small
	$(MAKE) CFLAGS_EXTRA="-DAUCTION_IDLE_SEC=3 -DCOUNTDOWN_INTERVAL_SEC=1" unit
	bash tests/run_all.sh

clean:
	rm -rf bin/*.o common/*.o server/*.o client/*.o tests/unit/*.o bin/server bin/client bin/server_small bin/test_*
