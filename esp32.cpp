#include "esp32.h"

// ================== CONFIGURAÇÃO MQTT ==================
const char* broker_mqtt = "localhost";
const int broker_mqtt_port = 1883;

// device_id deste dispositivo de irrigação (troque "irrig001" se precisar)
const char* topic_subscribe_cmd = "/TEF/irrig001/cmd";          // comandos chegam aqui
const char* topic_publish_attrs = "/TEF/irrig001/attrs";        // tópico geral de atributos
const char* topic_publish_attr_solo = "/TEF/irrig001/attrs/s";  // umidade do solo (0-1023)
const char* topic_publish_attr_umid = "/TEF/irrig001/attrs/u";  // umidade do ar (%)
const char* topic_publish_attr_temp = "/TEF/irrig001/attrs/t";  // temperatura (°C)

const char* default_ID_MQTT = "fiware_irrig001";

const char* topicPrefix = "irrigacao001";

// Aqui o objeto para o MQTT vem da classe ArduinoMQTT
// Não é necessário inicializar com um objeto "WiFiClient" como no ESP32 real
ArduinoMQTT MQTT;

// Para comunicação com o Arduino conectado na USB:
SerialCom Serial;

// Buffer que monta a mensagem enviada pelo Arduino: "solo;umidade;temperatura\n"
char bufferSerial[64];
int posBuffer = 0;

// Toda vez que um comando é enviado, essa função será chamada
void mqtt_callback(const char* topic, uint8_t* payload, unsigned int length){
    String msg((char*)payload, length);
    std::cout << "\n\n----------Topico: " << topic << "| Payload: " << msg << std::endl;
}

// Faz o parse de "solo;umidade;temperatura" e publica cada valor no seu tópico
void processarLeitura(char* leitura) {
    char* ptrSolo = strtok(leitura, ";");
    char* ptrUmid = strtok(NULL, ";");
    char* ptrTemp = strtok(NULL, ";");

    if (ptrSolo == NULL || ptrUmid == NULL || ptrTemp == NULL) {
        std::cout << "Mensagem invalida recebida do Arduino: " << leitura << std::endl;
        return;
    }

    bool mqttResponse = true;
    mqttResponse = MQTT.publish(topic_publish_attr_solo, ptrSolo) && mqttResponse;
    mqttResponse = MQTT.publish(topic_publish_attr_umid, ptrUmid) && mqttResponse;
    mqttResponse = MQTT.publish(topic_publish_attr_temp, ptrTemp) && mqttResponse;

    if (mqttResponse) { // MQTT publicado. Responde com OK para o Arduino
        std::cout << "MQTT Publicado! solo=" << ptrSolo
                  << " umidade=" << ptrUmid
                  << " temperatura=" << ptrTemp << std::endl;
        uint8_t bufferTx = 0x01;
        Serial.writeBytes((char*)&bufferTx, sizeof(bufferTx));
    } else {
        std::cout << "Falha ao publicar MQTT" << std::endl;
        uint8_t bufferTx = 0x00;
        Serial.writeBytes((char*)&bufferTx, sizeof(bufferTx));
    }
}

void setup()
{
    Serial.begin(9600);

    // seta o servidor mqtt -> localhost e porta 1883
    MQTT.setServer(broker_mqtt, broker_mqtt_port);
    // seta a função de callback quando recebemos comandos
    MQTT.setCallback(mqtt_callback);
    // conecta no servidor mqtt. o identificador do cliente deve ser único no servidor.
    MQTT.connect("irrigacao001");

    // Assina o tópico de comandos
    MQTT.subscribe(topic_subscribe_cmd);

    // Aguarda um tempo para a serial inicializar
    delay(2000);
}

void loop(){
    while(1){

        // Checa se recebeu uma mensagem do Arduino
        int bytesToRead = Serial.available();
        if(bytesToRead > 0){
            for (int i = 0; i < bytesToRead; i++) {
                char c;
                Serial.readBytes(&c, 1);

                if (c == '\n') { // fim da mensagem do Arduino
                    bufferSerial[posBuffer] = '\0';
                    processarLeitura(bufferSerial);
                    posBuffer = 0;
                } else if (posBuffer < (int)(sizeof(bufferSerial) - 1)) {
                    bufferSerial[posBuffer++] = c;
                }
            }
        }

        // a função .loop() deve ser chamada de tempo em tempo para processar
        // as mensagens recebidas (subscribed)
        MQTT.loop();
    }
}