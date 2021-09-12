EXEC=xwiv
OBJS=main.o wav.o visualise.o

CFLAGS=-O2 -Wall -Wno-bitwise-op-parentheses
LDFLAGS=
LDLIBS=-lX11 -lm

ifeq ($(shell uname), FreeBSD)
    CFLAGS += -I/usr/local/include
    LDFLAGS += -L/usr/local/lib
endif

$(EXEC) : $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS) $(LDLIBS)

clean :
	rm $(EXEC) $(OBJS)
