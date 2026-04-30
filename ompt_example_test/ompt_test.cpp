#include "ompt_test.hpp"

#include <cstdio>

void dispatch_parallel_begin(ompt_data_t* encountering_task_data,
                             const ompt_frame_t* encountering_task_frame,
                             ompt_data_t* parallel_data,
                             unsigned int requested_parallelism,
                             int flags,
                             const void* codeptr_ra) {
    (void)encountering_task_frame;

    omptool::ParallelRegionBeginEvent event{
        encountering_task_data,
        parallel_data,
        requested_parallelism,
        flags,
        codeptr_ra,
    };

    std::fprintf(stdout,
                 "[OMPT] codeptr_ra=%p parallel begin: requested=%u flags=%d parallel=%llu\n",
                 event.codeptr_ra,
                 event.requested_parallelism,
                 event.flags,
                 static_cast<unsigned long long>(event.parallel_data ? event.parallel_data->value : 0ULL));
}

int ompt_initialize(ompt_function_lookup_t lookup,
                    int /*initial_device_num*/,
                    ompt_data_t* tool_data) {
    if (tool_data) {
        tool_data->value = 1ULL;
    }

    auto set_callback = reinterpret_cast<ompt_set_callback_t>(lookup("ompt_set_callback"));
    if (!set_callback) {
        std::fprintf(stderr, "[OMPT] ompt_set_callback is unavailable\n");
        return 0;
    }

    set_callback(ompt_callback_parallel_begin,
                 reinterpret_cast<ompt_callback_t>(&dispatch_parallel_begin));
    return 1;
}

void ompt_finalize(ompt_data_t* /*tool_data*/) {
    (void)0;
}

extern "C" {

ompt_start_tool_result_t* ompt_start_tool(unsigned int /*omp_version*/,
                                         const char* /*runtime_version*/) {
    static ompt_start_tool_result_t result = {&ompt_initialize, &ompt_finalize, {0ULL}};
    return &result;
}

}