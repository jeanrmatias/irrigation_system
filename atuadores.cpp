#include "atuadores.h"
#include "sensor.h" 

Servo sensorServo;

unsigned long tempoAnteriorServo = 0;
const unsigned long intervaloServo = 3000;
bool valvulaAberta = false;

void inicializarServo() {
  sensorServo.attach(9); 
  sensorServo.write(0);  
}

void gerenciarFechamentoValvula(unsigned long tempoAtual) {
  if (valvulaAberta && (tempoAtual - tempoAnteriorServo >= intervaloServo)) {
    sensorServo.write(0);
    valvulaAberta = false;
    Serial.println("Servo retornado para 0°");
  }
}

void verificarCondicaoRegar(unsigned long tempoAtual) {
  if ((valorLeitura < 100 || umidade < 30) && !valvulaAberta) {
    Serial.println("Terra seca! Acionando válvula de água...");
    sensorServo.write(90);             
    tempoAnteriorServo = tempoAtual;   
    valvulaAberta = true;              
  }
}
