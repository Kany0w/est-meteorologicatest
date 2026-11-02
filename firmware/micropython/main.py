import network
import time
import ujson
import urequests
from machine import Pin, ADC

# ========================================================
# CONFIGURAÇÕES DA ESTAÇÃO
# ========================================================
# No Wokwi use "Wokwi-GUEST" e "". No celular use seu Wi-Fi.
WIFI_SSID = "Wokwi-GUEST"
WIFI_SENHA = ""

# URL da sua API no Render (sem barra no final)
API_URL = "https://est-meteorologica.onrender.com/api/v1/leituras"
API_CHAVE = "estacao-esp32-demo-key-001"
ESTACAO_ID = 1

INTERVALO_ENVIO_SEGUNDOS = 15

# ========================================================
# MAPEAMENTO DE PINOS (GPIO)
# ========================================================
PIN_DHT = 4
PIN_MQ135 = 34
PIN_LDR = 35
PIN_PLUVIOMETRO = 27
PIN_LED_STATUS = 2

FATOR_MM_POR_PULSO = 0.25

# Limiares de validação física
TEMP_MIN, TEMP_MAX = -20.0, 60.0
UMID_MIN, UMID_MAX = 0.0, 100.0
PRESS_MIN, PRESS_MAX = 870.0, 1080.0
AR_MIN, AR_MAX = 0.0, 1500.0
LUM_MIN, LUM_MAX = 0.0, 100.0
CHUVA_MIN, CHUVA_MAX = 0.0, 250.0

# ========================================================
# INICIALIZAÇÃO DE HARDWARE
# ========================================================
led = Pin(PIN_LED_STATUS, Pin.OUT)

# Sensor DHT22
try:
    import dht
    sensor_dht = dht.DHT22(Pin(PIN_DHT))
except Exception as e:
    print("Aviso no DHT22:", e)
    sensor_dht = None

# Sensores Analógicos (ADC)
adc_ar = ADC(Pin(PIN_MQ135))
adc_ar.atten(ADC.ATTN_11DB) # Faixa de 0 a 3.3V

adc_ldr = ADC(Pin(PIN_LDR))
adc_ldr.atten(ADC.ATTN_11DB)

# Pluviômetro com Interrupção
pulsos_chuva = 0

def contar_pulso(pino):
    global pulsos_chuva
    pulsos_chuva += 1

pin_chuva = Pin(PIN_PLUVIOMETRO, Pin.IN, Pin.PULL_UP)
pin_chuva.irq(trigger=Pin.IRQ_FALLING, handler=contar_pulso)

# ========================================================
# FUNÇÃO DE CONEXÃO WI-FI
# ========================================================
def conectar_wifi():
    wlan = network.WLAN(network.STA_IF)
    wlan.active(True)
    if not wlan.isconnected():
        print(f"Conectando ao Wi-Fi: {WIFI_SSID}...")
        wlan.connect(WIFI_SSID, WIFI_SENHA)
        inicio = time.time()
        while not wlan.isconnected():
            led.value(not led.value())
            time.sleep(0.3)
            if time.time() - inicio > 15:
                print("Tempo esgotado para conexão Wi-Fi.")
                break
    
    if wlan.isconnected():
        led.value(1)
        print("Wi-Fi Conectado com sucesso! IP:", wlan.ifconfig()[0])
        return True
    else:
        led.value(0)
        return False

# ========================================================
# LOOP PRINCIPAL DE TELEMETRIA
# ========================================================
print("\n=== Estação Meteorológica ESP32 (MicroPython) Iniciando ===")
conectar_wifi()

while True:
    try:
        # 1. Leitura do DHT22
        temperatura = 25.0
        umidade = 60.0
        if sensor_dht:
            try:
                sensor_dht.measure()
                temperatura = round(sensor_dht.temperature(), 2)
                umidade = round(sensor_dht.humidity(), 2)
            except Exception as erro_dht:
                print("Erro ao ler DHT22:", erro_dht)

        # 2. Leitura dos sensores analógicos
        leitura_ar = adc_ar.read()
        tensao_ar = (leitura_ar / 4095.0) * 3.3
        qualidade_ar = round(tensao_ar * 220.0, 2)

        leitura_ldr = adc_ldr.read()
        luminosidade = round((leitura_ldr / 4095.0) * 100.0, 2)

        # 3. Pressão Atmosférica
        pressao = 1013.25

        # 4. Pluviometria
        chuva = round(pulsos_chuva * FATOR_MM_POR_PULSO, 2)
        pulsos_chuva = 0

        # 5. Validação física
        valida = (
            TEMP_MIN <= temperatura <= TEMP_MAX and
            UMID_MIN <= umidade <= UMID_MAX and
            PRESS_MIN <= pressao <= PRESS_MAX and
            AR_MIN <= qualidade_ar <= AR_MAX and
            LUM_MIN <= luminosidade <= LUM_MAX and
            CHUVA_MIN <= chuva <= CHUVA_MAX
        )

        # 6. Montagem do Payload
        payload = {
            "estacao_id": ESTACAO_ID,
            "temperatura": temperatura,
            "umidade": umidade,
            "pressao": pressao,
            "qualidade_ar": qualidade_ar,
            "luminosidade": luminosidade,
            "pluviometria": chuva,
            "origem": "esp32",
            "valida_local": valida
        }

        print("\nPayload preparado:", ujson.dumps(payload))

        # 7. Envio HTTPS POST para o Render
        headers = {
            "Content-Type": "application/json",
            "X-API-Key": API_CHAVE
        }

        print(f"[HTTP] Enviando para: {API_URL}")
        resposta = urequests.post(API_URL, data=ujson.dumps(payload), headers=headers)
        
        print(f"[HTTP] Código de resposta: {resposta.status_code}")
        print(f"[HTTP] Resposta da API: {resposta.text}")
        
        if 200 <= resposta.status_code < 300:
            led.value(1)
        else:
            led.value(0)
            
        resposta.close()

    except Exception as erro:
        print("Erro durante o ciclo de envio:", erro)

    time.sleep(INTERVALO_ENVIO_SEGUNDOS)

