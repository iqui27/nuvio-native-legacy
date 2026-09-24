#!/usr/bin/env python3
# Sideload helper para Hisense VIDAA: DNS e HTTPS server.
#
# O VIDAA so permite chamar Hisense_installApp() de uma pagina servida por
# vidaahub.com. Este script:
#   1. Intercepta DNS para vidaahub.com e www.vidaahub.com, respondendo com
#      o IP desta maquina (ou um IP customizado via --ip)
#   2. Serve HTTPS em porta 443 (requer sudo) com certificado auto-assinado
#   3. A pagina que serve chama Hisense_installApp() para instalar nuvio
#
# Uso:
#   sudo python3 tools/vidaa-instalar/instalar.py
#   sudo python3 tools/vidaa-instalar/instalar.py --ip 192.168.1.100
#   sudo python3 tools/vidaa-instalar/instalar.py --url https://seu-dominio.com/tv/
#
# Requer root para portas < 1024 (DNS na 53, HTTPS na 443).
#
# RISCO: voce esta fazendo DNS spoofing. Desfaca no final mudando o DNS da TV
# de volta para automatico.

import argparse
import ssl
import socket
import struct
import sys
import threading
import time
from http.server import HTTPServer, BaseHTTPRequestHandler
from pathlib import Path

# Versao extraida de deploy/app/appinfo.json — mesma que em vidaa-site.sh
def get_app_version():
    appinfo_path = Path(__file__).parent.parent.parent / "deploy" / "app" / "appinfo.json"
    if not appinfo_path.exists():
        return "1.0.0"
    try:
        import json
        with open(appinfo_path) as f:
            data = json.load(f)
            return data.get("version", "1.0.0")
    except:
        return "1.0.0"

class InstallPageHandler(BaseHTTPRequestHandler):
    """Serve a HTML page que chama Hisense_installApp()"""
    
    app_url = None
    server_ip = None
    
    def do_GET(self):
        if self.path == "/" or self.path == "/index.html":
            version = get_app_version()
            html = f"""<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <title>Nuvio Installer</title>
    <style>
        body {{ font-family: Arial, sans-serif; padding: 40px; text-align: center; }}
        button {{ padding: 15px 30px; font-size: 16px; margin: 10px; cursor: pointer; }}
        #status {{ margin-top: 20px; font-size: 14px; }}
        .success {{ color: green; }}
        .error {{ color: red; }}
    </style>
</head>
<body>
    <h1>Nuvio</h1>
    <p>Streaming app for Hisense VIDAA</p>
    <p>Version {version}</p>
    <button onclick="instalar()">Install</button>
    <button onclick="desinstalar()">Uninstall</button>
    <div id="status"></div>
    
    <script>
        function instalar() {{
            var status = document.getElementById('status');
            status.textContent = 'Installing...';
            status.className = '';
            
            Hisense_installApp(
                'nuvio.native_debug',
                'Nuvio',
                // Icones do proprio worker: vidaahub.com some quando o DNS da TV volta
                // ao normal, e o launcher busca o icone de novo depois.
                '{self.app_url}icone-220.png',
                '{self.app_url}icone-400.png',
                '{self.app_url}icone-400.png',
                '{self.app_url}',
                'store',
                function(result) {{
                    if (!result) {{  // trialuser/vidaa-appstore: status falso = instalado
                        status.textContent = 'Installation successful!';
                        status.className = 'success';
                    }} else {{
                        status.textContent = 'Installation failed: ' + result;
                        status.className = 'error';
                    }}
                }}
            );
        }}
        
        function desinstalar() {{
            var status = document.getElementById('status');
            status.textContent = 'Uninstalling...';
            status.className = '';
            
            if (typeof Hisense_uninstallApp === 'function') {{
                Hisense_uninstallApp(
                    'nuvio.native_debug',
                    function(result) {{
                        if (result) {{  // no desinstalar e o contrario: verdadeiro = removido
                            status.textContent = 'Uninstall successful!';
                            status.className = 'success';
                        }} else {{
                            status.textContent = 'Uninstall failed: ' + result;
                            status.className = 'error';
                        }}
                    }}
                );
            }} else {{
                status.textContent = 'Uninstall not supported on this TV version';
                status.className = 'error';
            }}
        }}
    </script>
</body>
</html>"""
            self.send_response(200)
            self.send_header('Content-Type', 'text/html; charset=utf-8')
            self.send_header('Content-Length', len(html.encode()))
            self.end_headers()
            self.wfile.write(html.encode())
        else:
            self.send_error(404)
    
    def log_message(self, format, *args):
        print(f"[HTTPS] {format % args}")

class DNSQuery:
    """Parser minimalista para DNS queries (RFC 1035)"""
    def __init__(self, data):
        self.data = data
        self.domain = b""
        self.qtype = None
        self.parse()
    
    def parse(self):
        # Header: 12 bytes, depois vem question
        if len(self.data) < 12:
            return
        
        pos = 12
        # Parse domain name (labels com length byte)
        while pos < len(self.data):
            length = self.data[pos]
            if length == 0:
                break
            if length & 0xc0:  # Pointer
                break
            pos += 1
            if pos + length <= len(self.data):
                self.domain += self.data[pos:pos+length] + b"."
                pos += length
            else:
                break
        
        self.domain = self.domain.rstrip(b".")
        
        # qtype em 2 bytes depois do domain
        if pos < len(self.data) - 3:
            self.qtype = struct.unpack(">H", self.data[pos:pos+2])[0]

class DNSResponse:
    """Constroi resposta DNS minimalista"""
    @staticmethod
    def build_response(query_data, domain, ip_address):
        """Retorna resposta DNS A record"""
        # Parse query para extrair header ID
        if len(query_data) < 12:
            return b""
        
        query_id = query_data[0:2]
        
        # Header da resposta: ID + flags (0x8180 = resposta + recursion available)
        response = query_id + b"\x81\x80"
        response += b"\x00\x01"  # 1 question
        response += b"\x00\x01"  # 1 answer
        response += b"\x00\x00"  # 0 authority
        response += b"\x00\x00"  # 0 additional
        
        # Echo da question
        # Encode domain name
        domain_bytes = domain if isinstance(domain, bytes) else domain.encode()
        for label in domain_bytes.split(b"."):
            response += bytes([len(label)]) + label
        response += b"\x00"
        response += b"\x00\x01"  # qtype A
        response += b"\x00\x01"  # qclass IN
        
        # Answer: same domain, type A, class IN, TTL=60, RDLEN=4, RDATA=IP
        for label in domain_bytes.split(b"."):
            response += bytes([len(label)]) + label
        response += b"\x00"
        response += b"\x00\x01"  # type A
        response += b"\x00\x01"  # class IN
        response += b"\x00\x00\x00\x3c"  # TTL = 60 seconds
        response += b"\x00\x04"  # RDLEN = 4 (IPv4)
        
        # Parse IP e converte para bytes
        parts = ip_address.split(".")
        for part in parts:
            response += bytes([int(part)])
        
        return response

def run_dns_server(host, port, target_ip):
    """DNS server que responde vidaahub.com com target_ip"""
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    try:
        sock.bind((host, port))
        print(f"[DNS] Listening on {host}:{port}")
        print(f"[DNS] vidaahub.com -> {target_ip}")
        print(f"[DNS] www.vidaahub.com -> {target_ip}")
        
        while True:
            data, addr = sock.recvfrom(512)
            try:
                query = DNSQuery(data)
                domain_str = query.domain.decode('utf-8', errors='ignore').lower()
                
                # Responde para vidaahub.com e www.vidaahub.com
                if domain_str in ["vidaahub.com", "www.vidaahub.com"]:
                    response = DNSResponse.build_response(data, query.domain, target_ip)
                    sock.sendto(response, addr)
                    print(f"[DNS] {domain_str} -> {target_ip} (from {addr[0]})")
                else:
                    # Recusa outras domains (NXDOMAIN)
                    response = data[0:2] + b"\x81\x83" + data[4:]
                    sock.sendto(response, addr)
            except Exception as e:
                print(f"[DNS] Error: {e}")
    finally:
        sock.close()

def run_https_server(host, port, certfile, keyfile, app_url, server_ip):
    """HTTPS server com self-signed certificate"""
    InstallPageHandler.app_url = app_url
    InstallPageHandler.server_ip = server_ip
    
    server = HTTPServer((host, port), InstallPageHandler)
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.load_cert_chain(certfile, keyfile)
    server.socket = context.wrap_socket(server.socket, server_side=True)
    
    print(f"[HTTPS] Listening on {host}:{port}")
    print(f"[HTTPS] App URL: {app_url}")
    print(f"[HTTPS] Access: https://vidaahub.com/")
    server.serve_forever()

def generate_self_signed_cert(certfile, keyfile):
    """Gera certificado auto-assinado se nao existir"""
    if Path(certfile).exists() and Path(keyfile).exists():
        return
    
    print("[SSL] Generating self-signed certificate...")
    import subprocess
    try:
        subprocess.run([
            "openssl", "req", "-x509", "-newkey", "rsa:2048",
            "-keyout", keyfile, "-out", certfile, "-days", "365",
            "-nodes", "-subj", "/CN=vidaahub.com"
        ], check=True, capture_output=True)
        print(f"[SSL] Certificate: {certfile}")
        print(f"[SSL] Key: {keyfile}")
    except subprocess.CalledProcessError as e:
        print(f"[SSL] Error generating certificate: {e}")
        sys.exit(1)
    except FileNotFoundError:
        print("[SSL] Error: openssl not found")
        sys.exit(1)

def main():
    parser = argparse.ArgumentParser(
        description="Hisense VIDAA sideload installer",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  sudo python3 instalar.py
  sudo python3 instalar.py --ip 192.168.1.100
  sudo python3 instalar.py --url https://nuvio.example.com/app/
        """
    )
    parser.add_argument("--ip", default=None,
                        help="Local IP for DNS (default: auto-detect)")
    parser.add_argument("--url", default="https://nuvio-recomendacoes.henriquef29.workers.dev/tv/",
                        help="Application URL (default: https://nuvio-recomendacoes.henriquef29.workers.dev/tv/)")
    parser.add_argument("--dns-port", type=int, default=53,
                        help="DNS port (default: 53, requires root)")
    parser.add_argument("--https-port", type=int, default=443,
                        help="HTTPS port (default: 443, requires root)")
    parser.add_argument("--https-only", action="store_true",
                        help="Skip DNS server, only run HTTPS")
    
    args = parser.parse_args()
    
    # Detecta IP local se nao foi passado
    if args.ip:
        server_ip = args.ip
    else:
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            s.connect(("8.8.8.8", 80))
            server_ip = s.getsockname()[0]
            s.close()
        except:
            server_ip = "127.0.0.1"
    
    # Os icones sao <url>icone-220.png: sem a barra no fim o caminho quebra.
    if not args.url.endswith("/"):
        args.url += "/"
    print(f"Server IP: {server_ip}")
    print(f"App URL: {args.url}")
    print()
    
    # Certificado auto-assinado
    cert_dir = Path(__file__).parent / "certs"
    cert_dir.mkdir(exist_ok=True)
    certfile = str(cert_dir / "vidaahub.com.crt")
    keyfile = str(cert_dir / "vidaahub.com.key")
    
    if not Path(certfile).exists():
        generate_self_signed_cert(certfile, keyfile)
    
    # Verifica permissoes
    if args.dns_port < 1024 or args.https_port < 1024:
        if sys.platform != "win32":
            import os
            if os.geteuid() != 0:
                print("Error: Ports < 1024 require root (use sudo)")
                sys.exit(1)
    
    # Inicia servers em threads
    if not args.https_only:
        dns_thread = threading.Thread(
            target=run_dns_server,
            args=("0.0.0.0", args.dns_port, server_ip),
            daemon=True
        )
        dns_thread.start()
        time.sleep(0.5)
    
    https_thread = threading.Thread(
        target=run_https_server,
        args=("0.0.0.0", args.https_port, certfile, keyfile, args.url, server_ip),
        daemon=True
    )
    https_thread.start()
    
    print()
    print("=" * 60)
    print("NEXT STEPS:")
    print("=" * 60)
    print(f"1. On the TV, go to Settings > System > About > Type 1234 (enable Developer Mode)")
    print(f"2. Go to Network settings and change DNS to: {server_ip}")
    print(f"3. Open the TV browser and navigate to: https://vidaahub.com/")
    print(f"4. Click 'Install' to sideload Nuvio")
    print(f"5. After installation, change DNS back to automatic")
    print()
    print("Press Ctrl+C to stop the server")
    print("=" * 60)
    print()
    
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print("\nShutting down...")
        sys.exit(0)

if __name__ == "__main__":
    main()
