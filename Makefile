CFLAGS=-O2
LDFLAGS=-L../gumbo-parser/.libs -L$(HOME)/inst/lib -ltcl8.6 -lgumbo
INCLUDE=-I$(HOME)/inst/include -I../gumbo-parser/src

tclgumbo.so: tclgumbo.o
	$(CC) $(CFLAGS) $(LDFLAGS) -shared -o $@ $<

tclgumbo.o: tclgumbo.c
	$(CC) $(CFLAGS) $(INCLUDE) -shared -c -o $@ $<
