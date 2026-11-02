#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Wire.h>
#include <Adafruit_BMP280.h>
#include <DHT.h>
#include <PubSubClient.h>
#include "config.h"

DHT dht(PIN_DHT, DHT22);
Adafruit_BMP280 bmp;
WiFiClient wifiCliente;
PubSubClient mqttCliente(wifiCliente);

volatile unsigned long pulsosPluviometro = 0;
unsigned long ultimoEnvioMs = 0;
bool bmpDisponivel = false;

void IRAM_ATTR contarPulsoChuva() {
  pulsosPluviometro++;
}

bool valorDentroFaixa(float valor, float minimo, float maximo) {
  return valor >= minimo && valor <= maximo;
}

float mapearLuminosidade(int adc) {
  float percentual = (adc / 4095.0f) * 100.0f;
  if (percentual < 0.0f) {
    percentual = 0.0f;
  }
  if (percentual > 100.0f) {
    percentual = 100.0f;
  }
  return percentual;
}

float estimarPpmMq135(int adc) {
  float tensao = (adc / 4095.0f) * 3.3f;
  float ppm = tensao * 220.0f;
  if (ppm < 0.0f) {
    ppm = 0.0f;
  }
  return ppm;
}

void conectarWifi() {
  Serial.print("Conectando ao Wi-Fi: ");
  Serial.println(WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_SENHA);
  unsigned long inicio = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - inicio < 20000UL) {
    digitalWrite(PIN_LED_STATUS, !digitalRead(PIN_LED_STATUS));
    delay(400);
    Serial.print(".");
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Wi-Fi Conectado com sucesso! IP: ");
    Serial.println(WiFi.localIP());
    digitalWrite(PIN_LED_STATUS, HIGH);
  } else {
    Serial.println("Falha ao conectar no Wi-Fi.");
    digitalWrite(PIN_LED_STATUS, LOW);
  }
}

void reconectarMqtt() {
  if (!MQTT_ATIVO) {
    return;
  }
  if (mqttCliente.connected()) {
    return;
  }
  mqttCliente.setServer(MQTT_BROKER, MQTT_PORTA);
  if (strlen(MQTT_USUARIO) > 0) {
    mqttCliente.connect("estacao-meteorologica-esp32", MQTT_USUARIO, MQTT_SENHA);
  } else {
    mqttCliente.connect("estacao-meteorologica-esp32");
  }
}

String montarJson(float temperatura, float umidade, float pressao, float qualidadeAr, float luminosidade, float pluviometria, bool valida) {
  String json = "{";
  json += "\"estacao_id\":" + String(ESTACAO_ID) + ",";
  json += "\"temperatura\":" + String(temperatura, 2) + ",";
  json += "\"umidade\":" + String(umidade, 2) + ",";
  json += "\"pressao\":" + String(pressao, 2) + ",";
  json += "\"qualidade_ar\":" + String(qualidadeAr, 2) + ",";
  json += "\"luminosidade\":" + String(luminosidade, 2) + ",";
  json += "\"pluviometria\":" + String(pluviometria, 2) + ",";
  json += "\"origem\":\"esp32\",";
  json += "\"valida_local\":" + String(valida ? "true" : "false");
  json += "}";
  return json;
}

bool enviarHttp(const String& json) {
  if (WiFi.status() != WL_CONNECTED) {
    conectarWifi();
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("[HTTP] Sem conexao Wi-Fi para envio.");
      return false;
    }
  }

  HTTPClient http;
  String url;
  if (USE_HTTPS) {
    if (API_PORTA == 443) {
      url = "https://" + String(API_HOST) + String(API_CAMINHO);
    } else {
      url = "https://" + String(API_HOST) + ":" + String(API_PORTA) + String(API_CAMINHO);
    }
    WiFiClientSecure secureClient;
    secureClient.setInsecure(); // Permite conexões HTTPS sem necessidade de gerenciar certificados SSL locais
    http.begin(secureClient, url);
  } else {
    if (API_PORTA == 80) {
      url = "http://" + String(API_HOST) + String(API_CAMINHO);
    } else {
      url = "http://" + String(API_HOST) + ":" + String(API_PORTA) + String(API_CAMINHO);
    }
    http.begin(wifiCliente, url);
  }

  Serial.print("[HTTP] Enviando para: ");
  Serial.println(url);

  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-API-Key", API_CHAVE);
  int codigo = http.POST(json);
  
  Serial.print("[HTTP] Codigo de resposta: ");
  Serial.println(codigo);
  if (codigo > 0) {
    String resposta = http.getString();
    Serial.print("[HTTP] Resposta da API: ");
    Serial.println(resposta);
  } else {
    Serial.print("[HTTP] Erro na conexao: ");
    Serial.println(http.errorToString(codigo).c_str());
  }

  http.end();
  return codigo >= 200 && codigo < 300;
}

bool enviarMqtt(const String& json) {
  if (!MQTT_ATIVO) {
    return false;
  }
  reconectarMqtt();
  if (!mqttCliente.connected()) {
    return false;
  }
  return mqttCliente.publish(MQTT_TOPICO, json.c_str());
}

void setup() {
  pinMode(PIN_LED_STATUS, OUTPUT);
  pinMode(PIN_PLUVIOMETRO, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_PLUVIOMETRO), contarPulsoChuva, FALLING);

  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== Estacao Meteorologica ESP32 Iniciando ===");

  dht.begin();
  Wire.begin();
  bmpDisponivel = bmp.begin(0x76);
  if (!bmpDisponivel) {
    bmpDisponivel = bmp.begin(0x77);
  }
  if (bmpDisponivel) {
    Serial.println("Sensor BMP280 detectado com sucesso!");
    bmp.setSampling(
      Adafruit_BMP280::MODE_NORMAL,
      Adafruit_BMP280::SAMPLING_X2,
      Adafruit_BMP280::SAMPLING_X16,
      Adafruit_BMP280::FILTER_X16,
      Adafruit_BMP280::STANDBY_MS_500
    );
  } else {
    Serial.println("Aviso: BMP280 nao encontrado. Usando pressao padrao 1013.25 hPa.");
  }

  conectarWifi();
  if (MQTT_ATIVO) {
    reconectarMqtt();
  }
}

void loop() {
  if (MQTT_ATIVO) {
    mqttCliente.loop();
  }

  unsigned long agora = millis();
  if (agora - ultimoEnvioMs < INTERVALO_COLETA_MS) {
    delay(50);
    return;
  }
  ultimoEnvioMs = agora;

  float temperatura = dht.readTemperature();
  float umidade = dht.readHumidity();
  float pressao = bmpDisponivel ? bmp.readPressure() / 100.0f : 1013.25f;
  int adcAr = analogRead(PIN_MQ135);
  int adcLuz = analogRead(PIN_LDR);
  float qualidadeAr = estimarPpmMq135(adcAr);
  float luminosidade = mapearLuminosidade(adcLuz);

  noInterrupts();
  unsigned long pulsos = pulsosPluviometro;
  pulsosPluviometro = 0;
  interrupts();
  float pluviometria = pulsos * FATOR_MM_POR_PULSO;

  if (isnan(temperatura) || isnan(umidade)) {
    Serial.println("Falha na leitura do DHT22. Verifique as conexoes.");
    digitalWrite(PIN_LED_STATUS, LOW);
    return;
  }
  if (isnan(pressao)) {
    pressao = 1013.25f;
  }

  bool valida = valorDentroFaixa(temperatura, TEMP_MIN, TEMP_MAX) &&
                valorDentroFaixa(umidade, UMID_MIN, UMID_MAX) &&
                valorDentroFaixa(pressao, PRESS_MIN, PRESS_MAX) &&
                valorDentroFaixa(qualidadeAr, AR_MIN, AR_MAX) &&
                valorDentroFaixa(luminosidade, LUM_MIN, LUM_MAX) &&
                valorDentroFaixa(pluviometria, CHUVA_MIN, CHUVA_MAX);

  String json = montarJson(temperatura, umidade, pressao, qualidadeAr, luminosidade, pluviometria, valida);
  Serial.print("Payload preparado: ");
  Serial.println(json);

  bool httpOk = enviarHttp(json);
  bool mqttOk = enviarMqtt(json);
  digitalWrite(PIN_LED_STATUS, (httpOk || mqttOk) ? HIGH : LOW);
}
