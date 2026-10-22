// AUTO-ATUALIZACAO DO .tpk — decisao de carga e memfd COMPARTILHADOS.
//
// A libnuvio.so E o app inteiro (UI, catalogo, player, sync); o host .NET quase
// nunca muda. Este arquivo deixa o app trocar a .so SEM o usuario reinstalar o
// .tpk: o lado C (src/atualizacao.c, sob NV_TPK) baixa uma libnuvio.so mais nova,
// CONFERE o sha256 (vindo do digest da release por HTTPS) e ENCENA em
// data/libnuvio.staged.so (+ .sha256 + .ver). Aqui, NO ARRANQUE, o host decide
// UMA VEZ o que carregar:
//
//   - Se data/libnuvio.staged.so existe, o sha256 dele bate com
//     data/libnuvio.staged.sha256, e data/libnuvio.staged.ver e mais novo que a
//     versao EMPACOTADA (res/versao.txt) -> memfd-carrega a .so encenada.
//   - Senao -> carrega a .so EMPACOTADA do jeito de sempre (o caminho provado:
//     6+ faz dlopen simples em Program.cs; 4/5 faz memfd/ELF em Program40.cs).
//
// SEGURANCA: isto carrega CODIGO NATIVO REMOTO. A barreira e o sha256, RECONFERIDO
// aqui contra o arquivo em disco antes de qualquer carga. Em QUALQUER duvida
// (hash nao bate, versao nao e mais nova, falha de leitura) o staging e APAGADO e
// o app segue com a .so empacotada — uma tentativa de atualizacao NUNCA impede o
// app de abrir.
//
// Compartilhado pelos quatro pacotes (NuvioTpk, 60, 65 e 40).
using System;
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Security.Cryptography;

namespace NuvioTpk
{
    static class NvCarga
    {
        [DllImport("libdl.so.2")] static extern IntPtr dlopen(string path, int flags);
        [DllImport("libdl.so.2")] static extern IntPtr dlsym(IntPtr h, string sym);
        [DllImport("libdl.so.2")] static extern IntPtr dlerror();
        // libc do Tizen nao exporta memfd_create como simbolo (so o dispatcher
        // syscall generico); igual ao Program40. 385 = ARMv7 EABI.
        [DllImport("libc.so.6", SetLastError = true, EntryPoint = "syscall")]
        static extern int syscall_memfd(int number, IntPtr name, int flags);
        [DllImport("libc.so.6", SetLastError = true)] static extern IntPtr write(int fd, IntPtr buf, IntPtr count);

        const int RTLD_NOW = 2, RTLD_GLOBAL = 0x100;
        const int SYS_memfd_create = 385;

        const string ARQ_SO = "libnuvio.staged.so";
        const string ARQ_SHA = "libnuvio.staged.sha256";
        const string ARQ_VER = "libnuvio.staged.ver";
        const string ARQ_DL = "libnuvio.download";

        // fd do memfd, vivo pela vida do processo: /proc/self/fd/N depende dele.
        static int memfdFd = -1;

        // Versao EMPACOTADA (a da .so do pacote). tpk.sh grava res/versao.txt =
        // $VER no build. "" quando o arquivo nao existe (pacotes antigos).
        public static string VersaoEmpacotada(string resourceDir)
        {
            try
            {
                string p = Path.Combine(resourceDir, "versao.txt");
                if (File.Exists(p)) return File.ReadAllText(p).Trim();
            }
            catch { }
            return "";
        }

        // Decide, uma vez no arranque, se ha uma .so encenada VALIDA e MAIS NOVA.
        // Devolve o caminho dela para carregar por memfd, ou null para usar a
        // empacotada. Um staging invalido (hash nao bate, versao alcancada pela
        // empacotada, leitura falha) e APAGADO aqui mesmo.
        public static string DecidirStaged(string dados, string versaoEmpacotada, out string log)
        {
            log = "";
            if (string.IsNullOrEmpty(dados)) return null;
            string so = Path.Combine(dados, ARQ_SO);
            string sha = Path.Combine(dados, ARQ_SHA);
            string ver = Path.Combine(dados, ARQ_VER);
            try
            {
                if (!File.Exists(so) || !File.Exists(sha)) return null;   // nada encenado
                string esperado = File.ReadAllText(sha).Trim().ToLowerInvariant();
                string stagedVer = File.Exists(ver) ? File.ReadAllText(ver).Trim() : "";
                if (esperado.Length != 64) { log = "sha do staging invalido"; ApagarStaged(dados); return null; }
                // Versao: so aplica o que for ESTRITAMENTE mais novo que a
                // empacotada. Depois de um reinstalar do .tpk que ja inclua (ou
                // ultrapasse) a versao encenada, o staging deixa de valer e some.
                if (string.IsNullOrEmpty(stagedVer) || !MaisNova(stagedVer, versaoEmpacotada ?? ""))
                {
                    log = "staging nao e mais novo que o empacotado (" + stagedVer + " vs " + versaoEmpacotada + ")";
                    ApagarStaged(dados);
                    return null;
                }
                // Barreira de seguranca: reconfere o hash do ARQUIVO em disco.
                string real = Sha256Hex(so);
                if (!string.Equals(real, esperado, StringComparison.OrdinalIgnoreCase))
                {
                    log = "sha256 do staging nao confere (real " + Curto(real) + " != esperado " + Curto(esperado) + ")";
                    ApagarStaged(dados);
                    return null;
                }
                log = "staging " + stagedVer + " valido";
                return so;
            }
            catch (Exception e) { log = "erro ao avaliar staging: " + e.Message; try { ApagarStaged(dados); } catch { } return null; }
        }

        // memfd_create(385) + dlopen(/proc/self/fd/N) RTLD_GLOBAL: a .so entra no
        // link map do loader com o soname libnuvio.so. ISSO NAO BASTA para os
        // DllImport (#184): o runtime procura "libnuvio.so" por CAMINHO nas
        // pastas nativas do app antes do nome nu, acha lib/libnuvio.so (outro
        // inode) e carrega a EMPACOTADA ao lado. Quem chama isto no 6+ tem de
        // chamar RotearDllImport com o handle devolvido. Confere o ponto de
        // entrada antes de dar por boa.
        public static IntPtr MemfdDlopen(byte[] elf, out string falha)
        {
            falha = null;
            IntPtr nome = Marshal.StringToHGlobalAnsi("nuvio");
            int fd;
            try { fd = syscall_memfd(SYS_memfd_create, nome, 0); }
            finally { Marshal.FreeHGlobal(nome); }
            if (fd < 0) { falha = "syscall(385) falhou (errno=" + Marshal.GetLastWin32Error() + ")"; return IntPtr.Zero; }

            GCHandle pin = GCHandle.Alloc(elf, GCHandleType.Pinned);
            try
            {
                IntPtr baseP = pin.AddrOfPinnedObject();
                int off = 0;
                while (off < elf.Length)
                {
                    int n = (int)write(fd, baseP + off, (IntPtr)(elf.Length - off));
                    if (n <= 0) { falha = "write falhou em " + off + "/" + elf.Length + " (errno=" + Marshal.GetLastWin32Error() + ")"; return IntPtr.Zero; }
                    off += n;
                }
            }
            finally { pin.Free(); }

            string path = "/proc/self/fd/" + fd;
            dlerror();
            IntPtr h = dlopen(path, RTLD_NOW | RTLD_GLOBAL);
            if (h == IntPtr.Zero) { falha = "dlopen(" + path + ") -> " + Err(); return IntPtr.Zero; }
            if (dlsym(h, "nv_tpk_iniciar") == IntPtr.Zero) { falha = "carregou mas nv_tpk_iniciar faltou"; return IntPtr.Zero; }
            memfdFd = fd;   // segura o fd viva a vida toda
            return h;
        }

        // ROTA DOS DllImport PARA A .so ENCENADA (#184). MEDIDO na S90D (Tizen 9)
        // e na UE75U8072F: o rastro do arranque diz "loaded staged lib by memfd" e
        // o C do MESMO processo imprime "instalada 1.6.1" (a empacotada), sessao
        // apos sessao — os DllImport("libnuvio.so") nao caiam na memfd. Com um
        // resolvedor por assembly, "libnuvio.so" devolve o handle da memfd antes
        // de qualquer busca do runtime. Por reflexao: NativeLibrary existe no
        // runtime das TVs 6+ (.NET Core 3.1 / .NET 6), mas nao na superficie de
        // tizen80/tizen90 para que estes pacotes compilam.
        static IntPtr rotaHandle;
        public static int RotaUsos;

        static MethodInfo MetodoRota(out Type tipo)
        {
            Assembly core = typeof(Marshal).Assembly;
            tipo = core.GetType("System.Runtime.InteropServices.DllImportResolver");
            Type nl = core.GetType("System.Runtime.InteropServices.NativeLibrary");
            return tipo == null || nl == null ? null : nl.GetMethod("SetDllImportResolver", BindingFlags.Public | BindingFlags.Static);
        }

        // true quando o runtime sabe rotear DllImport. Sem isso a encenada seria
        // carregada a toa (o app rodaria a empacotada), entao nem se tenta.
        public static bool PodeRotear()
        {
            try { return MetodoRota(out _) != null; } catch { return false; }
        }

        // Liga "libnuvio.so" dos DllImport de `asm` ao `handle`. Uma vez por
        // assembly (o runtime recusa a segunda). Antes do primeiro DllImport.
        public static bool RotearDllImport(Assembly asm, IntPtr handle, out string falha)
        {
            falha = null;
            try
            {
                MethodInfo set = MetodoRota(out Type tipo);
                if (set == null) { falha = "runtime sem NativeLibrary.SetDllImportResolver"; return false; }
                MethodInfo m = typeof(NvCarga).GetMethod(nameof(Resolver), BindingFlags.NonPublic | BindingFlags.Static);
                rotaHandle = handle;
                set.Invoke(null, new object[] { asm, Delegate.CreateDelegate(tipo, m) });
                return true;
            }
            catch (Exception e)
            {
                rotaHandle = IntPtr.Zero;
                var ie = e is TargetInvocationException && e.InnerException != null ? e.InnerException : e;
                falha = ie.GetType().Name + ": " + ie.Message;
                return false;
            }
        }

        // Nada de log aqui dentro: nv_tpk_log e ele mesmo um DllImport.
        static IntPtr Resolver(string nome, Assembly asm, DllImportSearchPath? caminho)
        {
            if (rotaHandle == IntPtr.Zero || nome != "libnuvio.so") return IntPtr.Zero;
            System.Threading.Interlocked.Increment(ref RotaUsos);
            return rotaHandle;
        }

        public static void ApagarStaged(string dados)
        {
            if (string.IsNullOrEmpty(dados)) return;
            foreach (var nome in new[] { ARQ_SO, ARQ_SHA, ARQ_VER, ARQ_DL })
                try { string p = Path.Combine(dados, nome); if (File.Exists(p)) File.Delete(p); } catch { }
        }

        // So depois de nv_tpk_iniciar confirmar o arranque. A copia empacotada
        // e a reserva; staging + pacote ja sao duas versoes, sem arquivo extra.
        public static void LimparAntigos(string dados)
        {
            if (string.IsNullOrEmpty(dados)) return;
            try
            {
                foreach (string p in Directory.GetFiles(dados, "libnuvio*.so"))
                {
                    string n = Path.GetFileName(p);
                    if (n == ARQ_SO || !(n.StartsWith("libnuvio-", StringComparison.Ordinal) ||
                                         n.StartsWith("libnuvio.previous", StringComparison.Ordinal))) continue;
                    try
                    {
                        File.Delete(p);
                        foreach (string ext in new[] { ".sha256", ".ver" })
                            File.Delete(Path.ChangeExtension(p, ext));
                    }
                    catch { }
                }
                File.Delete(Path.Combine(dados, ARQ_DL));
            }
            catch { } // Pasta sem permissao nunca impede o arranque.
        }

        static string Sha256Hex(string arquivo)
        {
            using (var s = File.OpenRead(arquivo))
            using (var h = SHA256.Create())
            {
                byte[] d = h.ComputeHash(s);
                var sb = new System.Text.StringBuilder(64);
                foreach (var b in d) sb.Append(b.ToString("x2"));
                return sb.ToString();
            }
        }

        static string Curto(string s) => string.IsNullOrEmpty(s) ? "?" : (s.Length > 12 ? s.Substring(0, 12) : s);

        static string Err()
        {
            IntPtr e = dlerror();
            string s = e == IntPtr.Zero ? null : Marshal.PtrToStringAnsi(e);
            return string.IsNullOrEmpty(s) ? "recusado sem mensagem" : s;
        }

        // "1.5.4" > "1.5.3"? Compara campo a campo; nao-numero vale 0. Equivale ao
        // maisNova() de src/atualizacao.c para versoes pontuadas normais.
        static bool MaisNova(string a, string b)
        {
            long[] pa = Partes(a), pb = Partes(b);
            int n = Math.Max(pa.Length, pb.Length);
            for (int i = 0; i < n; i++)
            {
                long va = i < pa.Length ? pa[i] : 0, vb = i < pb.Length ? pb[i] : 0;
                if (va != vb) return va > vb;
            }
            return false;
        }

        static long[] Partes(string s)
        {
            if (string.IsNullOrEmpty(s)) return new long[0];
            string[] br = s.Split('.');
            var r = new long[br.Length];
            for (int i = 0; i < br.Length; i++)
            {
                long v = 0; int k = 0;
                while (k < br[i].Length && br[i][k] >= '0' && br[i][k] <= '9') { v = v * 10 + (br[i][k] - '0'); k++; }
                r[i] = v;
            }
            return r;
        }
    }
}
