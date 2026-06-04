#include <Wire.h>
#include <MPU6500_WE.h>

#define MPU6500_ADDR 0x68 
MPU6500_WE myMPU6500 = MPU6500_WE(MPU6500_ADDR);

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);
  myMPU6500.init();
  myMPU6500.autoOffsets(); 
}

void loop() {
  xyzFloat gValue = myMPU6500.getGValues();
  xyzFloat gyr    = myMPU6500.getGyrValues();

  // Formato: "Etiqueta:Valor," (El último no lleva coma al final)
  Serial.print("AcX:"); Serial.print(gValue.x); Serial.print(",");
  Serial.print("AcY:"); Serial.print(gValue.y); Serial.print(",");
  Serial.print("AcZ:"); Serial.print(gValue.z); Serial.print(",");
  Serial.print("GyX:"); Serial.print(gyr.x); Serial.print(",");
  Serial.print("GyY:"); Serial.print(gyr.y); Serial.print(",");
  Serial.print("GyZ:"); Serial.println(gyr.z); // println para saltar línea

  delay(20); // Delay corto para mayor fluidez
}
