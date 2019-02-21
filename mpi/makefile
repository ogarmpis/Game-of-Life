CC = gcc
MPICC = mpicc
CFLAGS = -Wall -g


all: gol-serial gol-mpi

gol-serial: gol-serial.o
	$(CC) $(CFLAGS) -o gol-serial gol-serial.o

gol-mpi: gol-mpi.o functions.o
	$(MPICC) $(CFLAGS) -o gol-mpi gol-mpi.o functions.o -lm

gol-serial.o: gol-serial.c
	$(CC) $(CFLAGS) -c gol-serial.c

gol-mpi.o: gol-mpi.c
	$(MPICC) $(CFLAGS) -c gol-mpi.c

functions.o: functions.c
	$(CC) $(CFLAGS) -c functions.c


.PHONY: clean

clean:
	rm -f gol-serial gol-serial.o
	rm -f gol-mpi gol-mpi.o
	rm -f functions.o