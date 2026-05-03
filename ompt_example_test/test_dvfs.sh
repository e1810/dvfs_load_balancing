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
export KMP_AFFINITY=granularity=core,compact


icpx -qopenmp -O3 -fPIC -shared  -o libompt_test.so \
	ompt_test.cpp ../msr_freq.cpp

mpiicpx -g -qopenmp -O3 -march=native -o ompt_dvfs_example \
	iteration_dvfs.cpp ../msr_freq.cpp ../power_meter.cpp


export OMP_NUM_THREADS=4
export OMP_TOOL=enabled OMP_TOOL_LIBRARIES=$PWD/libompt_test.so
mpirun -n 2 ./ompt_dvfs_example