#include <Wire.h>

const int MPU_ADDR = 0x68; 
int16_t AcX, AcY, AcZ, Tmp, GyX, GyY, GyZ;

void setup() {
  Serial.begin(115200);
  // Mantenemos TUS pines que sí funcionaron (SDA=21, SCL=47)
  Wire.begin(21, 47, 100000); 
  
  // Despertar al sensor
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B);  
  Wire.write(0);     
  Wire.endTransmission(true);
  
  Serial.println("Iniciando Plotter...");
  delay(100);
}

void loop() {
  // 1. Pedir los datos al sensor
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);  
  Wire.endTransmission(false); // IMPORTANTE: false para mantener conexión
  Wire.requestFrom(MPU_ADDR, 14, true); 

  // 2. Leer los bytes y unirlos (Bit shifting)
  AcX = Wire.read() << 8 | Wire.read(); 
  AcY = Wire.read() << 8 | Wire.read(); 
  AcZ = Wire.read() << 8 | Wire.read(); 
  Tmp = Wire.read() << 8 | Wire.read(); 
  GyX = Wire.read() << 8 | Wire.read(); 
  GyY = Wire.read() << 8 | Wire.read(); 
  GyZ = Wire.read() << 8 | Wire.read(); 

  // 3. Imprimir con formato especial para el Plotter
  // El formato es "Etiqueta:Valor,"
  
  // --- Acelerómetro (Gravedad) ---
  Serial.print("Acel_X:"); Serial.print(AcX); Serial.print(",");
  Serial.print("Acel_Y:"); Serial.print(AcY); Serial.print(",");
  Serial.print("Acel_Z:"); Serial.print(AcZ); Serial.print(",");

  // --- Giroscopio (Rotación) ---
  // Imprimimos el último con println para terminar la línea
  Serial.print("Giro_X:"); Serial.print(GyX); Serial.print(",");
  Serial.print("Giro_Y:"); Serial.print(GyY); Serial.print(",");
  Serial.print("Giro_Z:"); Serial.println(GyZ);

  // 4. Pausa corta para ver el movimiento fluido
  delay(50); 
}
