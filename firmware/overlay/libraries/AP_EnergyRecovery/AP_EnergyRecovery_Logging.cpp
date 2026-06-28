#include "AP_EnergyRecovery_Logging.h"

#if AP_ENERGY_RECOVERY_ENABLED && AP_LOP_EREC_LOGGING_ENABLED && HAL_LOGGING_ENABLED

#include <AP_Logger/AP_Logger.h>

void AP_EnergyRecovery_Logging::Write(uint64_t time_us,
                                     float voltage,
                                     float current,
                                     float power,
                                     float energy_wh,
                                     float energy_mah,
                                     uint8_t status)
{
    AP_Logger *logger = AP_Logger::get_singleton();
    if (logger == nullptr) {
        return;
    }

    struct log_LOP_EREC pkt {
        LOG_PACKET_HEADER_INIT(LOG_LOP_EREC_MSG),
        time_us     : time_us,
        voltage     : voltage,
        current     : current,
        power       : power,
        energy_wh   : energy_wh,
        energy_mah  : energy_mah,
        status      : status,
    };
    logger->WriteBlock(&pkt, sizeof(pkt));
}

#endif
