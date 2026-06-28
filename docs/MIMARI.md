# Uçuş Kontrol Paneli — Sistem Mimarisi

Bu belge, Uçuş Kontrol Paneli projesinin yazılım mimarisini özetler.

## Genel Bakış

Sistem üç ana katmandan oluşur:

1. **Gömülü katman** — ArduPilot tabanlı uçuş kontrol yazılımı (SITL veya fiziksel STM32 kart)
2. **Köprü katmanı** — Python telemetri sunucusu; MAVLink mesajlarını JSON/SSE formatına dönüştürür
3. **Sunum katmanı** — Web kontrol paneli; operatöre harita, attitude ve sistem durumunu sunar

## Haberleşme Akışı

```
ArduCopter (SITL / FC)
        │
        │ MAVLink 2 (UDP 14550 / 14551)
        ▼
dashboard/server.py  ──SSE──►  Tarayıcı (app.js)
```

Sunucu, `pymavlink` ile heartbeat, GPS, attitude, batarya ve özel enerji mesajlarını okur. Her 300 ms'de bir `text/event-stream` üzerinden istemciye iletir.

## Dizin Yapısı

| Dizin | Açıklama |
|-------|----------|
| `dashboard/server.py` | aiohttp sunucusu, SITL başlatıcı, MAVLink okuyucu |
| `dashboard/static/` | HTML, CSS, JavaScript arayüz dosyaları |
| `scripts/setup-env.sh` | Toolchain indirme ve SITL derlemesi |
| `scripts/start-panel.sh` | Tek komutla panel + simülasyon |
| `ardupilot/` | ArduPilot kaynak ağacı, özel kart hwdef ve modüller |

## Simülasyon Modu

`start-panel.sh` çalıştığında sunucu `arducopter` SITL sürecini otomatik başlatır. Simülasyon modunda varsayılan konum İstanbul koordinatlarıdır; GPS fix, attitude ve batarya verileri gerçekçi biçimde üretilir.

## Donanım Modu

Fiziksel uçuş kontrol kartı bağlandığında aynı MAVLink hattı kullanılır. Firmware `scripts/build-firmware.sh` ile derlenir; yükleme `scripts/flash-firmware.sh` ile yapılır.

## Genişletme Noktaları

- Yeni sensör verileri: `server.py` içindeki MAVLink mesaj işleyicilerine ekleme
- Yeni arayüz bileşenleri: `dashboard/static/index.html` ve `app.js`
- Yeni otopilot modları: ArduPilot parametre setleri (`defaults.parm`)
