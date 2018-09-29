all:
	gcc -g -Wall -ansi -std=c99 dsp.c util.c ax25.c hdlc.c main.c -lliquid -lm -lsndfile -o liquidwolf

clean:
	rm liquidwolf
