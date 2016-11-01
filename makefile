all: bin/parque bin/gerador

bin/parque: parque.c resources.h
	gcc parque.c -o bin/parque -D_REENTRANT -lpthread -Wall

bin/gerador: gerador.c resources.h
	gcc gerador.c -o bin/gerador -D_REENTRANT -lpthread -Wall
