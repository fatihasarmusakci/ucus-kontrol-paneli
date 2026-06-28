#pragma once

#include "AP_EnergyRecovery_config.h"
#include <AP_Logger/AP_Logger_config.h>

#if AP_ENERGY_RECOVERY_ENABLED && AP_LOP_EREC_LOGGING_ENABLED && HAL_LOGGING_ENABLED

#include <AP_Logger/LogStructure.h>

/**
 * @brief Kara kutu (DataFlash) loglama — ENERGY_RECOVERY kayıtları.
 *
 * Asenkron AP_Logger altyapısını kullanır; uçuş kontrol döngüsünü bloke etmez.
 * IMU/GPS/LIDAR logları ArduPilot'un yerleşik LOG_* mesajları ile @400Hz kaydedilir.
 */
class AP_EnergyRecovery_Logging {
public:
    /**
     * @param status bit0=charging, bit1=fault, bit2=healthy
     */
    static void Write(uint64_t time_us,
                      float voltage,
                      float current,
                      float power,
                      float energy_wh,
                      float energy_mah,
                      uint8_t status);
};

#endif  // AP_ENERGY_RECOVERY_ENABLED && logging
