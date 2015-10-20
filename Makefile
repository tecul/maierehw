
VPATH=core:source:acquisition
CFLAGS=-Iinclude -Isource -Iacquisition -O0 -g
LDFLAGS=-lm

gps2: main.o message_broker.o file_source.o acquisition_basic.o
	$(CC) $(LDFLAGS) $^ $(LDFLAGS) -o $@

clean:
	rm -f *.o
	rm -f gps2
