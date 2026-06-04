#include <Wire.h>
#include <MPU6500_WE.h>

// La dirección por defecto del GY-6500 suele ser 0x68 
#define MPU6500_ADDR 0x68 

// Creamos el objeto del sensor
MPU6500_WE myMPU6500 = MPU6500_WE(MPU6500_ADDR);

void setup() {
  Serial.begin(115200);
  
  // Iniciamos I2C en los pines del ESP32
  Wire.begin(21, 47);

  Serial.println("\n--- CARRITO NEUROMORFO: Iniciando MPU-6500 ---");

  // Iniciar el sensor con la librería
  if (!myMPU6500.init()) {
    Serial.println("ERROR: MPU6500 no responde.");
    Serial.println("Revisa los cables y asegúrate de que AD0 no esté conectado a 3.3V.");
    while (1) { delay(10); } // Freno de emergencia
  }
  
  Serial.println("¡MPU-6500 detectado y configurado!");

  // --- CALIBRACIÓN AUTOMÁTICA ---
  // IMPORTANTE: El sensor debe estar totalmente quieto y plano durante este paso
  Serial.println("Calibrando... No muevas el sensor durante 2 segundos.");
  delay(1000);
  myMPU6500.autoOffsets(); 
  Serial.println("Calibración terminada. Listo para leer.");

  // Ajustamos los rangos para un robot móvil
  myMPU6500.setAccRange(MPU6500_ACC_RANGE_8G);
  myMPU6500.setGyrRange(MPU6500_GYRO_RANGE_500);
}

void loop() {
  // La librería entrega los datos en una estructura llamada "xyzFloat"
  // Esto ya nos da los valores reales en fuerzas 'G' y grados por segundo
  xyzFloat gValue = myMPU6500.getGValues();
  xyzFloat gyr    = myMPU6500.getGyrValues();

  // Imprimir Aceleración (Gravedad)
  Serial.print("Aceleración [g]: X="); 
  Serial.print(gValue.x);
  Serial.print(" | Y="); 
  Serial.print(gValue.y);
  Serial.print(" | Z="); 
  Serial.println(gValue.z);

  // Imprimir Giroscopio (Rotación)
  Serial.print("Giroscopio [°/s]: X="); 
  Serial.print(gyr.x);
  Serial.print(" | Y="); 
  Serial.print(gyr.y);
  Serial.print(" | Z="); 
  Serial.println(gyr.z);
  
  Serial.println("-----------------------------------------");

  delay(500); // Pausa de medio segundo para poder leer
}
