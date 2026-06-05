#include "esp_camera.h"
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

// ==========================================
// 1. CONFIGURACIÓN DE RED
// ==========================================
const char* ssid = "Tu_wifi_Aquí";
const char* password = "Tu_Contraseña_Aquí";

AsyncWebSocketClient * globalClient = NULL;
void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);

// ==========================================
// 2. DEFINICIÓN DE PINES (ESP32-S3 WROOM)
// ==========================================
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

// ==========================================
// 3. PÁGINA WEB (HTML + CSS + JS)
// ==========================================
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Cámara Neuromórfica S3</title>
  <style>
    body { font-family: 'Segoe UI', sans-serif; margin: 0; background-color: #121212; color: #e0e0e0; display: flex; height: 100vh; overflow: hidden; }
    .sidebar { width: 320px; background-color: #1e1e1e; padding: 15px; box-sizing: border-box; overflow-y: auto; border-right: 1px solid #333; font-size: 13px; }
    .sidebar::-webkit-scrollbar { width: 8px; }
    .sidebar::-webkit-scrollbar-thumb { background: #555; border-radius: 4px; }
    .sidebar h2 { margin-top: 0; color: #00bcd4; font-size: 18px; text-align: center; border-bottom: 1px solid #333; padding-bottom: 10px; }
    .section-title { color: #888; font-weight: bold; margin: 15px 0 5px 0; text-transform: uppercase; font-size: 11px; letter-spacing: 1px;}
    .control-group { margin-bottom: 10px; background: #2a2a2a; padding: 8px; border-radius: 5px; }
    .control-group label { display: block; margin-bottom: 4px; color: #ccc;}
    select, input[type="range"] { width: 100%; box-sizing: border-box; background: #444; color: #fff; border: none; padding: 5px; border-radius: 3px; outline: none; }
    .toggle { display: flex; align-items: center; justify-content: space-between; margin-bottom: 5px; }
    .main-content { flex-grow: 1; display: flex; justify-content: center; align-items: center; position: relative; background-color: #000; }
    img { max-width: 100%; max-height: 100vh; object-fit: contain; }
    #fps-display { position: absolute; top: 15px; left: 15px; background-color: rgba(0,0,0,0.7); color: #0f0; padding: 5px 10px; font-weight: bold; border-radius: 5px; font-size: 16px; pointer-events: none; z-index: 10; }
  </style>
</head>
<body>

  <div class="sidebar">
    <h2>Control de Visión</h2>
    
    <div class="section-title">Básico</div>
    
    <div class="control-group">
      <label for="framesize">Resolución</label>
      <select id="framesize" onchange="updateConfig(this)">
        <option value="13">UXGA (1600x1200)</option>
        <option value="12">SXGA (1280x1024)</option>
        <option value="10">XGA (1024x768)</option>
        <option value="9">SVGA (800x600)</option>
        <option value="8" selected>VGA (640x480)</option>
        <option value="7">HVGA (480x320)</option>
        <option value="6">CIF (400x296)</option>
        <option value="5">QVGA (320x240)</option>
        <option value="4">240x240 (Cuadrada)</option>
        <option value="3">HQVGA (240x176)</option>
        <option value="2">QCIF (176x144)</option>
        <option value="1">QQVGA (160x120)</option>
        <option value="0">96x96 (IA Mini)</option>
      </select>
    </div>

    <div class="control-group">
      <label>Calidad JPEG (<span id="quality-val">12</span>)</label>
      <input type="range" id="quality" min="10" max="63" value="12" onchange="updateConfig(this)" oninput="document.getElementById('quality-val').innerText = this.value">
    </div>

    <div class="section-title">Imagen y Color</div>

    <div class="control-group">
      <label>Brillo (<span id="brightness-val">0</span>)</label>
      <input type="range" id="brightness" min="-2" max="2" value="0" onchange="updateConfig(this)" oninput="document.getElementById('brightness-val').innerText = this.value">
    </div>

    <div class="control-group">
      <label>Contraste (<span id="contrast-val">0</span>)</label>
      <input type="range" id="contrast" min="-2" max="2" value="0" onchange="updateConfig(this)" oninput="document.getElementById('contrast-val').innerText = this.value">
    </div>
    
    <div class="control-group">
      <label>Saturación (<span id="saturation-val">0</span>)</label>
      <input type="range" id="saturation" min="-2" max="2" value="0" onchange="updateConfig(this)" oninput="document.getElementById('saturation-val').innerText = this.value">
    </div>

    <div class="control-group">
      <label for="special_effect">Filtro / Efecto</label>
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

    <div class="section-title">Sensores Avanzados (IA)</div>

    <div class="control-group">
      <div class="toggle"><label>Exposición Automática (AEC)</label><input type="checkbox" id="aec" checked onchange="updateConfig(this)"></div>
      <div class="toggle"><label>Ganancia Automática (AGC)</label><input type="checkbox" id="agc" checked onchange="updateConfig(this)"></div>
      <div class="toggle"><label>Balance Blancos Auto (AWB)</label><input type="checkbox" id="awb" checked onchange="updateConfig(this)"></div>
      <div class="toggle"><label>Corrección Lente (LENC)</label><input type="checkbox" id="lenc" checked onchange="updateConfig(this)"></div>
    </div>

    <div class="control-group">
      <label for="wb_mode">Modo Luz (Requiere AWB ON)</label>
      <select id="wb_mode" onchange="updateConfig(this)">
        <option value="0" selected>Automático</option>
        <option value="1">Soleado</option>
        <option value="2">Nublado</option>
        <option value="3">Oficina</option>
        <option value="4">Casa (Incandescente)</option>
      </select>
    </div>

    <div class="section-title">Orientación y Pruebas</div>

    <div class="control-group">
      <div class="toggle"><label>Espejo Horizontal</label><input type="checkbox" id="hmirror" onchange="updateConfig(this)"></div>
      <div class="toggle"><label>Volteo Vertical</label><input type="checkbox" id="vflip" onchange="updateConfig(this)"></div>
      <div class="toggle"><label style="color:red;">Color Bar (Prueba)</label><input type="checkbox" id="colorbar" onchange="updateConfig(this)"></div>
    </div>

  </div>

  <div class="main-content">
    <div id="fps-display">FPS: 0</div>
    <img id="stream-image" src="">
  </div>
  
  <script>
    function updateConfig(el) {
      let value = el.type === 'checkbox' ? (el.checked ? 1 : 0) : el.value;
      fetch(`/control?var=${el.id}&val=${value}`)
        .then(response => { if(!response.ok) console.error("Error"); });
    }

    var gateway = `ws://${window.location.hostname}/ws`;
    var websocket;
    var frameCount = 0;
    var lastTime = Date.now();
    var fpsDisplay = document.getElementById("fps-display");

    function initWebSocket() {
      websocket = new WebSocket(gateway);
      websocket.binaryType = "blob";
      websocket.onclose = function(event) { setTimeout(initWebSocket, 2000); };
      websocket.onmessage = function(event) {
        var imageBlob = event.data;
        var urlObject = URL.createObjectURL(imageBlob);
        var imgTag = document.getElementById("stream-image");
        if (imgTag.src) URL.revokeObjectURL(imgTag.src);
        imgTag.src = urlObject;

        frameCount++;
        var now = Date.now();
        if (now - lastTime >= 1000) {
          fpsDisplay.innerText = "FPS: " + frameCount;
          frameCount = 0;
          lastTime = now;
        }
      };
    }
    window.onload = initWebSocket;
  </script>
</body>
</html>
)rawliteral";

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

void setup() {
  Serial.begin(115200);

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  
  config.frame_size = FRAMESIZE_QVGA;
  config.pixel_format = PIXFORMAT_JPEG;
  config.jpeg_quality = 12;
  config.fb_count = 3;
  config.grab_mode = CAMERA_GRAB_LATEST; // Mantiene el buffer limpio de fotos viejas

  if (esp_camera_init(&config) != ESP_OK) {
    Serial.println("Error cámara");
    return;
  }

  sensor_t * s = esp_camera_sensor_get();
  s->set_brightness(s, 0);     
  s->set_contrast(s, 0);       
  s->set_saturation(s, 0);     
  s->set_special_effect(s, 0); 
  s->set_whitebal(s, 1);       
  s->set_awb_gain(s, 1);       
  s->set_wb_mode(s, 0);        
  s->set_exposure_ctrl(s, 1);  
  s->set_aec2(s, 1);           
  s->set_gain_ctrl(s, 1);      
  s->set_bpc(s, 0);            
  s->set_wpc(s, 1);            
  s->set_raw_gma(s, 1);        
  s->set_lenc(s, 1);           
// Baja la potencia de transmisión para reducir el calor
  //WiFi.setTxPower(WIFI_POWER_11dBm);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\nWiFi OK!");
  Serial.println(WiFi.localIP());

  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html);
  });

  server.on("/control", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("var") && request->hasParam("val")) {
      String var = request->getParam("var")->value();
      int val = request->getParam("val")->value().toInt();
      
      sensor_t * s = esp_camera_sensor_get();
      int res = 0;
      
      if (var == "framesize") { if(s->pixformat == PIXFORMAT_JPEG) res = s->set_framesize(s, (framesize_t)val); }
      else if (var == "quality") res = s->set_quality(s, val);
      else if (var == "contrast") res = s->set_contrast(s, val);
      else if (var == "brightness") res = s->set_brightness(s, val);
      else if (var == "saturation") res = s->set_saturation(s, val);
      else if (var == "special_effect") res = s->set_special_effect(s, val);
      else if (var == "hmirror") res = s->set_hmirror(s, val);
      else if (var == "vflip") res = s->set_vflip(s, val);
      else if (var == "colorbar") res = s->set_colorbar(s, val);
      else if (var == "awb") res = s->set_whitebal(s, val);
      else if (var == "aec") res = s->set_exposure_ctrl(s, val);
      else if (var == "agc") res = s->set_gain_ctrl(s, val);
      else if (var == "lenc") res = s->set_lenc(s, val);
      else if (var == "wb_mode") {
        if (val == 0) { s->set_awb_gain(s, 1); } 
        else { s->set_awb_gain(s, 0); }          
        res = s->set_wb_mode(s, val);
      }
      
      if(res == 0) request->send(200, "text/plain", "OK");
      else request->send(500, "text/plain", "Error sensor");
    } else {
      request->send(400, "text/plain", "Faltan parametros");
    }
  });

  server.begin();
}

void loop() {
  ws.cleanupClients();
  
  // === OPTIMIZACIÓN EXTREMA DE CPU ===
  // Primero verificamos que haya un cliente y que el WiFi NO esté saturado.
  // Si está ocupado, simplemente no hacemos nada.
  if (globalClient != NULL && !globalClient->queueIsFull()) {
    
    // Solo pedimos el esfuerzo físico de capturar la imagen si sabemos 
    // que la podemos enviar.
    camera_fb_t * fb = esp_camera_fb_get();
    
    if (fb) {
       ws.binaryAll((uint8_t*)fb->buf, fb->len);
       esp_camera_fb_return(fb); // Liberamos la memoria RAM
    }
  }
  
  // Como ya no ahogamos a la placa capturando fotos que no se pueden enviar,
  // el delay puede ser más corto, solo para permitir que la tarea WiFi respire.
  delay(11); 
}

void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) globalClient = client;
  else if (type == WS_EVT_DISCONNECT) globalClient = NULL;
}
