EXEC=xwiv
OBJS=main.o wav.o visualise.o

CFLAGS=-I/usr/local/include -O2 -Wall -Wno-bitwise-op-parentheses
LDFLAGS=-L/usr/local/lib
LDLIBS=-lX11 -lm

$(EXEC) : $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS) $(LDLIBS)

clean :
	rm -f $(EXEC) $(OBJS)
