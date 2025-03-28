# NodeMCU I2C Multi-Sensör & LCD Kontrol Sistemi

Bu proje, Lada Samara aracınızın radyo kısmına yerleştirilecek yenilikçi bir kontrol sistemidir.
Sistem, çeşitli I2C bileşenleri ve çevresel sensörlerin yanı sıra, kullanımı sezgisel bir menü arayüzü sunar. İlk açılışta hotspot olarak çalışır ve dahili WiFi Manager sayesinde kablosuz ağ ayarlarını kolayca yapabilirsiniz.

## İçindekiler

- [Özellikler](#özellikler)
- [Bileşen Listesi](#bileşen-listesi)
- [Pin Bağlantıları](#pin-bağlantıları)
- [Sistem Fonksiyonları](#sistem-fonksiyonları)
- [Kurulum ve Yapılandırma](#kurulum-ve-yapılandırma)
- [Notlar ve İpuçları](#notlar-ve-ipuçları)

## Özellikler

- **Çoklu I2C Cihaz Desteği:**  
  - 16x2 LCD ekran, DS1307 RTC ve ADXL345 ivmeölçer aynı I2C hattında çalışır.
- **Çevresel Sensörler:**  
  - DHT11 ile sıcaklık ve nem ölçümü.  
  - DS18B20 ile hassas sıcaklık ölçümü (OneWire).
- **Kullanıcı Arayüzü:**  
  - Potansiyometre ile sağa, sola, yukarı, aşağı menü kontrolleri.  
  - Buton ile seçim yapma ve geri dönüş işlemleri.
- **WiFi Yönetimi:**  
  - İlk açılışta hotspot modunda çalışır, WiFi Manager sayesinde yerel ağa kolayca entegre edilir.
- **Zaman Yönetimi:**  
  - DS1307 RTC sayesinde otomatik saat senkronizasyonu.
- **Dinamik Menü Sistemi:**  
  - Kullanıcı dostu ve sezgisel arayüz ile kontrol imkanı.

## Bileşen Listesi

- **NodeMCU ESP8266**
- **16x2 LCD Ekran (I2C)**
- **DS1307 RTC Modülü (I2C)**
- **ADXL345 İvmeölçer (I2C)**
- **DHT11 Sıcaklık ve Nem Sensörü**
- **DS18B20 Su Geçirmez Sıcaklık Sensörü (OneWire)**
- **Potansiyometre**
- **Buton**

## Pin Bağlantıları

Aşağıdaki tablo, bileşenlerin NodeMCU üzerindeki varsayılan bağlantılarını göstermektedir:

| **Bileşen**               | **Bağlantı**         | **NodeMCU Pin (Varsayılan)**    |
|---------------------------|----------------------|---------------------------------|
| **16x2 LCD (I2C)**        | SDA, SCL             | D2 (SDA), D1 (SCL)              |
| **DS1307 RTC (I2C)**      | SDA, SCL             | D2 (SDA), D1 (SCL)              |
| **ADXL345 (I2C)**         | SDA, SCL             | D2 (SDA), D1 (SCL)              |
| **DHT11**                 | Data                 | D5 (GPIO14)                     |
| **DS18B20 (OneWire)**     | Data (+ 4.7kΩ pull-up)| D6 (GPIO12)                   |
| **Potansiyometre**        | Analog giriş         | A0                              |
| **Buton**                 | Dijital giriş        | D7 (GPIO13)                     |

> **Not:** I2C cihazlar SDA ve SCL hatlarını paylaşır. Eğer modüllerinizde gerekli pull-up dirençler yoksa, eklemeniz gerekebilir.

## Sistem Fonksiyonları

- **Navigasyon:**  
  Potansiyometre, menüde sağa, sola, yukarı ve aşağı hareket etmenizi sağlar. Bu sayede, menüler arasında doğal ve akıcı bir geçiş mümkün olur.

- **Seçim ve Geri İşlemleri:**  
  Tek bir buton, menüde seçim yapmanızı ve geri dönüş işlemlerini gerçekleştirmenizi sağlayarak, sistemde sadelik ve pratiklik sunar.

- **WiFi Yönetimi:**  
  İlk açılışta cihaz hotspot moduna geçer. Dahili WiFi Manager ile cihazınızı yerel WiFi ağına kolayca bağlayabilirsiniz.

- **Zaman Senkronizasyonu:**  
  DS1307 RTC modülü, sistemin doğru saat bilgisiyle çalışmasını garanti eder.

## Kurulum ve Yapılandırma

1. **Donanım Bağlantıları:**  
   Yukarıdaki tabloyu referans alarak tüm bileşenlerinizi NodeMCU’ya bağlayın.

2. **Kod Yükleme:**  
   Gerekli kütüphaneleri (örn. `LiquidCrystal_I2C`, `RTClib`, `Adafruit_ADXL345`, `DHT`, `OneWire`, `DallasTemperature`, `WiFiManager`) içeren Arduino IDE projesini oluşturun. Kod, tüm bileşenlerin entegre çalışmasını sağlayacak şekilde yazılmıştır.

3. **İlk Açılış ve WiFi Konfigürasyonu:**  
   NodeMCU’yu ilk başlattığınızda cihaz hotspot modunda çalışacaktır. WiFi Manager arayüzüne bağlanarak, cihazınızı yerel ağınıza entegre edin.

4. **Test ve Kalibrasyon:**  
   Menü navigasyonunu, sensör okumalarını ve diğer fonksiyonları test ederek sistemin sorunsuz çalıştığından emin olun.

## Notlar ve İpuçları

- **Güç ve Voltaj Yönetimi:**  
  I2C cihazlarınızın çalışma voltajlarına dikkat edin. NodeMCU 3.3V çıkış verir; modüllerinizin uyumlu olması ya da uygun seviye dönüştürücü kullanmanız önemlidir.

- **Yenilikçi Tasarım:**  
  Basit ama etkili bir arayüz, minimalizm ve fonksiyonellik ön planda tutulmuştur. Bu, projenin uzun ömürlü ve dayanıklı olmasını sağlar.

- **Sorun Giderme:**  
  Herhangi bir bileşen çalışmıyorsa, bağlantıları, güç beslemesini ve kütüphane uyumluluklarını kontrol edin.

- **Güvenilirlik:**  
  Aracınızın içindeki elektronik sistemlerde sağlam ve stabil bir yapı, uzun vadeli kullanımda başarının anahtarıdır. Tasarımda sade, pratik ve ileri görüşlü yaklaşımlar benimsendi.

