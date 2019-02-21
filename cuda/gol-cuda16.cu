#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

#define BUFSIZE 64
#define BLOCK_SIZE 16

// Perdiodicty Preservation retains our periodicity
// Runs on CPU 
void periodicityPreservationCPU(int N, char *cells)
{
    int i;
    //rows
    for (i = 1; i <= N; ++i)
    {
        //Copy first real row to bottom extra row
        cells[(N+2)*(N+1)+i] = cells[(N+2)+i];
        //Copy last real row to top extra row
        cells[i] = cells[(N+2)*N + i];
    }
    //cols
    for (i = 0; i <= N+1; ++i)
    {
        //Copy first real column to right last extra column
        cells[i*(N+2)+N+1] = cells[i*(N+2)+1];
        //Copy last real column to left last extra column 
        cells[i*(N+2)] = cells[i*(N+2) + N];  
    }
}

// Runs on GPU
__global__ void periodicityPreservationGPU(int N, char *cells)
{
    int i;
    //rows
    for (i = 1; i <= N; ++i)
    {
        //Copy first real row to bottom extra row
        cells[(N+2)*(N+1)+i] = cells[(N+2)+i];
        //Copy last real row to top extra row
        cells[i] = cells[(N+2)*N + i];
    }
    //cols
    for (i = 0; i <= N+1; ++i)
    {
        //Copy first real column to right last extra column
        cells[i*(N+2)+N+1] = cells[i*(N+2)+1];
        //Copy last real column to left last extra column 
        cells[i*(N+2)] = cells[i*(N+2) + N];  
    }
}


/* Our evolve kernels shoulder our evolutions procedure ,
   specifically [GridSize] blocks X [256] threads run towards 
   our evolve kernels . Each thread looks after the evolution
   of one cell by calculating each neighbors and abiding 
   the evolution rules */

// Based on global memoery
__global__ void evovle_kernel(int N, char *oldGen, char *newGen, int *allzeros, int *change)
{
    // Achieve indexng on 2D blocks
    int ix = blockDim.x * blockIdx.x + threadIdx.x + 1;
    int iy = blockDim.y * blockIdx.y + threadIdx.y + 1;
    // Thread calculates its global id
    int id = ix * (N+2) + iy;
 
    int neighbors;
 
    if (ix <= N && iy <= N) {
        neighbors = oldGen[id+(N+2)] + oldGen[id-(N+2)]     //lower upper
                    + oldGen[id+1] + oldGen[id-1]           //right left
                    + oldGen[id+(N+3)] + oldGen[id-(N+3)]   //diagonals
                    + oldGen[id-(N+1)] + oldGen[id+(N+1)];
 
        char cell  = oldGen[id];
        newGen[id] = neighbors == 3 || (neighbors == 2 && cell); // Fill in the cells

        // Terminating Checkings
        if (newGen[id] != 0) (*allzeros)++;             // Check if all cells are dead
        if (newGen[id] != oldGen[id]) (*change)++;      // Check if life stayed the same

    } 
}


// With the help of shared memory
__global__ void evovle_kernel_shared(int N, char *oldGen, char *newGen, int *allzeros, int *change)
{
	// Global
    int ix = (blockDim.x - 2) * blockIdx.x + threadIdx.x;       //Different indexing as we declared more blocks (see SideGrid)
    int iy = (blockDim.y - 2) * blockIdx.y + threadIdx.y;
    int id = ix * (N+2) + iy;
 
    int i = threadIdx.x;
    int j = threadIdx.y;
    int neighbors;
 
    // Declare the shared memory on a per block level
    __shared__ char oldGen_shared[BLOCK_SIZE][BLOCK_SIZE];
 
    // Copy cells into shared memory
    if (ix <= N+1 && iy <= N+1)
        oldGen_shared[i][j] = oldGen[id];           //Copy each cell and in the sides of shared array the blocks' neighbors
 
    // Sync threads on block
    __syncthreads();
 
    if (ix <= N && iy <= N) {
        if(i != 0 && i != (blockDim.y-1) && j != 0 && j != (blockDim.x-1)) {

            // Get the number of neighbors for a given oldGen point
            neighbors = oldGen_shared[i+1][j] + oldGen_shared[i-1][j]         //lower upper
                    + oldGen_shared[i][j+1] + oldGen_shared[i][j-1]           //right left
                    + oldGen_shared[i+1][j+1] + oldGen_shared[i-1][j-1]       //diagonals
                    + oldGen_shared[i-1][j+1] + oldGen_shared[i+1][j-1];
 
            char cell  = oldGen_shared[i][j];
			newGen[id] = neighbors == 3 || (neighbors == 2 && cell); // Fill in  the cells

            // Terminating Checkings
            if (newGen[id] != 0) (*allzeros)++;        // Check if all cells are dead
            if (newGen[id] != oldGen[id]) (*change)++; // Check if life stayed the same
        }
    }
}



int main(int argc, char* argv[])
{    
    int i, j;
    int N;              // Dimension of cells 
    int generations;    // Generations of evolution
    FILE *fp = NULL;    // A file for input (optional)
    int shared = 0;     // Use share memory or not  
    int output = 0;     // Print the array in every generation, at the end or not at all
    int periodicity = 1;   // Choose if we want the calculate the periodicity of side cells in cpu or gpu
    int doom = 0 ; 		// With terminal checking or Not 

    /*Read the arguments*/
    for (i = 0; i < argc; i++){
        if (!strcmp(argv[i], "-n")) N = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-g")) generations = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-i")) fp = fopen(argv[++i], "r");
        else if (!strcmp(argv[i], "-s")) shared = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-p")) periodicity = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-o1")) output = 1;
        else if (!strcmp(argv[i], "-o2")) output = 2;
        else if (!strcmp(argv[i], "-d")) doom = atoi(argv[++i]);
    }

    // Definitions of one dimension arrays on host and device
    // Actually are a 2D size array but we declare them 1D as we want contiguous memory allocation
    char* h_cells; // our results will be copied on CPU cells
    char* d_old;   // Device 2D cells for oldGen cells
    char* d_new;   // Device 2D cells for new generations cells
    char* d_Swap;  // Swap cells just like game_mpi
    
    // Allocation of host cells, we allocate more byte space [(N+2)^2], to retain our periodicity of cells
    int bytes = sizeof(char)*(N+2)*(N+2);
    h_cells   = (char*)malloc(bytes);
 
    // If we don't have a file, fill it with mighty randomness
    if (fp == NULL)
    {
        srand(time(NULL));
        for(i = 1; i<=N; i++) {
            for(j = 1; j<=N; j++) {
                h_cells[i*(N+2)+j] = rand() % 2;
            }
        }
    }
    else // fill the cells from file
    {
        /*Read from input file the position of initial live cells (if there is input file)*/
        if (fp != NULL){
            char line[BUFSIZE], *token, delim[2] = " ";
            fgets(line, BUFSIZE, fp);
            while (!feof(fp)){                  //Till the end of the file read from it
                token = strtok(line, delim);
                i = atoi(token);
                token = strtok(NULL, delim);
                j = atoi(token);
                h_cells[i*(N+2)+j] = 1;         //Assign a live cell in the coordinates given
                fgets(line, BUFSIZE, fp);
            }
            fclose(fp); //Close the file
        }
    }

    // Start Timer After Initialising The Array
    cudaEvent_t event1, event2;
    cudaEventCreate(&event1);
	cudaEventCreate(&event2);

	// Start
	cudaEventRecord(event1, 0); //where 0 is the default stream

    // Allocate device arrays on GPU memory
    cudaMalloc(&d_old, bytes);
    cudaMalloc(&d_new, bytes);

    int *h_allzeros, *h_change;
    int *d_allzeros, *d_change;
    h_allzeros = (int*)malloc(sizeof(int));
    h_change   = (int*)malloc(sizeof(int));
    cudaMalloc(&d_allzeros, sizeof(int));
    cudaMalloc(&d_change, sizeof(int));

    // Let's fill our device cells 
    cudaMemcpy(d_old, h_cells, bytes, cudaMemcpyHostToDevice);
 
    // Set the prefferes cache configuration for the device function if we want to use shared memory
    if (shared == 1) cudaFuncSetCacheConfig(evovle_kernel_shared, cudaFuncCachePreferShared);

    // Find the Blocks each side of Grid has (e.g For N = 128 , we will need 8 blocks on each side of the grid)
    int SideGrid;
    if (shared)
        SideGrid = (int)ceil(N/(float)(BLOCK_SIZE-2));       //For easier copy in shared memory, we declare more blocks per side of Grid
    else
        SideGrid = (int)ceil(N/(float)BLOCK_SIZE);

 	// For the evolution kernel we specify a two dimensional block size , 16x16 size , 256 threads 
    dim3 blockSize(BLOCK_SIZE, BLOCK_SIZE, 1);
    
    // Create a 2D Grid , to hold our blocks
    // e.g. N = 128 , we will call four 8x8 grid = 64 blocks , 64 x 256 = 16384 threads for the N x N = 16384 cells
    dim3 gridSize(SideGrid, SideGrid, 1);


    // Start Evolutioning for given generations
    for (i = 0; i < generations; i++)
    {
        // Print the state of our cells (if told so in command line flags)
        if (output == 2 || (output == 1 && i == (generations-1)))
        {
            cudaMemcpy(h_cells, d_old, bytes, cudaMemcpyDeviceToHost);
            int r, c;
            printf("\n///////////////////////////////////////////////////\n\n");
            for (r = 1; r <= N; r++){
                for (c = 1; c <= N; c++){
                    if (h_cells[r*(N+2)+c] == 0) printf("-");
                    else if (h_cells[r*(N+2)+c] == 1) printf("X");
                    else printf("?");
                }
                printf("\n");
            }
        }

        if (i != (generations-1))
        {
            if ((doom == 1) && ((i%10) == 0) && (i != 0)){
                *h_allzeros = 0;
                *h_change   = 0;
                cudaMemcpy(d_allzeros, h_allzeros, sizeof(int), cudaMemcpyHostToDevice);
                cudaMemcpy(d_change, h_change, sizeof(int), cudaMemcpyHostToDevice);
            }

            if (periodicity == 0){                                       // Calculate periodicity on cpu (faster processor but expensive memcopy)
                cudaMemcpy(h_cells, d_old, bytes, cudaMemcpyDeviceToHost);
                periodicityPreservationCPU(N, h_cells);                      // CPU is better if N is small (due to for loop) (CPU-GHz GPU-MHz)
                cudaMemcpy(d_old, h_cells, bytes, cudaMemcpyHostToDevice);   // GPU is better in big numbers as we avoid transfer data (memcopy)
            }
            else periodicityPreservationGPU<<<1,1>>>(N, d_old);    // else on gpu (no memcopy on cpu but much slower gpu processor)

            // Evolution of the cells, using shared memory in gpu or not
            if (shared)
                evovle_kernel_shared<<<gridSize, blockSize>>>(N, d_old, d_new, d_allzeros, d_change);
            else
                evovle_kernel<<<gridSize, blockSize>>>(N, d_old, d_new, d_allzeros, d_change);

            if ((doom == 1) && ((i%10) == 0) && (i != 0)){
                cudaMemcpy(h_allzeros, d_allzeros, sizeof(int), cudaMemcpyDeviceToHost);
                cudaMemcpy(h_change, d_change, sizeof(int), cudaMemcpyDeviceToHost);
                //printf("Zeros:%d Change:%d\n", (*h_allzeros), (*h_change));
                if ((*h_change) == 0 || (*h_allzeros) == 0){
                    printf("Program terminated (nothing changed or all extinguisted in this generation)\n");
                    break;
                }
            }

            // Swap our grids and proceed to next generation
            d_Swap = d_old;
            d_old  = d_new;
            d_new  = d_Swap;
        }
    }

    // Copy back results and sum
    cudaMemcpy(h_cells, d_old, bytes, cudaMemcpyDeviceToHost);
 
    // Release memory
    cudaFree(d_allzeros);
    cudaFree(d_change);
    cudaFree(d_old);
    cudaFree(d_new);
    free(h_cells);
    free(h_change);
    free(h_allzeros);

    // Stop The Timer
    cudaEventRecord(event2, 0);
 
 	// Calculate Elapsed Time
    //synchronize
	cudaEventSynchronize(event1); //optional
	cudaEventSynchronize(event2); //wait for the event to be executed!

	//calculate time
	float dt_ms;
	cudaEventElapsedTime(&dt_ms, event1, event2);

    printf("--------------------------------------------------------------\n");
    printf("Runtime %f \n", dt_ms/1000);
    printf("--------------------------------------------------------------\n");

    return 0;
}