
VPATH=core:source:acquisition:tracking
CFLAGS=-Iinclude -Isource -Iacquisition -Itracking -O2 -g
LDFLAGS=-lm

maierehw: main.o core.o message_broker.o file_source.o acquisition_basic.o tracking_manager.o tracking_loop.o pll.o
	$(CC) $(LDFLAGS) $^ $(LDFLAGS) -o $@

clean:
	rm -f *.o
	rm -f maierehw
