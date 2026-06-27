<h1 align="center">🚨 ESP32 MQTT-Based IoT Alarm System</h1>
<p align="center"><b>PIR sensörü ile tetiklenen, MQTT üzerinden veri yayınlayan ve Node-RED dashboard'dan kontrol edilen kızılötesi-ultrasonik hırsız alarm sistemi</b></p>

<p align="center">
  <img src="https://img.shields.io/badge/Platform-ESP32-00979D?style=flat&logo=espressif&logoColor=white" />
  <img src="https://img.shields.io/badge/Protocol-MQTT-660066?style=flat&logo=mqtt&logoColor=white" />
  <img src="https://img.shields.io/badge/Dashboard-Node--RED-8F0000?style=flat&logo=nodered&logoColor=white" />
  <img src="https://img.shields.io/badge/Simulated%20on-Wokwi-1A1A1A?style=flat&logo=arduino&logoColor=white" />
  <img src="https://img.shields.io/badge/License-MIT-green?style=flat" />
</p>

<p align="center">
  <a href="https://wokwi.com/projects/464392081577753601"><b>▶️ Canlı simülasyonu Wokwi'de çalıştır</b></a>
</p>

---

## 📌 Genel Bakış

**ESP32 MQTT-Based IoT Alarm System**, harekete duyarlı bir izinsiz giriş tespit sistemidir: bir PIR sensörü hareketi izler, hareket algılandığında bir HC-SR04 ultrasonik sensör algılanan nesneye olan mesafeyi ölçer. Mesafe alarm aralığına düşerse, RGB LED — bloklayıcı bir `delay()` yerine **donanım timer interrupt'ı** ile sürülerek — kırmızı renkte yanıp söner; bu sırada anlık mesafe değeri bir **MQTT broker'a** yayınlanır. Bir **Node-RED dashboard'u** mesafeyi gauge üzerinde görselleştirir ve kullanıcının alarm LED'ini uzaktan açıp kapatmasına imkân tanır.

---

## ✨ Özellikler

- 🏃 **PIR ile tetiklenen ölçüm** — ultrasonik ölçüm yalnızca hareket algılandığında çalışır, sürekli taramadan tasarruf sağlar
- 📏 **Ultrasonik mesafe ölçümü** (HC-SR04) ile algılanan hareketin ne kadar yakın olduğu belirlenir
- 🔴 **Interrupt tabanlı alarm LED'i** — ana döngüden tamamen bağımsız olarak ESP32 donanım timer'ı + ISR ile her 500ms'de bir yanıp söner
- 🔒 **Thread-safe paylaşılan durum** — ISR ile ana döngü arasında paylaşılan bayraklar `portENTER_CRITICAL`/`_ISR` ile korunur, race condition önlenir
- 📡 **MQTT ile veri yayını** — anlık mesafe ölçümleri herkese açık bir broker'a publish edilir
- 🎛️ **Node-RED üzerinden uzaktan kontrol** — dashboard butonları LED'i açmak/kapatmak için ON/OFF komutu yayınlar
- 📊 **Gerçek zamanlı dashboard gauge'u** (0–400 cm) ile canlı mesafe izleme

---

## 🧰 Donanım Bileşenleri

| Bileşen | Görevi | Bağlantı |
|---|---|---|
| ESP32 DevKit C V4 | Ana kontrolcü — WiFi + MQTT istemcisi | — |
| PIR hareket sensörü | Hareket tetikleyici | OUT → GPIO 33 |
| HC-SR04 | Ultrasonik mesafe sensörü | TRIG → GPIO 25, ECHO → GPIO 35 |
| RGB LED (ortak katot) | Alarm göstergesi (kırmızı yanıp sönme) | R → GPIO 5, G → GPIO 17, B → GPIO 16 (her biri 1kΩ direnç üzerinden) |
| 3× 1kΩ direnç | RGB kanalları için akım sınırlama | — |

---

## 🌐 Yazılım ve Platformlar

| Araç | Amacı |
|---|---|
| **Wokwi** | Devre simülasyon ortamı |
| **Arduino (C++)** | ESP32 firmware — WiFi, MQTT, sensör okuma, timer interrupt |
| **MQTT** (PubSubClient) | ESP32 ile Node-RED arasında publish/subscribe mesajlaşma |
| **broker.hivemq.com:1883** | Herkese açık MQTT broker (Wokwi'nin `test.mosquitto.org` erişimi kısıtlı olduğundan kullanılır) |
| **Node-RED** + dashboard | Görsel akış programlama — mesafe gauge'unu ve LED kontrol butonlarını gösterir |

---

## 📡 MQTT Topic Yapısı

> Aşağıdaki topic'ler genel bir placeholder isim alanı kullanır — birden fazla cihaz dağıtacaksan `esp32-alarm` kısmını kendi cihaz ID'in ile değiştirebilirsin.

| Topic | Yön | Açıklama |
|---|---|---|
| `esp32-alarm/distance` | ESP32 → Node-RED | Mesafe sensörü verisi (cm) |
| `esp32-alarm/led` | Node-RED → ESP32 | LED kontrol komutu (`ON` / `OFF`) |

---

## ⚙️ Nasıl Çalışır

**1. Harekete bağlı ölçüm**
Ana döngü, ultrasonik ölçümü yalnızca PIR sensörü hareket bildirdiğinde tetikler — sürekli sorgulamadan kaçınılır.

**2. Mesafe değerlendirmesi**
Ölçülen mesafe 0–200 cm aralığındaysa sistem bunu alarm durumu olarak kabul eder; bu aralığın dışında LED kapalı kalır.

**3. Interrupt tabanlı yanıp sönme**
Donanım timer'ı, ana döngü ne yapıyor olursa olsun her 500ms'de bir tetiklenir. ISR içinde, alarm durumu aktifse **ve** LED uzaktan devre dışı bırakılmamışsa RGB LED kırmızı renkte toggle edilir. Paylaşılan tüm bayraklar (`blinkActive`, `ledEnabled`, `ledState`) ISR ile ana döngü arasındaki race condition'ları önlemek için kritik bölgelerle korunur.

**4. MQTT yayını**
Her geçerli mesafe ölçümü string formatına çevrilip broker'a publish edilir; Node-RED bu veriyi alıp dashboard gauge'unda gösterir.

**5. Uzaktan açma/kapama**
Node-RED dashboard butonları LED topic'ine `ON`/`OFF` yayınlar. ESP32'nin MQTT callback fonksiyonu `ledEnabled` bayrağını buna göre güncelleyerek; `OFF` komutu LED'i bir sonraki ISR tetiklenmesini beklemeden hemen söndürür.

---

## 🔌 Devre Şeması

Tüm kablolama ve bileşen yerleşimi canlı simülasyonda mevcuttur:

👉 **[Wokwi'de Aç](https://wokwi.com/projects/464392081577753601)**

<details>
<summary>📐 Pin bağlantı özeti</summary>

```
PIR OUT       → GPIO 33
HC-SR04 TRIG  → GPIO 25
HC-SR04 ECHO  → GPIO 35
RGB LED R     → GPIO 5  (1kΩ üzerinden)
RGB LED G     → GPIO 17 (1kΩ üzerinden)
RGB LED B     → GPIO 16 (1kΩ üzerinden)
```
</details>

---

## 📊 Örnek Seri Çıktısı

```
Hareket algilandi! Mesafe olculuyor...
Mesafe: 403.44 cm
MQTT publish: esp32-alarm/distance -> 403.44
LED blink PASIF (200 cm ustu).
```

```
LED blink AKTIF (0-200 cm araliginda).
```

```
MQTT mesaji alindi [esp32-alarm/led]: OFF
LED devre disi birakildi.
```

---

## 🚀 Başlarken

### Adım 1 — Node.js Kurulumu
1. [nodejs.org](https://nodejs.org) adresine git ve **LTS** sürümünü indir
2. Kurulumu çalıştır
3. Bir terminal (CMD / PowerShell / Terminal) aç ve doğrula:
   ```
   node --version
   npm --version
   ```
   Her iki komut da bir versiyon numarası döndürüyorsa Node.js hazırdır.

### Adım 2 — Node-RED Kurulumu
1. Terminali **yönetici olarak** aç (Windows: Start → "cmd" → sağ tık → *Yönetici olarak çalıştır*)
2. Şunu çalıştır:
   ```
   npm install -g --unsafe-perm node-red
   ```
3. Kurulumun tamamlanmasını bekle (genellikle 2–5 dakika)

### Adım 3 — Node-RED'i Başlatma
1. Terminalde şunu çalıştır:
   ```
   node-red
   ```
2. Şu mesajı görene kadar bekle:
   ```
   Server now running at http://127.0.0.1:1880/
   ```
3. Tarayıcında `http://localhost:1880` adresini aç
4. **Terminal penceresini açık tut** — Node-RED arka planda çalışmaya devam etmeli

### Adım 4 — Dashboard Paketinin Kurulumu
1. Sağ üstteki **☰** menüsüne tıkla → **Manage Palette**
2. **Install** sekmesine geç
3. `node-red-dashboard` yazarak ara
4. **@flowfuse/node-red-dashboard** paketini bul ve **Install**'a tıkla (sorulursa onayla)
5. "Nodes added to palette" mesajını bekle
6. **Close** butonuna tıkla

### Adım 5 — Flow'u Oluşturma
Şunları içeren bir flow oluştur:
- `esp32-alarm/distance` topic'ine abone olan bir **MQTT-in** node'u, bir **gauge** node'una bağlı (aralık 0–400)
- "LED AÇ" / "LED KAPAT" adında iki **button** node'u, `esp32-alarm/led` topic'ine `ON`/`OFF` yayını yapan bir **MQTT-out** node'una bağlı

Flow'un hazır olduğunda sağ üstteki kırmızı **Deploy** butonuna tıkla.

### Adım 6 — MQTT Broker Adresini Ayarlama
1. MQTT node'una çift tıkla
2. **Server** alanının yanındaki kalem (✏️) ikonuna tıkla
3. Server alanına şunu yaz:
   ```
   broker.hivemq.com
   ```
4. Port'u `1883` olarak bırak
5. **Update** → **Done** → **Deploy** sırasıyla tıkla

> Bu broker ayarı her iki MQTT node'unu da (abone olan ve yayın yapan) etkiler, çünkü ikisi de aynı broker konfigürasyonunu paylaşır.

### Adım 7 — Dashboard'u Açma
1. Yeni bir tarayıcı sekmesinde şu adrese git:
   ```
   http://localhost:1880/ui
   ```
2. Mesafe gauge'unu (0–400 cm) ve LED AÇ/KAPAT butonlarını görmelisin

### Adım 8 — Wokwi Simülasyonunu Başlatma
1. [Wokwi proje linkini](https://wokwi.com/projects/464392081577753601) aç
2. Yeşil ▶️ **Play** butonuna tıkla
3. Sağ alttaki Serial Monitor'de başarılı bağlantı mesajını görene kadar bekle (30–60 saniye sürebilir)

### Adım 9 — Sistemi Test Etme

**Test 1 — Hareket Algılama ve Mesafe Ölçümü**
1. Wokwi ekranında PIR sensörüne tıkla
2. **Simulate motion** seçeneğine tıkla
3. Serial Monitor'de ölçülen mesafeyi ve başarılı bir MQTT publish mesajını görmelisin
4. Dashboard'daki gauge değeri buna göre güncellenmeli

**Test 2 — Alarm LED'i (0–200 cm aralığı)**
1. HC-SR04 sensörüne tıkla
2. Mesafe kaydırıcısını 200cm'nin altına çek (örn. 100 cm)
3. Serial Monitor'de LED'in artık yanıp söndüğünü onaylayan mesajı görmelisin
4. Simülasyondaki RGB LED kırmızı renkte yanıp sönmeli, dashboard gauge'u yeni mesafeyi yansıtmalı

**Test 3 — Dashboard'dan Uzaktan LED Kontrolü**
1. Dashboard'da **LED KAPAT** butonuna tıkla
2. Serial Monitor'de `OFF` komutunun alındığını ve LED'in devre dışı bırakıldığını onaylayan mesajı görmelisin
3. **LED AÇ** butonuna tıklayıp LED'in tekrar yanıp sönmeye başladığını doğrula

---

**Gerçek donanımda çalıştırma:**
1. Bileşenleri yukarıdaki [pin bağlantı özetine](#-devre-şeması) göre kabloya
2. Gerekli kütüphaneyi yükle: `PubSubClient`
3. `sketch.ino` dosyasını Arduino IDE veya PlatformIO ile ESP32'ye yükle
4. Canlı logları görmek için Seri Monitörü `115200` baud hızında aç
5. Fiziksel cihaz için de Node-RED dashboard'unu kurmak üzere yukarıdaki Adım 1–7'yi takip et

---

## 📁 Proje Yapısı

```
esp32-mqtt-based-iot-alarm-system/
├── sketch.ino          # Ana firmware — WiFi, MQTT, sensörler, timer ISR
├── diagram.json         # Wokwi devre/kablolama tanımı
└── libraries.txt        # Gerekli Arduino kütüphaneleri
```

---

## 🛠️ Teknoloji Yığını

<p>
  <img src="https://img.shields.io/badge/ESP32-00979D?style=flat&logo=espressif&logoColor=white" />
  <img src="https://img.shields.io/badge/Arduino%20C%2B%2B-00979D?style=flat&logo=arduino&logoColor=white" />
  <img src="https://img.shields.io/badge/MQTT-660066?style=flat&logo=mqtt&logoColor=white" />
  <img src="https://img.shields.io/badge/Node--RED-8F0000?style=flat&logo=nodered&logoColor=white" />
  <img src="https://img.shields.io/badge/HC--SR04-Ultrasonic-orange?style=flat" />
  <img src="https://img.shields.io/badge/Wokwi-Simulator-1A1A1A?style=flat" />
</p>

---

## 📄 Lisans

Bu proje **MIT Lisansı** ile lisanslanmıştır.

---

## 🙋 Geliştirici

**Kübra Parmak**
- GitHub: [@KbrPrmk](https://github.com/KbrPrmk)
- Wokwi: [@kbrprmk](https://wokwi.com/makers/kbrprmk)
