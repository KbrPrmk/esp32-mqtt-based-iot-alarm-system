// WiFi bağlantısı için ESP32'nin dahili kütüphanesi dahil ediliyor
#include <WiFi.h>
// MQTT protokolü ile broker haberleşmesi için kütüphane dahil ediliyor
#include <PubSubClient.h>


// PIN TANIMLARI


// PIR hareket sensörünün sinyal çıkış pini ESP32'nin 33. pinine bağlı
#define PIR_PIN     33
// HC-SR04 ultrasonik sensörün tetikleme (trigger) pini ESP32'nin 25. pinine bağlı
#define TRIG_PIN    25
// HC-SR04 ultrasonik sensörün yankı (echo) pini ESP32'nin 35. pinine bağlı (input only)
#define ECHO_PIN    35
// RGB LED'in kırmızı renk pini ESP32'nin 5. pinine bağlı
#define LED_R       5
// RGB LED'in yeşil renk pini ESP32'nin 17. pinine bağlı
#define LED_G       17
// RGB LED'in mavi renk pini ESP32'nin 16. pinine bağlı
#define LED_B       16


// WiFi AYARLARI

// Wokwi simülasyon ortamının sanal WiFi ağı SSID'si tanımlanıyor
const char* ssid     = "Wokwi-GUEST";
// Wokwi-GUEST ağı şifre gerektirmediği için boş bırakılıyor
const char* password = "";


// MQTT AYARLARI

// Ücretsiz ve herkese açık Mosquitto test broker adresi tanımlanıyor
const char* mqtt_server = "broker.hivemq.com";
// MQTT'nin standart port numarası olan 1883 tanımlanıyor
const int   mqtt_port      = 1883;
// Mesafe verisinin yayınlanacağı MQTT topic'i tanımlanıyor
const char* mqtt_pub_topic = "esp32-alarm/distance";
// Node-RED'den LED kontrolü için dinlenecek MQTT topic'i tanımlanıyor
const char* mqtt_sub_topic = "esp32-alarm/led";
// Broker'a bağlanırken kullanılacak benzersiz istemci kimliği tanımlanıyor
const char* mqtt_client_id = "ESP32_IoT_AlarmSystem";


// NESNE TANIMLARI

// ESP32'nin WiFi bağlantısını temsil eden istemci nesnesi oluşturuluyor
WiFiClient   espClient;
// MQTT haberleşmesini yönetecek istemci nesnesi WiFi istemcisi ile ilişkilendirilerek oluşturuluyor
PubSubClient client(espClient);


// GLOBAL DEĞİŞKENLER

// LED'in Node-RED butonuyla açık/kapalı olduğunu tutan değişken, başlangıçta açık
// volatile: ISR içinde de kullanıldığı için derleyiciye önbellekleme yapmaması söyleniyor
volatile bool ledEnabled  = true;
// LED'in o anki yanık/sönük durumunu tutan değişken, başlangıçta sönük
volatile bool ledState    = false;
// LED'in yanıp sönmesi gerekip gerekmediğini belirten bayrak değişkeni
volatile bool blinkActive = false;


// TIMER TANIMLARI

// ESP32 donanım timer nesnesini tutacak pointer değişkeni tanımlanıyor
hw_timer_t* timer = NULL;
// ISR (Interrupt Service Routine) ile ana kod arasında güvenli veri paylaşımı için
// kritik bölge kilidi (mutex) tanımlanıyor
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;


// TIMER ISR FONKSİYONU
// Her 500ms'de bir donanım timer tarafından otomatik olarak çağrılır
// IRAM_ATTR: Bu fonksiyonun RAM'de tutulmasını sağlar, flash'tan yavaş erişimi önler

void IRAM_ATTR onTimer() {
  // ISR'ye girişte kritik bölge kilidi alınıyor, diğer interrupt'lar engelleniyor
  portENTER_CRITICAL_ISR(&timerMux);

  // Hem blink aktif hem de LED etkinleştirilmiş ise LED toggle işlemi yapılır
  if (blinkActive && ledEnabled) {
    // LED durumu tersine çevriliyor: yanıksa söndür, sönükse yak
    ledState = !ledState;

    // LED durumu true ise LED kırmızı renkte yakılıyor (tehlike/alarm rengi)
    if (ledState) {
      // Kırmızı pin HIGH yapılarak kırmızı renk aktif ediliyor
      digitalWrite(LED_R, HIGH);
      // Yeşil pin LOW yapılarak yeşil renk devre dışı bırakılıyor
      digitalWrite(LED_G, LOW);
      // Mavi pin LOW yapılarak mavi renk devre dışı bırakılıyor
      digitalWrite(LED_B, LOW);
    } else {
      // LED durumu false ise tüm pinler LOW yapılarak LED söndürülüyor
      digitalWrite(LED_R, LOW);
      digitalWrite(LED_G, LOW);
      digitalWrite(LED_B, LOW);
    }
  } else {
    // Blink aktif değilse veya LED kapatıldıysa tüm LED pinleri söndürülüyor
    digitalWrite(LED_R, LOW);
    digitalWrite(LED_G, LOW);
    digitalWrite(LED_B, LOW);
    // LED durumu sönük olarak güncelleniyor
    ledState = false;
  }

  // ISR'den çıkışta kritik bölge kilidi serbest bırakılıyor
  portEXIT_CRITICAL_ISR(&timerMux);
}


// WiFi BAĞLANTI FONKSİYONU
// ESP32'yi Wokwi-GUEST ağına bağlar, bağlantı kurulana kadar bekler
void connectWiFi() {
  // Bağlantı denemesi başladığı seri monitöre yazdırılıyor
  Serial.print("WiFi'ye baglaniliyor: ");
  Serial.println(ssid);

  // Tanımlanan SSID ve şifre ile WiFi bağlantısı başlatılıyor
  WiFi.begin(ssid, password);

  // Bağlantı durumu WL_CONNECTED olana kadar döngüde bekleniyor
  while (WiFi.status() != WL_CONNECTED) {
    // Her 500ms'de bir nokta yazdırılarak bağlantı bekleniyor
    delay(500);
    Serial.print(".");
  }

  // Bağlantı başarıyla kurulduğunda bilgi mesajı yazdırılıyor
  Serial.println("\nWiFi baglantisi kuruldu!");
  // ESP32'ye atanan IP adresi seri monitöre yazdırılıyor
  Serial.print("IP Adresi: ");
  Serial.println(WiFi.localIP());
}


// MQTT MESAJ CALLBACK FONKSİYONU
// Abone olunan topic'e mesaj geldiğinde otomatik olarak çağrılır
// Node-RED dashboard butonundan gelen LED komutları burada işlenir

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  // Gelen byte dizisi (payload) okunabilir string'e dönüştürülüyor
  String message = "";
  // Payload'ın her byte'ı char'a çevrilerek string'e ekleniyor
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  // Hangi topic'ten hangi mesajın geldiği seri monitöre yazdırılıyor
  Serial.print("MQTT mesaji alindi [");
  Serial.print(topic);
  Serial.print("]: ");
  Serial.println(message);

  // Gelen mesaj "ON" ise LED yanıp sönme özelliği etkinleştiriliyor
  if (message == "ON") {
    // Shared değişkene erişimde kritik bölge kilidi alınıyor
    portENTER_CRITICAL(&timerMux);
    // LED etkin bayrağı true yapılarak ISR'nin LED'i yakmasına izin veriliyor
    ledEnabled = true;
    // Kritik bölge kilidi serbest bırakılıyor
    portEXIT_CRITICAL(&timerMux);
    Serial.println("LED aktif edildi.");
  }
  // Gelen mesaj "OFF" ise LED yanıp sönme özelliği devre dışı bırakılıyor
  else if (message == "OFF") {
    // Shared değişkene erişimde kritik bölge kilidi alınıyor
    portENTER_CRITICAL(&timerMux);
    // LED etkin bayrağı false yapılarak ISR'nin LED'i yakması engelleniyor
    ledEnabled = false;
    // Kritik bölge kilidi serbest bırakılıyor
    portEXIT_CRITICAL(&timerMux);
    // LED hemen söndürülüyor, ISR'nin bir sonraki çalışmasını beklemeye gerek yok
    digitalWrite(LED_R, LOW);
    digitalWrite(LED_G, LOW);
    digitalWrite(LED_B, LOW);
    Serial.println("LED devre disi birakildi.");
  }
}


// MQTT BAĞLANTI FONKSİYONU
// Broker'a bağlantı kurar, bağlantı koptuğunda yeniden bağlanır
void connectMQTT() {
  // Broker bağlantısı yoksa bağlanana kadar döngüde kalınıyor
  while (!client.connected()) {
    Serial.print("MQTT broker'a baglaniliyor...");

    // Tanımlanan client ID ile broker'a bağlantı kurulmaya çalışılıyor
    if (client.connect(mqtt_client_id)) {
      // Bağlantı başarılı olduğunda bilgi mesajı yazdırılıyor
      Serial.println(" Baglandi!");
      // LED kontrolü için Node-RED'den mesaj dinlenecek topic'e abone olunuyor
      client.subscribe(mqtt_sub_topic);
      Serial.print("Abone olundu: ");
      Serial.println(mqtt_sub_topic);
    } else {
      // Bağlantı başarısız olduğunda hata kodu yazdırılıyor
      Serial.print(" Basarisiz. Durum kodu: ");
      Serial.println(client.state());
      // 5 saniye beklendikten sonra tekrar bağlantı deneniyor
      Serial.println("5 saniye sonra tekrar deneniyor...");
      delay(5000);
    }
  }
}


// MESAFE ÖLÇÜM FONKSİYONU
// HC-SR04 ultrasonik sensörden cm cinsinden mesafe ölçer
// Çalışma prensibi: TRIG'e 10µs darbe gönderilir, ses dalgası yayılır,
// engelden yansıyan dalga ECHO'da algılanır, süre mesafeye çevrilir
float readDistance() {
  // TRIG pini önce LOW yapılarak olası parazit sinyaller temizleniyor
  digitalWrite(TRIG_PIN, LOW);
  // 2 mikrosaniye beklenerek pin stabil hale getirilir
  delayMicroseconds(2);

  // TRIG pini 10 mikrosaniye HIGH yapılarak ölçüm başlatma darbesi gönderiliyor
  digitalWrite(TRIG_PIN, HIGH);
  // Sensörün ses dalgası göndermesi için gereken 10µs bekleniyor
  delayMicroseconds(10);
  // TRIG pini tekrar LOW yapılarak darbe sonlandırılıyor
  digitalWrite(TRIG_PIN, LOW);

  // ECHO pininin HIGH kaldığı süre ölçülüyor, maksimum 30ms bekleniyor
  // Bu süre sesin gidip gelmesi için geçen zamandır
  long duration = pulseIn(ECHO_PIN, HIGH, 30000);

  // Süre 0 ise sensör yanıt vermedi veya nesne menzil dışında
  if (duration == 0) {
    // Hata durumunu belirtmek için -1 döndürülüyor
    return -1;
  }

  // Ölçülen süre santimetreye çevriliyor
  // Ses hızı = 0.0343 cm/µs, gidiş-dönüş olduğu için 2'ye bölünüyor
  float distance = duration * 0.0343 / 2.0;
  // Hesaplanan mesafe değeri döndürülüyor
  return distance;
}


// SETUP FONKSİYONU
// Mikrodenetleyici açıldığında veya resetlendiğinde bir kez çalışır
// Tüm başlangıç ayarları burada yapılır
void setup() {
  // Seri haberleşme 115200 baud hızında başlatılıyor (debug mesajları için)
  Serial.begin(115200);
  Serial.println("Sistem baslatiliyor...");

  // PIR sensör çıkış sinyali okunacağı için INPUT moduna alınıyor
  pinMode(PIR_PIN,  INPUT);
  // HC-SR04'e tetikleme sinyali gönderileceği için TRIG OUTPUT moduna alınıyor
  pinMode(TRIG_PIN, OUTPUT);
  // HC-SR04'ten yankı sinyali okunacağı için ECHO INPUT moduna alınıyor
  pinMode(ECHO_PIN, INPUT);
  // RGB LED kırmızı pinine sinyal gönderileceği için OUTPUT moduna alınıyor
  pinMode(LED_R,    OUTPUT);
  // RGB LED yeşil pinine sinyal gönderileceği için OUTPUT moduna alınıyor
  pinMode(LED_G,    OUTPUT);
  // RGB LED mavi pinine sinyal gönderileceği için OUTPUT moduna alınıyor
  pinMode(LED_B,    OUTPUT);

  // Başlangıçta RGB LED'in kırmızı pini söndürülüyor
  digitalWrite(LED_R, LOW);
  // Başlangıçta RGB LED'in yeşil pini söndürülüyor
  digitalWrite(LED_G, LOW);
  // Başlangıçta RGB LED'in mavi pini söndürülüyor
  digitalWrite(LED_B, LOW);

  // Wokwi simülasyonunda dış ağ bağlantısı için gerekli ayar
  WiFi.setHostname("wokwi-esp32");

  // WiFi bağlantısı kuruluyor
  connectWiFi();

  // MQTT broker adresi ve port numarası ayarlanıyor
  client.setServer(mqtt_server, mqtt_port);
  // Broker'dan mesaj geldiğinde çağrılacak callback fonksiyonu kaydediliyor
  client.setCallback(mqttCallback);

  // MQTT bağlantı zaman aşımı süresi 10 saniyeye çıkarılıyor
  client.setSocketTimeout(10);
  // MQTT keep-alive süresi ayarlanıyor
  client.setKeepAlive(60);

  // ESP32 donanım timer'ı 1MHz (1 mikrosaniye çözünürlük) frekansında başlatılıyor
  // ESP32 Arduino core v3 API'si kullanılıyor
  timer = timerBegin(1000000);

  // Tanımlanan ISR fonksiyonu timer kesmesine bağlanıyor
  timerAttachInterrupt(timer, &onTimer);

  // Timer alarmı 500.000 tick = 500ms aralıkla ve otomatik tekrarla ayarlanıyor
  // Son parametre (0): otomatik yeniden yükleme için başlangıç değeri
  timerAlarm(timer, 500000, true, 0);

  // Timer kurulumu tamamlandı mesajı yazdırılıyor
  Serial.println("Timer interrupt kuruldu (500ms).");
  Serial.println("Setup tamamlandi!");
}


// LOOP FONKSİYONU
// Setup tamamlandıktan sonra sürekli olarak tekrar tekrar çalışır
void loop() {
  // MQTT broker bağlantısı kopmuşsa yeniden bağlanma fonksiyonu çağrılıyor
  if (!client.connected()) {
    connectMQTT();
  }

  // MQTT kütüphanesinin iç döngüsü çalıştırılıyor
  // Bu çağrı gelen MQTT mesajlarını işler ve callback'i tetikler
  client.loop();

  // PIR sensörünün dijital çıkış değeri okunuyor
  // HIGH (1) = hareket algılandı, LOW (0) = hareket yok
  int pirValue = digitalRead(PIR_PIN);

  // PIR sensörü hareket algıladıysa mesafe ölçümü yapılıyor
  if (pirValue == HIGH) {
    // Hareket algılandığı seri monitöre yazdırılıyor
    Serial.println("Hareket algilandi! Mesafe olculuyor...");

    // HC-SR04'ten mesafe değeri cm cinsinden alınıyor
    float distance = readDistance();

    // Sensörden geçerli ölçüm alınamadıysa (timeout)
    if (distance < 0) {
      // Hata mesajı yazdırılıyor
      Serial.println("Mesafe olculemedi (timeout).");
      // LED yanıp sönmesi durduruluyor
      portENTER_CRITICAL(&timerMux);
      blinkActive = false;
      portEXIT_CRITICAL(&timerMux);
    }
    else {
      // Ölçülen mesafe değeri seri monitöre yazdırılıyor
      Serial.print("Mesafe: ");
      Serial.print(distance);
      Serial.println(" cm");

      // Float tipindeki mesafe değeri char dizisine dönüştürülüyor
      // MQTT payload olarak gönderilebilmesi için string formatına çevriliyor
      char payload[16];
      // dtostrf: float değeri 6 karakter genişliğinde, 2 ondalık basamakla char dizisine yazar
      dtostrf(distance, 6, 2, payload);

      // Mesafe değeri MQTT broker'a tanımlı topic üzerinden yayınlanıyor
      client.publish(mqtt_pub_topic, payload);
      // Yayınlanan mesaj seri monitöre yazdırılıyor
      Serial.print("MQTT publish: ");
      Serial.print(mqtt_pub_topic);
      Serial.print(" -> ");
      Serial.println(payload);

      // Mesafe 0 ile 200 cm arasında ise LED yanıp sönmesi aktif ediliyor
      if (distance >= 0 && distance <= 200) {
        // Kritik bölge kilidi alınarak blinkActive bayrağı güvenli şekilde true yapılıyor
        portENTER_CRITICAL(&timerMux);
        // Bu bayrak ISR tarafından kontrol edilerek LED toggle işlemi yapılacak
        blinkActive = true;
        portEXIT_CRITICAL(&timerMux);
        Serial.println("LED blink AKTIF (0-200 cm araliginda).");
      } else {
        // Mesafe 200 cm'den fazla ise LED yanıp sönmesi durdurluyor
        portENTER_CRITICAL(&timerMux);
        // ISR bir sonraki çalışmasında LED'i söndürecek
        blinkActive = false;
        portEXIT_CRITICAL(&timerMux);
        Serial.println("LED blink PASIF (200 cm ustu).");
      }
    }
  } else {
    // PIR sensörü hareket algılamıyorsa LED yanıp sönmesi durduruluyor
    portENTER_CRITICAL(&timerMux);
    // blinkActive false yapılarak ISR LED'i söndürüyor
    blinkActive = false;
    portEXIT_CRITICAL(&timerMux);
  }

  // Ana döngü 300ms bekliyor, bu sürede sensör okumaları kararlı hale geliyor
  delay(300);
}