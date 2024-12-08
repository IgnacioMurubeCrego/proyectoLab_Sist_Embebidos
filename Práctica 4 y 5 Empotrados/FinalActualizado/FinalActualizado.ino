/*
FUNCIONAMIENTO PROGRAMA:
  - Sensor de humedad y temperatura -> unica forma de comprobar acercando un mechero (CUIDADO) --> (En teoría arreglado)
  - Sensor de inclinacion + Buzzer -> se se gira el sensor de inclinacion se enciende el buzzer
  - Sensor de luz -> con la linterna del movil se ve como sube
  - Sensor de nivel de agua + led RGB -> dependiendo del nivel cambia de color el led RGB (Verde : Nulo/Bajo ; Azul : Bajo ; Rojo : Alto)
  - Incluye OTA ('On the Air' uploads) para cargar programas a la ESP32S3 mediante conexión a un red Wifi.
  - Monta un servidor web donde se pueden visualizar los valores de los sensores en tiempo real
*/

#include <DHT.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoOTA.h>

// Configuración de red WiFi
const char *ssid = "IMCGalaxy";
const char *password = "tund3965";

// Configuración del servidor web
WebServer server(80);

// Configuración del sensor DHT11
#define DHTPIN 9
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// Pines del LED RGB y otros sensores
const int pinAzul = 42;
const int pinVerde = 41;
const int pinRojo = 40;
int SENSOR;
int photoPin = 1;
const int pinBuzzer = 4;  // Pin buzzer activo
const int pinSensor = 7;  // Pin sensor de inclinación

// Variables de los sensores
float h = 0.0, t = 0.0, hic = 0.0; // Humedad, temperatura, índice de calor
int light = 0; // Nivel de luz

// Declaración del timer
hw_timer_t *timer = NULL;

// Bandera para controlar la medición
volatile bool medir = false;

// Función asociada a la interrupción del timer
void IRAM_ATTR onTimer() {
  medir = true; // Activamos el flag
}

// Función para establecer el color del LED RGB
void setColor(int rojo, int verde, int azul) {
  digitalWrite(pinRojo, rojo > 0 ? HIGH : LOW);
  digitalWrite(pinVerde, verde > 0 ? HIGH : LOW);
  digitalWrite(pinAzul, azul > 0 ? HIGH : LOW);
}

void handleRoot() {
  String content = "<html>"
                   "<head><meta charset='utf-8'><title>Sensores ESP32</title>"
                   "<script>"
                   "function updateData() {"
                   "  fetch('/data').then(response => response.json()).then(data => {"
                   "    document.getElementById('humedad').innerText = data.humedad + '%';"
                   "    document.getElementById('temperatura').innerText = data.temperatura + '°C';"
                   "    document.getElementById('calor').innerText = data.calor + '°C';"
                   "    document.getElementById('luz').innerText = data.luz;"
                   "    document.getElementById('agua').innerText = data.nivelAgua;"
                   "  });"
                   "}"
                   "setInterval(updateData, 1000);"
                   "</script>"
                   "</head>"
                   "<body onload='updateData()'>"
                   "<h1>Lecturas de Sensores</h1>"
                   "<p>Humedad: <span id='humedad'></span></p>"
                   "<p>Temperatura: <span id='temperatura'></span></p>"
                   "<p>Índice de Calor: <span id='calor'></span></p>"
                   "<p>Luz: <span id='luz'></span></p>"
                   "<p>Nivel de Agua: <span id='agua'></span></p>"
                   "</body>"
                   "</html>";
  server.send(200, "text/html", content);
}

void handleData() {
  // Determinar nivel de agua en texto según el valor del sensor
  String nivelAgua;
  if (SENSOR < 10) {
    nivelAgua = "Nulo/Bajo";
  } else if (SENSOR > 10 && SENSOR <= 370) {
    nivelAgua = "Medio";
  } else {
    nivelAgua = "Alto";
  }

  String json = "{";
  json += "\"humedad\": " + String(h) + ",";
  json += "\"temperatura\": " + String(t) + ",";
  json += "\"calor\": " + String(hic) + ",";
  json += "\"luz\": " + String(light) + ",";
  json += "\"nivelAgua\": \"" + nivelAgua + " (Valor: " + String(SENSOR) + ")\"";
  json += "}";

  server.send(200, "application/json", json);
}

// Función para manejar una página no encontrada
void handleNotFound() {
  server.send(404, "text/plain", "Page not found");
}

// Configuración inicial
void setup() {
  Serial.begin(115200);

  // Configurar los pines
  pinMode(pinBuzzer, OUTPUT);
  pinMode(pinSensor, INPUT);
  pinMode(pinRojo, OUTPUT);
  pinMode(pinVerde, OUTPUT);
  pinMode(pinAzul, OUTPUT);

  // Inicializar el sensor DHT
  dht.begin();

 // Inicializamos el timer: frecuencia de 1 MHz (1 tick = 1 microsegundo)
  timer = timerBegin(1000000); 

  // Asociamos la interrupción al timer
  timerAttachInterrupt(timer, &onTimer);

  // Configuramos el periodo de la alarma (5,000,000 ticks = 5 segundos)
  timerAlarm(timer, 5000000, true, 0); // Alarma en 5 segundos, modo autoreload activado

  // Iniciar el timer
  timerWrite(timer,0);
  timerStart(timer);

  // Conexión a WiFi
  WiFi.begin(ssid, password);
  Serial.print("Conectando a WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    ;
  }
  Serial.println("\nWiFi conectado.");
  Serial.print("Dirección IP: ");
  Serial.println(WiFi.localIP());

  // Configurar el servidor web
  server.on("/sensorsReadings", handleRoot);
  server.on("/data", handleData);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("Servidor HTTP iniciado.");

  // Configurar OTA
  ArduinoOTA.setHostname("ESP32-OTA");
  ArduinoOTA.setPassword("74108520");

  // ArduinoOTA.setPort(4000); 
  ArduinoOTA.onStart([]() {
  Serial.println("Iniciando OTA...");
  String tipo = (ArduinoOTA.getCommand() == U_FLASH) ? "sketch" : "filesystem";
  Serial.println("Actualizando: " + tipo);
  });

  ArduinoOTA.onEnd([]() {
    Serial.println("OTA completado.");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progreso: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error OTA [%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Error de autenticación.");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Error al iniciar.");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Error de conexión.");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Error de recepción.");
    else if (error == OTA_END_ERROR) Serial.println("Error al finalizar.");
  });
  ArduinoOTA.begin();
  Serial.println("OTA listo.");
}

// Bucle principal
void loop() {
  // Verificar el flag del timer para realizar mediciones
  if (medir) {
    medir = false; // Reseteamos el flag

    light = analogRead(photoPin);
    int estadoSensor = digitalRead(pinSensor);
    SENSOR = analogRead(2);

    // Control del buzzer para el sensor de inclinación
    if (estadoSensor == HIGH) {
      digitalWrite(pinBuzzer, HIGH);
    } else {
      digitalWrite(pinBuzzer, LOW);
    }

    // Leer datos del sensor DHT
    h = dht.readHumidity();
    t = dht.readTemperature();
    hic = dht.computeHeatIndex(t, h, false);

    // Verificar errores del sensor
    if (isnan(h) || isnan(t) || isnan(hic)) {
      Serial.println("Error al leer el sensor DHT11.");
    }

    // Control del LED RGB según el nivel de agua
    if (SENSOR < 10) {
      setColor(0, 255, 0); // Verde
    } else if (SENSOR > 10 && SENSOR <= 370) {
      setColor(0, 0, 255); // Azul
    } else {
      setColor(255, 0, 0); // Rojo
    }

    // Imprimir lecturas en el monitor serie
    Serial.println("Lecturas de sensores:");
    Serial.printf("Humedad: %.2f%%\n", h);
    Serial.printf("Temperatura: %.2f °C\n", t);
    Serial.printf("Índice de calor: %.2f °C\n", hic);
    Serial.printf("Luz: %d\n", light);
    Serial.printf("Nivel de agua: %d\n", SENSOR);
    Serial.println("-----------------------------");
  }

  // Manejar solicitudes del servidor web
  server.handleClient();

  // Manejar actualizaciones OTA
  ArduinoOTA.handle();
}


