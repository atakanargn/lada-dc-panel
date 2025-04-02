#include <Wire.h>
#include <DHT.h>
#include <LiquidCrystal_I2C.h>
#include <RTClib.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_ADXL345_U.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <EEPROM.h>
#include <math.h>
#include <WiFiManager.h>
#include <ESP8266WebServer.h>

#define DHTPIN D5
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

#define DS18B20_PIN D2
OneWire oneWire(DS18B20_PIN);
DallasTemperature ds18(&oneWire);

#define DS18WATER_PIN D6
OneWire oneWireWater(DS18WATER_PIN);
DallasTemperature waterSensor(&oneWireWater);

#define ENCODER_CLK D4
#define ENCODER_DT  D8
#define ENCODER_SW  D7

#define BUZZER_PIN D0
#define RELAY_PIN D3

RTC_DS1307 rtc;
LiquidCrystal_I2C lcd(0x27, 16, 2);
Adafruit_ADXL345_Unified accel = Adafruit_ADXL345_Unified(12345);
ESP8266WebServer server(80);

#define EEPROM_SIZE 32

struct RelayConfig {
  uint8_t mode;
  uint8_t sensor;
  float limit;
  uint8_t state;
};
RelayConfig relayConfig;

struct BacklightConfig {
  uint8_t mode;
  uint8_t state;
  int fixedVal;
};
BacklightConfig backlightConfig;

bool debugEnabled = true;

const char* wifiSSID = "Kerim's Lada";
const char* wifiPassword = "k3r1mL4dA";

unsigned long lastWifiUpdate = 0;
bool displayWifiCredentials = true;

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

int currentScreen = 0;
int lastEncoderClk = HIGH;
int encoderCount = 0;
const int encoderThreshold = 6;

bool transitionMode = false;
unsigned long transitionStart = 0;
const unsigned long transitionTimeout = 2000;

unsigned long lastDHTUpdate   = 0;
unsigned long lastTimeUpdate  = 0;
unsigned long lastAccelUpdate = 0;
unsigned long lastRelayUpdate = 0;
unsigned long lastSettingsUpdate = 0;
unsigned long lastWaterUpdate = 0;

float estimatedSpeed = 0.0;
unsigned long lastAccelReadTime = 0;

bool buttonActive = false;
unsigned long buttonPressStart = 0;
const unsigned long longPressDuration = 500;

ICACHE_FLASH_ATTR void splashScreen();
ICACHE_FLASH_ATTR void transitionEffect();
ICACHE_FLASH_ATTR void showRelaySettingsMenu();
ICACHE_FLASH_ATTR void showBacklightSettingsMenu();
ICACHE_FLASH_ATTR void debugInfo();
void handleVeriSayfasi();
void handleVeriGuncelle();

void setup() {
  Serial.begin(115200);
  Serial.println("Versiyon: v0.2"); // v0.2 eklendi
  Serial.println("Proje Baslatiliyor: Ortam, Tarih/Saat, ADXL345, Role, Ayar Menusu, Arka Isik");
  
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("LCD Basladi");
  delay(500);
  
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Tum inputlar");
  lcd.setCursor(0, 1);
  lcd.print("Kapali");
  delay(1000);
  
  // Yeni: Hoşgeldiniz ve geri sayım ekranı
  bool wifiUpdateMode = false;
  for (int i = 3; i >= 0; i--) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Hosgeldiniz");
    lcd.setCursor(0, 1);
    lcd.print("Geri sayim: ");
    lcd.print(i);
    delay(1000);
    if (digitalRead(ENCODER_SW) == LOW) {
      wifiUpdateMode = true;
    }
  }
  
  if (wifiUpdateMode) {
    WiFi.mode(WIFI_STA);
    pinMode(ENCODER_SW, INPUT_PULLUP);
    WiFiManager wifiManager;
    wifiManager.setConfigPortalTimeout(300); // 5 dakika timeout
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Wireless islemleri");
    unsigned long startTime = millis();
    bool wifiConnected = false;
    int spinnerIndex = 0;
    char spinnerChars[] = "-\\|/";
    while ((millis() - startTime) < 300000 && !wifiConnected) { // 5 dakika
      if (digitalRead(ENCODER_SW) == LOW) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Wifi iptal");
        delay(1000);
        break;
      }
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
  } else {
    // Normal yüklenme animasyonu
    splashScreen();
  }
  
  dht.begin();
  ds18.begin();
  waterSensor.begin();
  accel.begin();
  accel.setRange(ADXL345_RANGE_16_G);
  
  pinMode(ENCODER_CLK, INPUT_PULLUP);
  pinMode(ENCODER_DT, INPUT_PULLUP);
  pinMode(ENCODER_SW, INPUT_PULLUP);
  lastEncoderClk = digitalRead(ENCODER_CLK);
  
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  
  pinMode(RELAY_PIN, OUTPUT);
  
  lcd.init();
  lcd.backlight();
  lcd.clear();
  
  lcd.createChar(3, thermoIcon);
  lcd.createChar(4, clockIcon);
  lcd.createChar(5, accelIcon);
  lcd.createChar(6, relayIcon);
  lcd.createChar(7, carIcon);
  
  splashScreen();
  
  if (!rtc.begin()) {
    Serial.println("RTC Bulunamadi!");
    lcd.setCursor(0,0);
    lcd.print("RTC Yok!");
  } else {
    Serial.println("RTC Guncelleniyor...");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
  
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
    backlightConfig.state = 1;
    backlightConfig.fixedVal = 1;
  }
  
  lastAccelReadTime = millis();
  debugInfo();
  debugEnabled = false;
}

void loop() {
  unsigned long currentMillis = millis();
  
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.reconnect();
  }
  
  server.handleClient();

  int currentClk = digitalRead(ENCODER_CLK);
  if (currentClk != lastEncoderClk) {
    if (digitalRead(ENCODER_DT) != currentClk) {
      encoderCount++;
    } else {
      encoderCount--;
    }
    lastEncoderClk = currentClk;
    if (!transitionMode && abs(encoderCount) >= encoderThreshold) {
      transitionMode = true;
      transitionStart = currentMillis;
      encoderCount = 0;
    }
  }
  
  if (transitionMode && (currentMillis - transitionStart > transitionTimeout)) {
    transitionMode = false;
    encoderCount = 0;
    transitionEffect();
  }
  
  if (transitionMode && abs(encoderCount) >= encoderThreshold) {
    if (encoderCount > 0) {
      currentScreen++;
      if (currentScreen > 7) currentScreen = 0;
    } else {
      currentScreen--;
      if (currentScreen < 0) currentScreen = 7;
    }
    transitionMode = false;
    encoderCount = 0;
    transitionEffect();
  }
  
  int buttonState = digitalRead(ENCODER_SW);
  if (buttonState == LOW && (millis() - buttonPressStart > 50)) {
    if (!buttonActive) {
      buttonActive = true;
      buttonPressStart = millis();
    }
  } else if (buttonActive && buttonState == HIGH) {
    unsigned long pressDuration = millis() - buttonPressStart;
    if (currentScreen == 4) {
      if (pressDuration < longPressDuration) {
        relayConfig.sensor = (relayConfig.sensor + 1) % 3;
        relayConfig.mode = (relayConfig.sensor == 0) ? 0 : 1;
      } else {
        if (relayConfig.sensor != 0) {
          currentScreen = 7;
        } else {
          relayConfig.state = !relayConfig.state;
          digitalWrite(RELAY_PIN, relayConfig.state ? LOW : HIGH);
        }
      }
      EEPROM.put(0, relayConfig);
      EEPROM.commit();
    } else if (currentScreen == 7) {
      EEPROM.put(0, relayConfig);
      EEPROM.commit();
      currentScreen = 4;
    } else if (currentScreen == 5) {
      if (pressDuration >= longPressDuration) {
        backlightConfig.mode = (backlightConfig.mode == 0) ? 1 : 0;
      } else {
        if (backlightConfig.mode == 0) {
          backlightConfig.state = !backlightConfig.state;
          if (backlightConfig.state)
            lcd.backlight();
          else
            lcd.noBacklight();
        } else {
          backlightConfig.fixedVal = (encoderCount > 0) ? 1 : 0;
          encoderCount = 0;
        }
      }
      EEPROM.put(8, backlightConfig);
      EEPROM.commit();
    } else {
      if (relayConfig.mode == 0) {
        relayConfig.state = !relayConfig.state;
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
  
  switch (currentScreen) {
    case 0: {
      if (currentMillis - lastDHTUpdate >= 2000) {
        lastDHTUpdate = currentMillis;
        float hum = dht.readHumidity();
        float temp = dht.readTemperature();
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.write(byte(3));
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
    case 1: {
      if (currentMillis - lastTimeUpdate >= 2000) {
        lastTimeUpdate = currentMillis;
        DateTime now = rtc.now();
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.write(byte(4));
        lcd.print(" Tarih/Saat");
        lcd.setCursor(0, 1);
        char buf[17];
        snprintf(buf, sizeof(buf), "%02d/%02d/%04d %02d:%02d", now.day(), now.month(), now.year(), now.hour(), now.minute());
        lcd.print(buf);
      }
      break;
    }
    case 2: {
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
        lcd.write(byte(5));
        lcd.print(" Arac Verisi");
        lcd.setCursor(0, 1);
        lcd.print("Hiz:");
        lcd.print(speedKmh, 1);
        lcd.print(" km/h");
      }
      break;
    }
    case 3: {
      if (currentMillis - lastRelayUpdate >= 1000) {
        lastRelayUpdate = currentMillis;
        if (relayConfig.mode == 1) {
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
    case 4: {
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
    case 5: {
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
    case 6: {
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
    case 7: {
      if (currentMillis - lastSettingsUpdate >= 500) {
        lastSettingsUpdate = currentMillis;
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Limit Ayarla");
        lcd.setCursor(0, 1);
        lcd.print("T:");
        lcd.print(relayConfig.limit, 1);
        if (encoderCount != 0) {
          relayConfig.limit += 0.5 * encoderCount;
          encoderCount = 0;
          lcd.setCursor(0, 1);
          lcd.print("T:");
          lcd.print(relayConfig.limit, 1);
          lcd.print("C");
        }
      }
      break;
    }
  }
}

ICACHE_FLASH_ATTR void transitionEffect() {
  for (int i = 0; i < 3; i++) {
    lcd.noBacklight();
    delay(100);
    lcd.backlight();
    delay(100);
  }
}

ICACHE_FLASH_ATTR void splashScreen() {
  for (int pos = 0; pos < 16; pos++) {
    lcd.clear();
    lcd.setCursor(pos, 0);
    lcd.write(byte(7));
    delay(100);
  }
  lcd.clear();
  lcd.setCursor(2, 1);
  lcd.print("Hosgeldiniz");
  delay(1000);
  lcd.clear();
}

ICACHE_FLASH_ATTR void debugInfo() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Debug: Program");
  lcd.setCursor(0, 1);
  lcd.print("Basladi, Calisiyor.");
  delay(1000);
}

void handleVeriSayfasi() {
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

void handleVeriGuncelle() {
  if (server.hasArg("limit")) {
    relayConfig.limit = server.arg("limit").toFloat();
  }
  if (server.hasArg("sensor")) {
    relayConfig.sensor = server.arg("sensor").toInt();
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
