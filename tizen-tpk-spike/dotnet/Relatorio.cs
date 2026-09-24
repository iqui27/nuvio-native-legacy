// Envio do resultado do spike para o mesmo endpoint de registros do app
// (/v1/registro do worker de recomendacoes), no modo "diagnostico" do #77:
// sem login, token DIAG_TOKEN embutido no build, grava sob "diag:<tv>".
// Os valores vem de Segredos.g.cs, gerado por tools/tizen-tpk-spike.sh e fora
// do git; sem eles o envio fica desligado e a tela pede foto.
using System;
using System.Collections.Generic;
using System.Net;
using System.Net.Http;
using System.Text;
using System.Threading.Tasks;

namespace NvSpike
{
    static class Relatorio
    {
        public static bool Ligado => Segredos.Token.Length > 0 && Segredos.Url.Length > 0;

        // Codigo curto na tela para o testador citar junto da foto/comentario
        // e casar com a linha gravada no servidor.
        public static readonly string Codigo = Guid.NewGuid().ToString("N").Substring(0, 6).ToUpperInvariant();

        public static async Task<string> Enviar(string pacote, string tv, IEnumerable<string> linhas)
        {
            if (!Ligado) return "Envio desligado neste build: mande uma foto desta tela.";
            var texto = new StringBuilder();
            texto.Append("[spike-tpk] codigo=").Append(Codigo).Append(" pacote=").Append(pacote).Append('\n');
            foreach (var l in linhas) texto.Append(l).Append('\n');
            string corpo = "{\"versao\":\"spike-tpk-0.1.0\",\"plataforma\":\"tizen-tpk\",\"quando\":\"" +
                           DateTime.UtcNow.ToString("yyyy-MM-dd HH:mm:ss") + " UTC\",\"tv\":\"spike-" +
                           Esc(tv) + "\",\"execucao_id\":\"" + Codigo + "\",\"texto\":\"" + Esc(texto.ToString()) + "\"}";
            try
            {
                return await Postar(corpo, false);
            }
            catch (HttpRequestException e) when (ErroDeCertificado(e))
            {
                // TV antiga com cadeia de CA vencida recusa o TLS do worker.
                // Sem validar nao expoe nada novo: o token ja viaja dentro do
                // .tpk, que qualquer um descompacta, e so permite gravar registro.
                try { return await Postar(corpo, true) + " (TLS sem validacao)"; }
                catch (Exception e2) { return "Envio FALHOU: " + e2.GetType().Name + ": " + e2.Message + ". Mande uma foto."; }
            }
            catch (Exception e)
            {
                return "Envio FALHOU: " + e.GetType().Name + ": " + e.Message + ". Mande uma foto.";
            }
        }

        static async Task<string> Postar(string corpo, bool semValidar)
        {
            var h = new HttpClientHandler();
            if (semValidar) h.ServerCertificateCustomValidationCallback = (m, c, ch, er) => true;
            using (var http = new HttpClient(h) { Timeout = TimeSpan.FromSeconds(20) })
            {
                var req = new HttpRequestMessage(HttpMethod.Post, Segredos.Url + "/v1/registro");
                req.Headers.TryAddWithoutValidation("Authorization", "Bearer " + Segredos.Token);
                req.Headers.TryAddWithoutValidation("X-Nuvio-Auth", "diagnostico");
                req.Content = new StringContent(corpo, Encoding.UTF8, "application/json");
                var resp = await http.SendAsync(req);
                string r = await resp.Content.ReadAsStringAsync();
                return resp.IsSuccessStatusCode && r.Contains("registro_id")
                    ? "Relatorio enviado. Codigo " + Codigo + "."
                    : "Envio FALHOU: HTTP " + (int)resp.StatusCode + ". Mande uma foto.";
            }
        }

        static bool ErroDeCertificado(Exception e)
        {
            for (var x = e; x != null; x = x.InnerException)
            {
                string m = x.Message ?? "";
                if (x is System.Security.Authentication.AuthenticationException ||
                    m.IndexOf("certificate", StringComparison.OrdinalIgnoreCase) >= 0 ||
                    m.IndexOf("SSL", StringComparison.Ordinal) >= 0)
                    return true;
            }
            return false;
        }

        static string Esc(string s)
        {
            var b = new StringBuilder(s.Length + 16);
            foreach (char c in s)
            {
                if (c == '"' || c == '\\') b.Append('\\').Append(c);
                else if (c == '\n') b.Append("\\n");
                else if (c < 0x20) b.Append("\\u").Append(((int)c).ToString("x4"));
                else b.Append(c);
            }
            return b.ToString();
        }
    }
}
