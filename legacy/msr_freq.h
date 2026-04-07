#ifndef MSR_FREQ_H
#define MSR_FREQ_H

#ifdef __cplusplus
extern "C" {
#endif

int msr_current_cpu_(void);
void msr_sample_(unsigned long long *aperf, unsigned long long *mperf);
double msr_compute_freq_mhz_(double *base_mhz,
                             unsigned long long *aperf1,
                             unsigned long long *mperf1,
                             unsigned long long *aperf2,
                             unsigned long long *mperf2);
int msr_set_freq_on_cpu_(int *cpu, double *freq_mhz, double *bus_mhz);
int msr_set_freq_multi_(int *cpu_list, int *num_cpus,
                        double *freq_mhz, double *bus_mhz);
int msr_set_freq_(double *freq_mhz, double *bus_mhz);
int sysfs_set_freq_on_cpu_(int *cpu, int *freq_khz);

#ifdef __cplusplus
}
#endif

#endif
