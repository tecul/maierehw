
VPATH=core:source
CFLAGS=-Iinclude -Isource -O0 -g

gps2: main.o message_broker.o file_source.o
	$(CC) $(LFFLAGS) $^  -o $@

clean:
	rm -f *.o
	rm -f gps2
