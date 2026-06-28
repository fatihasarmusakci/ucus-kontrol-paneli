#pragma once

#include "AP_EnergyRecovery_config.h"

#if AP_ENERGY_RECOVERY_ENABLED

#include "AP_EnergyRecovery_ADC.h"
#include "AP_EnergyRecovery_Safety.h"

#include <AP_Param/AP_Param.h>
#include <GCS_MAVLink/GCS_MAVLink.h>

/**
 * @class AP_EnergyRecovery
 * @brief Mikro ölçekli rüzgâr akışı tabanlı kinetik enerji geri kazanım modülü.
 *
 * Mimari (SOLID — Single Responsibility):
 *   AP_EnergyRecovery_ADC     → ADC okuma + median filtre
 *   AP_EnergyRecovery_Safety  → Safe-Mode kesme / fail-safe
 *   AP_EnergyRecovery_Logging → DataFlash kara kutu kaydı
 *   AP_EnergyRecovery         → Orkestrasyon, enerji integrasyonu, MAVLink
 *
 * Scheduler: LOW_PRIORITY @ AP_LOP_EREC_SCHED_HZ (varsayılan 5 Hz)
 */
class AP_EnergyRecovery
{
public:
    AP_EnergyRecovery();

    CLASS_NO_COPY(AP_EnergyRecovery);

    static AP_EnergyRecovery *get_singleton() { return _singleton; }

    void init();
    void update();

    bool enabled() const { return _enable != 0; }
    bool healthy() const { return _adc.healthy(); }
    bool fault_active() const { return _safety.fault_active(); }

    float get_voltage() const { return _voltage; }
    float get_current() const { return _current; }
    float get_power() const { return _power; }
    float get_energy_wh() const { return _energy_wh; }
    float get_energy_mah() const { return _energy_mah; }
    bool charging_enabled() const { return _safety.charging_enabled(); }

    void send_energy_recovery_data(const class GCS_MAVLINK &link) const;

    static const struct AP_Param::GroupInfo var_info[];

private:
    static AP_EnergyRecovery *_singleton;

    void integrate_energy(float dt_sec);
    void write_log(uint64_t time_us);
    void notify_fault();

    AP_EnergyRecovery_ADC _adc;
    AP_EnergyRecovery_Safety _safety;

    AP_Int8 _enable;
    AP_Int8 _volt_pin;
    AP_Int8 _curr_pin;
    AP_Float _volt_mult;
    AP_Float _curr_mult;
    AP_Float _max_volt;
    AP_Float _max_curr;
    AP_Int8 _cutoff_pin;
    AP_Int8 _log_rate_hz;

    float _voltage;
    float _current;
    float _power;
    float _energy_wh;
    float _energy_mah;
    uint32_t _last_update_ms;
    uint32_t _last_log_ms;
    bool _fault_notified;
};

namespace AP {
AP_EnergyRecovery &energy_recovery();
}

#endif  // AP_ENERGY_RECOVERY_ENABLED
