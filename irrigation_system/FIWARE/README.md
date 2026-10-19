# FIWARE

Usamos o mesmo ambiente das aulas: [fabiocabrini/fiware](https://github.com/fabiocabrini/fiware), com Orion, IoT Agent UltraLight, Mosquitto, MongoDB e STH-Comet em Docker.

## 1. Subir o FIWARE

Pode ser na sua máquina (Docker Desktop + WSL2) ou numa VM na nuvem. Na VM, libere as portas 1026, 1883, 4041 e 8666.

```bash
git clone https://github.com/fabiocabrini/fiware
cd fiware
docker compose up -d
```

## 2. Cadastrar o dispositivo

```bash
python3 FIWARE/provisionar.py                  # FIWARE em localhost
python3 FIWARE/provisionar.py --host <IP>      # FIWARE em outra máquina
```

O script faz o mesmo que a coleção do Postman da aula, mas para o nosso dispositivo:

| Passo | Chamada |
|---|---|
| Health check | `GET :4041/iot/about`, `:1026/version`, `:8666/version` |
| Service group | `POST :4041/iot/services` (apikey `TEF`) |
| Dispositivo | `POST :4041/iot/devices` (`irrig001` → `urn:ngsi-ld:Irrigacao:001`, 7 atributos, 2 comandos) |
| Comandos | `POST :1026/v2/registrations` (só se ainda não existir) |
| Histórico | `POST :1026/v2/subscriptions` → STH-Comet |

Pode rodar de novo sem problema: o que já existe ele só confirma.
Para remover: `python3 FIWARE/provisionar.py --apagar`.

> Se o seu docker-compose usa outros nomes de serviço, ajuste `--orion-interno`, `--iota-interno` e `--sth-interno`. Esses são os endereços que os containers usam para falar entre si. O padrão segue o docker-compose do fabiocabrini.

## 3. Testar sem o hardware

```bash
# publica um dado como se fosse o ESP32
mosquitto_pub -h <IP> -t /TEF/irrig001/attrs -m "s|250|u|55.0|t|24.0|v|0|m|A|e|0|l|1"

# lê a entidade
curl -H 'fiware-service: smart' -H 'fiware-servicepath: /' \
  "http://<IP>:1026/v2/entities/urn:ngsi-ld:Irrigacao:001?options=keyValues"

# manda um comando (o ESP32 precisa estar rodando para responder)
curl -X PATCH -H 'Content-Type: application/json' -H 'fiware-service: smart' -H 'fiware-servicepath: /' \
  "http://<IP>:1026/v2/entities/urn:ngsi-ld:Irrigacao:001/attrs" \
  -d '{"regar": {"type": "command", "value": "5"}}'
```

Depois do comando, a entidade mostra `regar_status` (`PENDING` → `OK`) e `regar_info` (a resposta do Arduino).
