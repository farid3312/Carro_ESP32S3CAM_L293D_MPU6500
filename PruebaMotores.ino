// ===== PINES L293D =====
const int IN1 = 1;  // Motor A (Izquierdo)
const int IN2 = 42;  
const int ENA = 2; // Velocidad Motor A

const int IN3 = 14; // Motor B (Derecho)
const int IN4 = 41; 
const int ENB = 38; // Velocidad Motor B

// ===== CONFIGURACIÓN DE VELOCIDAD =====
const int frecuencia = 1000; // 1 kHz
const int resolucion = 8;    // 8 bits (0 a 255)
int velocidad = 150;         // Velocidad de prueba (aprox 60%)

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("\n--- Prueba Super Basica L293D ---");

  // 1. Configurar los pines de dirección como salidas
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  // 2. Configurar los pines de Enable para la velocidad (PWM)
  ledcAttach(ENA, frecuencia, resolucion);
  ledcAttach(ENB, frecuencia, resolucion);
}

// --- FUNCIONES DE MOVIMIENTO DIRECTO ---

void moverAdelante() {
  Serial.println("Movimiento: ADELANTE");
  
  // Encender Motor A hacia adelante
  digitalWrite(IN1, LOW);  digitalWrite(IN2, HIGH);
  ledcWrite(ENA, velocidad);
  
  // Encender Motor B hacia adelante
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
  ledcWrite(ENB, velocidad);
}

void moverAtras() {
  Serial.println("Movimiento: ATRAS");
  
  // Invertir Motor A (Giro hacia atrás)
  digitalWrite(IN1, HIGH);  digitalWrite(IN2, LOW);
  ledcWrite(ENA, velocidad);
  
  // Invertir Motor B (Giro hacia atrás)
  digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH);
  ledcWrite(ENB, velocidad);
}

void derecha(){
  Serial.println("Movimiento: derecha");
  
  // Invertir Motor A (Giro hacia atrás)
  digitalWrite(IN1, LOW);  digitalWrite(IN2, HIGH);
  ledcWrite(ENA, velocidad);
  
  // Invertir Motor B (Giro hacia atrás)
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
  ledcWrite(ENB, velocidad);
}

void izquierda(){
   Serial.println("Movimiento: izquierda");
  
  // Invertir Motor A (Giro hacia atrás)
  digitalWrite(IN1, HIGH);  digitalWrite(IN2, LOW);
  ledcWrite(ENA, velocidad);
  
  // Invertir Motor B (Giro hacia atrás)
  digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH);
  ledcWrite(ENB, velocidad);
}

void detener() {
  Serial.println("Movimiento: DETENIDO");
  
  // Apagar Motor A
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  ledcWrite(ENA, 0);
  
  // Apagar Motor B
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
  ledcWrite(ENB, 0);
}

// --- BUCLE PRINCIPAL ---
void loop() {
  detener();
  delay(3000); // Pausa de 3 segundos antes de repetir

  moverAdelante();
  delay(3000); // Mantiene el movimiento por 3 segundos
  
  detener();
  delay(1000); // Pausa de 1 segundo

  moverAtras();
  delay(3000);

  detener();
  delay(1000); // Pausa de 3 segundos antes de repetir

  //moverAtras();
  //delay(3000); // Mantiene el movimiento por 3 segundos

}
