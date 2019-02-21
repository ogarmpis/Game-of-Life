#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include "mpi.h"
#include "functions.h"


#define BUFSIZE 64



/*Main MPI program - In command line (example): mpiexec -n 4 ./gol-mpi_parallel_io -n 16 -g 3 -i ./"Input Files"/fglider*/
int main(int argc, char *argv[])
{
	int  i, j, N = 8, SideBlocks, SideProcesses, generations = 3;
	int  processes, my_rank, output = 0, doom = 0;
	int  nozero, diff, allzeros, change;
	char **cells, **blocks, filename[BUFSIZE];


	/*Initialize MPI to share the calculations between different processors*/
	MPI_Init(&argc, &argv);
	MPI_Comm_size(MPI_COMM_WORLD, &processes);
	MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);


	/*Read the arguments from command line*/
	for (i = 0; i < argc; i++){
		if (!strcmp(argv[i], "-n")) N = atoi(argv[++i]);
		else if (!strcmp(argv[i], "-g")) generations = atoi(argv[++i]);
		else if (!strcmp(argv[i], "-i")) strcpy(filename, argv[++i]);
		else if (!strcmp(argv[i], "-o1")) output = 1;
		else if (!strcmp(argv[i], "-o2")) output = 2;
		else if (!strcmp(argv[i], "-d")) doom = atoi(argv[++i]);
	}

	cells = allocateArray(N);		//Allocate memory for cells array and initialize it with 0s

	/*Allocate memory for cells array, initialize it with 0s and check if the flag values are proper*/
	if (my_rank == 0)
	{
		/*Check if the input file and command line's size are similar, or else exit*/
		FILE *fp1 = fopen(filename, "r");
		fseek(fp1, 0, SEEK_END);
		int size = ftell(fp1);
		if (size != (N*N*2)){
			printf("Different input file's from command line's array size!\n");
			MPI_Abort(MPI_COMM_WORLD, 1);
		}
		fclose(fp1);

		/*Check if the number of processes are powers, or else exit*/
		float blockside = sqrt((float)processes);
		if ((blockside - (int)blockside) != 0){
			if (my_rank == 0) fprintf(stderr, "The processes must be powers of 2, 3, etc.\n");
			MPI_Abort(MPI_COMM_WORLD, 1);
		}
	}

	/*Start timer - finish just before MPI_Finalize()*/
	double start, finish;
	MPI_Barrier(MPI_COMM_WORLD);
	start = MPI_Wtime();

	/*Allocate memory for blocks in every process*/
	SideProcesses = (int) sqrt((float)processes);
	SideBlocks = (int) (N / SideProcesses);
	blocks = allocateArray(SideBlocks);


	/*Create a datatype for the subarrays (blocks) of the global cells array*/
	int sizes[2]    = {N, N};														//Global cells array size
	int subsizes[2] = {SideBlocks, SideBlocks};										//Local blocks array size
	int starts[2]   = {0, 0};														//The start point of blocks array
	MPI_Datatype type, subarraytype;
	MPI_Type_create_subarray(2, sizes, subsizes, starts, MPI_ORDER_C, MPI_CHAR, &type);		//Create the datatype
	MPI_Type_create_resized(type, 0, SideBlocks*sizeof(char), &subarraytype);				//Extend the above type
	MPI_Type_commit(&subarraytype);															//Commit the subarray datatype

	/*Scatter the array to all processors*/
	int counts[processes], starting_point[processes], displacement;
	if (my_rank == 0){
		for (i = 0; i < processes; i++)
			counts[i] = 1;								//How many datatypes each process receives
		displacement = 0;
		for (i = 0; i < SideProcesses; i++){
			for (j = 0; j < SideProcesses; j++){
				starting_point[i*SideProcesses+j] = displacement;		//The starting point of each datatype
				displacement++;
			}
			displacement += (SideBlocks-1)*SideProcesses;				//Each starting point is at every block extend
		}
	}

	/*Read from input file the proper block of data (for each process) and copy them to blocks array (parallel IO here)*/
	char *line, *token, delim[2] = " ";
	line = malloc(2*SideBlocks * sizeof(char));
	MPI_File fp;
	MPI_Status stat;
	MPI_Offset start_offset;
	MPI_File_open(MPI_COMM_WORLD, filename, MPI_MODE_RDONLY, MPI_INFO_NULL, &fp);
	start_offset = ((my_rank/SideProcesses) * SideBlocks*(N*2)) + ((my_rank % SideProcesses) * (SideBlocks*2));		//Proper indexing
	for (i = 0; i < SideBlocks; i++){
		MPI_File_seek(fp, (start_offset + i*2*N), MPI_SEEK_SET);			//Indexing for reading
		MPI_File_read(fp, line, (2*SideBlocks), MPI_CHAR, &stat);
		token = strtok(line, delim);
		blocks[i][0] = atoi(token);
		for (j = 1; j < SideBlocks; j++){				//Copy each value in the proper cell
			token = strtok(NULL, delim);
			blocks[i][j] = atoi(token);
		}
	}
	MPI_File_close(&fp);			//Close file descriptor


	/*The subarrays must arranged as Cartesian coordinate structure for proper communication between the local ones*/
	int dim_size[2], periods[2];
	MPI_Comm new_comm;
	for (i = 0; i < 2; i++){
		dim_size[i] = SideProcesses;		//How many Rows and Columns
		periods[i]  = 1;					//Both dimensions are periodic
	}
	MPI_Cart_create(MPI_COMM_WORLD, 2, dim_size, periods, 0, &new_comm);		//Create the coordinate structure (a 2D array)

	/*Find the neighbours of each subarray along with its coordinates*/
	int upleft, up, upright, right, downright, down, downleft, left, coords[2], neighbour[2];
	MPI_Cart_coords(new_comm, my_rank, 2, coords);
	neighbour[0] = coords[0] - 1;
	neighbour[1] = coords[1] - 1;
	MPI_Cart_rank(new_comm, neighbour, &upleft);			//Upleft
	neighbour[0] = coords[0] - 1;
	neighbour[1] = coords[1];
	MPI_Cart_rank(new_comm, neighbour, &up);				//Up
	neighbour[0] = coords[0] - 1;
	neighbour[1] = coords[1] + 1;
	MPI_Cart_rank(new_comm, neighbour, &upright);			//Upright
	neighbour[0] = coords[0];
	neighbour[1] = coords[1] + 1;
	MPI_Cart_rank(new_comm, neighbour, &right);				//Right
	neighbour[0] = coords[0] + 1;
	neighbour[1] = coords[1] + 1;
	MPI_Cart_rank(new_comm, neighbour, &downright);			//Downright
	neighbour[0] = coords[0] + 1;
	neighbour[1] = coords[1];
	MPI_Cart_rank(new_comm, neighbour, &down);				//Down
	neighbour[0] = coords[0] + 1;
	neighbour[1] = coords[1] - 1;
	MPI_Cart_rank(new_comm, neighbour, &downleft);			//Downleft
	neighbour[0] = coords[0];
	neighbour[1] = coords[1] - 1;
	MPI_Cart_rank(new_comm, neighbour, &left);				//Left


	/*Declare waitall() variables and arrays for copy a line or cell (neighbour_border) and receive (received_border)*/
	MPI_Request request[8];
	MPI_Status  status[8];
	char *neighbour_border, side_border[SideBlocks], **received_border, *help1, **help2;
	neighbour_border = malloc(SideBlocks * sizeof(char));
	help1 = neighbour_border;											//Copy array - neighbour_border
	received_border = malloc(8 * sizeof(char*));
	for (i = 0; i < 8; i++)												//Paste (receive) array - received_border
		received_border[i] = malloc(SideBlocks * sizeof(char));
	help2 = received_border;

	/*A temporary array to find the new values of cells for the next generation and then copy it to blocks array*/
	char **new_gen, **swap;
	new_gen = allocateArray(SideBlocks);


	/*All generations are sychronized and each output is shown by the master process*/
	for (i = 0; i < generations; i++)
	{
		if ((my_rank == 0) && (output == 2))
			show(cells, N);						//Print the cells array as modified in this generation (-o2 in cmd)

		if (i != generations-1)
		{
			/*Send the border 1D (line) arrays to the proper neighbour, who receives this neighbouring line or cell*/
			neighbour_border = &(blocks[0][0]);
			MPI_Isend(neighbour_border, 1, MPI_CHAR, upleft, 0, MPI_COMM_WORLD, &request[0]);
			MPI_Irecv(received_border[0], 1, MPI_CHAR, downright, 0, MPI_COMM_WORLD, &request[1]);			// 0 - downright cell
			neighbour_border = &(blocks[0][0]);
			MPI_Isend(neighbour_border, SideBlocks, MPI_CHAR, up, 0, MPI_COMM_WORLD, &request[2]);
			MPI_Irecv(received_border[1], SideBlocks, MPI_CHAR, down, 0, MPI_COMM_WORLD, &request[3]);		// 1 - down side
			neighbour_border = &(blocks[0][SideBlocks-1]);
			MPI_Isend(neighbour_border, 1, MPI_CHAR, upright, 0, MPI_COMM_WORLD, &request[4]);
			MPI_Irecv(received_border[2], 1, MPI_CHAR, downleft, 0, MPI_COMM_WORLD, &request[5]);			// 2 - downleft cell
			for (j = 0; j < SideBlocks; j++)
				side_border[j] = blocks[j][SideBlocks-1];
			MPI_Isend(side_border, SideBlocks, MPI_CHAR, right, 0, MPI_COMM_WORLD, &request[6]);
			MPI_Irecv(received_border[3], SideBlocks, MPI_CHAR, left, 0, MPI_COMM_WORLD, &request[7]);		// 3 - left side
			neighbour_border = &(blocks[SideBlocks-1][SideBlocks-1]);
			MPI_Isend(neighbour_border, 1, MPI_CHAR, downright, 0, MPI_COMM_WORLD, &request[8]);
			MPI_Irecv(received_border[4], 1, MPI_CHAR, upleft, 0, MPI_COMM_WORLD, &request[9]);				// 4 - upleft cell
			neighbour_border = &(blocks[SideBlocks-1][0]);
			MPI_Isend(neighbour_border, SideBlocks, MPI_CHAR, down, 0, MPI_COMM_WORLD, &request[10]);
			MPI_Irecv(received_border[5], SideBlocks, MPI_CHAR, up, 0, MPI_COMM_WORLD, &request[11]);		// 5 - up side
			neighbour_border = &(blocks[SideBlocks-1][0]);
			MPI_Isend(neighbour_border, 1, MPI_CHAR, downleft, 0, MPI_COMM_WORLD, &request[12]);
			MPI_Irecv(received_border[6], 1, MPI_CHAR, upright, 0, MPI_COMM_WORLD, &request[13]);			// 6 - upright cell
			for (j = 0; j < SideBlocks; j++)
				side_border[j] = blocks[j][0];
			MPI_Isend(side_border, SideBlocks, MPI_CHAR, left, 0, MPI_COMM_WORLD, &request[14]);
			MPI_Irecv(received_border[7], SideBlocks, MPI_CHAR, right, 0, MPI_COMM_WORLD, &request[15]);	// 7 - right side

			allzeros = 0;				//Check if there are all 0s (at least one 1, then evolve function returns 1 in allzeros variable)
			change   = 0;				//Check if the new generation is similar to the previous one (if not change is returned as 1)

			evolve_inner(blocks, new_gen, SideBlocks, &allzeros, &change);		//Until send-receive is done, compute the inner cells

			MPI_Waitall(8, request, status);	//Wait for all 8 non-blocking send and receives
			MPI_Barrier(MPI_COMM_WORLD);		//Wait for all processes to send and receive the neighbouring cells (must evolve simultaneously)

			evolve_sides(blocks, new_gen, received_border, SideBlocks, &allzeros, &change);		//Evolve the side cells of blocks

			/*Terminal checking (every 10 generations), if the array is still the same or is full of 0s, then stop the program*/
			if ((doom == 1) && ((i % 10) == 0) && (i != 0)){
				MPI_Reduce(&allzeros, &nozero, 1, MPI_INT, MPI_MAX, 0, MPI_COMM_WORLD);		//If at least one allzero == 1, then nozero = 1
				MPI_Reduce(&change, &diff, 1, MPI_INT, MPI_MAX, 0, MPI_COMM_WORLD);			//Same (MPI_MAX between 0 and 1)
				if (my_rank == 0 && (nozero == 0 || diff == 0)){
					fprintf(stderr, "Program terminated (nothing changed or all extinguisted in this generation)\n");
					MPI_Abort(MPI_COMM_WORLD, 1);
				}
			}

			swap    = blocks;
			blocks  = new_gen;			//Copy the temporary array to the initial one
			new_gen = swap;

			/*After each generation, gather all the blocks (with their new values) into the initial cells array (-o2 in cmd)*/
			if (output == 2)
				MPI_Gatherv(&(blocks[0][0]), N*N/processes, MPI_CHAR, &(cells[0][0]), counts, starting_point, subarraytype, 0, MPI_COMM_WORLD);
		}
	}
	/*After all processes are done, gather the blocks with their new values into the initial cells array (Whether there is output or not)*/
	MPI_Gatherv(&(blocks[0][0]), N*N/processes, MPI_CHAR, &(cells[0][0]), counts, starting_point, subarraytype, 0, MPI_COMM_WORLD);


	for (i = 0; i < 8; i++)
		free(help2[i]);					//Delete received_border 2D array (for receiving side cells depending on topology)
	free(help2);
	free(help1);						//Delete neighbour_border array (for copying side cells)

	if (my_rank == 0){
		if (output == 1)
			show(cells, N);				//If told from command line, print the array (-o1 in cmd)
		deleteArray(&cells);			//Delete cells array
	}
	deleteArray(&blocks);				//Delete blocks array on each process
	deleteArray(&new_gen);				//Delete the temporary array for copying new values

	MPI_Type_free(&subarraytype);		//Free the subarray type


	/*Stop timer and calculate the whole time of mpi procedures (generally the program time)*/
	double alltime, maxtime, mintime, sumtime;
	finish = MPI_Wtime();							//Stop timer
	alltime = finish - start;

	/*Calculate maximum, minimum and average time of mpi procedures*/
	MPI_Reduce(&alltime, &maxtime, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
	MPI_Reduce(&alltime, &mintime, 1, MPI_DOUBLE, MPI_MIN, 0, MPI_COMM_WORLD);
	MPI_Reduce(&alltime, &sumtime, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
	if (my_rank == 0){
		printf("\n///////////////////////////////////////////////////\n\n");
		printf("--------------------------------------------------------------\n");
		printf("Maximum Runtime = %f\n", maxtime);
		printf("Minimum Runtime = %f\n", mintime);
		printf("Average Runtime = %f\n", sumtime/processes);
		printf("--------------------------------------------------------------\n");
	}

	MPI_Finalize();						//End the MPI procedure

	return 0;
}