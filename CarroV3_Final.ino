#include "esp_camera.h"
#include <esp_wifi.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Wire.h>

// ===== RED =====
const char* ssid     = "Tu_Wifi_Aquí";
const char* password = "Tu_contraseña_Aquí";
 // ===== CONFIGURACIÓN IP FIJA =====
IPAddress local_IP(192, 168, 0, 100);
IPAddress gateway(192, 168, 0, 1);
IPAddress subnet(255, 255, 255, 0);
//IPAddress primaryDNS(8, 8, 8, 8);      // Opcional: DNS de Google

#define WIFI_MAX_RETRIES 20
int wifiRetryCount = 0;

// ===== GY-6500 I2C =====
const int MPU_ADDR = 0x68;
const int SDA_PIN  = 21;   
const int SCL_PIN  = 47;   
int16_t AcX, AcY, AcZ, Tmp, GyX, GyY, GyZ;
unsigned long tiempoAnteriorGY = 0;

// ===== PINES Y VARIABLES HC-SR04 =====
const int trigPin = 48;      // Pin TRIG del sensor ultrasónico
const int echoPin = 45;      // Pin ECHO del sensor ultrasónico
long duracion;
int distancia = 0;
unsigned long tiempoAnteriorUltrasonico = 0;
const int intervaloUltrasonico = 60; // Lectura cada 60ms (~16 Hz) para no saturar

// ===== PINES CAMARA  =====
#define PWDN_GPIO_NUM  -1
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM  15
#define SIOD_GPIO_NUM  4
#define SIOC_GPIO_NUM  5
#define Y9_GPIO_NUM    16
#define Y8_GPIO_NUM    17
#define Y7_GPIO_NUM    18
#define Y6_GPIO_NUM    12
#define Y5_GPIO_NUM    10
#define Y4_GPIO_NUM    8
#define Y3_GPIO_NUM    9
#define Y2_GPIO_NUM    11
#define VSYNC_GPIO_NUM 6
#define HREF_GPIO_NUM  7
#define PCLK_GPIO_NUM  13

// ===== PINES L293D =====
const int motorPin1 = 1; // IN1 - Motor A (Izquierdo)
const int motorPin2 = 42; // IN2 - Motor A
const int enAPin    = 2; // ENA - Velocidad A

const int motorPin3 = 14; // IN3 - Motor B (Derecho)
const int motorPin4 = 41; // IN4 - Motor B
const int enBPin    = 38; // ENB - Velocidad B

const int pwmFreq = 1000;  // 1 kHz
const int pwmRes  = 8;     // resolución 8 bits → duty 0-255

int motorSpeed = 128;  // velocidad global inicial (50%)

// ===== SERVIDOR / WEBSOCKET =====
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
uint32_t activeClientId = 0;

#define CAM_FRAME_INTERVAL_MS 33  // ~30 FPS max
#define WS_QUEUE_MAX 3
unsigned long tiempoAnteriorCam = 0;

// ===== FUNCIONES MOTOR =====

void stopMotors() {
  digitalWrite(motorPin1, LOW); 
  digitalWrite(motorPin2, LOW);
  digitalWrite(motorPin3, LOW); 
  digitalWrite(motorPin4, LOW);
  ledcWrite(enAPin, 0);
  ledcWrite(enBPin, 0);
}

// carCmd: lógica de movimiento del carro completo con tracción diferencial
void carCmd(char dir) {
  switch (dir) {
    case 'F': // Adelante
      digitalWrite(motorPin1, LOW);  digitalWrite(motorPin2, HIGH);
      digitalWrite(motorPin3, HIGH); digitalWrite(motorPin4, LOW);
      ledcWrite(enAPin, motorSpeed);
      ledcWrite(enBPin, motorSpeed);
      break;
    case 'B': // Atras
      digitalWrite(motorPin1, HIGH); digitalWrite(motorPin2, LOW);
      digitalWrite(motorPin3, LOW);  digitalWrite(motorPin4, HIGH);
      ledcWrite(enAPin, motorSpeed);
      ledcWrite(enBPin, motorSpeed);
      break;
    case 'L': // Giro izquierda: A hacia atras, B hacia adelante
      digitalWrite(motorPin1, HIGH); digitalWrite(motorPin2, LOW);
      digitalWrite(motorPin3, HIGH); digitalWrite(motorPin4, LOW);
      ledcWrite(enAPin, motorSpeed);
      ledcWrite(enBPin, motorSpeed);
      break;
    case 'R': // Giro derecha: A hacia adelante, B hacia atras
      digitalWrite(motorPin1, LOW);  digitalWrite(motorPin2, HIGH);
      digitalWrite(motorPin3, LOW);  digitalWrite(motorPin4, HIGH);
      ledcWrite(enAPin, motorSpeed);
      ledcWrite(enBPin, motorSpeed);
      break;
    case 'S': // Stop
    default:
      stopMotors(); break;
  }
}

// Función para leer el HC-SR04 y actualizar la distancia en cm
void leerUltrasonico() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  
  duracion = pulseIn(echoPin, HIGH, 20000); 
  
  if (duracion == 0) {
    distancia = 999; 
  } else {
    distancia = duracion * 0.0343 / 2;
  }

  // FRENADO DE EMERGENCIA ABSOLUTO: Si está a 5cm o menos se apaga para evitar daños físicos
  if (distancia <= 5 && distancia > 1) { 
    stopMotors(); 
  }
}

// ===== HTML =====
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>ESP32-S3 Camara + Motores</title>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body { font-family: 'Segoe UI', sans-serif; background: #121212; color: #e0e0e0; display: flex; height: 100vh; overflow: hidden; }

    /* Sidebar camara */
    .sidebar {
      width: 295px; background: #1e1e1e; padding: 13px;
      overflow-y: auto; border-right: 1px solid #333; font-size: 13px; flex-shrink: 0;
    }
    .sidebar::-webkit-scrollbar { width: 6px; }
    .sidebar::-webkit-scrollbar-thumb { background: #555; border-radius: 3px; }
    .sidebar h2 { color: #00bcd4; font-size: 16px; text-align: center; border-bottom: 1px solid #333; padding-bottom: 8px; margin-bottom: 10px; }
    .sec { color: #777; font-weight: bold; margin: 11px 0 4px; text-transform: uppercase; font-size: 10px; letter-spacing: 1px; }
    .cg  { background: #2a2a2a; padding: 7px; border-radius: 5px; margin-bottom: 7px; }
    .cg label { display: block; margin-bottom: 3px; color: #bbb; font-size: 12px; }
    select, input[type="range"] { width: 100%; background: #444; color: #fff; border: none; padding: 4px; border-radius: 3px; outline: none; }
    .tog { display: flex; align-items: center; justify-content: space-between; margin-bottom: 4px; font-size: 12px; }

    /* Estilo de animación para alerta */
    @keyframes parpadeo {
      from { opacity: 1; }
      to { opacity: 0.4; }
    }

    /* Centro: video + panel motores */
    .center { flex-grow: 1; display: flex; flex-direction: column; overflow: hidden; background: #000; }
    .video-wrap { flex-grow: 1; position: relative; display: flex; align-items: center; justify-content: center; overflow: hidden; }
    #stream-image { max-width: 100%; max-height: 100%; object-fit: contain; }
    #fps-display {
      position: absolute; top: 10px; left: 10px; background: rgba(0,0,0,.7);
      color: #0f0; padding: 4px 9px; font-weight: bold; border-radius: 4px; font-size: 15px; pointer-events: none;
    }
    #ws-status {
      position: absolute; top: 10px; right: 10px; background: rgba(0,0,0,.7);
      color: #f90; padding: 4px 9px; font-weight: bold; border-radius: 4px; font-size: 12px; pointer-events: none;
    }

    /* Panel motores debajo del video */
    .motor-panel {
      background: #1a1a1a; border-top: 2px solid #00bcd4; padding: 10px 18px;
      display: flex; align-items: center; gap: 22px; flex-wrap: wrap; justify-content: center;
    }
    .motor-label { color: #00bcd4; font-size: 12px; font-weight: bold; text-align: center; margin-bottom: 4px; }

    /* ---- D-PAD CARRO (cruz tipo gamepad) ---- */
    .dpad-wrap { display: flex; flex-direction: column; align-items: center; gap: 4px; }
    .dpad-wrap .motor-label { color: #00bcd4; font-size: 12px; font-weight: bold; }

    .dpad {
      display: grid;
      grid-template-columns: 52px 52px 52px;
      grid-template-rows:    52px 52px 52px;
      gap: 0;
    }

    .dpad > * { display: flex; align-items: center; justify-content: center; }
    .dpad .dc { background: transparent; }
    .dpad button {
      background: #2c2c2c; border: none; color: #e0e0e0; font-size: 22px;
      cursor: pointer; transition: background .1s, transform .08s; user-select: none; -webkit-tap-highlight-color: transparent;
    }
    .dpad button:hover  { background: #3a3a3a; }
    .dpad button:active { background: #00bcd4; color: #000; transform: scale(.93); }

    .d-up    { border-radius: 12px 12px 0 0; }
    .d-left  { border-radius: 12px 0 0 12px; }
    .d-center {
      background: #5a1a1a !important; border-radius: 50% !important;
      width: 44px; height: 44px; margin: auto; font-size: 16px !important;
    }
    .d-center:active { background: #c0392b !important; }
    .d-right { border-radius: 0 12px 12px 0; }
    .d-down  { border-radius: 0 0 12px 12px; }

    .vdiv { width: 1px; background: #333; height: 100%; min-height: 120px; }

    /* Opciones enable + velocidad */
    .motor-opts { display: flex; flex-direction: column; gap: 8px; font-size: 12px; min-width: 175px; }
    .en-row { display: flex; justify-content: space-between; align-items: center; color: #ccc; }
    .en-row span { font-size: 11px; }
    .spd-wrap label { color: #aaa; font-size: 11px; }
    .spd-wrap input[type=range] { width: 100%; margin-top: 4px; }
    .spd-val { color: #00e5ff; font-weight: bold; font-size: 13px; margin-left: 5px; }
    .btn-stop-all {
      background: #7a1a1a; color: #fff; border: none; padding: 8px;
      border-radius: 5px; cursor: pointer; font-size: 13px; width: 100%; margin-top: 2px;
    }
    .btn-stop-all:hover  { background: #9a2020; }
    .btn-stop-all:active { background: #c0392b; }
  </style>
</head>
<body>

<div class="sidebar">
  <h2>Control de Vision</h2>

  <div class="sec">Sensor GY-6500</div>
  <div class="cg">
    <div id="gy-data" style="color:#00bcd4;font-family:monospace;font-size:13px;line-height:1.6">Esperando I2C...</div>
  </div>

  <div class="sec">Sensor de Proximidad</div>
  <div class="cg">
    <p>Distancia Frontal: <span id="distancia-val" style="font-size: 18px; font-weight: bold; color: #00e676;">0</span> cm</p>
    <div id="alerta-choque" style="display: none; background-color: #ff1744; color: white; text-align: center; padding: 10px; font-weight: bold; border-radius: 5px; margin-top: 10px; animation: parpadeo 0.5s infinite alternate; font-size: 11px;">
      ⚠️ ¡PELIGRO de CHOQUE INMINENTE! ⚠️
    </div>
  </div>

  <div class="sec">Basico</div>
  <div class="cg">
    <label>Resolucion</label>
    <select id="framesize" onchange="updateConfig(this)">
      <option value="13">UXGA 1600x1200</option>
      <option value="12">SXGA 1280x1024</option>
      <option value="10">XGA 1024x768</option>
      <option value="9">SVGA 800x600</option>
      <option value="8" selected>VGA 640x480</option>
      <option value="7">HVGA 480x320</option>
      <option value="6">CIF 400x296</option>
      <option value="5">QVGA 320x240</option>
      <option value="4">240x240</option>
      <option value="3">HQVGA 240x176</option>
      <option value="2">QCIF 176x144</option>
      <option value="1">QQVGA 160x120</option>
      <option value="0">96x96</option>
    </select>
  </div>
  <div class="cg">
    <label>Calidad JPEG (<span id="quality-val">12</span>)</label>
    <input type="range" id="quality" min="10" max="63" value="12"
      onchange="updateConfig(this)" oninput="document.getElementById('quality-val').innerText=this.value">
  </div>

  <div class="sec">Imagen y Color</div>
  <div class="cg">
    <label>Brillo (<span id="brightness-val">0</span>)</label>
    <input type="range" id="brightness" min="-2" max="2" value="0"
      onchange="updateConfig(this)" oninput="document.getElementById('brightness-val').innerText=this.value">
  </div>
  <div class="cg">
    <label>Contraste (<span id="contrast-val">0</span>)</label>
    <input type="range" id="contrast" min="-2" max="2" value="0"
      onchange="updateConfig(this)" oninput="document.getElementById('contrast-val').innerText=this.value">
  </div>
  <div class="cg">
    <label>Saturacion (<span id="saturation-val">0</span>)</label>
    <input type="range" id="saturation" min="-2" max="2" value="0"
      onchange="updateConfig(this)" oninput="document.getElementById('saturation-val').innerText=this.value">
  </div>
  <div class="cg">
    <label>Filtro / Efecto</label>
    <select id="special_effect" onchange="updateConfig(this)">
      <option value="0" selected>Normal</option>
      <option value="1">Negativo</option>
      <option value="2">Escala de Grises</option>
      <option value="3">Tinte Rojo</option>
      <option value="4">Tinte Verde</option>
      <option value="5">Tinte Azul</option>
      <option value="6">Sepia</option>
    </select>
  </div>

  <div class="sec">Sensores Avanzados</div>
  <div class="cg">
    <div class="tog"><label>Exposicion Auto (AEC)</label><input type="checkbox" id="aec" checked onchange="updateConfig(this)"></div>
    <div class="tog"><label>Ganancia Auto (AGC)</label><input type="checkbox" id="agc" checked onchange="updateConfig(this)"></div>
    <div class="tog"><label>Balance Blancos (AWB)</label><input type="checkbox" id="awb" checked onchange="updateConfig(this)"></div>
    <div class="tog"><label>Correccion Lente (LENC)</label><input type="checkbox" id="lenc" checked onchange="updateConfig(this)"></div>
  </div>
  <div class="cg">
    <label>Modo Luz</label>
    <select id="wb_mode" onchange="updateConfig(this)">
      <option value="0" selected>Automatico</option>
      <option value="1">Soleado</option>
      <option value="2">Nublado</option>
      <option value="3">Oficina</option>
      <option value="4">Incandescente</option>
    </select>
  </div>

  <div class="sec">Orientacion</div>
  <div class="cg">
    <div class="tog"><label>Espejo Horizontal</label><input type="checkbox" id="hmirror" onchange="updateConfig(this)"></div>
    <div class="tog"><label>Volteo Vertical</label><input type="checkbox" id="vflip" onchange="updateConfig(this)"></div>
    <div class="tog"><label style="color:#f77">Color Bar (Prueba)</label><input type="checkbox" id="colorbar" onchange="updateConfig(this)"></div>
  </div>
</div>

<div class="center">
  <div class="video-wrap">
    <div id="fps-display">FPS: 0</div>
    <div id="ws-status">WS: Conectando...</div>
    <img id="stream-image" src="">
  </div>

  <div class="motor-panel">
    <div class="dpad-wrap">
      <div class="motor-label">Control Carro</div>
      <div class="dpad">
        <div class="dc"></div>
        <button class="d-up" onmousedown="carCmd('F')" onmouseup="carCmd('S')" ontouchstart="carCmd('F')" ontouchend="carCmd('S')" title="Adelante">&#9650;</button>
        <div class="dc"></div>
        <button class="d-left" onmousedown="carCmd('L')" onmouseup="carCmd('S')" ontouchstart="carCmd('L')" ontouchend="carCmd('S')" title="Izquierda">&#9664;</button>
        <button class="d-center" onclick="carCmd('S')" title="Detener">&#9632;</button>
        <button class="d-right" onmousedown="carCmd('R')" onmouseup="carCmd('S')" ontouchstart="carCmd('R')" ontouchend="carCmd('S')" title="Derecha">&#9654;</button>
        <div class="dc"></div>
        <button class="d-down" onmousedown="carCmd('B')" onmouseup="carCmd('S')" ontouchstart="carCmd('B')" ontouchend="carCmd('S')" title="Atras">&#9660;</button>
        <div class="dc"></div>
      </div>
    </div>

    <div class="vdiv"></div>

    <div class="motor-opts">
      <div class="en-row">
        <span>Enable A — GPIO 40</span>
        <input type="checkbox" id="enA" checked onchange="setEnable('A', this.checked)">
      </div>
      <div class="en-row">
        <span>Enable B — GPIO 38</span>
        <input type="checkbox" id="enB" checked onchange="setEnable('B', this.checked)">
      </div>
      <div class="spd-wrap">
        <label>Velocidad: <span id="spd-val" class="spd-val">50%</span></label>
        <input type="range" id="speedSlider" min="0" max="255" value="128"
          oninput="updateSpeed(this.value)" onchange="sendSpeed(this.value)">
      </div>
      <button class="btn-stop-all" onclick="carCmd('S')">&#9632; DETENER TODO</button>
    </div>
  </div>
</div>

<script>
  function carCmd(dir) {
    fetch('/car?d=' + dir).catch(e => console.error(e));
  }

  function setEnable(m, on) {
    var v = on ? document.getElementById('speedSlider').value : 0;
    fetch('/enable?m=' + m + '&v=' + v).catch(e => console.error(e));
  }

  function updateSpeed(v) {
    document.getElementById('spd-val').innerText = Math.round(v / 255 * 100) + '%';
  }

  function sendSpeed(v) {
    updateSpeed(v);
    fetch('/speed?v=' + v).catch(e => console.error(e));
  }

  function updateConfig(el) {
    var value = el.type === 'checkbox' ? (el.checked ? 1 : 0) : el.value;
    fetch('/control?var=' + el.id + '&val=' + value).catch(e => console.error(e));
  }

  var gateway = 'ws://' + window.location.hostname + '/ws';
  var websocket, reconnectDelay = 1000, reconnectTimer = null;
  var frameCount = 0, lastTime = Date.now();

  function initWebSocket() {
    if (websocket && websocket.readyState !== WebSocket.CLOSED) return;
    var st = document.getElementById('ws-status');
    st.innerText = 'WS: Conectando...'; st.style.color = '#f90';
    websocket = new WebSocket(gateway);
    websocket.binaryType = 'blob';

    websocket.onopen = function() {
      reconnectDelay = 1000;
      st.innerText = 'WS: Conectado'; st.style.color = '#0f0';
    };
    websocket.onclose = function() {
      st.innerText = 'WS: Desconectado'; st.style.color = '#f00';
      if (reconnectTimer) clearTimeout(reconnectTimer);
      reconnectTimer = setTimeout(function() {
        reconnectDelay = Math.min(reconnectDelay * 2, 16000);
        initWebSocket();
      }, reconnectDelay);
    };
    websocket.onerror = function() { st.innerText = 'WS: Error'; st.style.color = '#f00'; };

    websocket.onmessage = function(event) {
      if (typeof event.data === 'string') {
        if (event.data.startsWith("US:")) {
          let dist = parseInt(event.data.split(":")[1]);
          document.getElementById('distancia-val').innerText = dist;
          let alerta = document.getElementById('alerta-choque');
          let distVal = document.getElementById('distancia-val');
          
          if (dist <= 5) {
            alerta.style.display = 'block';
            distVal.style.color = '#ff1744';
          } else if (dist <= 20) {
            alerta.style.display = 'none';
            distVal.style.color = '#ff9100';
          } else {
            alerta.style.display = 'none';
            distVal.style.color = '#00e676';
          }
        } else {
          document.getElementById('gy-data').innerHTML = event.data;
        }
      } else {
        var url = URL.createObjectURL(event.data);
        var img = document.getElementById('stream-image');
        var old = img.src;
        img.src = url;
        if (old && old.startsWith('blob:')) URL.revokeObjectURL(old);
        frameCount++;
        var now = Date.now();
        if (now - lastTime >= 1000) {
          document.getElementById('fps-display').innerText = 'FPS: ' + frameCount;
          frameCount = 0; lastTime = now;
        }
      }
    };
  }

  window.addEventListener('beforeunload', function() { if (websocket) websocket.close(); });
  window.onload = initWebSocket;
</script>
</body>
</html>
)rawliteral";

// ===== WEBSOCKET EVENTOS =====
void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    if (activeClientId != 0) {
      AsyncWebSocketClient* old = server->client(activeClientId);
      if (old) old->close();
    }
    activeClientId = client->id();
    Serial.printf("[WS] Conectado ID:%u\n", activeClientId);
  } else if (type == WS_EVT_DISCONNECT || type == WS_EVT_ERROR) {
    if (activeClientId == client->id()) activeClientId = 0;
  }
}

// ===== RECONEXION WIFI =====
void checkWiFiConnection() {
  if (WiFi.status() == WL_CONNECTED) { wifiRetryCount = 0; return; }
  Serial.println("[WiFi] Reconectando...");
  WiFi.disconnect(true); delay(100);
  WiFi.begin(ssid, password);
  int a = 0;
  while (WiFi.status() != WL_CONNECTED && a < WIFI_MAX_RETRIES) { delay(500); a++; }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("[WiFi] OK: " + WiFi.localIP().toString()); wifiRetryCount = 0;
  } else {
    if (++wifiRetryCount >= 5) { Serial.println("[WiFi] Reiniciando..."); delay(1000); ESP.restart(); }
  }
}

// ===== SETUP =====
void setup() {
  Serial.begin(115200); delay(1000);
  Serial.println("\n--- Iniciando ---");
  // Inicialización HC-SR04
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  // I2C GY-6500
  Wire.begin(SDA_PIN, SCL_PIN, 100000);
  Wire.beginTransmission(MPU_ADDR);
  if (Wire.endTransmission() != 0) {
    Serial.println("ERROR: GY-6500 no detectado (pines 21/47)");
  } else {
    Wire.beginTransmission(MPU_ADDR); Wire.write(0x6B); Wire.write(0); Wire.endTransmission(true);
    Serial.println("GY-6500 OK");
  }

  // Camara
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0; config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0=Y2_GPIO_NUM; config.pin_d1=Y3_GPIO_NUM; config.pin_d2=Y4_GPIO_NUM;
  config.pin_d3=Y5_GPIO_NUM; config.pin_d4=Y6_GPIO_NUM; config.pin_d5=Y7_GPIO_NUM;
  config.pin_d6=Y8_GPIO_NUM; config.pin_d7=Y9_GPIO_NUM; config.pin_xclk=XCLK_GPIO_NUM;
  config.pin_pclk=PCLK_GPIO_NUM; config.pin_vsync=VSYNC_GPIO_NUM; config.pin_href=HREF_GPIO_NUM;
  config.pin_sccb_sda=SIOD_GPIO_NUM; config.pin_sccb_scl=SIOC_GPIO_NUM;
  config.pin_pwdn=PWDN_GPIO_NUM; config.pin_reset=RESET_GPIO_NUM;
  config.xclk_freq_hz=20000000; config.frame_size=FRAMESIZE_VGA;
  config.pixel_format=PIXFORMAT_JPEG; config.jpeg_quality=12;
  config.fb_count=3; config.grab_mode=CAMERA_GRAB_LATEST;

  if (esp_camera_init(&config) != ESP_OK) {
    Serial.println("ERROR: Camara no iniciada");
  } else {
    sensor_t* s = esp_camera_sensor_get();
    if (s) {
      s->set_brightness(s,0); s->set_contrast(s,0); s->set_saturation(s,0);
      s->set_special_effect(s,0); s->set_whitebal(s,1); s->set_awb_gain(s,1);
      s->set_wb_mode(s,0); s->set_exposure_ctrl(s,1); s->set_aec2(s,1);
      s->set_gain_ctrl(s,1); s->set_bpc(s,0); s->set_wpc(s,1);
      s->set_raw_gma(s,1); s->set_lenc(s,1);
      Serial.println("Camara OK");
    }
  }

  // WiFi
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  
  // Paso CRUCIAL: Configurar la IP antes de iniciar conexión
  if (!WiFi.config(local_IP, gateway, subnet)) {
    Serial.println("Fallo al configurar IP Fija");
  }
  WiFi.setTxPower(WIFI_POWER_20dBm);
  //cambiar si su esp32s3 presenta fallas de conexion: esp_wifi_set_max_tx_power(30); // Limita potencia a 10dBm para evitar crash del S3
  WiFi.setAutoReconnect(true); 
  WiFi.persistent(false);
  WiFi.begin(ssid, password);
  Serial.print("WiFi");
  int t = 0;
  while (WiFi.status() != WL_CONNECTED && t < 40) { delay(500); Serial.print("."); t++; }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nIP: " + WiFi.localIP().toString());
    Serial.printf("RSSI: %d dBm\n", WiFi.RSSI());
  } else { Serial.println("\nERROR WiFi"); delay(2000); ESP.restart(); }

  // L293D: pines de direccion
  pinMode(motorPin1, OUTPUT); pinMode(motorPin2, OUTPUT);
  pinMode(motorPin3, OUTPUT); pinMode(motorPin4, OUTPUT);

  // L293D: PWM para enable (canales 1 y 2, canal 0 lo usa la camara)
  ledcAttach(enAPin, pwmFreq, pwmRes);
  ledcAttach(enBPin, pwmFreq, pwmRes);
  stopMotors();

  // Servidor
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest* r){ r->send_P(200,"text/html",index_html); });

  // Control camara
  server.on("/control", HTTP_GET, [](AsyncWebServerRequest* request){
    if (!request->hasParam("var")||!request->hasParam("val")){ request->send(400,"text/plain","Parametros"); return; }
    String var = request->getParam("var")->value();
    int val    = request->getParam("val")->value().toInt();
    sensor_t* s = esp_camera_sensor_get();
    if (!s){ request->send(500,"text/plain","Sin sensor"); return; }
    int res=0;
    if      (var=="framesize")     { if(s->pixformat==PIXFORMAT_JPEG) res=s->set_framesize(s,(framesize_t)val); }
    else if (var=="quality")       res=s->set_quality(s,val);
    else if (var=="contrast")      res=s->set_contrast(s,val);
    else if (var=="brightness")    res=s->set_brightness(s,val);
    else if (var=="saturation")    res=s->set_saturation(s,val);
    else if (var=="special_effect")res=s->set_special_effect(s,val);
    else if (var=="hmirror")       res=s->set_hmirror(s,val);
    else if (var=="vflip")         res=s->set_vflip(s,val);
    else if (var=="colorbar")      res=s->set_colorbar(s,val);
    else if (var=="awb")           res=s->set_whitebal(s,val);
    else if (var=="aec")           res=s->set_exposure_ctrl(s,val);
    else if (var=="agc")           res=s->set_gain_ctrl(s,val);
    else if (var=="lenc")          res=s->set_lenc(s,val);
    else if (var=="wb_mode")       { s->set_awb_gain(s,val==0?1:0); res=s->set_wb_mode(s,val); }
    res==0 ? request->send(200,"text/plain","OK") : request->send(500,"text/plain","Error");
  });

  // Control carro unificado: /car?d=F|B|L|R|S
  server.on("/car", HTTP_GET, [](AsyncWebServerRequest* request){
    if (!request->hasParam("d")){ request->send(400,"text/plain","Parametros"); return; }
    String d = request->getParam("d")->value();
    if (d.length() == 1 && String("FBLRS").indexOf(d) >= 0) {
      carCmd(d[0]);
      request->send(200,"text/plain","OK");
    } else {
      request->send(400,"text/plain","Comando invalido");
    }
  });

  // Enable individual: /enable?m=A|B&v=0-255
  server.on("/enable", HTTP_GET, [](AsyncWebServerRequest* request){
    if (!request->hasParam("m")||!request->hasParam("v")){ request->send(400,"text/plain","Parametros"); return; }
    String m = request->getParam("m")->value();
    int v    = constrain(request->getParam("v")->value().toInt(), 0, 255);
    if      (m=="A") ledcWrite(enAPin, v);
    else if (m=="B") ledcWrite(enBPin, v);
    else { request->send(400,"text/plain","Motor invalido"); return; }
    request->send(200,"text/plain","OK");
  });

  // Velocidad global: /speed?v=0-255
  server.on("/speed", HTTP_GET, [](AsyncWebServerRequest* request){
    if (!request->hasParam("v")){ request->send(400,"text/plain","Parametros"); return; }
    motorSpeed = constrain(request->getParam("v")->value().toInt(), 0, 255);
    if (digitalRead(motorPin1)||digitalRead(motorPin2)) ledcWrite(enAPin, motorSpeed);
    if (digitalRead(motorPin3)||digitalRead(motorPin4)) ledcWrite(enBPin, motorSpeed);
    request->send(200,"text/plain","OK");
  });

  // Estado del sistema
  server.on("/status", HTTP_GET, [](AsyncWebServerRequest* request){
    String j="{\"ip\":\""+WiFi.localIP().toString()+"\",\"rssi\":"+String(WiFi.RSSI())+",\"clients\":"+String(ws.count())+"}";
    request->send(200,"application/json",j);
  });

  server.begin();
  Serial.println("Servidor OK.");
}

// ===== LOOP =====
void loop() {
  ws.cleanupClients();

  static unsigned long tWifi = 0;
  unsigned long now = millis();
  if (now - tWifi >= 10000) { tWifi = now; checkWiFiConnection(); }
  
  // 1. Lectura física del HC-SR04 de manera asíncrona (Cada 60ms)
  if (now - tiempoAnteriorUltrasonico >= intervaloUltrasonico) {
    tiempoAnteriorUltrasonico = now;
    leerUltrasonico();
    Serial.printf("Distancia frontal: %d cm\n", distancia);
  }

  AsyncWebSocketClient* client = nullptr;
  if (activeClientId != 0) {
    client = ws.client(activeClientId);
    if (client && client->status() != WS_CONNECTED) { client = nullptr; activeClientId = 0; }
  }

  // 2. Envío de datos por WebSocket si el cliente está activo
  if (client && !client->queueIsFull()) {
    
    // Transmitir distancia del ultrasonido por WebSocket (Bucle propio cada 100ms)
    static unsigned long tiempoAnteriorEnvioUS = 0;
    if (now - tiempoAnteriorEnvioUS >= 100) {
      tiempoAnteriorEnvioUS = now;
      String datosUS = "US:" + String(distancia);
      client->text(datosUS);
    }

    // Transmitir datos del GY-6500 por WebSocket (Cada 100ms en desfase)
    if (now - tiempoAnteriorGY >= 100) {
      tiempoAnteriorGY = now;
      Wire.beginTransmission(MPU_ADDR); Wire.write(0x3B); Wire.endTransmission(false);
      Wire.requestFrom(MPU_ADDR, 14, true);
      if (Wire.available() == 14) {
        AcX=Wire.read()<<8|Wire.read(); AcY=Wire.read()<<8|Wire.read(); AcZ=Wire.read()<<8|Wire.read();
        Tmp=Wire.read()<<8|Wire.read();
        GyX=Wire.read()<<8|Wire.read(); GyY=Wire.read()<<8|Wire.read(); GyZ=Wire.read()<<8|Wire.read();
        
        // Formato estándar para el contenedor de la interfaz
        String info = "Ax:"+String(AcX)+" Ay:"+String(AcY)+" Az:"+String(AcZ)+"<br>"+
                      "Gx:"+String(GyX)+" Gy:"+String(GyY)+" Gz:"+String(GyZ);
        client->text(info);
      }
    }

    // Transmitir frame de la cámara (Control de FPS y cola de video)
    if (now - tiempoAnteriorCam >= CAM_FRAME_INTERVAL_MS) {
      tiempoAnteriorCam = now;
      if (client->queueLen() < WS_QUEUE_MAX) {
        camera_fb_t* fb = esp_camera_fb_get();
        if (fb) { client->binary((uint8_t*)fb->buf, fb->len); esp_camera_fb_return(fb); }
      }
    }
  }

  delay(1); // Pequeño respiro para el planificador de tareas del ESP32
}
