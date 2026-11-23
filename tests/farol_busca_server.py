import http.server
import json
import sys
import time

CHAVES_PEDIDO = {
    'frase', 'generos', 'idioma', 'historico',
    'vistos', 'infantil', 'privado', 'consentiu',
}


class Handler(http.server.BaseHTTPRequestHandler):
    def log_message(self, *args):
        pass

    def _send_json(self, status, obj):
        body = json.dumps(obj).encode('utf-8')
        self.send_response(status)
        self.send_header('Content-Type', 'application/json')
        self.send_header('Content-Length', str(len(body)))
        self.end_headers()
        try:
            self.wfile.write(body)
        except (BrokenPipeError, ConnectionResetError):
            pass

    def do_POST(self):
        if not self.path.endswith('/v1/farol/busca'):
            self._send_json(404, {'erro': 'nao encontrado'})
            return
        if not self.headers.get('Authorization') or not self.headers.get('X-Nuvio-Auth'):
            self._send_json(401, {'erro': 'nao autenticado'})
            return

        length = int(self.headers.get('Content-Length', '0'))
        if length > 8192:
            self._send_json(413, {'erro': 'corpo grande'})
            return
        raw = self.rfile.read(length) if length > 0 else b''
        try:
            data = json.loads(raw.decode('utf-8'))
        except Exception:
            self._send_json(400, {'erro': 'json invalido'})
            return

        if not isinstance(data, dict):
            self._send_json(400, {'erro': 'pedido invalido'})
            return
        for k in data.keys():
            if k not in CHAVES_PEDIDO:
                self._send_json(400, {'erro': 'campo nao permitido'})
                return
        frase = data.get('frase')
        if not isinstance(frase, str) or not frase.strip() or len(frase) > 200:
            self._send_json(400, {'erro': 'frase invalida'})
            return

        prefix = self.path[:-len('/v1/farol/busca')]

        if prefix == '/slow':
            time.sleep(10)
            self._send_json(200, {'ok': 1, 'ids': [], 'origem': 'llm', 'llm_status': 200})
            return

        if prefix == '/429':
            self._send_json(429, {'erro': 'limite diario'})
            return

        if prefix == '/500':
            self._send_json(500, {'erro': 'tmdb indisponivel'})
            return

        if prefix == '/400':
            self._send_json(400, {'erro': 'frase invalida'})
            return

        if prefix == '/malformado':
            body = b'{"ok":1,"ids":[{'
            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.send_header('Content-Length', str(len(body)))
            self.end_headers()
            try:
                self.wfile.write(body)
            except (BrokenPipeError, ConnectionResetError):
                pass
            return

        if prefix == '/vazio':
            self._send_json(200, {
                'ok': 1,
                'ids': [],
                'filtro': {
                    'generos': [],
                    'duracao_max': None,
                    'decada': None,
                    'parecido_com': 'Obra Inexistente',
                    'tipo': 'filme',
                },
                'origem': 'llm',
                'llm_status': 200,
            })
            return

        if prefix == '/privado':
            # Perfil privado NAO manda nada alem da frase (ADENDO:15).
            if set(data.keys()) != {'frase'}:
                self._send_json(418, {'erro': 'vazou campo em perfil privado', 'chaves': sorted(data.keys())})
                return
            self._send_json(200, {
                'ok': 1,
                'ids': [{'tmdb': 550, 'imdb': 'tt0000101', 'tipo': 'movie'}],
                'filtro': {'generos': ['suspense'], 'duracao_max': 100, 'decada': None, 'parecido_com': None, 'tipo': 'filme'},
                'origem': 'llm',
                'llm_status': 200,
            })
            return

        if prefix == '/sem-consent':
            # Sem consentimento deve enviar consentiu:0 explicito e nada pessoal (ressalva farol-29).
            if data.get('consentiu') != 0 or 'generos' in data or 'historico' in data or 'vistos' in data:
                self._send_json(418, {'erro': 'consentiu:0 ausente ou dado pessoal enviado'})
                return
            self._send_json(200, {
                'ok': 1,
                'ids': [{'tmdb': 550, 'imdb': 'tt0000101', 'tipo': 'movie'}],
                'filtro': {'generos': ['drama'], 'duracao_max': None, 'decada': None, 'parecido_com': None, 'tipo': 'filme'},
                'origem': 'llm',
                'llm_status': 200,
            })
            return

        if prefix == '/infantil':
            if data.get('infantil') != 1 or data.get('consentiu') != 0 or 'generos' in data or 'historico' in data or 'vistos' in data:
                self._send_json(418, {'erro': 'perfil infantil fora do contrato'})
                return
            self._send_json(200, {
                'ok': 1,
                'ids': [{'tmdb': 550, 'imdb': 'tt0000101', 'tipo': 'movie'}],
                'filtro': {'generos': ['animacao'], 'duracao_max': 90, 'decada': None, 'parecido_com': None, 'tipo': 'filme'},
                'origem': 'llm',
                'llm_status': 200,
            })
            return

        if prefix == '/com-consent':
            if data.get('consentiu') != 1 or not isinstance(data.get('generos'), list) or not isinstance(data.get('historico'), list):
                self._send_json(418, {'erro': 'consentiu:1 ou contexto ausente'})
                return
            self._send_json(200, {
                'ok': 1,
                'ids': [{'tmdb': 550, 'imdb': 'tt0000101', 'tipo': 'movie'}],
                'filtro': {'generos': ['suspense'], 'duracao_max': 100, 'decada': 1990, 'parecido_com': None, 'tipo': 'filme'},
                'origem': 'llm',
                'llm_status': 200,
            })
            return

        if prefix == '/palavras':
            self._send_json(200, {
                'ok': 1,
                'ids': [{'tmdb': 550, 'imdb': 'tt0000101', 'tipo': 'movie'}],
                'filtro': {'generos': [], 'duracao_max': None, 'decada': None, 'parecido_com': None, 'tipo': 'qualquer'},
                'origem': 'palavras',
                'llm_status': 422,
            })
            return

        # Default /ok (ou raiz): 2 itens no catalogo (1 por imdb, 1 so por tmdb) + 2 fora (1 imdb, 1 so tmdb).
        self._send_json(200, {
            'ok': 1,
            'ids': [
                {'tmdb': 550, 'imdb': 'tt0000101', 'tipo': 'movie'},
                {'tmdb': 1396, 'imdb': '', 'tipo': 'series'},
                {'tmdb': 999001, 'imdb': 'tt9990001', 'tipo': 'movie'},
                {'tmdb': 999002, 'imdb': '', 'tipo': 'series'},
            ],
            'filtro': {
                'generos': ['suspense', 'crime'],
                'duracao_max': 100,
                'decada': 1990,
                'parecido_com': 'Obra Ref',
                'tipo': 'filme',
            },
            'origem': 'llm',
            'llm_status': 200,
        })


server = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Handler)
with open(sys.argv[1], 'w') as f:
    f.write(str(server.server_port))
server.serve_forever()
