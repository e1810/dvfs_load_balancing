#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sched.h>
#include <string.h>

// Minimal MSR helper API:
//  - msr_sample_(aperf*, mperf*) : read APERF/MPERF on current CPU
//  - msr_compute_freq_mhz_(ref_mhz, a1, m1, a2, m2) : compute MHz
//  - msr_current_cpu_() : return current CPU id (for logging if desired)

// Keep MSR file descriptors open for fast access
#define MAX_CPUS 32
static int msr_fd_read[MAX_CPUS];
static int msr_fd_write[MAX_CPUS];
static int msr_initialized = 0;

// Initialize MSR access (open all MSR devices)
static void init_msr_access() {
    if (msr_initialized) return;
    
    for (int cpu = 0; cpu < MAX_CPUS; cpu++) {
        msr_fd_read[cpu] = -1;
        msr_fd_write[cpu] = -1;
        
        char path[64];
        snprintf(path, sizeof(path), "/dev/cpu/%d/msr", cpu);
        
        // Try to open for reading
        msr_fd_read[cpu] = open(path, O_RDONLY);
        
        // Try to open for writing (needs root)
        msr_fd_write[cpu] = open(path, O_WRONLY);
    }
    
    msr_initialized = 1;
}

static int read_msr_on_cpu(int cpu, uint32_t msr, uint64_t *value) {
    if (!msr_initialized) init_msr_access();
    
    if (cpu < 0 || cpu >= MAX_CPUS || msr_fd_read[cpu] < 0) return -1;
    
    ssize_t n = pread(msr_fd_read[cpu], value, sizeof(uint64_t), (off_t)msr);
    if (n != sizeof(uint64_t)) return -2;
    return 0;
}

static int write_msr_on_cpu(int cpu, uint32_t msr, uint64_t value) {
    if (!msr_initialized) init_msr_access();
    
    if (cpu < 0 || cpu >= MAX_CPUS || msr_fd_write[cpu] < 0) return -1;
    
    ssize_t n = pwrite(msr_fd_write[cpu], &value, sizeof(uint64_t), (off_t)msr);
    if (n != sizeof(uint64_t)) return -2;
    return 0;
}

// Return current CPU/core id
int msr_current_cpu_() {
    int cpu = sched_getcpu();
    if (cpu < 0) return -1;
    return cpu;
}

// Read APERF (0xE8) and MPERF (0xE7) for the current CPU
void msr_sample_(unsigned long long *aperf, unsigned long long *mperf) {
    int cpu = sched_getcpu();
    uint64_t a = 0, m = 0;
    if (cpu < 0) { *aperf = 0ULL; *mperf = 0ULL; return; }
    if (read_msr_on_cpu(cpu, 0xE8, &a) != 0) a = 0ULL;
    if (read_msr_on_cpu(cpu, 0xE7, &m) != 0) m = 0ULL;
    *aperf = (unsigned long long)a;
    *mperf = (unsigned long long)m;
}

// Compute frequency (MHz) given two APERF/MPERF samples and base clock (MHz)
// base_mhz is the base/reference clock (e.g., 1500 MHz for this platform)
// Frequency = base_mhz * (APERF_delta / MPERF_delta)
double msr_compute_freq_mhz_(double *base_mhz,
                             unsigned long long *aperf1,
                             unsigned long long *mperf1,
                             unsigned long long *aperf2,
                             unsigned long long *mperf2) {
    unsigned long long a1 = *aperf1, a2 = *aperf2;
    unsigned long long m1 = *mperf1, m2 = *mperf2;
    if (a2 <= a1 || m2 <= m1) return -1.0;
    double r = (double)(a2 - a1) / (double)(m2 - m1);
    return (*base_mhz) * r;
}

// Set CPU frequency by writing to IA32_PERF_CTL MSR (0x199)
// freq_mhz: target frequency in MHz (e.g., 1500, 2100, 4300)
// bus_mhz: base bus frequency (typically 100 MHz for modern Intel CPUs)
// Returns 0 on success, -1 on error
// Note: Requires root privileges and msr module loaded
int msr_set_freq_on_cpu_(int *cpu, double *freq_mhz, double *bus_mhz) {
    int target_cpu = *cpu;
    double target_freq = *freq_mhz;
    double bus_freq = *bus_mhz;
    
    // Read current HWP capabilities to determine max ratio for this CPU
    uint64_t hwp_cap = 0;
    int max_ratio = -1;
    
    if (read_msr_on_cpu(target_cpu, 0x771, &hwp_cap) == 0) {
        // MSR 0x771 = IA32_HWP_CAPABILITIES
        max_ratio = (int)(hwp_cap & 0xFF);
    }
    
    // If freq_mhz == 0, set to maximum frequency for this core
    if (target_freq == 0.0 && max_ratio > 0) {
        // Use max_ratio directly
        int ratio = max_ratio;
        
        uint64_t hwp_value = 0;
        if (read_msr_on_cpu(target_cpu, 0x774, &hwp_value) == 0) {
            uint64_t new_hwp = (ratio & 0xFF) |           // Min = Max
                               ((ratio & 0xFF) << 8) |     // Max
                               ((ratio & 0xFF) << 16) |    // Desired = Max
                               (0x80ULL << 24);            // EPP = 128 (balance)
            int ret = write_msr_on_cpu(target_cpu, 0x774, new_hwp);
            if (ret == 0) {
                return 0;  // Success with HWP
            }
        }
        return -1;
    }
    
    // Calculate P-state ratio (multiplier)
    // For Intel CPUs: frequency = bus_freq * ratio
    int ratio = (int)(target_freq / bus_freq + 0.5);
    
    // If we have max_ratio, use it to calculate correct ratio
    if (max_ratio > 0) {
        double estimated_bus = (max_ratio >= 50) ? 5300.0 / max_ratio : 4200.0 / max_ratio;
        ratio = (int)(target_freq / estimated_bus + 0.5);
    }
    
    // Clamp ratio to reasonable range
    if (ratio < 8) ratio = 8;
    if (max_ratio > 0 && ratio > max_ratio) ratio = max_ratio;
    else if (ratio > 55) ratio = 55;
    
    // Try HWP (Hardware P-states) first - MSR 0x774 (IA32_HWP_REQUEST)
    // HWP is used by intel_pstate driver
    uint64_t hwp_value = 0;
    if (read_msr_on_cpu(target_cpu, 0x774, &hwp_value) == 0) {
        // HWP is available, use it
        // Set min, max, and desired to the target ratio
        uint64_t new_hwp = (ratio & 0xFF) |           // Min
                           ((ratio & 0xFF) << 8) |     // Max
                           ((ratio & 0xFF) << 16) |    // Desired
                           (0x80ULL << 24);            // EPP = 128 (balance)
        int ret = write_msr_on_cpu(target_cpu, 0x774, new_hwp);
        if (ret == 0) {
            return 0;  // Success with HWP
        }
    }
    
    // Fallback to IA32_PERF_CTL (0x199) for older systems
    // IA32_PERF_CTL format: bits [15:8] contain the target P-state ratio
    uint64_t value = ((uint64_t)ratio << 8);
    
    // Write to MSR 0x199 (IA32_PERF_CTL)
    int ret = write_msr_on_cpu(target_cpu, 0x199, value);
    
    if (ret != 0) {
        fprintf(stderr, "Failed to write MSR 0x199 on CPU %d (ret=%d)\n", 
                target_cpu, ret);
        if (msr_fd_write[target_cpu] < 0) {
            fprintf(stderr, "MSR write fd not open (needs root privileges)\n");
        }
        return -1;
    }
    
    return 0;
}

// Fast version: set frequency on multiple CPUs at once
// cpu_list: array of CPU IDs
// num_cpus: number of CPUs in the list
// freq_mhz: target frequency in MHz
// bus_mhz: base bus frequency
int msr_set_freq_multi_(int *cpu_list, int *num_cpus, 
                        double *freq_mhz, double *bus_mhz) {
    int n = *num_cpus;
    int failures = 0;
    
    for (int i = 0; i < n; i++) {
        if (msr_set_freq_on_cpu_(&cpu_list[i], freq_mhz, bus_mhz) != 0) {
            failures++;
        }
    }
    
    return failures == 0 ? 0 : -1;
}

// Set frequency for current CPU
int msr_set_freq_(double *freq_mhz, double *bus_mhz) {
    int cpu = sched_getcpu();
    if (cpu < 0) return -1;
    return msr_set_freq_on_cpu_(&cpu, freq_mhz, bus_mhz);
}

// Set CPU frequency via sysfs (alternative to MSR, more reliable)
// freq_khz: target frequency in kHz
// Returns 0 on success, -1 on error
int sysfs_set_freq_on_cpu_(int *cpu, int *freq_khz) {
    char path[128];
    int target_cpu = *cpu;
    int target_freq = *freq_khz;
    
    snprintf(path, sizeof(path), 
             "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_setspeed", 
             target_cpu);
    
    FILE *fp = fopen(path, "w");
    if (!fp) {
        perror("Failed to open scaling_setspeed");
        return -1;
    }
    
    if (fprintf(fp, "%d\n", target_freq) < 0) {
        fclose(fp);
        return -1;
    }
    
    fclose(fp);
    return 0;
}
