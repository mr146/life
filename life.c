#include <sys/time.h>
#include <stdio.h>
#include <mpi.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define ALIVE 'X'
#define DEAD '.'

int toindex(int row, int col, int N) {
    if (col < 0) {
        col = col + N;
    } else if (col >= N) {
        col = col - N;
    }
    return row * N + col;
}

void printgrid(char* grid, char* buf, FILE* f, int N) {
    int i;
    for (i = 0; i < N; ++i) {
        strncpy(buf, grid + i * N + N, N);
        buf[N] = 0;
        fprintf(f, "%s\n", buf);
    }
}

void master(int size, int argc, char* argv[])
{
//    fprintf(stderr, "i am master");
//    return;
    if (argc != 5) {
        fprintf(stderr, "Usage: %s N input_file iterations output_file\n", argv[0]);
        return;
    }
    int N = atoi(argv[1]); // grid size
    int iterations = atoi(argv[3]);
    int* lines = (int*) malloc(size * sizeof(int));
    int i;
    for (i = 0; i < size; i++)
    {
        lines[i] = N / size;
        if(i < N % size)
            lines[i]++;
    }
    FILE* input = fopen(argv[2], "r");
    char* grid = (char*) malloc((N + 2) * N * sizeof(char));
    char* buf = (char*) malloc(N * N * sizeof(char));
    for (i = 0; i < N; ++i) {
        fscanf(input, "%s", grid + i * N + N);
    }
    fclose(input);
    for(i = 0; i < N; i++)
    {
        grid[i] = grid[N * N + i];
        grid[N * (N + 1) + i] = grid[N + i];
    }

    MPI_Request* reqs = (MPI_Request*)malloc(size * sizeof(MPI_Request));
    MPI_Status* stats = (MPI_Status*)malloc(size * sizeof(MPI_Status));
    int iter;
    struct timeval tval_before;
    gettimeofday(&tval_before, NULL);
    for(iter = 0; iter < iterations; iter++)
    {
        int sent = 0;
        int sl;
        for(sl = 0; sl < size; sl++)
        {
            int cur_pntr = sent * N;
            MPI_Isend(grid + cur_pntr, (lines[sl] + 2) * N, MPI_CHARACTER, sl + 1, MPI_ANY_TAG, MPI_COMM_WORLD, &reqs[sl]);
            sent += lines[sl];
        }
        MPI_Waitall(size, reqs, stats);
        int received = 0;
        for(sl = 0; sl < size; sl++)
        {
            int cur_pntr = received * N;
            MPI_Irecv(grid + cur_pntr, lines[sl] * N, MPI_CHARACTER, sl + 1, MPI_ANY_TAG, MPI_COMM_WORLD, &reqs[sl]);
            received += lines[sl];
        }
        MPI_Waitall(size, reqs, stats);
        int i, j;
        for(i = 0; i < N; i++)
        {
            grid[i] = grid[N * N + i];
            grid[N * (N + 1) + i] = grid[N + i];
        }
    }
    struct timeval tval_after, tval_result;
    gettimeofday(&tval_after, NULL);
    timersub(&tval_after, &tval_before, &tval_result);
    fprintf(stderr, "Took %d ms\n", tval_result.tv_sec * 1000 + tval_result.tv_usec / 100);
    FILE* output = fopen(argv[4], "w");
    printgrid(grid, buf, output, N);
    fclose(output);
    free(grid);
    free(buf);
    free(reqs);
    free(stats);
    free(lines);
    return;
}

void slave(int rank, int size, int argc, char* argv[])
{
 //   fprintf(stderr, "i am slave %d\n", rank);
 //   return;
    if (argc != 5) {
        return;
    }
    int N = atoi(argv[1]); // grid size
    int iterations = atoi(argv[3]);

    int lines = N / size;
    if(rank <= N % size)
        lines++;
    char* buf = (char*) malloc(N * (lines + 2) * sizeof(char));
    char* inc = (char*) malloc(N * (lines + 2) * sizeof(char));
    int iter, i, j, di, dj;
    for(iter = 0; iter < iterations; iter++)
    {
        MPI_Status stat;
        MPI_Request req;
        MPI_Irecv(inc, (lines + 2) * N, MPI_CHARACTER, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &req);
        MPI_Wait(&req, &stat);
        for(i = 1; i <= lines; i++)
            for(j = 0; j < N; j++)
            {
                int alive_count = 0;
                for (di = -1; di <= 1; ++di) {
                    for (dj = -1; dj <= 1; ++dj) {
                        if ((di != 0 || dj != 0) && inc[toindex(i + di, j + dj, N)] == ALIVE) {
                            ++alive_count;
                        }
                    }
                }
                int current = i * N + j;
                if (alive_count == 3 || (alive_count == 2 && inc[current] == ALIVE)) {
                    buf[current] = ALIVE;
                } else {
                    buf[current] = DEAD;
                }
            }
        MPI_Isend(buf + N, lines * N, MPI_CHARACTER, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &req);
        MPI_Wait(&req, &stat);
    }
    free(buf);
    free(inc);
}

int main(int argc, char* argv[]) {
    int rank, size;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    if(rank == 0)
        master(size - 1, argc, argv);
    else
        slave(rank, size - 1, argc, argv);
    MPI_Finalize();
    return 0;
}
