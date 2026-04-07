#include<mpi.h>
#include<omp.h>

void trans_sdrc(int ist, int ied, int buf[], int rank, int num_ranks) {
    int req0, req1;
    // Send/receive full LOOPSIZE worth of data, not thread-specific size
    const int LOOPSIZE = 1e9;
    MPI_Isend(buf, LOOPSIZE, MPI_INT, (rank+1)%num_ranks, 0, MPI_COMM_WORLD, &req0);
    MPI_Irecv(buf, LOOPSIZE, MPI_INT, (rank-1+num_ranks)%num_ranks, 0, MPI_COMM_WORLD, &req1);
    MPI_Wait(&req0, MPI_STATUS_IGNORE);
    MPI_Wait(&req1, MPI_STATUS_IGNORE);
}


int main(int argc, char **argv) {
    const int LOOPSIZE = 1e9;
    const int BUFSIZE = 1e9;

    MPI_Init(&argc, &argv);
    int rank, num_ranks;
    MPI_Comm_size(MPI_COMM_WORLD, &num_ranks);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    int numth = omp_get_max_threads();

    int *buf = new int[BUFSIZE];
    double time_th_start[numth], time_th_end[numth];
    double time_th_sum[numth];
    int *ist = new int[numth];
    int *ied = new int[numth];
    ist[0] = 0; ied[0] = LOOPSIZE/10;
    for(int i=1; i<numth; i++) {
        if(i>0) ist[i] = ied[i-1];
        ied[i] = ist[i] + LOOPSIZE/10*3;
    }



    for(int iter=0; iter<1000; iter++) {

    if(rank == 0) {
        printf("Threads workloads\n");
        for(int i=0; i<numth; i++) printf("%d ", ied[i]-ist[i]);
        printf("\n");
        printf("Threads times\n");
    }

    int ith;
    #pragma omp parallel private(ith)
    {
        ith = omp_get_thread_num();
        time_th_start[ith] = omp_get_wtime();

        #pragma omp master
        {
            trans_sdrc(ist[ith], ied[ith], buf, rank, num_ranks);
        }
        
        for(int i=ist[ith]; i<ied[ith]; i++) {
            buf[i] = i;
        }

        time_th_end[ith] = omp_get_wtime();
    }// omp end parallel

    MPI_Barrier(MPI_COMM_WORLD);

    if(rank == 2) {
        for(int i=0; i<numth; i++) {
            printf("Thread %d: %f ms\n", 
                i, (time_th_end[i] - time_th_start[i])*1000);
            time_th_sum[i] += time_th_end[i] - time_th_start[i];
        }
    }

    }
    for(int i=0; i<numth; i++) {
        printf("sum of Thread %d: %f ms\n", i, time_th_sum[i]);
    }

    delete[] buf;
    delete[] ist;
    delete[] ied;
    MPI_Finalize();
    return 0;
} 
 