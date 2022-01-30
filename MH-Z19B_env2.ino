
// CO2 Sensor MH-Z19C にて計測
// LCD : CO2_WARNING_VALUE を超えると赤字で計測値
// LED : CO2_LIMIT_VALUE を超えると5,000msに一度100ms点灯
// Ambint : 5minに一度、CO2と気温を書込み 
// env -> env2 に変更　= dht12 -> sht30
 
#include <M5StickC.h>
// #include "DHT12.h"
#include <SHT3x.h>
#include <WiFi.h>
#include <Wire.h>
#include "MHZ19.h"
#include "Ambient.h"
#include "Adafruit_Sensor.h"
#include <Adafruit_BMP280.h>

// DHT12 dht12; //Preset scale CELSIUS and ID 0x5c.
Adafruit_BMP280 bme;
SHT3x sht30;

#define _DEBUG 1
#if _DEBUG
#define DBG(...) { Serial.print(__VA_ARGS__); }
#define DBGLED(...) { digitalWrite(__VA_ARGS__); }
#else
#define DBG(...)
#define DBGLED(...)
#endif /* _DBG */

#define RX_PIN 36                                          // Rx pin which the MHZ19 Tx pin is attached to
#define TX_PIN 26                                          // Tx pin which the MHZ19 Rx pin is attached to
#define BAUDRATE 9600                                      // Device to MH-Z19 Serial baudrate (should not be changed)

#define LCD_MODE_DIGIT 0
#define LCD_MODE_GRAPH 1

#define BRIGHTNESS 8

#define CO2_WARNING_VALUE 1200
#define CO2_LIMIT_VALUE 2000

const char* ssid = "elecom2g-8acf38";
const char* password = "3423077516367";

// Ambient Channel
unsigned int channelId = 32505;
const char* writeKey = "0f1ed9cb4e49d761";

WiFiClient client;
Ambient ambient;

MHZ19 myMHZ19;                                             // Constructor for library
HardwareSerial mySerial(1);                                // (ESP32 Example) create device to MH-Z19 serial

bool lcdOn = true;
int lcdMode = LCD_MODE_DIGIT;

bool ledOn = false;
bool ledValue = false;
unsigned long nextLedOperationTime = 0;
unsigned long nextAmbientOperationTime = 0;

unsigned long getDataTimer = 0;

int history[160] = {};
int historyPos = 0;

float tmp;
float hum;
float pressure;

void setup() {
  M5.begin();
  M5.Axp.ScreenBreath(BRIGHTNESS);

  Serial.begin(9600);                                     // Device to serial monitor feedback

    Wire.begin(32, 33, 100000);
//    Serial.println(F("ENV Unit(DHT12 and BMP280) test..."));
    Serial.println(F("ENV2 Unit(SHT30 and BMP280) test..."));
    if (!bme.begin(0x76)){  
      Serial.println("Could not find a valid BMP280 sensor, check wiring!");
      while (1);
    }
    sht30.Begin();
    
  WiFi.begin(ssid, password);
  int i = 0;
  while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      DBG(".");
  }

  DBG("WiFi connected\r\n");
  DBG("IP address: ");
  DBG(WiFi.localIP());
  DBG("\r\n");

  ambient.begin(channelId, writeKey, &client);
 
  mySerial.begin(BAUDRATE, SERIAL_8N1, RX_PIN, TX_PIN); // (ESP32 Example) device to MH-Z19 serial start
  myMHZ19.begin(mySerial);                                // *Serial(Stream) refence must be passed to library begin().
  myMHZ19.autoCalibration(true);

  M5.Lcd.setRotation(1);
  render_CO2();

  pinMode(M5_LED, OUTPUT);
}

void loop() {
  auto now = millis();

  // Aボタン: モードを切り替える
  M5.update();
  if ( M5.BtnA.wasPressed() ) {
    lcdMode = (lcdMode + 1) % 2;
    render_CO2();
  }

  // Bボタン: 画面表示を ON/OFF する
  if ( M5.BtnB.wasPressed() ) {
    lcdOn = !lcdOn;

    if (lcdOn) {
      render_CO2();
      M5.Axp.ScreenBreath(BRIGHTNESS);
    } else {
      M5.Axp.ScreenBreath(0);
    }
  }

  if ((ledOn || !ledValue) && nextLedOperationTime <= now) {
    ledValue = !ledValue;
    digitalWrite(M5_LED, ledValue);

    // Lチカ
    if (ledValue) {
      nextLedOperationTime = now + 5000;
    } else {
      nextLedOperationTime = now + 100;
    }
  }

  if (now - getDataTimer >= 3000) {
    /* note: getCO2() default is command "CO2 Unlimited". This returns the correct CO2 reading even
      if below background CO2 levels or above range (useful to validate sensor). You can use the
      usual documented command with getCO2(false) */

    int CO2 = myMHZ19.getCO2();                         // Request CO2 (as ppm)
//    int8_t temp = myMHZ19.getTemperature(false, true);  // Request Temperature (as Celsius)

    Serial.print("CO2 (ppm): ");
    Serial.print(CO2);
//    Serial.print(", Temperature (C): ");
//    Serial.println(temp);

    ledOn = CO2 >= CO2_LIMIT_VALUE;

//    tmp = dht12.readTemperature();
//    hum = dht12.readHumidity();
    sht30.UpdateData();
    tmp = sht30.GetTemperature();
    hum = sht30.GetRelHumidity();
    pressure = bme.readPressure();
    Serial.printf(" Temperature: %2.2f*C  Humedad: %0.2f%%  Pressure: %0.2fPa\r\n", tmp, hum, pressure);
 
    // 測定結果の表示
    historyPos = (historyPos + 1) % (sizeof(history) / sizeof(int));
    history[historyPos] = CO2;
    render_CO2();

    // Ambient 書込み
    if (nextAmbientOperationTime <= now) {
      ambient.set(1, tmp);                // データーがint型かfloat型であれば、直接セットすることができます。
//    dtostrf(humid, 3, 1, humidbuf);      // データーの桁数などを制御したければ自分で文字列形式に変換し、
      ambient.set(2, CO2);                 // セットします。
      ambient.set(3, hum);
      ambient.set(4, pressure);
      ambient.send();
      nextAmbientOperationTime = now + 300000;  // 5min に一度、Ambientへ書込み
    }

    getDataTimer = now;
  }
}

void render_CO2() {
  if (!lcdOn) {
    return;
  }

  // Clear
  int height = M5.Lcd.height();
  int width = M5.Lcd.width();
  M5.Lcd.fillRect(0, 0, width, height, BLACK);

  switch (lcdMode) {
    case LCD_MODE_DIGIT:
      M5.Lcd.setTextColor(WHITE);
      M5.Lcd.drawString((String)tmp + "*C / "  + (String)hum + "%", 12, 0, 2);
//      M5.Lcd.setTextSize(2);
//      M5.Lcd.setCursor(10, 0);
//      M5.Lcd.printf("%2.1f*C / %0.1f%%", 22.2f, 33.3f);

      M5.Lcd.setTextSize(1);
      M5.Lcd.setCursor(10, 20);
      M5.Lcd.printf("%0.1fPa", pressure);
      M5.Lcd.drawString("CO2", 12, 45, 2);
      M5.Lcd.drawString("[ppm]", 12, 58, 2);
      if(history[historyPos] > CO2_WARNING_VALUE) {
        M5.Lcd.setTextColor(RED);
      }
      M5.Lcd.drawRightString("    " + (String)history[historyPos], width, 24, 7);
      break;
    case LCD_MODE_GRAPH:
      int len = sizeof(history) / sizeof(int);
      for (int i = 0; i < len; i++) {
        auto value = max(0, history[(historyPos + 1 + i) % len] - 400);
        auto y = min(height, (int)(value * (height / 1200.0)));
        auto color = min(255, (int)(value * (255 / 1200.0)));
        M5.Lcd.drawLine(i, height - y, i, height, M5.Lcd.color565(255, 255 - color, 255 - color));
      }
      break;
  }
}
