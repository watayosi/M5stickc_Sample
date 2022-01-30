// from
// https://github.com/adafruit/Adafruit_AMG88xx/blob/master/examples/amg88xx_test/amg88xx_test.ino

#include <Adafruit_AMG88xx.h>

#include <M5StickC.h>

Adafruit_AMG88xx amg;

// the setup routine runs once when M5StickC starts up
void setup(){
  // Initialize the M5StickC object
  M5.begin();

  // LCD display
  M5.Lcd.setRotation(3);
  M5.Lcd.print("AMG88xx test");

  // Initialize AMG8833
  Serial.begin(9600);
  Serial.println(F("AMG88xx test"));

  bool status;
  
  // default settings
  status = amg.begin();
  if (!status) {
      M5.Lcd.print("Could not find a valid AMG88xx sensor, check wiring!");
      while (1);
  }
  
  Serial.println();

  delay(100); // let sensor boot up
}

// the loop routine runs over and over again forever
void loop() {
    M5.Lcd.setCursor(0, 20, 2);
    M5.Lcd.setTextColor(WHITE, BLACK);
    float tempbuf[64];
    M5.Lcd.print("Temperature = ");
    M5.Lcd.print(amg.readThermistor());
    amg.readPixels(tempbuf, 64);
    M5.Lcd.setCursor(0, 40, 2);
    M5.Lcd.print(tempbuf[0]);
    M5.Lcd.print(" *C ");
    M5.Lcd.print(tempbuf[32]);
    M5.Lcd.print(" *C");
  
    //delay a second
    delay(100);
}
