#pragma once

/**
 * @file AP_LopSystem_Architecture.h
 * @brief LÖP-FC gömülü yazılım mimarisi — RTOS görev öncelik haritası.
 *
 * Doğuş Üniversitesi · Özgün İHA Uçuş Kontrol Kartı
 * ChibiOS RTOS + ArduPilot AP_Scheduler
 *
 * =============================================================================
 * GÖREV ÖNCELİK MATRİSİ (yüksekten düşüğe)
 * =============================================================================
 *
 * | Katman          | Hz      | AP_Scheduler / ChibiOS     | Modül              |
 * |-----------------|---------|----------------------------|--------------------|
 * | HIGHEST         | 400     | fast_loop(), FAST_TASK     | Rate/Attitude PID  |
 * | HIGHEST         | 400     | INS update                 | IMU SPI1 (EKF girdi)|
 * | HIGH            | 100-400 | EKF3 / NavEKF3             | Sensör füzyonu     |
 * | NORMAL          | 10-50   | RangeFinder, GPS, Temp     | LIDAR I2C, Termal  |
 * | LOW             | 5       | AP_EnergyRecovery::update  | ADC enerji modülü  |
 * | IDLE            | 1-4     | GCS_MAVLink, Notify        | Telemetri stream   |
 *
 * =============================================================================
 * VERİ YOLU (Data Flow)
 * =============================================================================
 *
 *   SPI1 (IMU) ──→ AP_InertialSensor ──→ EKF3 ──→ Attitude Control (400Hz)
 *   I2C1 (LIDAR) ─→ AP_RangeFinder ────→ EKF3 (POSZ yardımcı)
 *   I2C1 (Termal) → AP_TemperatureSensor → MAVLink / Log
 *   USART2 (GPS) ─→ AP_GPS ────────────→ EKF3 (POSXY)
 *   ADC (Enerji) ─→ AP_EnergyRecovery ──→ MAVLink 227 + LOG_LOP_EREC
 *   USART1 (MAV) ←─ GCS_MAVLink ←──────── tüm telemetri akışları
 *
 * =============================================================================
 * BELLEK MODELİ
 * =============================================================================
 *
 * - Heap allocation YASAK (gömülü güvenlik politikası)
 * - AP_EnergyRecovery_ADC: statik ring buffer [FILTER_SIZE]
 * - Tüm sınıflar singleton veya stack tahsisli
 * - ChibiOS memory pools: HAL tarafından yönetilir
 *
 * =============================================================================
 */

#include "AP_EnergyRecovery_config.h"

#if AP_ENERGY_RECOVERY_ENABLED

#define AP_LOP_PRIORITY_FLIGHT_CONTROL_HZ     400
#define AP_LOP_PRIORITY_EKF_HZ                100
#define AP_LOP_PRIORITY_LIDAR_THERMAL_HZ       10
#define AP_LOP_PRIORITY_ENERGY_RECOVERY_HZ      AP_LOP_EREC_SCHED_HZ

#endif  // AP_ENERGY_RECOVERY_ENABLED
