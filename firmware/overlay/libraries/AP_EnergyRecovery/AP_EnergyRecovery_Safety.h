#pragma once

#include "AP_EnergyRecovery_config.h"

#if AP_ENERGY_RECOVERY_ENABLED

#include <AP_HAL/AP_HAL.h>
#include <stdint.h>

/**
 * @brief Güvenlik Modu Kesmesi (Safe-Mode Interrupt) yöneticisi.
 *
 * Aşırı gerilim, aşırı akım ve ters akım durumlarında şarj hattını
 * GPIO cutoff pini üzerinden anında devre dışı bırakır.
 * Hysteresis ile flapping (salınım) önlenir.
 */
class AP_EnergyRecovery_Safety {
public:
    enum class FaultReason : uint8_t {
        NONE = 0,
        ADC_FAIL = 1,
        OVERVOLTAGE = 2,
        OVERCURRENT = 3,
        REVERSE_CURRENT = 4,
    };

    AP_EnergyRecovery_Safety();

    void init(int8_t cutoff_pin, float max_volt, float max_curr);

    /**
     * @brief Ölçülen değerlere göre koruma durumunu günceller.
     * @return true şarj hattı aktif, false kesilmiş
     */
    bool evaluate(float voltage, float current, bool adc_healthy);

    bool fault_active() const { return _fault_active; }
    FaultReason fault_reason() const { return _fault_reason; }
    bool charging_enabled() const { return _charging_enabled; }

private:
    void set_cutoff(bool allow_charge);

    int8_t _cutoff_pin;
    float _max_volt;
    float _max_curr;
    bool _charging_enabled;
    bool _fault_active;
    FaultReason _fault_reason;
    uint32_t _fault_timestamp_ms;
};

#endif  // AP_ENERGY_RECOVERY_ENABLED
