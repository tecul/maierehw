
VPATH=core:source:acquisition:tracking:demod:ephemeris
CFLAGS=-Iinclude -Isource -Iacquisition -Itracking -Idemod -Iephemeris -O0 -g
LDFLAGS=-lm

maierehw: main.o core.o message_broker.o file_source.o acquisition_basic.o tracking_manager.o tracking_loop.o pll.o demod.o demod_word.o ephemeris.o
	$(CC) $(LDFLAGS) $^ $(LDFLAGS) -o $@

clean:
	rm -f *.o
	rm -f maierehw
