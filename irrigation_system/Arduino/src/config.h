/**
 * @file config.h
 * @brief Todas as "regulagens" do projeto num lugar só: pinos, tempos e limites.
 *
 * Analogia: é o painel de botões de um forno. A receita (o código) não muda;
 * você só gira os botões aqui.
 */
#ifndef CONFIG_H
#define CONFIG_H

// ===================== PINOS =====================
// D0/D1 ficam LIVRES: são a Serial (USB) usada para falar com o ESP32.
#define PINO_DHT        4     // DHT11: umidade e temperatura do ar
#define TIPO_DHT        DHT11 // troque para DHT22 se usar esse sensor
#define PINO_SOLO       A0    // sensor de umidade do solo / nível de água
#define PINO_SERVO      9     // servo que faz o papel da válvula
#define PINO_BTN_REGAR  2     // botão REGAR (entre o pino e o GND)
#define PINO_BTN_MODO   3     // botão MODO  (entre o pino e o GND)
#define PINO_LED_OK     5     // LED verde (com resistor de 220R a 1k)
#define PINO_LED_ERRO   6     // LED vermelho (com resistor de 220R a 1k)

// ============ COMUNICAÇÃO COM O ESP32 ============
// O simulador de ESP32 do professor abre a porta USB do Arduino
// (/dev/ttyACM0), então o canal é a própria Serial.
// Se um dia usar um ESP32 de verdade em outros pinos, troque por um
// SoftwareSerial e mude só esta linha (o resto do código usa um ponteiro Stream*).
#define LINK_SERIAL     Serial
#define LINK_BAUD       9600

// 1 = manda linhas de log "# ..." pela serial (o ESP32 mostra no terminal dele)
// 0 = silêncio total (só quadros do protocolo)
#define DEBUG_ATIVO     1

// ===================== TEMPOS (ms) =====================
#define INTERVALO_LEITURA_MS     2000UL   // DHT11 aceita no máximo 1 leitura/s
#define INTERVALO_ENVIO_MS       2000UL   // de quanto em quanto tempo manda $D
#define TIMEOUT_ESP_MS           6000UL   // sem quadro do ESP32 por 6 s = offline
#define DEBOUNCE_MS                50UL   // tempo para o botão "parar de tremer"
#define INTERVALO_MIN_REGAS_MS  30000UL   // no AUTO, espera 30 s entre regas

// ===================== REGA =====================
#define REGA_PADRAO_S    3    // botão REGAR / rega automática: 3 segundos
#define REGA_MAX_S      30    // limite de segurança para comandos da nuvem
#define SERVO_FECHADO    0    // ângulo da válvula fechada
#define SERVO_ABERTO    90    // ângulo da válvula aberta

// ============ LIMIARES DO SOLO (valor bruto 0..1023) ============
// Neste sensor, quanto MENOR o valor, MAIS SECO.
#define SOLO_SECO       100   // abaixo disso: SECO -> rega
#define SOLO_MEDIO      300   // 100..299 BAIXO | 300..499 MÉDIO | >=500 ALTO
#define SOLO_ALTO       500
#define SOLO_AMOSTRAS     8   // média de 8 leituras para tirar ruído
#define SOLO_DESCONECTADO 1015 // com pull-up ligado, >= isso = fio solto

// ============ LIMITES DO AR ============
#define UMIDADE_AR_BAIXA  30.0f  // % : ar muito seco
// Faixa aceitável de leitura (fora disso consideramos falha do sensor)
#define UMIDADE_MIN        0.0f
#define UMIDADE_MAX      100.0f
#define TEMPERATURA_MIN  -10.0f
#define TEMPERATURA_MAX   60.0f

#endif
