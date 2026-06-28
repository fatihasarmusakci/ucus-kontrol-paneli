# u-u-kontrol-paneli (UAV Control & Management Interface)

Bu proje; insansız hava araçları (İHA) ve otopilot sistemleri için gelişmiş, modüler ve yüksek performanslı bir kontrol, izleme ve yönetim paneli geliştirmeyi hedeflemektedir. Proje, sistem mimarisi olarak düşük seviyeli gömülü yazılım kontrolü ile kullanıcı arayüzü arasında köprü kuran kararlı bir altyapı sunmayı amaçlar.

## Temel Hedefler

- **Gelişmiş Telemetri ve Veri Görselleştirme:** Hava aracından gelen anlık sensör, konum ve sistem durum verilerini gecikmesiz işleyerek kullanıcıya aktarmak.
- **Modüler Kontrol Altyapısı:** Farklı otopilot yazılımları ve donanımları ile kolayca entegre olabilecek esnek bir kod mimarisi inşa etmek.
- **Görev Planlama ve Simülasyon:** Araç sahaya çıkmadan önce uçuş senaryolarını test edebilmeyi sağlayan simülasyon ve rota optimizasyon araçları sunmak.

## Teknik Mimari

### 1. Otopilot Entegrasyonu ve Haberleşme

- Proje, **ArduPilot** ekosistemi ile tam uyumlu çalışacak şekilde tasarlanmaktadır.
- Araç ile yer istasyonu arasındaki veri akışı **MAVLink** protokolü üzerinden kurgulanmaktadır.
- Web tabanlı kontrol paneli (`dashboard/`) SITL simülasyonu veya gerçek donanıma bağlanabilir.

### 2. Çapraz Derleme (Cross-Compilation) Altyapısı

- Gömülü bileşenler için **ARM Cortex-M** hedefli `gcc-arm-none-eabi` araç zinciri kullanılır.
- Derleyici ilk kurulumda `scripts/setup-env.sh` ile otomatik indirilir (repoya dahil değildir).

### 3. Sinyal İşleme ve Filtreleme

- Sensör verilerinin anlamlandırılması ve kararlı yönelim bilgisi için EKF tabanlı tahminleme ve filtreleme yöntemleri mimariye entegre edilmektedir.
- `AP_EnergyRecovery` modülü enerji geri kazanım algoritmalarını içerir.

## Proje Yapısı

```
u-u-kontrol-paneli/
├── dashboard/          # Web kontrol paneli (Python + aiohttp)
├── scripts/            # Kurulum, derleme ve çalıştırma betikleri
├── docs/               # Dokümantasyon ve yol haritası
├── ardupilot/          # ArduPilot fork (LOP-FC hwdef, SITL)
└── demo-toplanti/      # Demo materyalleri
```

## Geliştirme Ortamının Hazırlanması

### Gereksinimler

- macOS veya Linux
- Python 3.10+
- pyenv (önerilir)
- Git

### Kurulum

```bash
git clone https://github.com/fatihasarmusakci/u-u-kontrol-paneli.git
cd u-u-kontrol-paneli

# Ortam kurulumu + SITL derlemesi (ilk sefer ~2 dk)
bash scripts/setup-env.sh
```

### Kontrol Panelini Çalıştırma

```bash
bash scripts/start-panel.sh
```

Tarayıcıda panel adresi: **http://localhost:8080**

Panel; SITL simülasyonunu başlatır, MAVLink telemetrisini okur ve uçuş verilerini canlı gösterir.

### Diğer Komutlar

```bash
# Sadece SITL (Mission Planner / QGC: UDP 14550)
bash scripts/run-sitl.sh

# LOP-FC firmware derleme
bash scripts/build-lop-fc.sh
```

## Python Bağımlılıkları

```bash
pip install -r dashboard/requirements.txt
```

- `pymavlink` — MAVLink haberleşmesi
- `aiohttp` — Web sunucusu ve WebSocket

## Lisans

Bu proje ArduPilot ekosistemi üzerine inşa edilmiştir. ArduPilot GPL-3.0 lisansı altındadır.
