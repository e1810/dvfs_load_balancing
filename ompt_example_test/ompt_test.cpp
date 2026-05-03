#include "ompt_test.hpp"
#include "../msr_freq.hpp"

#include <cstdio>
#include <chrono>
#include <vector>
#include <unordered_map>
#include <omp.h>

namespace {

static bool debug = false;

struct RegionState {
    const void* codeptr_ra;
    unsigned int begin_count;
    int nthreads;
    std::vector<double> elapsed_ms;
    std::vector<unsigned int> target_mhz;
    std::vector<int> cpu_map;
    std::chrono::steady_clock::time_point start_time;

    RegionState(const omptool::ParallelRegionBeginEvent& ev)
        : codeptr_ra(ev.codeptr_ra), begin_count(0) {
        nthreads = (ev.requested_parallelism > 0) ? static_cast<std::size_t>(ev.requested_parallelism) : static_cast<std::size_t>(omp_get_max_threads());
        elapsed_ms.assign(nthreads, 0.0);
        target_mhz.assign(nthreads, 0U);
        cpu_map.assign(nthreads, -1);
    }

    void balance_freq() {
        if(debug) {
            std::fprintf(stderr, "[OMPT] balancing frequencies for region [%p] (begin_count=%u thread_count=%d)\n",
                        codeptr_ra, begin_count, nthreads);
        }

        int max_mhz = 4200;
        double max_elapsed = -1;
        for (int i = 0; i < nthreads; i++) max_elapsed = std::max(max_elapsed, elapsed_ms[i]);
        for (int i = 0; i < nthreads; i++) {
            target_mhz[i] = max_mhz * (elapsed_ms[i] / max_elapsed);
            if(debug) {
                std::fprintf(stderr, "\t(Thread %d: %u MHz previous elapsed: %.6f ms)\n",
                            i, target_mhz[i], elapsed_ms[i]);
            }
        }
    
        for (int tid = 0; tid < nthreads; tid++) {
            msr::set_freq_on_cpu(cpu_map[tid], target_mhz[tid], 100);
        }
    }
};

static std::unordered_map<const void*, RegionState> region_states;

}  // namespace



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

    auto iter = region_states.find(event.codeptr_ra);
    if (iter == region_states.end()) {
        iter = region_states.emplace(event.codeptr_ra, RegionState(event)).first;
    }
    auto* rs = &iter->second;

    rs->begin_count += 1;
    rs->start_time = std::chrono::steady_clock::now();
    parallel_data->ptr = rs;

    if(rs->begin_count >= 2) {
        rs->balance_freq();
    }

    if (debug) {
        std::fprintf(stderr, "[OMPT] begin region  [%p] (begin_count=%u thread_count=%d)\n",
                    event.codeptr_ra, rs->begin_count, rs->nthreads);
    }
}


void dispatch_barrier_wait(ompt_sync_region_t kind,
                          ompt_scope_endpoint_t endpoint,
                          ompt_data_t* parallel_data,
                          ompt_data_t* task_data,
                          const void* codeptr_ra) {
    (void)kind; (void)task_data; (void)codeptr_ra;
    if (endpoint != ompt_scope_begin) return;
    if (!parallel_data || !parallel_data->ptr) {
        std::fprintf(stderr, "[OMPT] parallel_data is not registered\n");
        return;
    }

    auto* rs = static_cast<RegionState*>(parallel_data->ptr);
    if (!rs) {
        std::fprintf(stderr, "[OMPT] RegionState is not found\n");
        return;
    }

    int tid = omp_get_thread_num();
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(now - rs->start_time);
    rs->elapsed_ms[static_cast<std::size_t>(tid)] = elapsed.count();

    if(rs->begin_count == 1) rs->cpu_map[static_cast<std::size_t>(tid)] = msr::current_cpu();
}


void dispatch_parallel_end(ompt_data_t* parallel_data,
                           ompt_data_t* task_data,
                           int flags,
                           const void* codeptr_ra) {
    (void)task_data; (void)flags; (void)codeptr_ra;
    if (debug) std::fprintf(stderr, "[OMPT] end region  [%p]\n", codeptr_ra);
    
    if (!parallel_data || !parallel_data->ptr) {
        std::fprintf(stderr, "[OMPT] parallel_data is not registered\n");
        return;
    }

    auto* rs = static_cast<RegionState*>(parallel_data->ptr);
    if (!rs) {
        std::fprintf(stderr, "[OMPT] RegionState is not found\n");
        return;
    }

    if(!debug) return;
    for (int i = 0; i < rs->nthreads; i++) {
        std::fprintf(stderr, "\t(thread %d: %.6f ms)\n",
                        i, rs->elapsed_ms[i]);
    }
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
    set_callback(ompt_callback_parallel_end,
                 reinterpret_cast<ompt_callback_t>(&dispatch_parallel_end));
    set_callback(ompt_callback_sync_region,
                 reinterpret_cast<ompt_callback_t>(&dispatch_barrier_wait));
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