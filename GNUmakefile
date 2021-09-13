EXEC=xwiv
OBJS=main.o wav.o visualise.o

CFLAGS=-O2 -Wall -Wno-bitwise-op-parentheses
LDLIBS=-lX11 -lm

$(EXEC) : $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS) $(LDLIBS)

clean :
	rm -f $(EXEC) $(OBJS)
