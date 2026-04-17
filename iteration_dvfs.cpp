#include<mpi.h>
#include<omp.h>
#include<cstdio>
#include "msr_freq.hpp"
#include "power_meter.hpp"


const int ITER_COUNT = 5;
const int LOOPSIZE = 1e9;
const int BUFSIZE = 1e9;

void trans_sdrc(int ist, int ied, int buf[], int rank, int num_ranks) {
    int req0, req1;
    MPI_Isend(buf, BUFSIZE, MPI_INT, (rank+1)%num_ranks, 0, MPI_COMM_WORLD, &req0);
    MPI_Irecv(buf, BUFSIZE, MPI_INT, (rank-1+num_ranks)%num_ranks, 0, MPI_COMM_WORLD, &req1);
    MPI_Wait(&req0, MPI_STATUS_IGNORE);
    MPI_Wait(&req1, MPI_STATUS_IGNORE);
}

void divloop(int loopsize, int *ist, int *ied, int numth) {
    ist[0] = 0; ied[0] = 0;
    for(int i=1; i<numth; i++) {
        if(i>0) ist[i] = ied[i-1];
        ied[i] = ist[i] + loopsize/(numth-1);
    }
}

int main(int argc, char **argv) {

    MPI_Init(&argc, &argv);
    int rank, num_ranks;
    MPI_Comm_size(MPI_COMM_WORLD, &num_ranks);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    int numth = omp_get_max_threads();

    int *comm_buf = new int[BUFSIZE];
    int *comp_buf = new int[BUFSIZE];
    double time_th_start[numth], time_th_end[numth];
    double time_th_sum[numth];
    for(int i=0; i<numth; i++) time_th_sum[i] = 0.0;
    int *ist = new int[numth];
    int *ied = new int[numth];
    divloop(LOOPSIZE, ist, ied, numth);


    // start iteration
    for(int iter=0; iter<ITER_COUNT; iter++) {
        MPI_Barrier(MPI_COMM_WORLD);
        // start iteration level measurement
        power::EnergyMeter energy_meter;
        bool energy_started = energy_meter.start();
        double time_iter_start = MPI_Wtime();


    if(rank == 0) {
        printf("\nThreads workloads\n");
        for(int i=0; i<numth; i++) printf("%d ", ied[i]-ist[i]);
        printf("\n");
    }

    // threads setting
    int ith;
    double target_mhz[numth], freq_mhz[numth];
    for(int i=0; i<numth; i++) {
        if(rank < 2) target_mhz[i] = 4800;
        else target_mhz[i] = 4800;
    }
    target_mhz[0] = 1000;
    double bus_mhz = 100.0, base_mhz = 2496.0;
    int cpu_id;
    msr::CounterSample sample1, sample2;

    if(rank == 0) {
        printf("\nTarget frequencies\n");
        for(int i=0; i<numth; i++) printf("%.0f ", target_mhz[i]);
        printf("\nThreads times\n");
    }

    // start thread parallel region
    #pragma omp parallel private(ith) \
        private(cpu_id,sample1,sample2)
    {
        
        ith = omp_get_thread_num();
        time_th_start[ith] = omp_get_wtime();
        cpu_id = msr::current_cpu();
        msr::set_freq_on_cpu(cpu_id, target_mhz[ith], bus_mhz);
        sample1 = msr::sample_on_cpu(cpu_id);

        #pragma omp master
        {
            //msr::set_freq_on_cpu(msr::current_cpu(), 800, 100);
            trans_sdrc(ist[ith], ied[ith], comm_buf, rank, num_ranks);
            //msr::set_freq_on_cpu(msr::current_cpu(), 0, 100);

        }
        
        for(int i=ist[ith]; i<ied[ith]; i++) {
            comp_buf[i] = i+iter;
            for(int j=0; j<5000; j++) {
                if(j%2) comp_buf[i]++;
                else comp_buf[i]--;
            }
        }

        sample2 = msr::sample_on_cpu(cpu_id);
        freq_mhz[ith] = msr::compute_freq_mhz(base_mhz, sample1, sample2);
        time_th_end[ith] = omp_get_wtime();
        msr::set_freq_on_cpu(cpu_id, 0, 100);
    }// omp end parallel


    // output thread metrics
    {
        for(int i=0; i<numth; i++) {
            printf("Rank %d, Thread %d [%.0f MHz]: %.0f ms\n", 
                rank, i, freq_mhz[i], (time_th_end[i] - time_th_start[i])*1000);
            time_th_sum[i] += time_th_end[i] - time_th_start[i];
        }
    }

    
    // stop iteration level measurement
    double time_iter_end = MPI_Wtime();
    printf("Rank %d, Iteration %d: %f ms\n", rank, iter, (time_iter_end - time_iter_start)*1000);
    energy_meter.stop();
    double energy_j = energy_meter.consumed_joule();
    printf("Rank %d, Iteration %d: %f J, %f W\n",
        rank, iter, energy_j, energy_j / (time_iter_end - time_iter_start));
    fflush(stdout);


    }// end of iterations



    for(int i=0; i<numth; i++) {
        printf("sum of Rank %d, Thread %d: %f ms\n", rank, i, time_th_sum[i]);
    }

    #pragma omp parallel
    msr::set_freq_on_cpu(msr::current_cpu(), 0, 100);
    delete[] comm_buf;
    delete[] comp_buf;
    delete[] ist;
    delete[] ied;
    MPI_Finalize();
    return 0;
} 
 