# LADA DC PANEL

Bu proje, Lada Samara aracım için geliştirdiğim kontrol sistemidir. Farklı I2C cihazlarını, çevresel sensörleri ve bir LCD ekranı bir araya getirerek, kullanımı kolay ve etkili bir menü sisteminde sunduğum bu yapıyı oluşturduğumda amacım, pratik ve güvenilir bir çözüm elde etmekti.

## İçindekiler

- [Özellikler](#özellikler)
- [Bileşen Listesi](#bileşen-listesi)
- [Pin Bağlantıları](#pin-bağlantıları)
- [Sistem Fonksiyonları](#sistem-fonksiyonları)
- [Kurulum ve Yapılandırma](#kurulum-ve-yapılandırma)
- [Notlar ve İpuçları](#notlar-ve-ipuçları)

## Özellikler

- **Çoklu I2C Cihaz Desteği:**  
  - 16x2 LCD ekran, DS1307 RTC ve ADXL345 ivmeölçerin aynı I2C hattında senkronize çalışması.
- **Çevresel Sensörler:**  
  - DHT11 ile sıcaklık ve nem ölçümü.  
  - DS18B20 ile hassas sıcaklık ölçümü (OneWire).
- **Kullanıcı Arayüzü:**  
  - Potansiyometre ile yönlendirme; sağ, sol, yukarı ve aşağı hareket imkanı.  
  - Tek buton ile seçim ve geri hareket kolaylığı.
- **WiFi Yönetimi:**  
  - İlk başta hotspot modunda çalışarak, dahili WiFi Manager sayesinde ağ ayarlarını yapabilme.
- **Zaman Yönetimi:**  
  - DS1307 RTC ile otomatik saat senkronizasyonu sağlanması.
- **Dinamik Menü Sistemi:**  
  - Basit ve anlaşılır arayüz ile sistem kontrolü.

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

Aşağıdaki tabloda, kullandığım bileşenlerin NodeMCU üzerindeki varsayılan bağlantıları yer almaktadır:

| **Bileşen**               | **Bağlantı**         | **NodeMCU Pin (Varsayılan)**    |
|---------------------------|----------------------|---------------------------------|
| **16x2 LCD (I2C)**        | SDA, SCL             | D2 (SDA), D1 (SCL)              |
| **DS1307 RTC (I2C)**      | SDA, SCL             | D2 (SDA), D1 (SCL)              |
| **ADXL345 (I2C)**         | SDA, SCL             | D2 (SDA), D1 (SCL)              |
| **DHT11**                 | Data                 | D5 (GPIO14)                     |
| **DS18B20 (OneWire)**     | Data (+ 4.7kΩ pull-up)| D6 (GPIO12)                   |
| **Potansiyometre**        | Analog giriş         | A0                              |
| **Buton**                 | Dijital giriş        | D7 (GPIO13)                     |

> **Not:** I2C cihazlar aynı SDA ve SCL hatlarını kullandığından, modüllerde gerekli pull-up dirençlerinin olduğundan emin olunuz.

## Sistem Fonksiyonları

- **Navigasyon:**  
  Potansiyometre ile menüde sağa, sola, yukarı ve aşağı yönlerde kolayca gezinme imkanı.
  
- **Seçim ve Geri İşlemleri:**  
  Tek buton kullanımıyla basit seçim ve geri dönüş işlemleri sağlanmaktadır.

- **WiFi Yönetimi:**  
  İlk başlatmada cihaz hotspot moduna geçer; dahili WiFi Manager sayesinde yerel ağa hızlıca bağlanabilirsiniz.

- **Zaman Senkronizasyonu:**  
  DS1307 RTC modülü ile sistem saati otomatik olarak ayarlanır.

## Kurulum ve Yapılandırma

1. **Donanım Bağlantıları:**  
   Yukarıdaki tablodaki bağlantılara uygun olarak tüm bileşenleri NodeMCU’ya bağlayın.

2. **Kod Yükleme:**  
   Gerekli kütüphaneler (`LiquidCrystal_I2C`, `RTClib`, `Adafruit_ADXL345`, `DHT`, `OneWire`, `DallasTemperature`, `WiFiManager`) kullanılarak oluşturulan Arduino projesini yükleyin. Bu sayede tüm bileşenler uyum içinde çalışacaktır.

3. **İlk Açılış ve WiFi Ayarları:**  
   NodeMCU başlatıldığında cihaz hotspot modunda açılır. WiFi Manager arayüzüne bağlanıp, WiFi ayarlarını yapmayı unutmayın.

4. **Test ve Kalibrasyon:**  
   Menü navigasyonu, sensör okumaları ve diğer fonksiyonları test ederek sistemin sorunsuz çalıştığından emin olun.

## Notlar ve İpuçları

- **Güç ve Voltaj Yönetimi:**  
  I2C cihazların çalışma voltajlarına dikkat edin. NodeMCU 3.3V çıkış verdiği için modüllerin uyumlu olması veya uygun seviye dönüştürücü kullanılması gerekebilir.

- **Özgün Tasarım:**  
  Projenin temelinde basitlikle birlikte işlevsellik yatar. Sistemi uzun ömürlü ve güvenilir kılmak için sade ama etkili çözümler tercih edilmiştir.

- **Sorun Giderme:**  
  Herhangi bir bileşen çalışmadığında bağlantıları, güç kaynaklarını ve kütüphane uyumluluklarını yeniden gözden geçirin.

- **Güvenilirlik:**  
  Aracınızın içerisindeki elektronik sistemlerin stabil çalışması uzun vadeli başarının anahtarıdır.

