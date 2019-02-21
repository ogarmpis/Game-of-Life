CC = gcc
MPICC = mpicc
CFLAGS = -Wall -g


all: create_file gol-serial gol-mpi_parallel_io

create_file: create_file.o
	$(CC) $(CFLAGS) -o create_file create_file.o

gol-serial: gol-serial.o
	$(CC) $(CFLAGS) -o gol-serial gol-serial.o

gol-mpi_parallel_io: gol-mpi_parallel_io.o functions.o
	$(MPICC) $(CFLAGS) -o gol-mpi_parallel_io gol-mpi_parallel_io.o functions.o -lm

create_file.o: create_file.c
	$(CC) $(CFLAGS) -c create_file.c

gol-serial.o: gol-serial.c
	$(CC) $(CFLAGS) -c gol-serial.c

gol-mpi_parallel_io.o: gol-mpi_parallel_io.c
	$(MPICC) $(CFLAGS) -c gol-mpi_parallel_io.c

functions.o: functions.c
	$(CC) $(CFLAGS) -c functions.c


.PHONY: clean

clean:
	rm -f create_file create_file.o
	rm -f gol-serial gol-serial.o
	rm -f gol-mpi_parallel_io gol-mpi_parallel_io.o
	rm -f functions.o