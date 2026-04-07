#!/bin/bash

source /opt/intel/oneapi/setvars.sh
export I_MPI_FABRICS=shm:ofi
export I_MPI_OFI_PROVIDER=tcp

export I_MPI_DEBUG=5
export I_MPI_PIN=1
export I_MPI_PIN_CELL=core
export I_MPI_PIN_DOMAIN=omp:compact
export I_MPI_PIN_ORDER="scatter"

export I_MPI_THREAD_LEVEL=funneled

export OMP_NUM_THREADS=4
#export OMP_PLACES=cores
export KMP_AFFINITY=granularity=core,compact


hostname
#mpirun -hostfile ${PBS_NODEFILE} -np 8 -ppn 2 ../Phase3.2/turbine_intel.exe
mpirun -n 4 ./main_intel.exe 2>&1 | tee data_intel.out
#vtune -collect hotspots mpirun -hostfile ${PBS_NODEFILE} -np 8 -ppn 2 ../Phase4/turbine_intel.exe
