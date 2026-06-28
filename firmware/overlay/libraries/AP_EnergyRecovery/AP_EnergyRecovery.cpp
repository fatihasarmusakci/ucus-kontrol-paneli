#include "AP_EnergyRecovery.h"

#if AP_ENERGY_RECOVERY_ENABLED

#include "AP_EnergyRecovery_Logging.h"
#include <AP_HAL/AP_HAL.h>
#include <GCS_MAVLink/GCS.h>

extern const AP_HAL::HAL &hal;

AP_EnergyRecovery *AP_EnergyRecovery::_singleton = nullptr;

const AP_Param::GroupInfo AP_EnergyRecovery::var_info[] = {
    // @Param: ENABLE
    // @DisplayName: Energy recovery enable
    // @Description: Enable wind energy recovery module
    // @Values: 0:Disabled,1:Enabled
    // @User: Standard
    AP_GROUPINFO("ENABLE", 1, AP_EnergyRecovery, _enable, 1),

    // @Param: VOLT_PIN
    // @DisplayName: Energy module voltage ADC pin
    // @User: Advanced
    AP_GROUPINFO("VOLT_PIN", 2, AP_EnergyRecovery, _volt_pin, HAL_LOP_ENERGY_VOLT_PIN),

    // @Param: CURR_PIN
    // @DisplayName: Energy module current ADC pin
    // @User: Advanced
    AP_GROUPINFO("CURR_PIN", 3, AP_EnergyRecovery, _curr_pin, HAL_LOP_ENERGY_CURR_PIN),

    // @Param: VOLT_MULT
    // @DisplayName: Voltage multiplier
    // @User: Advanced
    AP_GROUPINFO("VOLT_MULT", 4, AP_EnergyRecovery, _volt_mult, HAL_LOP_ENERGY_VOLT_SCALE),

    // @Param: CURR_MULT
    // @DisplayName: Current multiplier
    // @User: Advanced
    AP_GROUPINFO("CURR_MULT", 5, AP_EnergyRecovery, _curr_mult, HAL_LOP_ENERGY_CURR_SCALE),

    // @Param: MAX_VOLT
    // @DisplayName: Maximum charge voltage
    // @Units: V
    // @User: Standard
    AP_GROUPINFO("MAX_VOLT", 6, AP_EnergyRecovery, _max_volt, 16.8f),

    // @Param: MAX_CURR
    // @DisplayName: Maximum charge current
    // @Units: A
    // @User: Standard
    AP_GROUPINFO("MAX_CURR", 7, AP_EnergyRecovery, _max_curr, 2.0f),

    // @Param: CUTOFF_PIN
    // @DisplayName: Charge cutoff GPIO pin
    // @User: Advanced
    AP_GROUPINFO("CUTOFF_PIN", 8, AP_EnergyRecovery, _cutoff_pin, HAL_LOP_ENERGY_CUTOFF_PIN),

    // @Param: LOG_RATE
    // @DisplayName: DataFlash log rate
    // @Description: Energy recovery black-box log rate in Hz
    // @Units: Hz
    // @Range: 1 50
    // @User: Advanced
    AP_GROUPINFO("LOG_RATE", 9, AP_EnergyRecovery, _log_rate_hz, 10),

    AP_GROUPEND
};

AP_EnergyRecovery::AP_EnergyRecovery()
    : _voltage(0.0f),
      _current(0.0f),
      _power(0.0f),
      _energy_wh(0.0f),
      _energy_mah(0.0f),
      _last_update_ms(0),
      _last_log_ms(0),
      _fault_notified(false)
{
    AP_Param::setup_object_defaults(this, var_info);

    if (_singleton != nullptr) {
        AP_HAL::panic("AP_EnergyRecovery must be singleton");
    }
    _singleton = this;
}

void AP_EnergyRecovery::init()
{
    if (!enabled()) {
        return;
    }

    _adc.init(_volt_pin, _curr_pin, _volt_mult, _curr_mult);
    _safety.init(_cutoff_pin, _max_volt, _max_curr);
    _last_update_ms = AP_HAL::millis();
    _last_log_ms = _last_update_ms;
}

void AP_EnergyRecovery::update()
{
    if (!enabled()) {
        return;
    }

    const uint32_t now = AP_HAL::millis();
    const float dt = (now - _last_update_ms) * 0.001f;
    _last_update_ms = now;

    if (dt <= 0.0f || dt > 1.0f) {
        return;
    }

    // --- ADC katmanı: filtrelenmiş V/I okuma ---
    const bool adc_ok = _adc.read(_voltage, _current);
    _power = _voltage * _current;

    // --- Güvenlik katmanı: Safe-Mode kesme değerlendirmesi ---
    const bool charge_ok = _safety.evaluate(_voltage, _current, adc_ok);

    if (_safety.fault_active()) {
        notify_fault();
    } else {
        _fault_notified = false;
    }

    // --- Enerji integrasyonu: yalnızca güvenli şarj aktifken birikim ---
    if (charge_ok && _power > 0.0f) {
        integrate_energy(dt);
    }

    // --- Kara kutu logging (asenkron, LOW priority task içinde) ---
    write_log(now * 1000ULL);
}

void AP_EnergyRecovery::integrate_energy(float dt_sec)
{
    // P = V × I  →  E(Wh) = ∫P dt / 3600
    const float energy_ws = _power * dt_sec;
    _energy_wh += energy_ws / 3600.0f;
    _energy_mah += (_current * 1000.0f * dt_sec) / 3600.0f;
}

void AP_EnergyRecovery::write_log(uint64_t time_us)
{
#if AP_LOP_EREC_LOGGING_ENABLED && HAL_LOGGING_ENABLED
    const uint8_t rate = constrain_int16(_log_rate_hz.get(), 1, 50);
    const uint32_t interval_ms = 1000U / rate;
    const uint32_t now_ms = time_us / 1000U;

    if (now_ms - _last_log_ms < interval_ms) {
        return;
    }
    _last_log_ms = now_ms;

    uint8_t status = 0;
    if (_safety.charging_enabled()) {
        status |= 0x01;
    }
    if (_safety.fault_active()) {
        status |= 0x02;
    }
    if (_adc.healthy()) {
        status |= 0x04;
    }

    AP_EnergyRecovery_Logging::Write(time_us, _voltage, _current, _power,
                                     _energy_wh, _energy_mah, status);
#else
    (void)time_us;
#endif
}

void AP_EnergyRecovery::notify_fault()
{
    if (_fault_notified) {
        return;
    }
    _fault_notified = true;
    GCS_SEND_TEXT(MAV_SEVERITY_CRITICAL, "LOP EREC fault reason=%u",
                  (unsigned)uint8_t(_safety.fault_reason()));
}

void AP_EnergyRecovery::send_energy_recovery_data(const GCS_MAVLINK &link) const
{
    if (!enabled()) {
        return;
    }

    mavlink_msg_energy_recovery_data_send(
        link.get_chan(),
        AP_HAL::millis(),
        _voltage,
        _current,
        _power,
        _energy_wh,
        _energy_mah,
        _safety.charging_enabled() ? 1 : 0,
        _safety.fault_active() ? 1 : 0);
}

namespace AP {
AP_EnergyRecovery &energy_recovery()
{
    return *AP_EnergyRecovery::get_singleton();
}
}

#endif  // AP_ENERGY_RECOVERY_ENABLED
