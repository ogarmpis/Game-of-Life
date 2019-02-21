#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>


int main(int argc, char const *argv[])
{
	int  i, j, N, random, cell;
	FILE *fp;

	for (i = 0; i < argc; i++){
		if (!strcmp(argv[i], "-n")) N = atoi(argv[++i]);
		else if (!strcmp(argv[i], "-f")) fp = fopen(argv[++i], "w");
		else if (!strcmp(argv[i], "-r")) random = atoi(argv[++i]);
	}

	srand(time(NULL));

	for (i = 0; i < N; i++)
	{
		for (j = 0; j < N; j++)
		{
			if (!random) cell = 0;
			else cell = rand() % 2;

			if (j != N-1) fprintf(fp, "%d ", cell);
			else fprintf(fp, "%d", cell);
		}
		fprintf(fp, "\n");
	}
	printf("Done!\n");

	return 0;
}