#include "DHT.h"
#include "Servo.h"

Servo sensorServo;

#define DHTPIN 4     // Pino digital onde o sensor está conectado
#define DHTTYPE DHT11 // Mude para DHT22 se estiver usando o modelo branco

DHT dht(DHTPIN, DHTTYPE);

int pinoSensor = A0;   // Pino analógico conectado ao sensor
int valorLeitura = 0;  // Variável para armazenar a leitura

void setup() {
  Serial.begin(9600);   // Inicia comunicação serial
  sensorServo.attach(6); // configura o pino D6 - controle do Servo Motor
  sensorServo.write(0);  // Inicia o servo na posição 0°
}

void loop() {
  valorLeitura = analogRead(pinoSensor);  // Lê o valor do sensor de humidade/nível de água (0-1023)
  
  float umidade = dht.readHumidity();
  float temperatura = dht.readTemperature();

  // Verifica se a leitura falhou
  if (isnan(umidade) || isnan(temperatura)) {
    Serial.println("Falha ao ler o sensor DHT!");
    delay(1000);
    return;

  }

  Serial.println("Sensor DHT11:");
  Serial.print("Umidade: ");
  Serial.print(umidade);
  Serial.print("%  |  Temperatura: ");
  Serial.print(temperatura);
  Serial.println("°C");

  Serial.print("Nivel de agua: ");
  Serial.println(valorLeitura);

  // Interpretação simples
  if (valorLeitura < 100) {
    Serial.println("Status: SECO");
  } else if (valorLeitura < 300) {
    Serial.println("Status: NIVEL BAIXO");
  } else if (valorLeitura < 500) {
    Serial.println("Status: NIVEL MEDIO");
  } else {
    Serial.println("Status: NIVEL ALTO");
  }

  if (valorLeitura < 100 || umidade < 30) {
    Serial.println("Terra seca! Acionando válvula de água...");
    sensorServo.write(90);   // Move o servo para 90°
    delay(3000);             // Aguarda 3 segundos
    sensorServo.write(0);    // Volta para 0° (retorna os 90°)
    Serial.println("Servo retornado para 0°");
  }

  delay(2000);  // Aguarda dois segundo
}