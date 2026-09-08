# Sistema de Irrigação Automática com Sensores

Projeto Arduino para monitoramento de umidade do solo/nível de água e condições ambientais (umidade e temperatura do ar), com acionamento automático de uma válvula (servo motor) quando o solo está seco.

## 📋 Descrição

O sistema lê periodicamente:
- **Sensor de umidade do solo / nível de água** (analógico no pino A0)
- **Sensor DHT11** (umidade e temperatura do ar no pino digital 4)

Quando detecta solo seco (`nível de água < 100`) **ou** umidade do ar muito baixa (`< 30%`), abre a válvula por 3 segundos via servo motor (pino 9) e depois a fecha automaticamente.

## 📁 Arquivos do Projeto

| Arquivo | Descrição |
|---------|-----------|
| `sensorHumidadeSolo_SensorNiveldagua.ino` | Versão original / monolítica do código (tudo dentro do `loop`) |
| `relatórioSobreCadaFuncao.ino` | Versão refatorada com funções isoladas e bem documentadas |

> A versão refatorada (`relatórioSobreCadaFuncao.ino`) é a recomendada para estudo e manutenção.

## 🛠️ Hardware Necessário

- Placa Arduino (Uno, Nano, etc.)
- Sensor DHT11 (umidade + temperatura)
- Sensor de umidade do solo / nível de água (analógico)
- Servo motor (para simular/abrir a válvula de água)
- Jumpers e breadboard (recomendado)
- Fonte de alimentação adequada para o servo

### Conexões

| Componente              | Pino Arduino |
|-------------------------|--------------|
| DHT11 (DATA)            | D4           |
| Sensor de solo / água   | A0           |
| Servo motor (sinal)     | D9           |
| DHT11 VCC / GND         | 5V / GND     |
| Sensor solo VCC / GND   | 5V / GND     |
| Servo VCC / GND         | 5V / GND*    |

\* Para servos de maior consumo, use fonte externa e conecte apenas o sinal ao Arduino (GND comum).

## 📚 Bibliotecas Necessárias

Instale via **Gerenciador de Bibliotecas** do Arduino IDE:

- **DHT sensor library** (por Adafruit) — depende também de **Adafruit Unified Sensor**
- **Servo** (já inclusa no Arduino IDE)

## ⚙️ Configurações Principais

```cpp
#define DHTPIN 4              // Pino do DHT11
#define DHTTYPE DHT11         // Tipo do sensor
const int pinoSensorAgua = A0; // Sensor de solo/água

const unsigned long intervaloLeitura = 2000; // Lê sensores a cada 2s
const unsigned long intervaloServo   = 3000; // Válvula aberta por 3s
```

### Limiares de decisão

| Condição                  | Valor          | Ação                  |
|---------------------------|----------------|-----------------------|
| Solo seco                 | `< 100`        | Abre válvula          |
| Umidade do ar baixa       | `< 30%`        | Abre válvula          |
| Nível baixo               | `100 – 299`    | Apenas monitoramento  |
| Nível médio               | `300 – 499`    | Apenas monitoramento  |
| Nível alto                | `≥ 500`        | Apenas monitoramento  |

## 🔄 Funcionamento (versão refatorada)

### Funções

| Função                        | Responsabilidade |
|-------------------------------|------------------|
| `inicializarServo()`          | Anexa o servo no pino 9 e posiciona em 0° |
| `lerSensores()`               | Lê nível de água + umidade/temperatura do DHT11 |
| `exibirDadosSerial()`         | Mostra leituras e status no Monitor Serial |
| `verificarCondicaoRegar()`    | Decide se deve abrir a válvula |
| `gerenciarFechamentoValvula()`| Fecha a válvula após o tempo programado |

### Fluxo no `loop()`

1. Verifica continuamente se é hora de fechar a válvula.
2. A cada 2 segundos:
   - Lê os sensores
   - Exibe os dados no Serial
   - Avalia se precisa regar

## 📟 Saída no Monitor Serial (exemplo)

```
=== RELATÓRIO DE LEITURA ===
Umidade do Ar: 45.00%
Temperatura  : 28.50°C
Nível de Água: 85 -> Status: SECO
Alerta: Solo Necessitado! Abrindo válvula de água...
Status: Irrigação Concluída. Válvula fechada.
```

## 🚀 Como Usar

1. Abra o arquivo desejado no Arduino IDE.
2. Instale as bibliotecas necessárias.
3. Selecione a placa e a porta correta.
4. Faça o upload.
5. Abra o **Monitor Serial** em `9600 baud`.
6. Observe as leituras e o acionamento automático da válvula.

## 📝 Observações

- O sensor de solo analógico retorna valores de **0 a 1023** (quanto menor, mais seco).
- O tempo de abertura da válvula e os limiares podem ser ajustados conforme a necessidade da planta/solo.
- A versão refatorada evita leitura bloqueante e organiza melhor o código para futuras expansões (mais sensores, display, Wi-Fi, etc.).

## 📄 Licença

Projeto educacional — livre para uso e modificação.
