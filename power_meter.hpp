#ifndef POWER_METER_HPP
#define POWER_METER_HPP

#include <cstdint>

namespace power {

struct EnergySample {
    std::uint64_t total_energy_uj;
    std::uint64_t total_max_range_uj;
    bool valid;
};

EnergySample sample_total_energy();
double compute_energy_joule(const EnergySample& first,
                            const EnergySample& second);

class EnergyMeter {
public:
    bool start();
    bool stop();
    double consumed_joule() const;
    bool valid() const;

private:
    EnergySample first_{0ULL, 0ULL, false};
    EnergySample second_{0ULL, 0ULL, false};
};

}  // namespace power

#endif
