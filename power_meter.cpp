#include "power_meter.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include <dirent.h>

namespace {

constexpr const char* kRaplBase = "/sys/class/powercap";

bool is_top_level_rapl_zone(const char* name) {
    if (std::strncmp(name, "intel-rapl:", 11) != 0) return false;
    int colon_count = 0;
    for (const char* p = name; *p != '\0'; ++p) {
        if (*p == ':') ++colon_count;
    }
    return colon_count == 1;
}

bool read_ull_from_file(const std::string& path, std::uint64_t* out) {
    FILE* fp = std::fopen(path.c_str(), "r");
    if (!fp) return false;

    unsigned long long value = 0ULL;
    const int scanned = std::fscanf(fp, "%llu", &value);
    std::fclose(fp);
    if (scanned != 1) return false;

    *out = static_cast<std::uint64_t>(value);
    return true;
}

}  // namespace

namespace power {

EnergySample sample_total_energy() {
    EnergySample sample{0ULL, 0ULL, false};

    DIR* dir = opendir(kRaplBase);
    if (!dir) return sample;

    std::uint64_t total_energy_uj = 0ULL;
    std::uint64_t total_max_range_uj = 0ULL;
    bool found_any = false;

    while (true) {
        dirent* entry = readdir(dir);
        if (!entry) break;

        if (entry->d_name[0] == '.') continue;
        if (!is_top_level_rapl_zone(entry->d_name)) continue;

        const std::string zone_path = std::string(kRaplBase) + "/" + entry->d_name;
        const std::string energy_path = zone_path + "/energy_uj";
        const std::string max_range_path = zone_path + "/max_energy_range_uj";

        std::uint64_t energy_uj = 0ULL;
        std::uint64_t max_range_uj = 0ULL;
        if (!read_ull_from_file(energy_path, &energy_uj)) continue;
        if (!read_ull_from_file(max_range_path, &max_range_uj)) continue;

        total_energy_uj += energy_uj;
        total_max_range_uj += max_range_uj;
        found_any = true;
    }

    closedir(dir);

    sample.total_energy_uj = total_energy_uj;
    sample.total_max_range_uj = total_max_range_uj;
    sample.valid = found_any && total_max_range_uj > 0ULL;
    return sample;
}

double compute_energy_joule(const EnergySample& first,
                            const EnergySample& second) {
    if (!first.valid || !second.valid) return -1.0;

    if (second.total_energy_uj >= first.total_energy_uj) {
        const std::uint64_t diff_uj = second.total_energy_uj - first.total_energy_uj;
        return static_cast<double>(diff_uj) / 1.0e6;
    }

    if (first.total_max_range_uj == 0ULL ||
        second.total_max_range_uj == 0ULL ||
        first.total_max_range_uj != second.total_max_range_uj) {
        return -1.0;
    }

    const std::uint64_t diff_uj = (first.total_max_range_uj - first.total_energy_uj) +
                                  second.total_energy_uj;
    return static_cast<double>(diff_uj) / 1.0e6;
}

bool EnergyMeter::start() {
    first_ = sample_total_energy();
    return first_.valid;
}

bool EnergyMeter::stop() {
    second_ = sample_total_energy();
    return second_.valid;
}

double EnergyMeter::consumed_joule() const {
    return compute_energy_joule(first_, second_);
}

bool EnergyMeter::valid() const {
    return first_.valid && second_.valid;
}

}  // namespace power
