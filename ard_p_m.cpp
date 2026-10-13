#include <avr/wdt.h> 
#include "sensor.h"
#include "atuadores.h"

unsigned long tempoAnteriorLeitura = 0;
// Intervalo aumentado para 10 segundos (ideal para o DHT11 e variações lentas)
const unsigned long intervaloLeitura = 10000;

// >>> ENVIO PARA O ESP32 (MQTT) — INÍCIO DA ADIÇÃO
// Envia pela Serial uma linha no formato: "valorLeitura;umidade;temperatura\n"
// O ESP32 (simulador MQTT) lê essa linha e publica cada valor no seu tópico.
void enviarDadosESP32() {
  // Só envia se a leitura do DHT foi válida (evita publicar "nan" no broker)
  if (isnan(umidade) || isnan(temperatura)) {
    return;
  }
  Serial.print(valorLeitura);      // umidade do solo (0-1023, menor = mais seco)
  Serial.print(";");
  Serial.print(umidade);           // umidade do ar (%)
  Serial.print(";");
  Serial.println(temperatura);     // temperatura (°C) — o println envia o '\n' que fecha a mensagem
}
// >>> ENVIO PARA O ESP32 (MQTT) — FIM DA ADIÇÃO

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
    enviarDadosESP32();  // >>> NOVO: envia a leitura para o ESP32 publicar via MQTT
  }
}