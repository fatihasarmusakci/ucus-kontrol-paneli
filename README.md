# Uçuş Kontrol Paneli

Web tabanlı İHA yer kontrol istasyonu ve canlı telemetri arayüzü.

Bu projede çok rotorlu insansız hava araçları için uçtan uca bir kontrol ve izleme altyapısı geliştirdim. Amaç; otopilot ile operatör arasında düşük gecikmeli, güvenilir ve genişletilebilir bir haberleşme katmanı kurmak ve uçuş verilerini anlaşılır biçimde görselleştirmektir.

## Projenin Amacı

İHA sistemlerinde sahaya çıkmadan önce senaryoların test edilebilmesi, uçuş sırasında anlık durumun izlenebilmesi ve gömülü donanımla yer istasyonu arasında tutarlı bir veri akışının sağlanması kritik öneme sahiptir. Bu çalışmada şu ihtiyaçlara odaklandım:

- **Canlı telemetri:** Konum, attitude, batarya, GPS kalitesi ve sistem durumunun tek ekranda takibi
- **Simülasyon desteği:** ArduPilot SITL ile donanım olmadan uçuş öncesi doğrulama
- **Özgün uçuş kontrol kartı:** STM32F405 tabanlı kart için ArduPilot portu ve firmware derleme hattı
- **Enerji izleme:** Uçuş sırasında güç tüketimi ve geri kazanım verilerinin kaydı ve görselleştirilmesi

## Sistem Mimarisi

```
┌─────────────────────────────────────────────────────────┐
│           Uçuş Kontrol Paneli (Web Arayüzü)               │
│         Harita · Attitude · Batarya · Enerji            │
└────────────────────────┬────────────────────────────────┘
                         │ MAVLink (UDP / SSE)
┌────────────────────────▼────────────────────────────────┐
│           Telemetri Sunucusu (Python / aiohttp)         │
└────────────────────────┬────────────────────────────────┘
                         │
         ┌───────────────┴───────────────┐
         ▼                               ▼
┌─────────────────┐             ┌─────────────────┐
│  ArduPilot SITL │             │  Fiziksel FC    │
│   (Simülasyon)  │             │  (STM32F405)    │
└─────────────────┘             └─────────────────┘
```

### Kullandığım Teknolojiler

| Katman | Teknoloji |
|--------|-----------|
| Otopilot | ArduPilot (ArduCopter) |
| Haberleşme | MAVLink 2 |
| Yer istasyonu | Python 3, aiohttp, pymavlink |
| Arayüz | HTML5, Leaflet, Server-Sent Events |
| Gömülü hedef | ARM Cortex-M4, gcc-arm-none-eabi |
| Simülasyon | ArduPilot SITL |

## Proje Yapısı

```
ucus-kontrol-paneli/
├── dashboard/       # Web arayüzü ve telemetri sunucusu
├── scripts/         # Kurulum, derleme ve çalıştırma betikleri
├── docs/            # Mimari ve geliştirme notları
└── ardupilot/       # ArduPilot kaynak ağacı (SITL + özel kart tanımı)
```

## Kurulum

### Gereksinimler

- macOS veya Linux
- Python 3.10 veya üzeri
- Git
- pyenv (önerilir)

### Adımlar

```bash
git clone https://github.com/fatihasarmusakci/ucus-kontrol-paneli.git
cd ucus-kontrol-paneli

# Bağımlılıklar + SITL derlemesi (ilk çalıştırmada birkaç dakika sürebilir)
bash scripts/setup-env.sh
```

## Çalıştırma

### Kontrol paneli (önerilen)

```bash
bash scripts/start-panel.sh
```

Tarayıcı otomatik açılır: **http://localhost:8080**

Bu komut SITL simülasyonunu başlatır, MAVLink bağlantısını kurar ve kontrol panelini ayağa kaldırır.

### Yalnızca simülasyon

```bash
bash scripts/run-sitl.sh
```

Harici GCS bağlantısı: **UDP 14550** (Mission Planner, QGroundControl)

### Firmware derleme

```bash
bash scripts/build-firmware.sh      # Uçuş kontrol kartı firmware
bash scripts/build-bootloader.sh    # Bootloader
```

## Python Bağımlılıkları

```bash
pip install -r dashboard/requirements.txt
```

- `pymavlink` — MAVLink protokol ayrıştırıcısı
- `aiohttp` — HTTP sunucusu ve SSE akışı

## Özellikler

- Taktik harita (uydu / sokak katmanı) ve uçuş izi kaydı
- GPX dışa aktarma
- Uçuş öncesi kontrol listesi (GPS, EKF, batarya, bağlantı)
- 3B araç attitude gösterimi ve pusula
- Ana güç ve enerji geri kazanım paneli
- Olay günlüğü ve bağlantı kalitesi göstergesi

## Geliştirme Durumu

| Bileşen | Durum |
|---------|--------|
| Web kontrol paneli | Çalışır durumda |
| SITL simülasyon entegrasyonu | Çalışır durumda |
| MAVLink telemetri akışı | Çalışır durumda |
| Özgün FC firmware derlemesi | Derlenebilir |
| Fiziksel kart flash / saha testi | Donanım bağlı olduğunda |

## Lisans

ArduPilot bileşenleri GPL-3.0 lisansı altındadır. Bu depo ArduPilot ekosistemi üzerine inşa edilmiştir.
