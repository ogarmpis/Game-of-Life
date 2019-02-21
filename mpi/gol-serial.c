#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>



/*Allocate memory for a 2D array and initialize it*/
char **allocateArray(int n, int init_flag)
{
	char **array;
	int  i, j;

	array = malloc(n * sizeof(char*));
	for (i = 0; i < n; i++){
		array[i] = malloc(n * sizeof(char));
		for (j = 0; j < n; j++){
			if (init_flag == 0) array[i][j] = 0;			//Initialize the array items as 0s
			else array[i][j] = rand() % 2;
		}
	}
	return array;
}



/*Delete a 2D array which has dynamically allocated*/
void deleteArray(char **array, int n)
{
	int i;
	for (i = 0; i < n; i++)
		free(array[i]);
	free(array);
}



/*Print the cells in command line*/
void show(char **cells, int N)
{
	int i, j;
	printf("\n///////////////////////////////////////////////////\n\n");
	for (i = 0; i < N; i++){
		for (j = 0; j < N; j++){
			if (cells[i][j] == 0) printf("-");
			else if (cells[i][j] == 1) printf("X");
			else printf("?");
		}
		printf("\n");
	}
}



/*The cells evolve - move to next generation*/
void evolve(char **old_gen, char **new_gen, int N)
{
	int i, j, ul, u, ur, l, r, dl, d, dr, neighbors;

	for (i = 0; i < N; i++)
	{
		for (j = 0; j < N; j++)
		{
			ul = old_gen[((i-1)+N)%N][((j-1)+N)%N]; 
			u  = old_gen[((i-1)+N)%N][((j)+N)%N];
			ur = old_gen[((i-1)+N)%N][((j+1)+N)%N];
			l  = old_gen[((i)+N)%N][((j-1)+N)%N];
			r  = old_gen[((i)+N)%N][((j+1)+N)%N];
			dl = old_gen[((i+1)+N)%N][((j-1)+N)%N];
			d  = old_gen[((i+1)+N)%N][((j)+N)%N];
			dr = old_gen[((i+1)+N)%N][((j+1)+N)%N];
			neighbors = u + ur + ul + l + r + dr + dl + d;

			char cell     = old_gen[i][j];
			new_gen[i][j] = neighbors == 3 || (neighbors == 2 && cell);
		}
	}
}



/*Main program*/
int main(int argc, char const *argv[])
{
	int  i, j, q, N, generations;
	char output = 0;
	char **cells, **new_gen, **swap;
	char line[64], *token, delim[2] = " ";
	FILE *fp = NULL;

	/*Read the arguments*/
	for (i = 0; i < argc; i++){
		if (!strcmp(argv[i], "-n")) N = atoi(argv[++i]);
		else if (!strcmp(argv[i], "-g")) generations = atoi(argv[++i]);
		else if (!strcmp(argv[i], "-i")) fp = fopen(argv[++i], "r");
		else if (!strcmp(argv[i], "-o1")) output = 1;
		else if (!strcmp(argv[i], "-o2")) output = 2;
	}

	/*Allocate memory for cells and new_gen arrays and initialize them with 0s or randomly*/
	if (fp == NULL){
		srand(time(NULL));
		cells = allocateArray(N, 1);
	}
	else cells = allocateArray(N, 0);
	new_gen = allocateArray(N, 0);

	/*Read from input file the position of initial live cells (if there is input file)*/
	if (fp != NULL){
		fgets(line, 64, fp);
		while (!feof(fp)){
			token = strtok(line, delim);
			i = atoi(token);
			token = strtok(NULL, delim);
			j = atoi(token);
			cells[i][j] = 1;
			fgets(line, 64, fp);
		}
		fclose(fp);
	}

	clock_t start = clock();							//Begin counting time

	/*Calculate every generation and print it*/
	for (q = 0; q < generations; q++)
	{
		if (output == 2) show(cells, N);		//Print the cells array in every generation if told so in command line (-o2)

		if (q != generations-1){
			evolve(cells, new_gen, N);
			swap    = cells;
			cells   = new_gen;
			new_gen = swap;
		}
	}
	if (output == 1) show(cells, N);			//Print the cells array only at the end (-o1)

	deleteArray(cells, N);
	deleteArray(new_gen, N);

	/*Finish counting time, print it*/
	clock_t finish = clock();
	double alltime = (double)(finish - start) / CLOCKS_PER_SEC;
	printf("\n///////////////////////////////////////////////////\n\n");
	printf("--------------------------------------------------------------\n");
	printf("Runtime %f \n", alltime);
	printf("--------------------------------------------------------------\n");

	return 0;
}