CC = gcc
MPICC = mpicc
CFLAGS = -Wall -g
OMPFLAGS = -fopenmp


all: gol-serial gol-mpi_omp

gol-serial: gol-serial.o
	$(CC) $(CFLAGS) -o gol-serial gol-serial.o

gol-mpi_omp: gol-mpi_omp.o functions.o
	$(MPICC) $(CFLAGS) $(OMPFLAGS) -o gol-mpi_omp gol-mpi_omp.o functions.o -lm

gol-serial.o: gol-serial.c
	$(CC) $(CFLAGS) -c gol-serial.c

gol-mpi_omp.o: gol-mpi_omp.c
	$(MPICC) $(CFLAGS) -c gol-mpi_omp.c

functions.o: functions.c
	$(CC) $(CFLAGS) $(OMPFLAGS) -c functions.c


.PHONY: clean

clean:
	rm -f gol-serial gol-serial.o
	rm -f gol-mpi_omp gol-mpi_omp.o
	rm -f functions.o