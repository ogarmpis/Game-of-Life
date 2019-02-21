#ifndef __functions__
#define __functions__


char **allocateArray(int n, int init_flag);
void deleteArray(char ***array);
void show(char **cells, int N);
char is_doomsday(char **old_gen, char **new_gen, int N);
void evolve_sides(char **old_gen, char **new_gen, char **border, int N, int *allzeros, int *change);
void evolve_inner(char **old_gen, char **new_gen, int N, int *allzeros, int *change);


#endif