
VPATH=core:source:acquisition:tracking:demod:ephemeris:pvt
CFLAGS=-Iinclude -Isource -Iacquisition -Itracking -Idemod -Iephemeris -Ipvt -O2 -g
LDFLAGS=-lm

maierehw: main.o core.o event.o file_source.o acquisition_basic.o tracking_manager.o tracking_loop.o pll.o demod.o demod_word.o ephemeris.o
	$(CC) $(LDFLAGS) $^ $(LDFLAGS) -o $@ -lpthread

clean:
	rm -f *.o
	rm -f maierehw
