CC=gcc
CFLAGS=-Wall -g -O0 -I"/usr/local/opt/openssl@3/include"
LIBS=-lcrypto -L"/usr/local/opt/openssl@3/lib"
OBJS=options.o queue.o md5.o

PROGS=md5

all: $(PROGS)

%.o : %.c
	$(CC) $(CFLAGS) -c $<

md5: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LIBS)

clean:
	rm -f $(PROGS) *.o *~
