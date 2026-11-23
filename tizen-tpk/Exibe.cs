// Borda das legendas do Nuvio .tpk (src/tpkborda.c): o C pede, pela funcao
// registrada em nv_tpk_premult_registrar, que o GLView diga ao DALi que o
// nosso RGB ja vem multiplicado pelo alfa, so enquanto o libass desenha. Ver
// Premult. So no host Tizen 6+ (Program.cs); o GLView so existe no pacote do
// 8/9.
using System;
using System.Reflection;
using System.Runtime.InteropServices;

namespace NuvioTpk
{
    static class Exibe
    {
        [DllImport("libnuvio.so")] static extern void nv_tpk_premult_registrar(IntPtr premult);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate void FnInt(int v);

        // Referencia viva: o C guarda o ponteiro, o GC nao pode recolher.
        static FnInt fPremult;
        static Func<object[]> janelas;

        public static void Registrar(Action<Action> post, Func<object[]> windows)
        {
            janelas = windows;
            fPremult = v => post(() => Premult(v != 0));
            // Lib encenada mais velha que o host: sem o export, sem o conserto (nada quebra).
            try { nv_tpk_premult_registrar(Marshal.GetFunctionPointerForDelegate(fPremult)); }
            catch (EntryPointNotFoundException) { }
        }

        // Thread principal (post). BORDA ESCURA (24/10): o nosso GL escreve sobre o
        // furo transparente com blend SRC_ALPHA, entao o RGB ja sai multiplicado
        // pelo alfa; o DALi compoe o GLView como alfa "reto" e multiplica de novo:
        // toda borda suave da legenda escurecia (contorno preto das mascaras).
        // BlendPreMultipliedAlpha no Renderer do GLView conserta (provado na S95C),
        // mas muda a interface inteira, entao vale so enquanto o libass desenha.
        // Off devolve o valor que o renderer tinha. Reflexao: TizenFX sem a
        // propriedade = nao faz nada.
        static object premRend, premOrig;
        static PropertyInfo premProp;
        static bool premProcurado, premLigado;
        static void Premult(bool on)
        {
            try
            {
                if (!premProcurado)
                {
                    premProcurado = true;
                    object view = null;
                    foreach (var w in janelas != null ? janelas() : new object[0])
                        if (w != null && w.GetType().Name == "GLView") view = w;
                    if (view == null) return;   // only the Tizen 8/9 package draws in a GLView
                    var t = view.GetType();
                    var cnt = t.GetProperty("RendererCount");
                    var get = t.GetMethod("GetRendererAt", new[] { typeof(uint) });
                    if (cnt == null || get == null || Convert.ToUInt32(cnt.GetValue(view)) == 0)
                        return;
                    object r = get.Invoke(view, new object[] { 0u });
                    var prop = r != null ? r.GetType().GetProperty("BlendPreMultipliedAlpha") : null;
                    if (prop == null || !prop.CanWrite) return;
                    premRend = r; premProp = prop; premOrig = prop.GetValue(r);
                }
                if (premProp == null || on == premLigado) return;
                premProp.SetValue(premRend, on ? (object)true : premOrig);
                premLigado = on;
            }
            catch { premProp = null; }
        }
    }
}
