#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <RTClib.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_ADXL345_U.h>
#include <DHT.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <ESPAsyncWebServer.h>
#include <AsyncElegantOTA.h>

#define DHTPIN D5
#define DHTTYPE DHT11
#define DS18B20_PIN D6
#define BUTTON_PIN D7
const int potPin = A0;

int sensorReadDelay = 200;

const int totalMenus = 6;
String menuItems[totalMenus] = {"DHT11", "DS18B20", "ADXL345", "RTC", "WiFi", "Ayarlar"};

int currentMenu = 0;
bool inSubMenu = false;

bool buttonState = HIGH;
bool lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;

LiquidCrystal_I2C lcd(0x27, 16, 2);
RTC_DS1307 rtc;
Adafruit_ADXL345_Unified accel = Adafruit_ADXL345_Unified(12345);
DHT dht(DHTPIN, DHTTYPE);
OneWire oneWire(DS18B20_PIN);
DallasTemperature DS18B20(&oneWire);
AsyncWebServer server(80);

void setup() {
  Serial.begin(115200);
  lcd.init();
  lcd.backlight();
  lcd.clear();
  dht.begin();
  DS18B20.begin();
  if (!rtc.begin()) {
    Serial.println("RTC bulunamadı!");
  }
  if (!rtc.isrunning()) {
    Serial.println("RTC çalışmıyor, zamanı ayarlıyoruz...");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
  if (!accel.begin()){
    Serial.println("ADXL345 bulunamadı, bağlantıları kontrol edin!");
  }
  accel.setRange(ADXL345_RANGE_16_G);
  WiFiManager wifiManager;
  wifiManager.setTimeout(30);
  if (!wifiManager.autoConnect("NodeMCU-AP", "password")) {
    Serial.println("WiFi bağlantısı sağlanamadı, çalışmaya devam ediliyor.");
  } else {
    Serial.println("WiFi'ye bağlandı!");
  }
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  AsyncElegantOTA.begin(&server);
  server.on("/settings", HTTP_GET, [](AsyncWebServerRequest *request){
    String html = "<!DOCTYPE html><html><head><meta charset='utf-8'><title>Ayarlar</title></head><body>";
    html += "<h2>Sistem Ayarları</h2>";
    html += "<form action='/settings' method='POST'>";
    html += "Sensor Okuma Gecikmesi (ms): <input type='number' name='readDelay' value='" + String(sensorReadDelay) + "'><br><br>";
    html += "<input type='submit' value='Kaydet'>";
    html += "</form>";
    html += "<p>OTA için: <a href='/update'>/update</a></p>";
    html += "</body></html>";
    request->send(200, "text/html", html);
  });
  server.on("/settings", HTTP_POST, [](AsyncWebServerRequest *request){
    if(request->hasParam("readDelay", true)){
      String value = request->getParam("readDelay", true)->value();
      sensorReadDelay = value.toInt();
      Serial.print("Yeni sensor okuma gecikmesi: ");
      Serial.println(sensorReadDelay);
    }
    request->redirect("/settings");
  });
  server.onNotFound([](AsyncWebServerRequest *request){
    request->send(404, "text/plain", "Not found");
  });
  server.begin();
  Serial.println("Web sunucusu başlatıldı.");
  displayMainMenu();
}

void loop() {
  int potValue = analogRead(potPin);
  int newMenu = map(potValue, 0, 1023, 0, totalMenus - 1);
  if (newMenu != currentMenu && !inSubMenu) {
    currentMenu = newMenu;
    displayMainMenu();
  }
  bool reading = digitalRead(BUTTON_PIN);
  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != buttonState) {
      buttonState = reading;
      if (buttonState == LOW) {
        inSubMenu = !inSubMenu;
        displayMainMenu();
      }
    }
  }
  lastButtonState = reading;
  if (inSubMenu) {
    displaySubMenu();
  }
  delay(sensorReadDelay);
}

void displayMainMenu() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Menu:");
  lcd.print(menuItems[currentMenu]);
  lcd.setCursor(0, 1);
  lcd.print("Btn:Sec / Geri");
}

void displaySubMenu() {
  lcd.clear();
  lcd.setCursor(0, 0);
  if (menuItems[currentMenu] == "DHT11") {
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    lcd.print("T:");
    lcd.print(t, 1);
    lcd.print("C H:");
    lcd.print(h, 1);
    lcd.print("%");
  } else if (menuItems[currentMenu] == "DS18B20") {
    DS18B20.requestTemperatures();
    float temp = DS18B20.getTempCByIndex(0);
    lcd.print("DS18B20 T:");
    lcd.print(temp, 1);
    lcd.print("C");
  } else if (menuItems[currentMenu] == "ADXL345") {
    sensors_event_t event;
    accel.getEvent(&event);
    lcd.print("X:");
    lcd.print(event.acceleration.x, 1);
    lcd.print(" Y:");
    lcd.print(event.acceleration.y, 1);
    lcd.setCursor(0, 1);
    lcd.print("Z:");
    lcd.print(event.acceleration.z, 1);
  } else if (menuItems[currentMenu] == "RTC") {
    DateTime now = rtc.now();
    lcd.print(now.timestamp(DateTime::TIMESTAMP_TIME));
    lcd.setCursor(0, 1);
    lcd.print(now.timestamp(DateTime::TIMESTAMP_DATE));
  } else if (menuItems[currentMenu] == "WiFi") {
    if (WiFi.status() == WL_CONNECTED) {
      lcd.print("IP:");
      lcd.print(WiFi.localIP());
    } else {
      lcd.print("WiFi yok");
    }
  } else if (menuItems[currentMenu] == "Ayarlar") {
    lcd.print("Web Ayarlar:");
    lcd.setCursor(0, 1);
    lcd.print(WiFi.localIP());
  }
}
