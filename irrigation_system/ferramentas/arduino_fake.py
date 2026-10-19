#!/usr/bin/env python3
"""
arduino_fake.py - finge ser o Arduino numa porta serial VIRTUAL (Linux/WSL).

Serve para testar o ESP32 (simulador) e o FIWARE sem a placa ligada.
Fala exatamente o mesmo protocolo do firmware (Arduino/src/protocolo).
Só usa a biblioteca padrão do Python (nada de pip install).

Uso:
    python3 ferramentas/arduino_fake.py
    # ele mostra algo como:  SERIAL_PORTA=/dev/pts/5
    # em outro terminal:
    cd ESP32 && SERIAL_PORTA=/dev/pts/5 ./build/ESP32Mock

Teclas (digite e aperte Enter):
    r  = botão REGAR          m  = botão MODO
    d  = liga/desliga falha do DHT11
    s  = liga/desliga falha do sensor de solo
    c  = "corta o cabo" (para de mandar dados -> testa o timeout do ESP32)
    q  = sair
"""
import os
import random
import select
import sys
import termios
import time
import tty

INTERVALO_ENVIO = 2.0
TIMEOUT_ESP = 6.0
SOLO_SECO = 100
SOLO_MEDIO = 300
INTERVALO_MIN_REGAS = 30.0
REGA_PADRAO = 3


def checksum(conteudo: str) -> int:
    cs = 0
    for ch in conteudo:
        cs ^= ord(ch)
    return cs


def montar(conteudo: str) -> bytes:
    return f"${conteudo}*{checksum(conteudo):02X}\n".encode()


def ler_quadro(linha: str):
    """Valida '$...*HH' e devolve a lista de campos (tipo primeiro) ou None."""
    if not linha.startswith("$") or "*" not in linha:
        return None
    conteudo, _, cs = linha[1:].rpartition("*")
    try:
        if int(cs, 16) != checksum(conteudo):
            return None
    except ValueError:
        return None
    return conteudo.split(",")


class ArduinoFake:
    def __init__(self, fd):
        self.fd = fd
        self.seq = 0
        self.solo = 160.0          # começa "baixo", vai secando
        self.umidade = 55.0
        self.temperatura = 24.0
        self.modo = "A"
        self.valvula_ate = 0.0
        self.ultima_rega = -1e9
        self.falha_dht = False
        self.falha_solo = False
        self.cabo_cortado = False
        self.esp_online = False
        self.nuvem_online = False
        self.ultimo_quadro_esp = 0.0
        self.buffer = ""

    # ---------- saída ----------
    def enviar(self, conteudo):
        if not self.cabo_cortado:
            os.write(self.fd, montar(conteudo))

    def log(self, msg):
        print(f"  [arduino] {msg}")
        if not self.cabo_cortado:
            os.write(self.fd, f"# {msg}\n".encode())

    # ---------- válvula ----------
    def valvula_aberta(self):
        return time.monotonic() < self.valvula_ate

    def abrir(self, segundos):
        if segundos <= 0:
            self.valvula_ate = 0          # o loop principal avisa "Valvula FECHADA"
            return
        self.valvula_ate = time.monotonic() + segundos
        self.ultima_rega = time.monotonic()
        self.log(f"Valvula ABERTA por (s): {segundos}")

    # ---------- entrada ----------
    def receber(self, dados: bytes):
        self.buffer += dados.decode(errors="ignore")
        while "\n" in self.buffer:
            linha, self.buffer = self.buffer.split("\n", 1)
            campos = ler_quadro(linha.strip())
            if campos is None:
                continue
            if self.cabo_cortado:
                continue
            if not self.esp_online:
                self.log("ESP32 ONLINE")
            self.esp_online = True
            self.ultimo_quadro_esp = time.monotonic()
            tipo = campos[0]
            if tipo == "S" and len(campos) >= 2:
                self.nuvem_online = campos[1] == "1"
            elif tipo == "C":
                self.tratar_comando(campos[1:])

    def tratar_comando(self, args):
        ok = False
        nome = args[0] if args else "?"
        if nome == "REGAR" and len(args) >= 2 and args[1].isdigit():
            seg = int(args[1])
            if 0 <= seg <= 30:
                self.log("Comando da nuvem: REGAR")
                self.abrir(seg)
                ok = True
        elif nome == "MODO" and len(args) >= 2 and args[1] in ("A", "M"):
            self.modo = args[1]
            self.log("Modo: " + ("AUTO" if self.modo == "A" else "MANUAL"))
            ok = True
        self.enviar(f"A,{nome},{'OK' if ok else 'ERRO'}")

    # ---------- "física" de mentira ----------
    def simular(self, dt):
        if self.valvula_aberta():
            self.solo = min(700, self.solo + 60 * dt)       # água entrando
        else:
            self.solo = max(0, self.solo - 4 * dt)          # secando
        self.umidade += random.uniform(-0.3, 0.3)
        self.umidade = max(20, min(90, self.umidade))
        self.temperatura += random.uniform(-0.05, 0.05)

    def erros(self):
        return (1 if self.falha_dht else 0) | (2 if self.falha_solo else 0)

    def decidir_auto(self):
        agora = time.monotonic()
        if self.modo != "A" or self.valvula_aberta() or self.falha_solo:
            return
        if agora - self.ultima_rega < INTERVALO_MIN_REGAS:
            return
        seco = self.solo < SOLO_SECO
        ar_seco = (not self.falha_dht) and self.umidade < 30 and self.solo < SOLO_MEDIO
        if seco or ar_seco:
            self.log(f"AUTO: solo pedindo agua. Leitura = {int(self.solo)}")
            self.abrir(REGA_PADRAO)

    def enviar_dados(self):
        self.seq = (self.seq + 1) % 65536
        self.enviar("D,{},{},{},{},{},{},{}".format(
            self.seq, int(self.solo),
            0 if self.falha_dht else round(self.umidade * 10),
            0 if self.falha_dht else round(self.temperatura * 10),
            1 if self.valvula_aberta() else 0, self.modo, self.erros()))

    def status(self):
        v = "ABERTA " if self.valvula_aberta() else "fechada"
        esp = "online " if self.esp_online else "OFFLINE"
        nuvem = "online " if self.nuvem_online else "OFFLINE"
        return (f"solo={int(self.solo):4d} ar={self.umidade:4.1f}% t={self.temperatura:4.1f}C "
                f"valvula={v} modo={self.modo} erros={self.erros()} esp={esp} nuvem={nuvem}"
                + ("  [CABO CORTADO]" if self.cabo_cortado else ""))


def main():
    mestre, escravo = os.openpty()
    tty.setraw(escravo)                       # sem eco, sem tradução de \n
    porta = os.ttyname(escravo)
    print("=" * 60)
    print(" Arduino FAKE pronto.")
    print(f" Rode o ESP32 com:   SERIAL_PORTA={porta} ./build/ESP32Mock")
    print(" Teclas: r=regar  m=modo  d=falha DHT  s=falha solo  c=cortar cabo  q=sair")
    print("=" * 60)

    ard = ArduinoFake(mestre)
    ultimo_envio = ultimo_status = ultimo_passo = time.monotonic()
    valvula_antes = False

    while True:
        entradas = [mestre] + ([sys.stdin] if not sys.stdin.closed and sys.stdin.name != os.devnull else [])
        prontos, _, _ = select.select(entradas, [], [], 0.1)
        if mestre in prontos:
            try:
                ard.receber(os.read(mestre, 1024))
            except OSError:
                pass
        teclas = []
        if sys.stdin in prontos:
            # os.read em vez de readline: o readline guarda linhas extras num
            # buffer interno e o select() não avisa de novo.
            dados = os.read(sys.stdin.fileno(), 1024)
            if not dados:          # stdin fechado (ex.: rodando em segundo plano)
                sys.stdin = open(os.devnull)
            teclas = dados.decode(errors="ignore").lower().split()
        for tecla in teclas:
            if tecla == "q":
                raise KeyboardInterrupt
            elif tecla == "r":
                if ard.valvula_aberta():
                    ard.log("Botao REGAR: parando a rega")
                    ard.abrir(0)
                else:
                    ard.log("Botao REGAR: rega manual")
                    ard.abrir(REGA_PADRAO)
            elif tecla == "m":
                ard.modo = "M" if ard.modo == "A" else "A"
                ard.log("Modo: " + ("AUTO" if ard.modo == "A" else "MANUAL"))
            elif tecla == "d":
                ard.falha_dht = not ard.falha_dht
                ard.log("ERRO: falha na leitura do DHT11" if ard.falha_dht else "Sensores OK")
            elif tecla == "s":
                ard.falha_solo = not ard.falha_solo
                ard.log("ERRO: sensor de solo desconectado" if ard.falha_solo else "Sensores OK")
            elif tecla == "c":
                ard.cabo_cortado = not ard.cabo_cortado
                print("  >> cabo", "CORTADO" if ard.cabo_cortado else "religado")

        agora = time.monotonic()
        ard.simular(agora - ultimo_passo)
        ultimo_passo = agora

        if valvula_antes and not ard.valvula_aberta():
            ard.log("Valvula FECHADA")
        valvula_antes = ard.valvula_aberta()

        if ard.esp_online and agora - ard.ultimo_quadro_esp > TIMEOUT_ESP:
            ard.esp_online = False
            ard.nuvem_online = False
            ard.log("ERRO: sem comunicacao com o ESP32")

        if agora - ultimo_envio >= INTERVALO_ENVIO:
            ultimo_envio = agora
            ard.decidir_auto()
            ard.enviar_dados()

        if agora - ultimo_status >= 5:
            ultimo_status = agora
            print(ard.status())

    os.close(mestre)
    os.close(escravo)


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        pass
