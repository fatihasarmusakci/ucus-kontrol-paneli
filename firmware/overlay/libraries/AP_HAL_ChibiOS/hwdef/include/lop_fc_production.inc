# =============================================================================
# LÖP-FC Production Software Configuration Include
# RTOS görev önceliklendirme, uçuş döngüsü, kara kutu logging
# =============================================================================
#
# ArduPilot'ta gerçek zamanlı önceliklendirme AP_Scheduler + ChibiOS FAST_TASK
# ile yönetilir. Aşağıdaki tanımlar LOP-FC kartına özel varsayılanları belirler.
#
# Öncelik haritası (ChibiOS / AP_Scheduler semantiği):
#   HIGHEST  → fast_loop() @ SCHED_LOOP_RATE (400 Hz): Rate/Attitude PID, IMU
#   HIGH     → EKF3 / AHRS update (INS_GYRO_RATE ile senkron)
#   NORMAL   → LIDAR (RangeFinder), Termal (TemperatureSensor), GPS parse
#   LOW      → AP_EnergyRecovery telemetri / ADC (5–10 Hz arka plan)
#   IDLE     → GCS MAVLink stream, OSD, notify
#
# Not: Ham ChibiOS thread priority doğrudan değiştirilmez; ArduPilot mimarisi
# bu ayrımı scheduler_tasks[] tablosu ile garanti eder.
# =============================================================================

# --- Uçuş kontrol döngüsü: 400 Hz (Rate PID + attitude) ---
define SCHED_LOOP_RATE 400

# --- ESC çıkış: 50–400 Hz PWM veya DShot (MOT_PWM_* parametreleri) ---
# MOT_PWM_RATE defaults.parm içinde 400 olarak ayarlanır
# MOT_PWM_TYPE: 0=Normal PWM, 6=DShot600 vb.

# --- EKF3 aktif ---
define HAL_NAVEKF3_AVAILABLE 1

# --- Kara kutu: DataFlash + yüksek hızlı IMU logging ---
# LOG_BITMASK defaults.parm'da yapılandırılır

# --- LÖP enerji modülü scheduler: düşük öncelik, 5 Hz ---
define AP_LOP_EREC_SCHED_HZ 5
define AP_LOP_EREC_SCHED_MAX_US 100
define AP_LOP_EREC_SCHED_PRIORITY 254

# --- Enerji modülü ADC median filtre derinliği (statik tahsis) ---
define AP_LOP_EREC_ADC_FILTER_SIZE 8

# --- Safe-Mode: koruma tepki süresi hedefi (ms) ---
define AP_LOP_EREC_SAFETY_RESPONSE_MS 10

# --- DataFlash log mesajı etkin ---
define AP_LOP_EREC_LOGGING_ENABLED 1
