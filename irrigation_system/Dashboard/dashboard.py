#!/usr/bin/env python3
"""
dashboard.py - painel web da irrigação.

Só usa a biblioteca padrão do Python (nada de pip install).
O navegador abre a página (index.html) e ela pede os dados para ESTE
servidor, que repassa para o FIWARE. Assim o navegador nunca fala direto
com o FIWARE (evita problemas de CORS e esconde os endereços internos).

    navegador  <->  dashboard.py  <->  Orion (estado atual + comandos)
                                  <->  STH-Comet (histórico)

Uso:
    python3 Dashboard/dashboard.py                    # FIWARE em localhost
    python3 Dashboard/dashboard.py --fiware 20.30.40.50
    python3 Dashboard/dashboard.py --demo             # dados simulados, sem FIWARE
Depois abra http://localhost:8050
"""
import argparse
import json
import math
import os
import random
import time
import urllib.error
import urllib.parse
import urllib.request
from datetime import datetime, timezone
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

ENTIDADE_ID = "urn:ngsi-ld:Irrigacao:001"
ENTIDADE_TIPO = "Irrigacao"
SERVICO = "smart"
SERVICE_PATH = "/"
ATRIBUTOS_HISTORICO = {"umidadeSolo", "umidadeAr", "temperatura", "valvula"}
COMANDOS = {"regar", "definirModo"}

PASTA = os.path.dirname(os.path.abspath(__file__))


# Acesso ao FIWARE
class Fiware:
    def __init__(self, host):
        self.orion = f"http://{host}:1026"
        self.sth = f"http://{host}:8666"

    def _chamar(self, metodo, url, corpo=None):
        cab = {"fiware-service": SERVICO, "fiware-servicepath": SERVICE_PATH}
        dados = None
        if corpo is not None:
            dados = json.dumps(corpo).encode()
            cab["Content-Type"] = "application/json"
        req = urllib.request.Request(url, data=dados, headers=cab, method=metodo)
        with urllib.request.urlopen(req, timeout=5) as resp:
            texto = resp.read().decode()
            return json.loads(texto) if texto else None

    def estado(self):
        return self._chamar("GET", f"{self.orion}/v2/entities/{ENTIDADE_ID}?options=keyValues")

    def historico(self, atributo, n):
        url = (f"{self.sth}/STH/v1/contextEntities/type/{ENTIDADE_TIPO}/id/{ENTIDADE_ID}"
               f"/attributes/{atributo}?lastN={n}")
        dados = self._chamar("GET", url)
        valores = dados["contextResponses"][0]["contextElement"]["attributes"][0]["values"]
        pontos = []
        for v in valores:
            try:
                pontos.append({"t": v["recvTime"], "v": float(v["attrValue"])})
            except (KeyError, ValueError, TypeError):
                pass
        return pontos

    def comando(self, nome, valor):
        corpo = {nome: {"type": "command", "value": str(valor)}}
        self._chamar("PATCH", f"{self.orion}/v2/entities/{ENTIDADE_ID}/attrs", corpo)


# Modo demonstração (sem FIWARE): finge um dispositivo funcionando
class Demo:
    def __init__(self):
        self.inicio = time.time()
        self.solo = 180.0
        self.modo = "A"
        self.valvula_ate = 0.0
        self.ultima_rega = 0.0
        self.historia = {a: [] for a in ATRIBUTOS_HISTORICO}
        self.ultimo_passo = time.time()
        self.cmd = {}
        # pré-preenche 2 minutos de histórico
        agora = time.time()
        for i in range(60, 0, -1):
            self._passo(agora - i * 2, 2.0)

    def _passo(self, t, dt):
        aberta = t < self.valvula_ate
        self.solo = min(700, self.solo + 60 * dt) if aberta else max(0, self.solo - 3 * dt)
        if self.modo == "A" and not aberta and self.solo < 100 and t - self.ultima_rega > 30:
            self.valvula_ate, self.ultima_rega = t + 3, t
        umid = 50 + 8 * math.sin(t / 90) + random.uniform(-0.5, 0.5)
        temp = 24 + 2 * math.sin(t / 200) + random.uniform(-0.1, 0.1)
        iso = datetime.fromtimestamp(t, timezone.utc).isoformat().replace("+00:00", "Z")
        for nome, valor in (("umidadeSolo", int(self.solo)), ("umidadeAr", round(umid, 1)),
                            ("temperatura", round(temp, 1)), ("valvula", 1 if aberta else 0)):
            self.historia[nome].append({"t": iso, "v": valor})
            del self.historia[nome][:-300]

    def _atualizar(self):
        agora = time.time()
        while agora - self.ultimo_passo >= 2:
            self.ultimo_passo += 2
            self._passo(self.ultimo_passo, 2.0)

    def estado(self):
        self._atualizar()
        ultimo = {k: v[-1]["v"] for k, v in self.historia.items()}
        e = {
            "id": ENTIDADE_ID, "type": ENTIDADE_TIPO,
            "umidadeSolo": ultimo["umidadeSolo"], "umidadeAr": ultimo["umidadeAr"],
            "temperatura": ultimo["temperatura"], "valvula": ultimo["valvula"],
            "modo": self.modo, "erros": 0, "arduinoOnline": 1,
            "TimeInstant": self.historia["umidadeSolo"][-1]["t"],
        }
        e.update(self.cmd)
        return e

    def historico(self, atributo, n):
        self._atualizar()
        return self.historia.get(atributo, [])[-n:]

    def comando(self, nome, valor):
        if nome == "regar":
            seg = int(valor)
            self.valvula_ate = time.time() + seg
            self.ultima_rega = time.time()
        elif nome == "definirModo":
            self.modo = "M" if str(valor).upper().startswith("M") else "A"
        self.cmd[f"{nome}_status"] = "OK"
        self.cmd[f"{nome}_info"] = "OK"


# Servidor HTTP
def criar_handler(fonte):
    class Handler(BaseHTTPRequestHandler):
        def log_message(self, fmt, *args):   # silencia o log de cada requisição
            pass

        def _json(self, codigo, obj):
            corpo = json.dumps(obj).encode()
            self.send_response(codigo)
            self.send_header("Content-Type", "application/json; charset=utf-8")
            self.send_header("Cache-Control", "no-store")
            self.send_header("Content-Length", str(len(corpo)))
            self.end_headers()
            self.wfile.write(corpo)

        def _erro(self, e):
            if isinstance(e, urllib.error.HTTPError):
                msg = f"FIWARE respondeu HTTP {e.code}"
                if e.code == 404:
                    msg += " (entidade ainda não existe: rodou o provisionar.py? o ESP32 já mandou dados?)"
            elif isinstance(e, (urllib.error.URLError, OSError)):
                msg = f"Não consegui falar com o FIWARE ({e})"
            else:
                msg = f"Resposta inesperada do FIWARE ({e.__class__.__name__}: {e})"
            self._json(502, {"ok": False, "erro": msg})

        def do_GET(self):
            url = urllib.parse.urlparse(self.path)
            q = urllib.parse.parse_qs(url.query)
            if url.path in ("/", "/index.html"):
                with open(os.path.join(PASTA, "index.html"), "rb") as f:
                    corpo = f.read()
                self.send_response(200)
                self.send_header("Content-Type", "text/html; charset=utf-8")
                self.send_header("Content-Length", str(len(corpo)))
                self.end_headers()
                self.wfile.write(corpo)
            elif url.path == "/vendor/chart.umd.js":
                with open(os.path.join(PASTA, "vendor", "chart.umd.js"), "rb") as f:
                    corpo = f.read()
                self.send_response(200)
                self.send_header("Content-Type", "text/javascript; charset=utf-8")
                self.send_header("Content-Length", str(len(corpo)))
                self.send_header("Cache-Control", "max-age=86400")
                self.end_headers()
                self.wfile.write(corpo)
            elif url.path == "/api/estado":
                try:
                    self._json(200, {"ok": True, "dados": fonte.estado(),
                                     "demo": isinstance(fonte, Demo)})
                except Exception as e:  # noqa: BLE001 - mostramos o erro na tela
                    self._erro(e)
            elif url.path == "/api/historico":
                atributo = q.get("attr", [""])[0]
                n = max(1, min(500, int(q.get("n", ["60"])[0])))
                if atributo not in ATRIBUTOS_HISTORICO:
                    return self._json(400, {"ok": False, "erro": "atributo inválido"})
                try:
                    self._json(200, {"ok": True, "pontos": fonte.historico(atributo, n)})
                except Exception as e:  # noqa: BLE001
                    self._erro(e)
            else:
                self._json(404, {"ok": False, "erro": "não encontrado"})

        def do_POST(self):
            if self.path != "/api/comando":
                return self._json(404, {"ok": False, "erro": "não encontrado"})
            try:
                tamanho = int(self.headers.get("Content-Length", "0"))
                pedido = json.loads(self.rfile.read(tamanho) or b"{}")
                nome, valor = pedido.get("comando"), str(pedido.get("valor", ""))
            except (ValueError, json.JSONDecodeError):
                return self._json(400, {"ok": False, "erro": "JSON inválido"})
            if nome not in COMANDOS:
                return self._json(400, {"ok": False, "erro": "comando inválido"})
            if nome == "regar" and not (valor.isdigit() and 0 <= int(valor) <= 30):
                return self._json(400, {"ok": False, "erro": "segundos devem ser de 0 a 30"})
            if nome == "definirModo" and valor not in ("A", "M"):
                return self._json(400, {"ok": False, "erro": "modo deve ser A ou M"})
            try:
                fonte.comando(nome, valor)
                self._json(200, {"ok": True})
            except Exception as e:  # noqa: BLE001
                self._erro(e)

    return Handler


def main():
    p = argparse.ArgumentParser(description="Dashboard da irrigação")
    p.add_argument("--fiware", default="localhost", help="IP/host do FIWARE")
    p.add_argument("--porta", type=int, default=8050, help="porta do dashboard")
    p.add_argument("--demo", action="store_true", help="dados simulados, sem FIWARE")
    a = p.parse_args()

    fonte = Demo() if a.demo else Fiware(a.fiware)
    servidor = ThreadingHTTPServer(("0.0.0.0", a.porta), criar_handler(fonte))
    origem = "DEMONSTRAÇÃO (dados simulados)" if a.demo else f"FIWARE em {a.fiware}"
    print(f"Dashboard em http://localhost:{a.porta}  |  fonte: {origem}")
    try:
        servidor.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
