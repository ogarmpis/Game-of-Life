#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include "mpi.h"
#include "functions.h"


#define BUFSIZE 64



/*Main MPI program - In command line (example): mpiexec -n 4 ./gol-mpi_omp -n 16 -g 3 -i ./"Input Files"/glider -t 2*/
int main(int argc, char *argv[])
{
	int  i, j, N, SideBlocks, SideProcesses, generations;
	int  processes, my_rank, output = 0, doom = 0, thread_count = 2;
	int  nozero, diff, allzeros, change;
	char **cells, **blocks;
	FILE *fp = NULL;


	/*Initialize MPI to share the calculations between different processors*/
	MPI_Init(&argc, &argv);
	MPI_Comm_size(MPI_COMM_WORLD, &processes);
	MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);


	/*Read the arguments*/
	for (i = 0; i < argc; i++){
		if (!strcmp(argv[i], "-n")) N = atoi(argv[++i]);
		else if (!strcmp(argv[i], "-g")) generations = atoi(argv[++i]);
		else if (!strcmp(argv[i], "-i")) fp = fopen(argv[++i], "r");
		else if (!strcmp(argv[i], "-o1")) output = 1;
		else if (!strcmp(argv[i], "-o2")) output = 2;
		else if (!strcmp(argv[i], "-d")) doom = atoi(argv[++i]);
		else if (!strcmp(argv[i], "-t")) thread_count = atoi(argv[++i]);
	}


	/*Allocate memory for cells array and initialize it with 0s or randomly*/
	if (fp == NULL){
		srand(time(NULL));
		cells = allocateArray(N, 1);
	}
	else cells = allocateArray(N, 0);


	/*Allocate memory for cells array, initialize it and check if the flag values are proper*/
	if (my_rank == 0)
	{
		/*Read from input file the position of initial live cells (if there is input file)*/
		if (fp != NULL){
			char line[BUFSIZE], *token, delim[2] = " ";
			fgets(line, BUFSIZE, fp);
			while (!feof(fp)){							//Till the end of the file read from it
				token = strtok(line, delim);
				i = atoi(token);
				token = strtok(NULL, delim);
				j = atoi(token);
				cells[i][j] = 1;						//Assign a live cell in the coordinates given
				fgets(line, BUFSIZE, fp);
			}
			fclose(fp);									//Close the file
		}

		/*Check if the number of processes are powers, or else exit*/
		float blockside = sqrt((float)processes);
		if ((blockside - (int)blockside) != 0){
			if (my_rank == 0) fprintf(stderr, "The processes must be powers of 2, 3, etc.\n");
			MPI_Abort(MPI_COMM_WORLD,1);
		}
	}

	/*Start timer - finish just before MPI_Finalize()*/
	double start, finish;
	MPI_Barrier(MPI_COMM_WORLD);
	start = MPI_Wtime();

	/*Allocate memory for blocks in every process*/
	SideProcesses = (int) sqrt((float)processes);
	SideBlocks = (int) (N / SideProcesses);
	blocks = allocateArray(SideBlocks, 0);


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
	MPI_Scatterv(&(cells[0][0]), counts, starting_point, subarraytype, &(blocks[0][0]), N*N/processes, MPI_CHAR, 0, MPI_COMM_WORLD);


	/*The subarrays must arranged as Cartesian coordinate structure for proper communication between the local ones*/
	int dim_size[2], periods[2];
	MPI_Comm new_comm;
	for (i = 0; i < 2; i++){
		dim_size[i] = SideProcesses;		//How many Rows and Columns
		periods[i]  = 1;					//Both dimensions are periodic
	}
	MPI_Cart_create(MPI_COMM_WORLD, 2, dim_size, periods, 0, &new_comm);		//Create the coordinate structure (a 2D array)

	/*Find the neighbors of each subarray along with its coordinates*/
	int upleft, up, upright, right, downright, down, downleft, left, coords[2], neighbor[2];
	MPI_Cart_coords(new_comm, my_rank, 2, coords);
	neighbor[0] = coords[0] - 1;
	neighbor[1] = coords[1] - 1;
	MPI_Cart_rank(new_comm, neighbor, &upleft);			//Upleft
	neighbor[0] = coords[0] - 1;
	neighbor[1] = coords[1];
	MPI_Cart_rank(new_comm, neighbor, &up);				//Up
	neighbor[0] = coords[0] - 1;
	neighbor[1] = coords[1] + 1;
	MPI_Cart_rank(new_comm, neighbor, &upright);			//Upright
	neighbor[0] = coords[0];
	neighbor[1] = coords[1] + 1;
	MPI_Cart_rank(new_comm, neighbor, &right);				//Right
	neighbor[0] = coords[0] + 1;
	neighbor[1] = coords[1] + 1;
	MPI_Cart_rank(new_comm, neighbor, &downright);			//Downright
	neighbor[0] = coords[0] + 1;
	neighbor[1] = coords[1];
	MPI_Cart_rank(new_comm, neighbor, &down);				//Down
	neighbor[0] = coords[0] + 1;
	neighbor[1] = coords[1] - 1;
	MPI_Cart_rank(new_comm, neighbor, &downleft);			//Downleft
	neighbor[0] = coords[0];
	neighbor[1] = coords[1] - 1;
	MPI_Cart_rank(new_comm, neighbor, &left);				//Left


	/*Declare waitall() variables and arrays for copy a line or cell (neighbor_border) and receive (received_border)*/
	MPI_Request request[8];
	MPI_Status  status[8];
	char *neighbor_border, side_border[SideBlocks], **received_border, *help1, **help2;
	neighbor_border = malloc(SideBlocks * sizeof(char));
	help1 = neighbor_border;											//Copy array - neighbor_border
	received_border = malloc(8 * sizeof(char*));
	for (i = 0; i < 8; i++)												//Paste (receive) array - received_border
		received_border[i] = malloc(SideBlocks * sizeof(char));
	help2 = received_border;

	/*A temporary array to find the new values of cells for the next generation and then copy it to blocks array*/
	char **new_gen, **swap;
	new_gen = allocateArray(SideBlocks, 0);


	/*All generations are sychronized and each output is shown by the master process*/
	for (i = 0; i < generations; i++)
	{
		if ((my_rank == 0) && (output == 2))
			show(cells, N);						//Print the cells array as modified in this generation (-o2 in cmd)

		if (i != generations-1)
		{
			/*Send the border 1D (line) arrays to the proper neighbor, who receives this neighboring line or cell*/
			neighbor_border = &(blocks[0][0]);
			MPI_Isend(neighbor_border, 1, MPI_CHAR, upleft, 0, MPI_COMM_WORLD, &request[0]);
			MPI_Irecv(received_border[0], 1, MPI_CHAR, downright, 0, MPI_COMM_WORLD, &request[1]);			// 0 - downright cell
			neighbor_border = &(blocks[0][0]);
			MPI_Isend(neighbor_border, SideBlocks, MPI_CHAR, up, 0, MPI_COMM_WORLD, &request[2]);
			MPI_Irecv(received_border[1], SideBlocks, MPI_CHAR, down, 0, MPI_COMM_WORLD, &request[3]);		// 1 - down side
			neighbor_border = &(blocks[0][SideBlocks-1]);
			MPI_Isend(neighbor_border, 1, MPI_CHAR, upright, 0, MPI_COMM_WORLD, &request[4]);
			MPI_Irecv(received_border[2], 1, MPI_CHAR, downleft, 0, MPI_COMM_WORLD, &request[5]);			// 2 - downleft cell
			for (j = 0; j < SideBlocks; j++)
				side_border[j] = blocks[j][SideBlocks-1];
			MPI_Isend(side_border, SideBlocks, MPI_CHAR, right, 0, MPI_COMM_WORLD, &request[6]);
			MPI_Irecv(received_border[3], SideBlocks, MPI_CHAR, left, 0, MPI_COMM_WORLD, &request[7]);		// 3 - left side
			neighbor_border = &(blocks[SideBlocks-1][SideBlocks-1]);
			MPI_Isend(neighbor_border, 1, MPI_CHAR, downright, 0, MPI_COMM_WORLD, &request[8]);
			MPI_Irecv(received_border[4], 1, MPI_CHAR, upleft, 0, MPI_COMM_WORLD, &request[9]);				// 4 - upleft cell
			neighbor_border = &(blocks[SideBlocks-1][0]);
			MPI_Isend(neighbor_border, SideBlocks, MPI_CHAR, down, 0, MPI_COMM_WORLD, &request[10]);
			MPI_Irecv(received_border[5], SideBlocks, MPI_CHAR, up, 0, MPI_COMM_WORLD, &request[11]);		// 5 - up side
			neighbor_border = &(blocks[SideBlocks-1][0]);
			MPI_Isend(neighbor_border, 1, MPI_CHAR, downleft, 0, MPI_COMM_WORLD, &request[12]);
			MPI_Irecv(received_border[6], 1, MPI_CHAR, upright, 0, MPI_COMM_WORLD, &request[13]);			// 6 - upright cell
			for (j = 0; j < SideBlocks; j++)
				side_border[j] = blocks[j][0];
			MPI_Isend(side_border, SideBlocks, MPI_CHAR, left, 0, MPI_COMM_WORLD, &request[14]);
			MPI_Irecv(received_border[7], SideBlocks, MPI_CHAR, right, 0, MPI_COMM_WORLD, &request[15]);	// 7 - right side

			allzeros = 0;				//Check if there are all 0s (at least one 1, then evolve function returns 1 in allzeros variable)
			change   = 0;				//Check if the new generation is similar to the previous one (if not change is returned as 1)

			evolve_inner(blocks, new_gen, SideBlocks, thread_count, &allzeros, &change);	//Until send-receive is done, compute the inner cells

			MPI_Waitall(8, request, status);
			MPI_Barrier(MPI_COMM_WORLD);			//Wait for all processes to send and receive the neighboring cells

			evolve_sides(blocks, new_gen, received_border, SideBlocks, thread_count, &allzeros, &change);		//Evolve the side cells of blocks

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
	free(help1);						//Delete neighbor_border array (for copying side cells)

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
	finish = MPI_Wtime();						//Stop timer
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