# Özgün Firmware Overlay

Bu dizin, standart ArduPilot kaynağına uygulanan proje özel dosyalarını içerir.

`scripts/setup-env.sh` çalıştığında:

1. ArduPilot klonlanır (veya önbellekten kullanılır)
2. Bu dizindeki dosyalar `ardupilot/` üzerine kopyalanır
3. SITL ve özgün kart firmware'i derlenir

## İçerik

- `libraries/AP_EnergyRecovery/` — Enerji geri kazanım modülü
- `libraries/AP_HAL_ChibiOS/hwdef/LOP-FC/` — Özgün uçuş kontrol kartı donanım tanımı
- `modules/mavlink/.../ardupilotmega.xml` — Enerji telemetri MAVLink mesajı (ID 227)
- Entegrasyon dosyaları (`AP_Vehicle`, `GCS_MAVLink`, vb.)
