source /opt/intel/oneapi/setvars.sh

icpx -std=c++17 -O2 -fPIC -shared -qopenmp -o libompt_test.so \
	ompt_test.cpp

icpx -std=c++17 -O2 -fno-unroll-loops -qopenmp -o ompt_example \
	ompt_example.cpp


export OMP_NUM_THREADS=4
OMP_TOOL=enabled OMP_TOOL_LIBRARIES=$PWD/libompt_test.so ./ompt_example