#ifndef CONFIG_ESTACAO_H
#define CONFIG_ESTACAO_H

// --- Configurações Wi-Fi ---
#define WIFI_SSID "Luan"
#define WIFI_SENHA "luan1234"

// --- Configurações da API para Nuvem (Render / HTTPS) ---
// Configurado para nuvem com HTTPS na porta 443
#define USE_HTTPS true
#define API_HOST "est-meteorologica.onrender.com"
#define API_PORTA 443
#define API_CAMINHO "/api/v1/leituras"
#define API_CHAVE "estacao-esp32-demo-key-001"
#define ESTACAO_ID 1

// --- Configurações MQTT (Opcional) ---
#define MQTT_ATIVO false
#define MQTT_BROKER "192.168.0.10"
#define MQTT_PORTA 1883
#define MQTT_TOPICO "cidades-inteligentes/estacoes/1/leituras"
#define MQTT_USUARIO ""
#define MQTT_SENHA ""

// --- Hardware e Sensores ---
#define INTERVALO_COLETA_MS 5000UL
#define PIN_DHT 4
#define PIN_MQ135 34
#define PIN_LDR 35
#define PIN_PLUVIOMETRO 27
#define PIN_LED_STATUS 2
#define FATOR_MM_POR_PULSO 0.25f

// --- Limiares de Validação Física ---
#define TEMP_MIN -20.0f
#define TEMP_MAX 60.0f
#define UMID_MIN 0.0f
#define UMID_MAX 100.0f
#define PRESS_MIN 870.0f
#define PRESS_MAX 1080.0f
#define AR_MIN 0.0f
#define AR_MAX 1500.0f
#define LUM_MIN 0.0f
#define LUM_MAX 100.0f
#define CHUVA_MIN 0.0f
#define CHUVA_MAX 250.0f

#endif
