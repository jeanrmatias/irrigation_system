#include "DHT.h"
#include "Servo.h"

Servo sensorServo;

#define DHTPIN 4     // Pino digital onde o sensor está conectado
#define DHTTYPE DHT11 // Mude para DHT22 se estiver usando o modelo branco

DHT dht(DHTPIN, DHTTYPE);

int pinoSensor = A0;   // Pino analógico conectado ao sensor
int valorLeitura = 0;  // Variável para armazenar a leitura

unsigned long tempoAnteriorLeitura = 0;
const unsigned long intervaloLeitura = 2000;

unsigned long tempoAnteriorServo = 0;
const unsigned long intervaloServo = 3000;
bool valvulaAberta = false;

void setup() {
  Serial.begin(9600);   // Inicia comunicação serial
  sensorServo.attach(9); // configura o pino D9 - controle do Servo Motor
  sensorServo.write(0);  // Inicia o servo na posição 0°
}

void loop() {
  unsigned long tempoAtual = millis(); //captura o tempo atual do processador
  valorLeitura = analogRead(pinoSensor);  // Lê o valor do sensor de humidade/nível de água (0-1023)
  if (valvulaAberta && (tempoAtual - tempoAnteriorServo >= intervaloServo)){
    sensorServo.write(0);
    valvulaAberta = false;
    Serial.println("Servo retornado para 0°");
  };
  if (tempoAtual - tempoAnteriorLeitura >= intervaloLeitura){
    tempoAnteriorLeitura = tempoAtual;

    valorLeitura = analogRead(pinoSensor)

    float umidade = dht.readHumidity();
    float temperatura = dht.readTemperature();

    if (isnan(umidade) || isnan(temperatura)) {
    Serial.println("Falha ao ler o sensor DHT!"); // Verifica se a leitura falhou
    return;
    }
    Serial.println("\n--- Nova Leitura ---");
     Serial.print("Umidade: "); Serial.print(umidade); Serial.print("%  |  Temperatura: "); Serial.print(temperatura); Serial.println("°C");
    Serial.print("Nivel de agua: "); Serial.println(valorLeitura);
     // Interpretação do nível de água
    if (valorLeitura < 100) {
      Serial.println("Status: SECO");
    } else if (valorLeitura < 300) {
      Serial.println("Status: NIVEL BAIXO");
    } else if (valorLeitura < 500) {
      Serial.println("Status: NIVEL MEDIO");
    } else {
      Serial.println("Status: NIVEL ALTO");
    }

    // 3. VERIFICAÇÃO DA CONDIÇÃO DE REGAR
    // Só aciona se a condição for estrita E se a válvula já não estiver aberta no momento
    if ((valorLeitura < 100 || umidade < 30) && !valvulaAberta) {
      Serial.println("Terra seca! Acionando válvula de água...");
      sensorServo.write(90);             // Move o servo para 90°
      tempoAnteriorServo = tempoAtual;   // Salva o momento exato em que a válvula abriu
      valvulaAberta = true;              // Sinaliza que a válvula está aberta
    }
  }
}