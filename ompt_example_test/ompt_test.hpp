#ifndef OMPT_TOOL_HPP
#define OMPT_TOOL_HPP

#include <Openmp/omp-tools.h>

namespace omptool {

struct ParallelRegionBeginEvent {
    ompt_data_t* encountering_task_data;
    ompt_data_t* parallel_data;
    unsigned int requested_parallelism;
    int flags;
    const void* codeptr_ra;
};

}  // namespace omptool

#endif