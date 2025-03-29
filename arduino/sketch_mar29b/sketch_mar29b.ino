#include <Wire.h>
#include <DHT.h>
#include <LiquidCrystal_I2C.h>
#include <RTClib.h>             // RTClib: https://github.com/adafruit/RTClib
#include <Adafruit_Sensor.h>
#include <Adafruit_ADXL345_U.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <EEPROM.h>
#include <math.h>
#include <ElegantOTA.h>           // Yeni: ElegantOTA kütüphanesi
#include <WiFiManager.h>          // Yeni: WiFiManager kütüphanesi
#include <ESP8266WebServer.h>     // Yeni: ESP8266 Web Server

// -------------------- SENSÖR VE MODÜL TANIMLAMALARI --------------------

// DHT11
#define DHTPIN D5               // NodeMCU: D5
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// DS18B20 (otomatik mod için)
#define DS18B20_PIN D2          // DS18B20 veri hattı
OneWire oneWire(DS18B20_PIN);
DallasTemperature ds18(&oneWire);

// Yeni su sıcaklığı DS18B20 sensörü tanımlamaları:
#define DS18WATER_PIN D6          // Su sıcaklığı için DS18B20
OneWire oneWireWater(DS18WATER_PIN);
DallasTemperature waterSensor(&oneWireWater);

// Rotary Encoder
#define ENCODER_CLK D4
#define ENCODER_DT  D8
#define ENCODER_SW  D7

// Buzzer (D0)
#define BUZZER_PIN D0

// Röle (D3) – Aktif LOW!
#define RELAY_PIN D3

// RTC (DS1307) – Her yüklemede bilgisayar saatiyle güncelle
RTC_DS1307 rtc;

// I2C LCD (0x27, 16x2)
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ADXL345
Adafruit_ADXL345_Unified accel = Adafruit_ADXL345_Unified(12345);

// Web server nesnesi
ESP8266WebServer server(80);

// -------------------- EEPROM & KONFIGÜRASYON --------------------
/*
  EEPROM’da 0-7: Röle Konfigürasyonu (6 byte)
    byte 0: Röle mod (0 = MANUEL, 1 = OTOMATİK)
    byte 1: Sensör seçimi (0 = DHT11, 1 = DS18B20)
    float  : Limit (4 byte) – otomatik mod için sıcaklık limiti (C)
    byte 5: Röle durumu (0 = kapalı, 1 = açık)
  
  EEPROM’da 8-15: Arka Işık Konfigürasyonu (4 byte)
    byte 8: Arka ışık mod (0 = MANUEL, 1 = OTOMATİK)
    byte 9: Arka ışık durumu (0 = kapalı, 1 = açık) (MANUEL)
    int (2 byte): Arka ışık için sabit ayar (0 = kapalı, 1 = açık)
*/
#define EEPROM_SIZE 32

struct RelayConfig {
  uint8_t mode;    // 0: MANUEL, 1: OTOMATİK
  uint8_t sensor;  // 0: DHT11, 1: DS18B20
  float limit;     // Sıcaklık limiti (C)
  uint8_t state;   // 0: kapalı, 1: açık
};
RelayConfig relayConfig;

struct BacklightConfig {
  uint8_t mode;    // 0: MANUEL, 1: OTOMATİK
  uint8_t state;   // 0: kapalı, 1: açık (MANUEL)
  int fixedVal;    // Otomatik modda kullanılacak sabit ayar (0 = kapalı, 1 = açık)
};
BacklightConfig backlightConfig;

// Yeni: Başlangıçtan itibaren tüm seri mesajları için bayrak (startup bitince false olacak)
bool debugEnabled = true;

// WiFi kimlik bilgileri global olarak tanımlanıyor
const char* wifiSSID = "Kerim's Lada";
const char* wifiPassword = "k3r1mL4dA";

// Yeni WiFi ekranı için değişkenler
unsigned long lastWifiUpdate = 0;
bool displayWifiCredentials = true;

// -------------------- ÖZEL KARAKTERLER --------------------
// Toplam 8 özel karakter (0-7)

// İkonlar:
// 3: Termometre (ORTAM)
byte thermoIcon[8] = {
  0b00100,
  0b01010,
  0b01010,
  0b01110,
  0b01110,
  0b11111,
  0b11111,
  0b00000
};
// 4: Saat (TARIH/SAAT)
byte clockIcon[8] = {
  0b00000,
  0b01110,
  0b10101,
  0b10101,
  0b10101,
  0b10101,
  0b01110,
  0b00000
};
// 5: İvmeölçer (ARAÇ VERİ)
byte accelIcon[8] = {
  0b00100,
  0b01110,
  0b11111,
  0b11111,
  0b11111,
  0b01110,
  0b00100,
  0b00000
};
// 6: Röle (RÖLE KONTROL)
byte relayIcon[8] = {
  0b11111,
  0b10001,
  0b11011,
  0b10101,
  0b11011,
  0b10001,
  0b10001,
  0b11111
};
// 7: Araba (Splash)
byte carIcon[8] = {
  0b00000,
  0b00100,
  0b01110,
  0b10101,
  0b10101,
  0b01110,
  0b00100,
  0b00000
};

// -------------------- GLOBAL DEĞİŞKENLER --------------------
/*
  Ekranlar:
    0: Ortam (DHT11)
    1: Tarih/Saat (RTC, saniye hariç)
    2: ADXL345 (Araç verileri: anlık hız)
    3: Röle Kontrol
    4: Röle Ayar Menüsü
    5: Arka Işık Ayar Menüsü
    6: Su Sıcaklığı (DS18B20 D6)
    7: Röle Limit Ayarlama
    8: WiFi Bilgi Ekranı (Yeni)
*/
int currentScreen = 0;
int lastEncoderClk = HIGH;
int encoderCount = 0;
const int encoderThreshold = 6; // Daha fazla pulse gerekiyorsa

bool transitionMode = false;
unsigned long transitionStart = 0;
const unsigned long transitionTimeout = 2000; // 2 saniye

unsigned long lastDHTUpdate   = 0;  // ms
unsigned long lastTimeUpdate  = 0;
unsigned long lastAccelUpdate = 0;
unsigned long lastRelayUpdate = 0;
unsigned long lastSettingsUpdate = 0; // Ayar menüsü için (3000ms)
unsigned long lastWaterUpdate = 0; // Su sıcaklığı için zamanlayıcı

// ADXL345: Basit anlık hız (m/s), km/h = m/s * 3.6
float estimatedSpeed = 0.0;
unsigned long lastAccelReadTime = 0;

// Encoder buton uzun/kısa basımı
bool buttonActive = false;
unsigned long buttonPressStart = 0;
const unsigned long longPressDuration = 500; // 500 ms üzeri uzun basım

// -------------------- FONKSİYON PROTOTİPLERİ --------------------
ICACHE_FLASH_ATTR void splashScreen();
ICACHE_FLASH_ATTR void transitionEffect();
ICACHE_FLASH_ATTR void showRelaySettingsMenu();
ICACHE_FLASH_ATTR void showBacklightSettingsMenu();
ICACHE_FLASH_ATTR void debugInfo();
void handleVeriSayfasi();
void handleVeriGuncelle();

// -------------------- SETUP --------------------
void setup() {
  Serial.begin(115200);
  Serial.println("Proje Baslatiliyor: Ortam, Tarih/Saat, ADXL345, Role, Ayar Menusu, Arka Isik");
  
  // Yeni: Başlangıçta tüm input durumları kapalı ve LCD ilk açılış ekranı
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("LCD Basladi");
  delay(500);
  
  // Relay aktif LOW olduğundan kapalı için HIGH, buzzer kapalı
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  // Diğer input’lar kapalı (varsa eklenen durumlar burada)
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Tum inputlar");
  lcd.setCursor(0, 1);
  lcd.print("Kapali");
  delay(1000);

  // WiFi başlatma: STA modu, autoReconnect ile asenkron çalışma sağlanıyor
  WiFi.mode(WIFI_STA);
  WiFiManager wifiManager;
  wifiManager.setConfigPortalTimeout(10); // config portal 10 saniye sonra kapanır
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Wireless islemleri");
  unsigned long startTime = millis();
  bool wifiConnected = false;
  int spinnerIndex = 0;
  char spinnerChars[] = "-\\|/";
  while ((millis() - startTime) < 10000 && !wifiConnected) {
      wifiConnected = wifiManager.autoConnect(wifiSSID, wifiPassword);
      lcd.setCursor(0, 1);
      lcd.print("Bekleniyor: ");
      lcd.print(spinnerChars[spinnerIndex]);
      spinnerIndex = (spinnerIndex + 1) % 4;
      delay(200);
  }
  if (wifiConnected) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("WiFi Saglandi!");
      delay(1000);
  } else {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("WiFi Saglanamadi");
      delay(1000);
  }
  WiFi.setAutoReconnect(true);

  // Yeni: ElegantOTA başlatma (server referansı ile)
  ElegantOTA.begin(&server);
  server.begin();
  dht.begin();
  ds18.begin();
  waterSensor.begin(); // Yeni su sensörü başlatılıyor
  accel.begin();
  accel.setRange(ADXL345_RANGE_16_G);
  
  // Encoder pinleri (INPUT_PULLUP kullanılıyor)
  pinMode(ENCODER_CLK, INPUT_PULLUP);
  pinMode(ENCODER_DT, INPUT_PULLUP);
  pinMode(ENCODER_SW, INPUT_PULLUP);
  lastEncoderClk = digitalRead(ENCODER_CLK);
  
  // Buzzer
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  
  // Role (aktif LOW)
  pinMode(RELAY_PIN, OUTPUT);
  
  // LCD
  lcd.init();
  lcd.backlight();
  lcd.clear();
  
  // Özel karakterleri yükle (3: Termometre, 4: Saat, 5: İvmeölçer, 6: Röle, 7: Araba)
  lcd.createChar(3, thermoIcon);
  lcd.createChar(4, clockIcon);
  lcd.createChar(5, accelIcon);
  lcd.createChar(6, relayIcon);
  lcd.createChar(7, carIcon);
  
  // Splash ekranı: Araba ikonu soldan sağa hareket eder
  splashScreen();
  
  // RTC: Her yüklemede bilgisayar saatine göre güncelle
  if (!rtc.begin()) {
    Serial.println("RTC Bulunamadi!");
    lcd.setCursor(0,0);
    lcd.print("RTC Yok!");
  } else {
    Serial.println("RTC Guncelleniyor...");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
  
  // EEPROM’dan ayarlari yukle
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.get(0, relayConfig);
  if (relayConfig.mode > 1) {
    relayConfig.mode = 0;
    relayConfig.sensor = 0;
    relayConfig.limit = 25.0;
    relayConfig.state = 0;
  }
  digitalWrite(RELAY_PIN, relayConfig.state ? LOW : HIGH);
  
  EEPROM.get(8, backlightConfig);
  if (backlightConfig.mode > 1) {
    backlightConfig.mode = 0;
    backlightConfig.state = 1;  // Varsayılan MANUEL: açık
    backlightConfig.fixedVal = 1; // Varsayılan: ışık açık
  }
  
  lastAccelReadTime = millis();
  debugInfo(); // Yeni debug mesajı cagriliyor
  // Startup tamamlandıktan sonra debug mesajları kapatılsın:
  debugEnabled = false;
}

// -------------------- LOOP --------------------
void loop() {
  unsigned long currentMillis = millis();
  
  // Asenkron WiFi kontrolü: Bağlantı kesilmişse yeniden bağlanmaya çalış
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.reconnect();
  }
  
  server.handleClient();
  ElegantOTA.loop();

  // Rotary Encoder pulse okuma
  int currentClk = digitalRead(ENCODER_CLK);
  if (currentClk != lastEncoderClk) {
    if (digitalRead(ENCODER_DT) != currentClk) {
      encoderCount++;
    } else {
      encoderCount--;
    }
    lastEncoderClk = currentClk;
    // Geçiş modu başlangıç zamanını güncelle
    if (!transitionMode && abs(encoderCount) >= encoderThreshold) {
      transitionMode = true;
      transitionStart = currentMillis;
      encoderCount = 0;
    }
  }
  
  // Geçiş modu timeout kontrolü
  if (transitionMode && (currentMillis - transitionStart > transitionTimeout)) {
    transitionMode = false;
    encoderCount = 0;
    transitionEffect();  // Blink efekti
  }
  
  // Eğer geçiş modu aktif ve encoderCount threshold'a ulaşırsa, ekran geçişi
  if (transitionMode && abs(encoderCount) >= encoderThreshold) {
    if (encoderCount > 0) {
      currentScreen++;
      if (currentScreen > 8) currentScreen = 0;
    } else {
      currentScreen--;
      if (currentScreen < 0) currentScreen = 8;
    }
    transitionMode = false;
    encoderCount = 0;
    transitionEffect();
  }
  
  // Encoder butonuna basım kontrolü (kısa/uzun)
  int buttonState = digitalRead(ENCODER_SW);
  if (buttonState == LOW && (millis() - buttonPressStart > 50)) {
    if (!buttonActive) {
      buttonActive = true;
      buttonPressStart = millis();
    }
  } else if (buttonActive && buttonState == HIGH) {
    unsigned long pressDuration = millis() - buttonPressStart;
    if (currentScreen == 4) {  // Role Ayar Menüsü: mod seçimi
      if (pressDuration < longPressDuration) {
        // Kısa basım: mod sırasıyla artar (0->1->2->0)
        relayConfig.sensor = (relayConfig.sensor + 1) % 3;
        relayConfig.mode = (relayConfig.sensor == 0) ? 0 : 1;
        if(debugEnabled) { Serial.print("Role Mod Secildi: "); }
        if (relayConfig.sensor == 0)
          if(debugEnabled) { Serial.println("Manuel"); }
        else if (relayConfig.sensor == 1)
          if(debugEnabled) { Serial.println("Dht11"); }
        else if (relayConfig.sensor == 2)
          if(debugEnabled) { Serial.println("Ds18b20"); }
      } else {
        // Uzun basım: MANUEL dışı modda limit ayarlama ekranına geçiş;
        // MANUEL modda röle durumu toggle
        if (relayConfig.sensor != 0) {
          currentScreen = 7;
        } else {
          relayConfig.state = !relayConfig.state;
          if(debugEnabled) {
            Serial.print("Manuel Role: ");
            Serial.println(relayConfig.state ? "Acik" : "Kapali");
          }
          digitalWrite(RELAY_PIN, relayConfig.state ? LOW : HIGH);
        }
      }
      EEPROM.put(0, relayConfig);
      EEPROM.commit();
    } else if (currentScreen == 7) {
      // Limit ayarlama ekranındayken basım; yeni limit kaydedilip rol ayar ekranına dönülür.
      EEPROM.put(0, relayConfig);
      EEPROM.commit();
      currentScreen = 4;
      if(debugEnabled) {
        Serial.print("Limit Secildi: ");
        Serial.println(relayConfig.limit, 1);
      }
    } else if (currentScreen == 5) {
      // Arka Işık Ayar Menüsü
      if (pressDuration >= longPressDuration) {
        backlightConfig.mode = (backlightConfig.mode == 0) ? 1 : 0;
        if(debugEnabled) {
          Serial.print("Arka Isik Mod: ");
          Serial.println(backlightConfig.mode == 0 ? "Manuel" : "Otomatik");
        }
      } else {
        if (backlightConfig.mode == 0) {
          backlightConfig.state = !backlightConfig.state;
          if(debugEnabled) {
            Serial.print("Manuel Arka Isik: ");
            Serial.println(backlightConfig.state ? "Acik" : "Kapali");
          }
          if (backlightConfig.state)
            lcd.backlight();
          else
            lcd.noBacklight();
        } else {
          // Otomatik modda, sabit ayarı encoder ile ayarla (0 veya 1)
          backlightConfig.fixedVal = (encoderCount > 0) ? 1 : 0;
          if(debugEnabled) {
            Serial.print("Arka Isik Sabit Ayari: ");
            Serial.println(backlightConfig.fixedVal ? "Acik" : "Kapali");
          }
          encoderCount = 0;
        }
      }
      EEPROM.put(8, backlightConfig);
      EEPROM.commit();
    } else {
      // Eğer geçerli sayfa, ayar ekranı değilse
      if (relayConfig.mode == 0) {  // Manuel modda ise
        relayConfig.state = !relayConfig.state;
        if(debugEnabled) {
          Serial.print("Manuel Role (Genel): ");
          Serial.println(relayConfig.state ? "Acik" : "Kapali");
        }
        digitalWrite(RELAY_PIN, relayConfig.state ? LOW : HIGH);
        EEPROM.put(0, relayConfig);
        EEPROM.commit();
      } else {
        tone(BUZZER_PIN, 1000, 200);
      }
    }
    buttonActive = false;
    delay(300);
  }
  
  // Ekran içerikleri:
  switch (currentScreen) {
    case 0: {  // Ortam: DHT11
      if (currentMillis - lastDHTUpdate >= 2000) {
        lastDHTUpdate = currentMillis;
        float hum = dht.readHumidity();
        float temp = dht.readTemperature();
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.write(byte(3)); // Termometre İkonu
        lcd.print(" Ortam");
        lcd.setCursor(0, 1);
        if (isnan(hum) || isnan(temp)) {
          lcd.print("Okuma Hatasi!");
        } else {
          lcd.print("T:");
          lcd.print(temp, 0);
          lcd.print("C  N:");
          lcd.print(hum, 0);
          lcd.print("%");
        }
      }
      break;
    }
    case 1: {  // Tarih/Saat (RTC, saniye hariç)
      if (currentMillis - lastTimeUpdate >= 2000) {
        lastTimeUpdate = currentMillis;
        DateTime now = rtc.now();
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.write(byte(4)); // Saat İkonu
        lcd.print(" Tarih/Saat");
        lcd.setCursor(0, 1);
        char buf[17];
        // Format: gg/aa/yyyy HH:MM
        snprintf(buf, sizeof(buf), "%02d/%02d/%04d %02d:%02d", now.day(), now.month(), now.year(), now.hour(), now.minute());
        lcd.print(buf);
      }
      break;
    }
    case 2: {  // ADXL345: Araç verisi – Anlık Hız (km/h)
      if (currentMillis - lastAccelUpdate >= 500) {
        lastAccelUpdate = currentMillis;
        sensors_event_t event;
        accel.getEvent(&event);
        float ax = event.acceleration.x;
        float ay = event.acceleration.y;
        float az = event.acceleration.z;
        unsigned long nowTime = millis();
        float dt = (nowTime - lastAccelReadTime) / 1000.0;
        lastAccelReadTime = nowTime;
        float totalAcc = sqrt(ax * ax + ay * ay + az * az);
        float netAcc = totalAcc - 9.81;
        if (netAcc < 0.1) {
          estimatedSpeed = 0;
        } else {
          estimatedSpeed += netAcc * dt;
        }
        float speedKmh = estimatedSpeed * 3.6;
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.write(byte(5)); // Ivmeölçer İkonu
        lcd.print(" Arac Verisi");
        lcd.setCursor(0, 1);
        lcd.print("Hiz:");
        lcd.print(speedKmh, 1);
        lcd.print(" km/h");
      }
      break;
    }
    case 3: {  // Röle Kontrol ekranı
      if (currentMillis - lastRelayUpdate >= 1000) {
        lastRelayUpdate = currentMillis;
        if (relayConfig.mode == 1) {
          // Eğer sensor secimi MANUEL değilse otomatik tetikleme yap
          if (relayConfig.sensor != 0) {
            float measuredTemp;
            if (relayConfig.sensor == 1) {
              measuredTemp = dht.readTemperature();
            } else if (relayConfig.sensor == 2) {
              ds18.requestTemperatures();
              measuredTemp = ds18.getTempCByIndex(0);
            }
            if (!isnan(measuredTemp)) {
              if (measuredTemp >= relayConfig.limit && relayConfig.state == 0) {
                relayConfig.state = 1;
                digitalWrite(RELAY_PIN, LOW);
                EEPROM.put(0, relayConfig);
                EEPROM.commit();
              } else if (measuredTemp < relayConfig.limit && relayConfig.state == 1) {
                relayConfig.state = 0;
                digitalWrite(RELAY_PIN, HIGH);
                EEPROM.put(0, relayConfig);
                EEPROM.commit();
              }
            }
          }
        }
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Role Kontrol");
        lcd.setCursor(0, 1);
        if (relayConfig.mode == 0) {
          lcd.print("Mod: Manuel");
          lcd.setCursor(0, 1);
          lcd.print("Durum: ");
          lcd.print(relayConfig.state ? "Acik" : "Kapali");
        } else {
          lcd.print("Mod: Otomatik");
          lcd.setCursor(0, 1);
          lcd.print("Sens: ");
          if (relayConfig.sensor == 0)
            lcd.print("Manuel");
          else if (relayConfig.sensor == 1)
            lcd.print("Dht11");
          else if (relayConfig.sensor == 2)
            lcd.print("Ds18b20");
          lcd.print(" Lim: ");
          lcd.print(relayConfig.limit, 1);
          lcd.print("C");
        }
      }
      break;
    }
    case 4: {  // Röle Ayar Menüsü
      if (currentMillis - lastSettingsUpdate >= 3000) {
        lastSettingsUpdate = currentMillis;
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Role Ayarlari");
        lcd.setCursor(0, 1);
        lcd.print("Mod: ");
        lcd.print(relayConfig.mode == 0 ? "Manuel" : "Otomatik");
        lcd.setCursor(0, 2);
        if (relayConfig.mode == 0) {
          lcd.print("Durum: ");
          lcd.print(relayConfig.state ? "Acik" : "Kapali");
        } else {
          lcd.print("Sens: ");
          if (relayConfig.sensor == 0)
            lcd.print("Manuel");
          else if (relayConfig.sensor == 1)
            lcd.print("Dht11");
          else if (relayConfig.sensor == 2)
            lcd.print("Ds18b20");
          lcd.print(" Lim: ");
          lcd.print(relayConfig.limit, 1);
          lcd.print("C");
        }
      }
      break;
    }
    case 5: {  // Arka Işık Ayar Menüsü
      if (currentMillis - lastSettingsUpdate >= 3000) {
        lastSettingsUpdate = currentMillis;
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Arka Isik Ayarlari");
        lcd.setCursor(0, 1);
        lcd.print("Mod: ");
        lcd.print(backlightConfig.mode == 0 ? "Manuel" : "Otomatik");
        lcd.setCursor(0, 2);
        if (backlightConfig.mode == 0) {
          lcd.print("Durum: ");
          lcd.print(backlightConfig.state ? "Acik" : "Kapali");
        } else {
          lcd.print("Sabit: ");
          lcd.print(backlightConfig.fixedVal ? "Acik" : "Kapali");
        }
      }
      break;
    }
    case 6: {  // Su Sıcaklığı (DS18B20 D6)
      if (currentMillis - lastWaterUpdate >= 2000) {
        lastWaterUpdate = currentMillis;
        waterSensor.requestTemperatures();
        float waterTemp = waterSensor.getTempCByIndex(0);
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Su Sicakligi");
        lcd.setCursor(0, 1);
        if (isnan(waterTemp)) {
          lcd.print("Olcum Hatasi!");
        } else {
          lcd.print("T:");
          lcd.print(waterTemp, 1);
          lcd.print("C");
        }
      }
      break;
    }
    case 7: {  // Röle Limit Ayarlama ekranı
      if (currentMillis - lastSettingsUpdate >= 500) {
        lastSettingsUpdate = currentMillis;
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Limit Ayarla");
        lcd.setCursor(0, 1);
        lcd.print("T:");
        lcd.print(relayConfig.limit, 1);
        lcd.print("C");
        if (encoderCount != 0) {
          relayConfig.limit += 0.5 * encoderCount;  // 0.5 adım
          encoderCount = 0;
          lcd.setCursor(0, 1);
          lcd.print("T:");
          lcd.print(relayConfig.limit, 1);
          lcd.print("C");
        }
      }
      break;
    }
    case 8: {  // Yeni: WiFi Bilgi Ekranı
      if (currentMillis - lastWifiUpdate >= 2000) {
        lastWifiUpdate = currentMillis;
        displayWifiCredentials = !displayWifiCredentials;
      }
      lcd.clear();
      if (displayWifiCredentials) {
        lcd.setCursor(0, 0);
        lcd.print("WiFi: ");
        lcd.print(wifiSSID);
        lcd.setCursor(0, 1);
        lcd.print("Pwd: ");
        lcd.print(wifiPassword);
      } else {
        lcd.setCursor(0, 0);
        lcd.print("IP Adresi:");
        lcd.setCursor(0, 1);
        lcd.print(WiFi.localIP());
      }
      break;
    }
  }
}

// -------------------- GEÇİŞ EFEKTI --------------------
// Basit blink efekti
ICACHE_FLASH_ATTR void transitionEffect() {
  for (int i = 0; i < 3; i++) {
    lcd.noBacklight();
    delay(100);
    lcd.backlight();
    delay(100);
  }
}

// -------------------- SPLASH EKRANI --------------------
// Araba ikonu soldan sağa hareket eder
ICACHE_FLASH_ATTR void splashScreen() {
  for (int pos = 0; pos < 16; pos++) {
    lcd.clear();
    lcd.setCursor(pos, 0);
    lcd.write(byte(7)); // Araba ikonu
    delay(100);
  }
  // Animasyon tamamlandıktan sonra, alt satırda ortada Hosgeldiniz yazısı
  lcd.clear();
  lcd.setCursor(2, 1);
  lcd.print("Hosgeldiniz");
  delay(1000);
  lcd.clear();
}

// -------------------- DEBUG INFO --------------------
// Seri port üzerinden debug mesajı
ICACHE_FLASH_ATTR void debugInfo() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Debug: Program");
  lcd.setCursor(0, 1);
  lcd.print("Basladi, Calisiyor.");
  delay(1000);
}

// Yeni: Arac verileri ve Röle Ayarlarini gösteren Türkçe web sayfası
void handleVeriSayfasi() {
  // Sensör okuma islemleri
  float dhtHum = dht.readHumidity();
  float dhtTemp = dht.readTemperature();
  ds18.requestTemperatures();
  float ds18Temp = ds18.getTempCByIndex(0);
  waterSensor.requestTemperatures();
  float waterTemp = waterSensor.getTempCByIndex(0);
  sensors_event_t event;
  accel.getEvent(&event);
  float ax = event.acceleration.x;
  float ay = event.acceleration.y;
  float az = event.acceleration.z;
  float totalAcc = sqrt(ax * ax + ay * ay + az * az);
  DateTime now = rtc.now();
  
  String html = "<html><head><meta charset='UTF-8'><title>Arac Verileri</title></head><body>";
  html += "<h1>Arac Verileri</h1>";
  html += "<h2>Sensör Olcumleri</h2><ul>";
  html += "<li>DHT11: Sicaklik = " + String(dhtTemp, 1) + " C, Nem = " + String(dhtHum, 1) + " %</li>";
  html += "<li>DS18B20 (Otomatik): Sicaklik = " + String(ds18Temp, 1) + " C</li>";
  html += "<li>Su Sensörü: Sicaklik = " + String(waterTemp, 1) + " C</li>";
  html += "<li>ADXL345: Toplam ivme = " + String(totalAcc, 2) + " m/s²</li>";
  html += "<li>RTC: " + String(now.day()) + "/" + String(now.month()) + "/" + String(now.year()) + " " 
          + String(now.hour()) + ":" + String(now.minute()) + "</li>";
  html += "</ul>";
  
  html += "<h2>Role Ayarlari</h2>";
  html += "<form action='/veriler/guncelle' method='POST'>";
  html += "Role Modu: <select name='sensor'>";
  html += "<option value='0'" + String(relayConfig.sensor == 0 ? " selected" : "") + ">Manuel</option>";
  html += "<option value='1'" + String(relayConfig.sensor == 1 ? " selected" : "") + ">DHT11</option>";
  html += "<option value='2'" + String(relayConfig.sensor == 2 ? " selected" : "") + ">DS18B20</option>";
  html += "</select><br>";
  html += "Limit (C): <input type='number' step='0.1' name='limit' value='" + String(relayConfig.limit, 1) + "'><br>";
  html += "Role Durumu: <select name='state'>";
  html += "<option value='0'" + String(relayConfig.state == 0 ? " selected" : "") + ">Kapali</option>";
  html += "<option value='1'" + String(relayConfig.state == 1 ? " selected" : "") + ">Acik</option>";
  html += "</select><br>";
  html += "<input type='submit' value='Guncelle'>";
  html += "</form>";
  
  html += "</body></html>";
  server.send(200, "text/html", html);
}

// Yeni: Gonderilen form verileriyle role ayarlarini guncelle
void handleVeriGuncelle() {
  if (server.hasArg("limit")) {
    relayConfig.limit = server.arg("limit").toFloat();
  }
  if (server.hasArg("sensor")) {
    relayConfig.sensor = server.arg("sensor").toInt();
    // sensor 0 ise manuel, diger durumda otomatik mod
    relayConfig.mode = (relayConfig.sensor == 0) ? 0 : 1;
  }
  if (server.hasArg("state")) {
    relayConfig.state = server.arg("state").toInt();
    digitalWrite(RELAY_PIN, relayConfig.state ? LOW : HIGH);
  }
  EEPROM.put(0, relayConfig);
  EEPROM.commit();
  server.sendHeader("Location", "/veriler");
  server.send(303, "text/plain", "Ayarlar guncellendi. Yonlendiriliyorsunuz...");
}
