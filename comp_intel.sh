source /opt/intel/oneapi/setvars.sh

mpiicpx -g -qopenmp -O3 -march=native -o main_intel.exe \
	iteration_dvfs.cpp msr_freq.cpp