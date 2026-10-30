#!/usr/bin/env python3
"""
provisionar.py - cadastra o dispositivo de irrigação no FIWARE.

Faz, em ordem, o mesmo que a coleção do Postman "FIWARE Descomplicado" faz
para a Smart Lamp, só que para o nosso dispositivo:

  1. Health check do IoT Agent, Orion e STH-Comet
  2. Service group (apikey TEF)
  3. Dispositivo irrig001 -> entidade urn:ngsi-ld:Irrigacao:001
  4. Registro dos comandos no Orion (só se ainda não existir)
  5. Assinatura do STH-Comet (histórico para o dashboard)

Só usa a biblioteca padrão do Python. Uso:
    python3 FIWARE/provisionar.py                   # FIWARE na própria máquina
    python3 FIWARE/provisionar.py --host 20.30.40.50
    python3 FIWARE/provisionar.py --apagar          # remove tudo do dispositivo
"""
import argparse
import json
import sys
import urllib.error
import urllib.request

# ===== Tem que bater com ESP32/src/esp32.cpp =====
APIKEY = "TEF"
DEVICE_ID = "irrig001"
ENTIDADE_ID = "urn:ngsi-ld:Irrigacao:001"
ENTIDADE_TIPO = "Irrigacao"
SERVICO = "smart"
SERVICE_PATH = "/"

# object_id (o que o ESP32 manda no UltraLight) -> nome no Orion
ATRIBUTOS = [
    {"object_id": "s", "name": "umidadeSolo", "type": "Integer"},
    {"object_id": "u", "name": "umidadeAr", "type": "Number"},
    {"object_id": "t", "name": "temperatura", "type": "Number"},
    {"object_id": "v", "name": "valvula", "type": "Integer"},
    {"object_id": "m", "name": "modo", "type": "Text"},
    {"object_id": "e", "name": "erros", "type": "Integer"},
    {"object_id": "l", "name": "arduinoOnline", "type": "Integer"},
]
COMANDOS = ["regar", "definirModo"]
ATRIBUTOS_HISTORICO = ["umidadeSolo", "umidadeAr", "temperatura", "valvula"]


def chamar(metodo, url, corpo=None, com_servico=True):
    """Faz uma requisição HTTP e devolve (status, resposta_em_texto)."""
    cabecalhos = {}
    if com_servico:
        cabecalhos["fiware-service"] = SERVICO
        cabecalhos["fiware-servicepath"] = SERVICE_PATH
    dados = None
    if corpo is not None:
        dados = json.dumps(corpo).encode()
        cabecalhos["Content-Type"] = "application/json"
    req = urllib.request.Request(url, data=dados, headers=cabecalhos, method=metodo)
    try:
        with urllib.request.urlopen(req, timeout=10) as resp:
            return resp.status, resp.read().decode()
    except urllib.error.HTTPError as erro:
        return erro.code, erro.read().decode()
    except (urllib.error.URLError, OSError) as erro:
        return 0, str(erro)


def passo(titulo, status, texto, ok=(200, 201, 204)):
    if status in ok:
        print(f"  [OK]   {titulo}")
        return True
    if status == 409:
        print(f"  [OK]   {titulo} (já existia)")
        return True
    print(f"  [ERRO] {titulo}: HTTP {status} {texto[:200]}")
    return False


def provisionar(a):
    iota = f"http://{a.host}:4041"
    orion = f"http://{a.host}:1026"
    sth = f"http://{a.host}:8666"

    print("1) Health check")
    for nome, url in [("IoT Agent", f"{iota}/iot/about"),
                      ("Orion", f"{orion}/version"),
                      ("STH-Comet", f"{sth}/version")]:
        st, tx = chamar("GET", url, com_servico=False)
        if not passo(nome, st, tx):
            print("\nO FIWARE está rodando? (docker compose up -d) O host está certo?")
            sys.exit(1)

    print("2) Service group")
    st, tx = chamar("POST", f"{iota}/iot/services", {"services": [{
        "apikey": APIKEY,
        "cbroker": a.orion_interno,
        "entity_type": "Thing",
        "resource": "",
    }]})
    passo(f"apikey {APIKEY}", st, tx)

    print("3) Dispositivo")
    st, tx = chamar("POST", f"{iota}/iot/devices", {"devices": [{
        "device_id": DEVICE_ID,
        "entity_name": ENTIDADE_ID,
        "entity_type": ENTIDADE_TIPO,
        "protocol": "PDI-IoTA-UltraLight",
        "transport": "MQTT",
        "commands": [{"name": c, "type": "command"} for c in COMANDOS],
        "attributes": ATRIBUTOS,
    }]})
    passo(f"{DEVICE_ID} -> {ENTIDADE_ID}", st, tx)

    print("4) Registro dos comandos no Orion")
    st, tx = chamar("GET", f"{orion}/v2/registrations")
    ja_existe = st == 200 and ENTIDADE_ID in tx
    if ja_existe:
        print("  [OK]   já existe um registro de comandos para a entidade")
    else:
        st, tx = chamar("POST", f"{orion}/v2/registrations", {
            "description": "Comandos da irrigacao",
            "dataProvided": {
                "entities": [{"id": ENTIDADE_ID, "type": ENTIDADE_TIPO}],
                "attrs": COMANDOS,
            },
            "provider": {"http": {"url": a.iota_interno}, "legacyForwarding": True},
        })
        passo("registro criado", st, tx)

    print("5) Histórico (STH-Comet)")
    st, tx = chamar("GET", f"{orion}/v2/subscriptions")
    if st == 200 and ENTIDADE_ID in tx and "8666" in tx:
        print("  [OK]   assinatura do STH já existe")
    else:
        st, tx = chamar("POST", f"{orion}/v2/subscriptions", {
            "description": "Historico da irrigacao para o STH-Comet",
            "subject": {
                "entities": [{"id": ENTIDADE_ID, "type": ENTIDADE_TIPO}],
                "condition": {"attrs": ATRIBUTOS_HISTORICO},
            },
            "notification": {
                "http": {"url": a.sth_interno},
                "attrs": ATRIBUTOS_HISTORICO,
                "attrsFormat": "legacy",
            },
        })
        passo("assinatura criada", st, tx)

    print("\nPronto. Teste publicando um dado falso no MQTT:")
    print(f'  mosquitto_pub -h {a.host} -t /{APIKEY}/{DEVICE_ID}/attrs -m "s|250|u|55.0|t|24.0|v|0|m|A|e|0|l|1"')
    print("e veja a entidade:")
    print(f"  curl -H 'fiware-service: {SERVICO}' -H 'fiware-servicepath: /' "
          f"{orion}/v2/entities/{ENTIDADE_ID}?options=keyValues")


def apagar(a):
    iota = f"http://{a.host}:4041"
    orion = f"http://{a.host}:1026"
    print("Removendo...")
    st, tx = chamar("DELETE", f"{iota}/iot/devices/{DEVICE_ID}")
    passo("dispositivo no IoT Agent", st, tx, ok=(200, 204, 404))
    st, tx = chamar("DELETE", f"{orion}/v2/entities/{ENTIDADE_ID}")
    passo("entidade no Orion", st, tx, ok=(200, 204, 404))
    for tipo in ("subscriptions", "registrations"):
        st, tx = chamar("GET", f"{orion}/v2/{tipo}")
        if st != 200:
            continue
        for item in json.loads(tx):
            if ENTIDADE_ID in json.dumps(item):
                st2, tx2 = chamar("DELETE", f"{orion}/v2/{tipo}/{item['id']}")
                passo(f"{tipo[:-1]} {item['id']}", st2, tx2, ok=(200, 204, 404))
    print("(o service group TEF fica, pode estar sendo usado por outros dispositivos)")


def main():
    p = argparse.ArgumentParser(description="Provisiona a irrigação no FIWARE")
    p.add_argument("--host", default="localhost", help="IP/host onde o FIWARE roda")
    p.add_argument("--orion-interno", default="http://orion:1026",
                   help="URL do Orion vista de DENTRO do docker (padrão do docker-compose)")
    p.add_argument("--iota-interno", default="http://iot-agent:4041",
                   help="URL do IoT Agent vista pelo Orion")
    p.add_argument("--sth-interno", default="http://fiware-sth-comet:8666/notify",
                   help="URL de notificação do STH-Comet vista pelo Orion")
    p.add_argument("--apagar", action="store_true", help="remove o dispositivo")
    a = p.parse_args()
    apagar(a) if a.apagar else provisionar(a)


if __name__ == "__main__":
    main()
