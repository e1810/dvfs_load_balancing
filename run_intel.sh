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
start_energy_uj=$(cat /sys/class/powercap/intel-rapl:0/energy_uj)
start_time=$(date +%s.%N)

mpirun -n 4 ./main_intel.exe 2>&1 | tee data_intel.out
mpi_status=${PIPESTATUS[0]}

end_time=$(date +%s.%N)
end_energy_uj=$(cat /sys/class/powercap/intel-rapl:0/energy_uj)



energy_j=$(awk -v s="$start_energy_uj" -v e="$end_energy_uj" 'BEGIN{printf "%.6f", (e-s)/1000000.0}')
avg_w=$(awk -v ej="$energy_j" -v ts="$start_time" -v te="$end_time" 'BEGIN{dt=te-ts; if(dt>0) printf "%.6f", ej/dt; else print "N/A"}')

printf "\nExecution time: %f seconds\n" "$(awk -v s="$start_time" -v e="$end_time" 'BEGIN{printf "%.6f", e-s}')" | tee -a data_intel.out
printf "Whole-program energy: %s J\n" "$energy_j" | tee -a data_intel.out
printf "Whole-program average power: %s W\n" "$avg_w" | tee -a data_intel.out

exit "$mpi_status"