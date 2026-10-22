# Addon Stremio FALSO para tests/ctxlista.sh: um catalogo de filmes com 24
# titulos (a grade de "Ver tudo" e a pagina de colecao). Nomes inventados.
import http.server, socketserver, sys, json
PORTA = int(sys.argv[1])
NOMES = ["Aurora Vermelha", "Mar de Cinzas", "O Ultimo Farol", "Cidade Submersa",
         "Vozes do Deserto", "Rio Acima", "A Casa do Vento", "Linha de Fuga",
         "Noite em Lisboa", "Os Herdeiros", "Pedra e Sal", "Ventos do Norte"]
# Fundo, sinopse, generos e nota: o que a extensao do menu (ctxinfo.c) mostra
# sem rede nenhuma, direto do catalogo.
METAS = [{"id": "tt91%05d" % i, "type": "movie", "name": NOMES[i % 12] + (" II" if i >= 12 else ""),
          "poster": "deploy/app/art/%02d.jpg" % (i % 40), "releaseInfo": str(2000 + i),
          "background": "deploy/app/art/%02d.jpg" % ((i + 9) % 40),
          "description": "Uma travessia de inverno junta dois irmaos que nao se falam ha "
                         "dez anos, e cada parada no caminho devolve uma lembranca que "
                         "nenhum dos dois queria ter de volta.",
          "genres": ["Drama", "Aventura"], "imdbRating": "7.%d" % (i % 10)}
         for i in range(24)]
class H(http.server.BaseHTTPRequestHandler):
    def log_message(self, *a): pass
    def do_GET(self):
        p = self.path
        if p.startswith('/catalog/'):
            corpo = '{"metas":[]}' if 'skip=' in p and 'skip=0' not in p else json.dumps({"metas": METAS})
            b = corpo.encode()
            self.send_response(200); self.send_header('Content-Type', 'application/json')
            self.send_header('Content-Length', str(len(b))); self.end_headers(); self.wfile.write(b)
            return
        self.send_response(404); self.end_headers()
class S(socketserver.ThreadingMixIn, http.server.HTTPServer): daemon_threads = True
S(('127.0.0.1', PORTA), H).serve_forever()
