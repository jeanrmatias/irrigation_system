#include "DHT.h"
#include "Servo.h"

// ==========================================
// 1. CONFIGURAÇÕES E PINOS
// ==========================================
#define DHTPIN 4     
#define DHTTYPE DHT11 
const int pinoSensorAgua = A0;

DHT dht(DHTPIN, DHTTYPE);
Servo sensorServo;

// ==========================================
// 2. VARIÁVEIS GLOBAIS
// ==========================================
int valorNivelAgua = 0;
float umidadeAr = 0.0;
float temperaturaAr = 0.0;

// Variáveis de tempo (millis)
unsigned long tempoAnteriorLeitura = 0;
const unsigned long intervaloLeitura = 2000; 

unsigned long tempoAnteriorServo = 0;
const unsigned long intervaloServo = 3000;   
bool valvulaAberta = false;                  

// ==========================================
// 3. DECLARAÇÃO DAS FUNÇÕES
// ==========================================
void inicializarServo();
void lerSensores();
void exibirDadosSerial();
void verificarCondicaoRegar(unsigned long tempoAtual);
void gerenciarFechamentoValvula(unsigned long tempoAtual);

// ==========================================
// 4. FLUXO PRINCIPAL
// ==========================================
void setup() {
  Serial.begin(9600);
  inicializarServo();
  dht.begin();
}

void loop() {
  unsigned long tempoAtual = millis();

  // Executa monitoramento da válvula constantemente a cada ciclo de clock
  gerenciarFechamentoValvula(tempoAtual);

  // Executa leituras e tomadas de decisão de forma cadenciada a cada 2s
  if (tempoAtual - tempoAnteriorLeitura >= intervaloLeitura) {
    tempoAnteriorLeitura = tempoAtual;

    lerSensores();
    exibirDadosSerial();
    verificarCondicaoRegar(tempoAtual);
  }
}

// ==========================================
// 5. IMPLEMENTAÇÃO DAS FUNÇÕES ISOLADAS
// ==========================================

void inicializarServo() {
  sensorServo.attach(9); 
  sensorServo.write(0);  
}

void lerSensores() {
  valorNivelAgua = analogRead(pinoSensorAgua);
  umidadeAr = dht.readHumidity();
  temperaturaAr = dht.readTemperature();
}

void exibirDadosSerial() {
  if (isnan(umidadeAr) || isnan(temperaturaAr)) {
    Serial.println(F("Status: ERRO [Falha ao ler o sensor DHT11]"));
    return;
  }

  Serial.println(F("\n=== RELATÓRIO DE LEITURA ==="));
  Serial.print(F("Umidade do Ar: ")); Serial.print(umidadeAr); Serial.println(F("%"));
  Serial.print(F("Temperatura  : ")); Serial.print(temperaturaAr); Serial.println(F("°C"));
  Serial.print(F("Nível de Água: ")); Serial.print(valorNivelAgua);

  if (valorNivelAgua < 100) {
    Serial.println(F(" -> Status: SECO"));
  } else if (valorNivelAgua < 300) {
    Serial.println(F(" -> Status: NÍVEL BAIXO"));
  } else if (valorNivelAgua < 500) {
    Serial.println(F(" -> Status: NÍVEL MÉDIO"));
  } else {
    Serial.println(F(" -> Status: NÍVEL ALTO"));
  }
}

void verificarCondicaoRegar(unsigned long tempoAtual) {
  if (valvulaAberta) return; 

  if (valorNivelAgua < 100 || umidadeAr < 30) {
    Serial.println(F("Alerta: Solo Necessitado! Abrindo válvula de água..."));
    sensorServo.write(90);           
    tempoAnteriorServo = tempoAtual; 
    valvulaAberta = true;            
  }
}

void gerenciarFechamentoValvula(unsigned long tempoAtual) {
  if (valvulaAberta && (tempoAtual - tempoAnteriorServo >= intervaloServo)) {
    sensorServo.write(0);    
    valvulaAberta = false;   
    Serial.println(F("Status: Irrigação Concluída. Válvula fechada."));
  }
}
