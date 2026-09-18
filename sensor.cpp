#include "sensor.h"
#include <avr/wdt.h> // Incluído para resetar o WDT durante o warm-up

const int pinoSensor = A0;   
float umidade = 0.0;
float temperatura = 0.0;
int valorLeitura = 0;

DHT dht(DHTPIN, DHTTYPE);

void inicializarSensores() {
  dht.begin();
  
  // Dá um tempo de 2 segundos para o DHT11 estabilizar antes da primeira leitura
  Serial.println("Aguardando estabilização do DHT11...");
  delay(2000); 
  wdt_reset(); // Alimenta o watchdog após o delay para não resetar o Arduino
}

void lerSensores() {
  valorLeitura = analogRead(pinoSensor); 
  
  umidade = dht.readHumidity();
  temperatura = dht.readTemperature();

  if (isnan(umidade) || isnan(temperatura)) {
    Serial.println("Falha ao ler o sensor DHT! (Tentará novamente no próximo ciclo)");
    return;
  }

  Serial.println("\n--- Nova Leitura (Ambiente Estabilizado) ---");
  Serial.print("Umidade: "); Serial.print(umidade); Serial.print("%  |  Temperatura: "); Serial.print(temperatura); Serial.println("°C");
  Serial.print("Nivel de agua: "); Serial.println(valorLeitura);
  
  if (valorLeitura < 100) {
    Serial.println("Status: SECO");
  } else if (valorLeitura < 300) {
    Serial.println("Status: NIVEL BAIXO");
  } else if (valorLeitura < 500) {
    Serial.println("Status: NIVEL MEDIO");
  } else {
    Serial.println("Status: NIVEL ALTO");
  }
}
