#!/usr/bin/env python3
"""Servidor local de falhas sinteticas para o roteiro de validacao do Farol em TV.

Por padrao escuta apenas em 127.0.0.1 (seguro). Para apontar uma TV real na
rede local, passe --host 0.0.0.0 --port 8765.

Rotas e mapeamento para os vereditos do Farol (PLANO §2.4):
  (cabo / Wi-Fi fora na TV)   -> REDE_FORA
  GET /fonte/403              -> HTTP 403 Forbidden                     -> FONTE_RECUSOU
  GET /fonte/404              -> HTTP 404 Not Found                     -> FONTE_RECUSOU
  GET /fonte/sem-range        -> HTTP 200 ignorando Range, sem          -> FONTE_SEM_RANGE
                                 Accept-Ranges nem Content-Range
  GET /fonte/lenta-8mbps      -> MKV sintetico ~40 Mbps (50 MB / 10 s)  -> REDE_CURTA
                                 servido com banda limitada a 8 Mbps
                                 (aceita Range/206 para sonda e medida)
  GET /fonte/mkv-truncado     -> HTTP 200 declarando 1 MiB, envia       -> TV_FORMATO
                                 cabecalho EBML/Matroska valido e corta
                                 a conexao abruptamente
  GET /addon/manifest.json    -> Manifest Stremio valido apos 12 s      -> ADDON_LENTO
  GET /debrid/user            -> HTTP 401 estilo API debrid             -> DEBRID_DESCONECTOU
  GET /health                 -> HTTP 200 "ok" (prontidao local)

Uso:
  python3 tests/farol_falhas_server.py [--host 127.0.0.1] [--port 8765]
                                       [--port-file PATH]
                                       [--slow-seconds 12]
                                       [--rate-mbps 8]
"""
import argparse
import http.server
import json
import os
import re
import signal
import struct
import sys
import threading
import time

# Tamanho total do arquivo sintetico de ~40 Mbps com duracao declarada de 10 s:
# 50_000_000 bytes * 8 bits / 10 s = 40_000_000 bps = 40.0 Mbps.
ARQUIVO_40MBPS_TAM = 50_000_000
SEM_RANGE_TAM = 65_536
MKV_TRUNCADO_DECLARADO = 1_048_576
CHUNK_TAM = 16_384


def _vint_size(val: int) -> bytes:
    """Codifica tamanho EBML em 1..4 bytes (formato VINT)."""
    if val < 0x7F:
        return bytes([0x80 | val])
    if val < 0x3FFF:
        return struct.pack(">H", 0x4000 | val)
    if val < 0x1FFFFF:
        return bytes([0x20 | ((val >> 16) & 0x1F), (val >> 8) & 0xFF, val & 0xFF])
    return struct.pack(">I", 0x10000000 | val)


def construir_cabecalho_mkv(duracao_ms: float = 10000.0) -> bytes:
    """Monta cabecalho EBML valido (DocType='matroska') + Segment + Info."""
    # EBML Header (ID 0x1A45DFA3)
    ebml_body = (
        b"\x42\x86\x81\x01"            # EBMLVersion = 1
        b"\x42\xf7\x81\x01"            # EBMLReadVersion = 1
        b"\x42\xf2\x81\x04"            # EBMLMaxIDLength = 4
        b"\x42\xf3\x81\x08"            # EBMLMaxSizeLength = 8
        b"\x42\x82\x88matroska"        # DocType = "matroska"
        b"\x42\x87\x81\x04"            # DocTypeVersion = 4
        b"\x42\x85\x81\x02"            # DocTypeReadVersion = 2
    )
    ebml = b"\x1a\x45\xdf\xa3" + _vint_size(len(ebml_body)) + ebml_body

    # Segment Info (ID 0x1549A966): TimestampScale = 1_000_000 ns (1 ms), Duration
    info_body = (
        b"\x2a\xd7\xb1\x83\x0f\x42\x40"                 # TimestampScale = 1000000
        + b"\x44\x89\x84" + struct.pack(">f", duracao_ms)  # Duration (float32 ms)
        + b"\x4d\x80\x8d" + b"farol-falhas!"               # MuxingApp
    )
    info = b"\x15\x49\xa9\x66" + _vint_size(len(info_body)) + info_body

    # Segment (ID 0x18538067) com tamanho desconhecido (0x01FFFFFFFFFFFFFF)
    # seguido por Info e inicio de um Cluster (ID 0x1F43B675)
    cluster_inicio = (
        b"\x1f\x43\xb6\x75\x01\xff\xff\xff\xff\xff\xff\xff"
        b"\xe7\x81\x00"                # Timestamp = 0
        b"\xa3\x41\x00"                # SimpleBlock (cabecalho parcial)
    )
    return ebml + b"\x18\x53\x80\x67\x01\xff\xff\xff\xff\xff\xff\xff" + info + cluster_inicio


MKV_CABECALHO = construir_cabecalho_mkv(10000.0)
# Payload truncado: cabecalho EBML/Matroska valido + alguns bytes cortados no meio
MKV_TRUNCADO_CORPO = MKV_CABECALHO + (b"\x00\x00\x00\x01\x42\x01" * 40)
SEM_RANGE_CORPO = (MKV_CABECALHO + b"\x00" * SEM_RANGE_TAM)[:SEM_RANGE_TAM]
PAD_CHUNK = b"\x55" * CHUNK_TAM

MANIFEST_JSON = json.dumps(
    {
        "id": "org.nuvio.farol.falhas.lento",
        "version": "1.0.0",
        "name": "Farol Add-on Lento (12s)",
        "description": "Manifest de teste lento para o roteiro de falhas do Farol",
        "resources": ["stream"],
        "types": ["movie", "series"],
        "catalogs": [],
    },
    separators=(",", ":"),
).encode("utf-8")

DEBRID_401_JSON = json.dumps(
    {"error": "bad_token", "error_code": 8},
    separators=(",", ":"),
).encode("utf-8")

RANGE_RE = re.compile(r"^bytes=(\d*)-(\d*)$")


def parse_range(header: str, total: int):
    if not header:
        return None
    m = RANGE_RE.match(header.strip())
    if not m:
        return None
    s_ini, s_fim = m.group(1), m.group(2)
    if not s_ini and not s_fim:
        return None
    if not s_ini:
        suf = int(s_fim)
        if suf <= 0:
            return None
        ini = max(0, total - suf)
        fim = total - 1
    else:
        ini = int(s_ini)
        fim = int(s_fim) if s_fim else total - 1
    if ini >= total or ini > fim:
        return None
    fim = min(fim, total - 1)
    return ini, fim


class FalhasServer(http.server.ThreadingHTTPServer):
    allow_reuse_address = True
    daemon_threads = True

    def __init__(self, addr, handler_cls, slow_seconds: float, rate_mbps: float):
        super().__init__(addr, handler_cls)
        self.slow_seconds = slow_seconds
        self.rate_bytes_per_sec = (rate_mbps * 1_000_000.0) / 8.0


class FalhasHandler(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, fmt, *args):
        pass

    def _caminho(self) -> str:
        return self.path.split("?", 1)[0]

    def _responder_simples(self, status: int, content_type: str, body: bytes,
                           send_body: bool = True, extra_headers=None):
        self.send_response(status)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Connection", "close")
        if extra_headers:
            for k, v in extra_headers:
                self.send_header(k, v)
        self.end_headers()
        self.close_connection = True
        if send_body and body:
            try:
                self.wfile.write(body)
                self.wfile.flush()
            except (BrokenPipeError, ConnectionResetError, OSError):
                pass

    def _servir_lenta_8mbps(self, send_body: bool):
        total = ARQUIVO_40MBPS_TAM
        rng = parse_range(self.headers.get("Range", ""), total)
        if rng is not None:
            ini, fim = rng
            comp = fim - ini + 1
            self.send_response(206)
            self.send_header("Content-Range", f"bytes {ini}-{fim}/{total}")
        else:
            ini, fim = 0, total - 1
            comp = total
            self.send_response(200)
        self.send_header("Content-Type", "video/x-matroska")
        self.send_header("Accept-Ranges", "bytes")
        self.send_header("Content-Length", str(comp))
        self.send_header("X-Farol-Media-Mbps", "40")
        self.send_header("Connection", "close")
        self.end_headers()
        self.close_connection = True
        if not send_body:
            return

        rate = self.server.rate_bytes_per_sec
        t0 = time.monotonic()
        enviados = 0
        pos = ini
        hdr_len = len(MKV_CABECALHO)
        try:
            while enviados < comp:
                n = min(CHUNK_TAM, comp - enviados)
                if pos < hdr_len:
                    parte_hdr = MKV_CABECALHO[pos:pos + n]
                    falta = n - len(parte_hdr)
                    bloco = parte_hdr + (PAD_CHUNK[:falta] if falta > 0 else b"")
                else:
                    bloco = PAD_CHUNK[:n]
                self.wfile.write(bloco)
                self.wfile.flush()
                enviados += n
                pos += n
                alvo = t0 + (enviados / rate)
                atraso = alvo - time.monotonic()
                if atraso > 0:
                    time.sleep(atraso)
        except (BrokenPipeError, ConnectionResetError, OSError):
            pass

    def _servir_mkv_truncado(self, send_body: bool):
        self.send_response(200)
        self.send_header("Content-Type", "video/x-matroska")
        self.send_header("Content-Length", str(MKV_TRUNCADO_DECLARADO))
        self.send_header("Connection", "close")
        self.end_headers()
        self.close_connection = True
        if not send_body:
            return
        try:
            self.wfile.write(MKV_TRUNCADO_CORPO)
            self.wfile.flush()
        except (BrokenPipeError, ConnectionResetError, OSError):
            pass

    def _despachar(self, send_body: bool):
        rota = self._caminho()
        if rota == "/health":
            self._responder_simples(200, "text/plain; charset=utf-8", b"ok", send_body)
            return
        if rota == "/fonte/403":
            self._responder_simples(403, "text/plain; charset=utf-8", b"forbidden\n", send_body)
            return
        if rota == "/fonte/404":
            self._responder_simples(404, "text/plain; charset=utf-8", b"not found\n", send_body)
            return
        if rota == "/fonte/sem-range":
            # Ignora Range de proposito e nao envia Accept-Ranges nem Content-Range.
            self._responder_simples(200, "video/x-matroska", SEM_RANGE_CORPO, send_body)
            return
        if rota == "/fonte/lenta-8mbps":
            self._servir_lenta_8mbps(send_body)
            return
        if rota == "/fonte/mkv-truncado":
            self._servir_mkv_truncado(send_body)
            return
        if rota == "/addon/manifest.json":
            time.sleep(self.server.slow_seconds)
            self._responder_simples(200, "application/json; charset=utf-8", MANIFEST_JSON, send_body)
            return
        if rota == "/debrid/user" or rota.startswith("/debrid/"):
            self._responder_simples(
                401,
                "application/json; charset=utf-8",
                DEBRID_401_JSON,
                send_body,
                extra_headers=[("WWW-Authenticate", 'Bearer realm="debrid"')],
            )
            return
        self._responder_simples(404, "text/plain; charset=utf-8", b"route not found\n", send_body)

    def do_GET(self):
        self._despachar(send_body=True)

    def do_HEAD(self):
        self._despachar(send_body=False)


def main():
    p = argparse.ArgumentParser(description="Servidor de falhas sinteticas do Farol")
    p.add_argument("--host", default="127.0.0.1", help="Endereco de escuta (padrao: 127.0.0.1)")
    p.add_argument("--port", type=int, default=8765, help="Porta (0 = efemera; padrao: 8765)")
    p.add_argument("--port-file", default="", help="Grava a porta escolhida neste arquivo")
    p.add_argument("--slow-seconds", type=float, default=12.0, help="Atraso de /addon/manifest.json em segundos")
    p.add_argument("--rate-mbps", type=float, default=8.0, help="Banda maxima de /fonte/lenta-8mbps em Mbps")
    args = p.parse_args()

    srv = FalhasServer((args.host, args.port), FalhasHandler, args.slow_seconds, args.rate_mbps)

    def parar(_sig, _frame):
        threading.Thread(target=srv.shutdown, daemon=True).start()

    signal.signal(signal.SIGTERM, parar)
    signal.signal(signal.SIGINT, parar)

    if args.port_file:
        tmp = args.port_file + ".tmp"
        with open(tmp, "w", encoding="utf-8") as f:
            f.write(f"{srv.server_port}\n")
        os.replace(tmp, args.port_file)

    try:
        srv.serve_forever()
    finally:
        srv.server_close()


if __name__ == "__main__":
    main()
