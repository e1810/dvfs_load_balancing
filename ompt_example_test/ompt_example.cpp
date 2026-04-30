#include <omp.h>

#include <cstdio>

int main() {

    for(int i=0; i<10; i++) {
        #pragma omp parallel
        {
            std::printf("region 1: hello from thread %d\n", omp_get_thread_num());
        }
    

        #pragma omp parallel
        {
            std::printf("region 2: hello from thread %d\n", omp_get_thread_num());
        }
    }
    
    return 0;
}