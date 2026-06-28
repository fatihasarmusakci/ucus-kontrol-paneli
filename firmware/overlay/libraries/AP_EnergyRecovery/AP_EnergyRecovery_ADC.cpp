#include "AP_EnergyRecovery_ADC.h"

#if AP_ENERGY_RECOVERY_ENABLED

#include <AP_Math/AP_Math.h>

extern const AP_HAL::HAL &hal;

AP_EnergyRecovery_ADC::AP_EnergyRecovery_ADC()
    : _volt_source(nullptr),
      _curr_source(nullptr),
      _volt_mult(1.0f),
      _curr_mult(1.0f),
      _ring_index(0),
      _sample_count(0),
      _healthy(false)
{
    for (uint8_t i = 0; i < FILTER_SIZE; i++) {
        _volt_ring[i] = 0.0f;
        _curr_ring[i] = 0.0f;
    }
}

void AP_EnergyRecovery_ADC::init(int8_t volt_pin, int8_t curr_pin, float volt_mult, float curr_mult)
{
    _volt_mult = volt_mult;
    _curr_mult = curr_mult;
    _volt_source = hal.analogin->channel(volt_pin);
    _curr_source = hal.analogin->channel(curr_pin);
    _healthy = (_volt_source != nullptr && _curr_source != nullptr);
}

void AP_EnergyRecovery_ADC::push_sample(float v_raw, float i_raw)
{
    _volt_ring[_ring_index] = v_raw;
    _curr_ring[_ring_index] = i_raw;
    _ring_index = (_ring_index + 1) % FILTER_SIZE;
    if (_sample_count < FILTER_SIZE) {
        _sample_count++;
    }
}

float AP_EnergyRecovery_ADC::median_filter(float *ring, uint8_t count) const
{
    if (count == 0) {
        return 0.0f;
    }
    float sorted[FILTER_SIZE];
    for (uint8_t i = 0; i < count; i++) {
        sorted[i] = ring[i];
    }
    for (uint8_t i = 0; i < count - 1; i++) {
        for (uint8_t j = i + 1; j < count; j++) {
            if (sorted[j] < sorted[i]) {
                const float tmp = sorted[i];
                sorted[i] = sorted[j];
                sorted[j] = tmp;
            }
        }
    }
    return sorted[count / 2];
}

bool AP_EnergyRecovery_ADC::read(float &voltage_out, float &current_out)
{
    if (!_healthy) {
        return false;
    }

    const float v_adc = _volt_source->voltage_average();
    const float i_adc = _curr_source->voltage_average();

    push_sample(v_adc * _volt_mult, i_adc * _curr_mult);

    voltage_out = median_filter(_volt_ring, _sample_count);
    current_out = median_filter(_curr_ring, _sample_count);
    return true;
}

#endif  // AP_ENERGY_RECOVERY_ENABLED
