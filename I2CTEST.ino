#include <Wire.h>

void setup() {
  Serial.begin(115200);
  // SDA = 21, SCL = 22 por defecto en ESP32
  Wire.begin(21, 22); 
  Serial.println("\nEscaneando bus I2C...");
}

void loop() {
  byte error, address;
  int devices = 0;

  for(address = 1; address < 127; address++ ) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0) {
      Serial.print("Dispositivo I2C encontrado en la direccion 0x");
      if (address < 16) Serial.print("0");
      Serial.print(address, HEX);
      Serial.println(" !");
      devices++;
    }
  }

  if (devices == 0) {
    Serial.println("No se encontraron dispositivos I2C. Revisa el cableado.");
  } else {
    Serial.println("Escaneo finalizado.");
  }

  delay(5000); // Espera 5 segundos antes de volver a escanear
}
