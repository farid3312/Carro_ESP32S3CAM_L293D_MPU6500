#include <Wire.h>
#include <MPU6500_WE.h>

// Dirección I2C por defecto del sensor (AD0 a GND)
#define MPU_ADDR 0x68 

// Creación del objeto para el sensor
MPU6500_WE myMPU6500 = MPU6500_WE(MPU_ADDR);

// Variables para los ángulos y temperatura
float pitch = 0;
float roll = 0;
float yaw = 0;
float temperatura = 0;

// Variables de tiempo y filtro
unsigned long tiempoPrevio;
const float alfa = 0.98; // Peso del giroscopio para el Filtro Complementario

void setup() {
  Serial.begin(115200);
  
  // Configuración específica para I2C en la ESP32-S3
  Wire.begin(21, 47); 
  
  Serial.println("Iniciando sensor MPU6500...");

  if(!myMPU6500.init()){
    Serial.println("ERROR: Sensor no encontrado. Revisa los cables.");
    while(1); // Detiene la ejecución si falla
  }

  Serial.println("Calibrando sensor... (NO LO MUEVAS)");
  myMPU6500.autoOffsets(); 
  
  // Configuración de rangos (Sintaxis específica para tu compilador)
  myMPU6500.setAccRange(MPU6500_ACC_RANGE_2G);      
  myMPU6500.setGyrRange(MPU6500_GYRO_RANGE_250); 
  
  tiempoPrevio = millis();
  Serial.println("¡Sensor listo! Imprimiendo datos...");
}

void loop() {
  // 1. Calcular el diferencial de tiempo (dt) en segundos
  unsigned long tiempoActual = millis();
  float dt = (tiempoActual - tiempoPrevio) / 1000.0;
  tiempoPrevio = tiempoActual;

  // 2. Obtener datos procesados de la librería
  xyzFloat acc = myMPU6500.getGValues();
  xyzFloat gyr = myMPU6500.getGyrValues();
  temperatura = myMPU6500.getTemperature(); // La librería une los 2 bytes crudos por ti

  // 3. Ángulos crudos por Acelerómetro (Inclinación base)
  // atan2 nos da el ángulo respecto a la gravedad
  float pitchAcc = atan2(-acc.x, sqrt(acc.y * acc.y + acc.z * acc.z)) * 180.0 / M_PI;
  float rollAcc  = atan2(acc.y, acc.z) * 180.0 / M_PI;

  // 4. FILTRO COMPLEMENTARIO para Pitch y Roll
  // Fórmula: Ángulo = alfa * (Ángulo + Giroscopio * dt) + (1 - alfa) * Acelerómetro
  pitch = alfa * (pitch + gyr.y * dt) + (1.0 - alfa) * pitchAcc;
  roll  = alfa * (roll  + gyr.x * dt) + (1.0 - alfa) * rollAcc;
  
  // 5. Integración simple para el Yaw (No usa acelerómetro)
  yaw = yaw + (gyr.z * dt);

  // 6. Salida para Serial Plotter / Monitor Serie
  Serial.print("Pitch:"); Serial.print(pitch); Serial.print(",");
  Serial.print("Roll:");  Serial.print(roll);  Serial.print(",");
  Serial.print("Yaw:");   Serial.print(yaw);   Serial.print(",");
  Serial.print("Temp:");  Serial.println(temperatura);

  delay(10); // Alta frecuencia para asegurar la precisión del Filtro Complementario
}
