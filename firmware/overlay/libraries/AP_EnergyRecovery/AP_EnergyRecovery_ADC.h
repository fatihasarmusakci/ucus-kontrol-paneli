#pragma once

#include "AP_EnergyRecovery_config.h"

#if AP_ENERGY_RECOVERY_ENABLED

#include <AP_HAL/AP_HAL.h>
#include <stdint.h>

/**
 * @brief ADC okuma katmanı — tamamen statik bellek tahsisi.
 *
 * Shunt amplifikatör çıkışlarını median filtre ile gürültüden arındırır.
 * Dinamik bellek (heap) kullanılmaz; gömülü güvenlik gereksinimlerine uygundur.
 */
class AP_EnergyRecovery_ADC {
public:
    AP_EnergyRecovery_ADC();

    /**
     * @param volt_pin  HAL ADC pin numarası (hwdef HAL_LOP_ENERGY_VOLT_PIN)
     * @param curr_pin  HAL ADC pin numarası
     * @param volt_mult Gerilim kalibrasyon katsayısı (V/Vadc)
     * @param curr_mult Akım kalibrasyon katsayısı (A/Vadc)
     */
    void init(int8_t volt_pin, int8_t curr_pin, float volt_mult, float curr_mult);

    /**
     * @brief Filtrelenmiş gerilim ve akım okur.
     * @return false ADC kaynağı geçersiz veya okuma başarısız
     */
    bool read(float &voltage_out, float &current_out);

    bool healthy() const { return _healthy; }

private:
    static const uint8_t FILTER_SIZE = AP_LOP_EREC_ADC_FILTER_SIZE;

    AP_HAL::AnalogSource *_volt_source;
    AP_HAL::AnalogSource *_curr_source;
    float _volt_mult;
    float _curr_mult;

    float _volt_ring[FILTER_SIZE];
    float _curr_ring[FILTER_SIZE];
    uint8_t _ring_index;
    uint8_t _sample_count;
    bool _healthy;

    float median_filter(float *ring, uint8_t count) const;
    void push_sample(float v_raw, float i_raw);
};

#endif  // AP_ENERGY_RECOVERY_ENABLED
