LIQUIDWOLF_VERSION := "\"$(shell git describe --dirty)\""
all:
	gcc -g -Wall -ansi -std=c99 \
		-D_LIQUIDWOLF_VERSION=$(LIQUIDWOLF_VERSION) \
		bell202.c \
		util.c \
		ax25.c \
		hdlc.c \
		main.c \
		-lliquid -lm -lsndfile \
		-o liquidwolf

clean:
	rm liquidwolf
