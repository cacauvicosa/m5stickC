#include <M5StickC.h>

float gyroX = 0;
float gyroY = 0;
float gyroZ = 0;

void setup() {
  // put your setup code here, to run once:
  M5.begin();
  M5.Lcd.setRotation(3);
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextSize(1);
  M5.Lcd.setCursor(40, 0);
  M5.Lcd.println("MPU6886 TEST");
  M5.Lcd.setCursor(0, 15);
  M5.Lcd.setTextSize(4);
  M5.MPU6886.Init();
   
}


void loop() {
  // put your main code here, to run repeatedly:
  M5.MPU6886.getGyroData(&gyroX,&gyroY,&gyroZ);

  M5.Lcd.setCursor(0, 20);
  
  M5.Lcd.printf("X %.2f     ", gyroX);
  delay(300);
}
