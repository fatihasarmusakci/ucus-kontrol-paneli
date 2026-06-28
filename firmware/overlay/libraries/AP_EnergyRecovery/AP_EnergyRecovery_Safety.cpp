#include "AP_EnergyRecovery_Safety.h"

#if AP_ENERGY_RECOVERY_ENABLED

extern const AP_HAL::HAL &hal;

AP_EnergyRecovery_Safety::AP_EnergyRecovery_Safety()
    : _cutoff_pin(-1),
      _max_volt(16.8f),
      _max_curr(2.0f),
      _charging_enabled(false),
      _fault_active(false),
      _fault_reason(FaultReason::NONE),
      _fault_timestamp_ms(0)
{
}

void AP_EnergyRecovery_Safety::init(int8_t cutoff_pin, float max_volt, float max_curr)
{
    _cutoff_pin = cutoff_pin;
    _max_volt = max_volt;
    _max_curr = max_curr;
    set_cutoff(true);
}

bool AP_EnergyRecovery_Safety::evaluate(float voltage, float current, bool adc_healthy)
{
    if (!adc_healthy) {
        _fault_active = true;
        _fault_reason = FaultReason::ADC_FAIL;
        set_cutoff(false);
        return false;
    }

    // --- Anlık koruma: aşırı gerilim, aşırı akım, ters akım ---
    if (voltage > _max_volt) {
        _fault_active = true;
        _fault_reason = FaultReason::OVERVOLTAGE;
        _fault_timestamp_ms = AP_HAL::millis();
        set_cutoff(false);
        return false;
    }

    if (current > _max_curr) {
        _fault_active = true;
        _fault_reason = FaultReason::OVERCURRENT;
        _fault_timestamp_ms = AP_HAL::millis();
        set_cutoff(false);
        return false;
    }

    if (current < -0.1f) {
        _fault_active = true;
        _fault_reason = FaultReason::REVERSE_CURRENT;
        _fault_timestamp_ms = AP_HAL::millis();
        set_cutoff(false);
        return false;
    }

    // --- Hysteresis ile recovery: %95 gerilim, %90 akım eşiği ---
    if (_fault_active) {
        if (voltage < _max_volt * 0.95f &&
            current < _max_curr * 0.9f &&
            current >= 0.0f) {
            _fault_active = false;
            _fault_reason = FaultReason::NONE;
            set_cutoff(true);
        } else {
            set_cutoff(false);
            return false;
        }
    }

    set_cutoff(true);
    return true;
}

void AP_EnergyRecovery_Safety::set_cutoff(bool allow_charge)
{
    if (_charging_enabled == allow_charge) {
        return;
    }
    _charging_enabled = allow_charge;

    if (_cutoff_pin < 0) {
        return;
    }

    const uint8_t pin = uint8_t(_cutoff_pin);
    hal.gpio->pinMode(pin, HAL_GPIO_OUTPUT);

#if HAL_LOP_ENERGY_CUTOFF_ACTIVE_HIGH
    // ACTIVE_HIGH: pin=1 → şarj aktif, pin=0 → kesme
    hal.gpio->write(pin, allow_charge ? 1 : 0);
#else
    hal.gpio->write(pin, allow_charge ? 0 : 1);
#endif
}

#endif  // AP_ENERGY_RECOVERY_ENABLED
