
VPATH=core:source:acquisition:tracking:demod:ephemeris:pvt
CFLAGS=-Iinclude -Isource -Iacquisition -Itracking -Idemod -Iephemeris -Ipvt -O2 -g
LDFLAGS=-lm

maierehw: main.o core.o event.o
	$(CC) $(LDFLAGS) $^ $(LDFLAGS) -o $@ -lpthread

clean:
	rm -f *.o
	rm -f maierehw
