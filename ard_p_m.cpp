#include <avr/wdt.h> 
#include "sensor.h"
#include "atuadores.h"

unsigned long tempoAnteriorLeitura = 0;
// Intervalo aumentado para 10 segundos (ideal para o DHT11 e variações lentas)
const unsigned long intervaloLeitura = 10000; 

void setup() {
  Serial.begin(9600);   
  
  // Habilita o Watchdog ANTES para proteger o sistema, mas note que
  // a inicialização dos sensores limpa o WDT internamente para não estourar os 8s.
  wdt_enable(WDTO_8S); 
  
  inicializarSensores(); // Agora inclui o tempo de aquecimento de 2s com segurança
  inicializarServo();    

  Serial.println("Sistema Pronto e Watchdog ativo!");
}

void loop() {
  wdt_reset(); // Alimenta o Watchdog constantemente a cada ciclo do loop (executa em microssegundos)
  
  unsigned long tempoAtual = millis(); 

  // O servo fecha após 3 segundos independentemente dos sensores
  gerenciarFechamentoValvula(tempoAtual);

  // Executa a leitura física e a tomada de decisão a cada 10 segundos
  if (tempoAtual - tempoAnteriorLeitura >= intervaloLeitura) {
    tempoAnteriorLeitura = tempoAtual;

    lerSensores();                       
    verificarCondicaoRegar(tempoAtual);   
  }
}
