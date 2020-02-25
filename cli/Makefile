all: sendihex8

sendihex8: sendihex8.c sio.c nio.c ../ihex8.c
	cc sendihex8.c sio.c nio.c ../ihex8.c -o sendihex8

clean:
	-rm -f *.o sendihex8
