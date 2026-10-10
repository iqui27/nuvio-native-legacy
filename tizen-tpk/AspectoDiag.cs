// Compiled ONLY with NV_ASPECTO_DIAG. The normal player never calls these APIs.
// Native declarations match Samsung/TizenFX Interop.Display.cs / Interop.Player.cs.
#if NV_ASPECTO_DIAG
using System;
using System.Globalization;
using System.Runtime.InteropServices;
using System.Text;
using Tizen.Multimedia;

namespace NuvioTpk
{
    partial class Video
    {
        [DllImport("libcapi-media-player.so.0")] static extern int player_set_display_mode(IntPtr p, PlayerDisplayMode mode);
        [DllImport("libcapi-media-player.so.0")] static extern int player_set_display_roi_area(IntPtr p, int x, int y, int w, int h);
        // Since Tizen 5; P/Invoke avoids referencing a missing managed member on API4.
        [DllImport("libcapi-media-player.so.0")] static extern int player_set_video_roi_area(IntPtr p, double x, double y, double w, double h);

        int aspectoDiag = -1;
        bool aspectoFonteSuja;
        string aspectoTizen = "?", aspectoModelo = "?";
        static readonly string[] AspectoMetodos = { "A:DisplayMode", "B:DisplayRoi", "C:Window", "D:VideoRoi" };
        static readonly string[] AspectoModos = { "Original", "Fill", "Zoom" };

        void AspectoInfo()
        {
            try { Tizen.System.Information.TryGetValue<string>("http://tizen.org/feature/platform.version", out aspectoTizen); } catch { }
            try { Tizen.System.Information.TryGetValue<string>("http://tizen.org/system/model_name", out aspectoModelo); } catch { }
        }

        // One line per request, including every native rc, even when a previous
        // call failed. No fallback: a failed method must not masquerade as another.
        bool AspectoChamada(StringBuilder log, string nome, Func<int> chamada)
        {
            try { int rc = chamada(); log.Append(" ").Append(nome).Append("=0x").Append(rc.ToString("x8")); return rc == 0; }
            catch (Exception e) { log.Append(" ").Append(nome).Append("=").Append(e.GetType().Name).Append(":").Append(e.Message.Replace('\n', ' ').Replace('\r', ' ')); return false; }
        }

        bool AspectoJanela(StringBuilder log, int x, int y, int w, int h)
        {
            if (GeometriaJanela == null) { log.Append(" window=unavailable"); return false; }
            try {
                string erro = GeometriaJanela(x, y, w, h);
                log.Append(" window(").Append(x).Append(',').Append(y).Append(',').Append(w).Append(',').Append(h)
                   .Append(")=").Append(erro == null ? "void:accepted" : erro.Replace('\n', ' ').Replace('\r', ' '));
                // ElmSharp.Geometry wraps void move/resize calls: no numeric rc.
                janelaMovida = true;
                return erro == null;
            } catch (Exception e) { log.Append(" window=").Append(e.GetType().Name); return false; }
        }

        bool AspectoLimpar(StringBuilder log)
        {
            bool ok = true;
            if (aspectoFonteSuja && player != null) {
                bool reset = AspectoChamada(log, "source-reset", () => player_set_video_roi_area(player.Handle, 0, 0, 1, 1));
                if (reset) aspectoFonteSuja = false;
                ok &= reset;
            }
            if (GeometriaJanela != null && janelaMovida) {
                bool reset = AspectoJanela(log, 0, 0, telaW, telaH);
                if (reset) janelaMovida = false;
                ok &= reset;
            }
            return ok;
        }

        void AspectoAplicar(int pedido)
        {
            int metodo = pedido / 3, modo = pedido % 3;
            if (pedido < -1 || pedido > 11) return;
            var log = new StringBuilder("[aspecto-diag] tizen=" + aspectoTizen + " model=" + aspectoModelo);
            log.Append(" method=").Append(pedido < 0 ? "OFF" : AspectoMetodos[metodo]);
            log.Append(" mode=").Append(pedido < 0 ? "restore" : AspectoModos[modo]);
            bool ok = false;
            aspectoDiag = pedido;
            try {
                if (player == null) { log.Append(" video=? player=none"); return; }
                log.Append(" video-query=GetVideoProperties");
                var size = player.StreamInfo.GetVideoProperties().Size;
                log.Append(" video=").Append(size.Width).Append('x').Append(size.Height).Append(" state=").Append(player.State);
                bool clean = AspectoLimpar(log);
                if (!clean) { log.Append(" reset=failed;reopen-video"); return; }
                if (pedido < 0) { ok = AspectoChamada(log, "mode:LetterBox", () => player_set_display_mode(player.Handle, PlayerDisplayMode.LetterBox)); return; }
                if (size.Width <= 0 || size.Height <= 0) { log.Append(" dimensions=unknown;retry"); return; }
                // Same contain/fill/cover target for all methods. Zoom means
                // centered cover, not the production player's additional 1.15x.
                double scale = modo == 2 ? Math.Max((double)telaW / size.Width, (double)telaH / size.Height)
                                         : Math.Min((double)telaW / size.Width, (double)telaH / size.Height);
                int w = modo == 1 ? telaW : (int)Math.Round(size.Width * scale);
                int h = modo == 1 ? telaH : (int)Math.Round(size.Height * scale);
                int x = (telaW - w) / 2, y = (telaH - h) / 2;
                log.Append(" target=").Append(x).Append(',').Append(y).Append(',').Append(w).Append(',').Append(h);
                if (metodo == 0) {
                    var mode = modo == 0 ? PlayerDisplayMode.LetterBox : modo == 1 ? PlayerDisplayMode.FullScreen : PlayerDisplayMode.CroppedFull;
                    ok = AspectoChamada(log, "mode:" + mode, () => player_set_display_mode(player.Handle, mode));
                } else if (metodo == 1) {
                    ok = AspectoChamada(log, "mode:Roi", () => player_set_display_mode(player.Handle, PlayerDisplayMode.Roi));
                    ok &= AspectoChamada(log, "display-roi", () => player_set_display_roi_area(player.Handle, x, y, w, h));
                } else if (metodo == 2) {
                    ok = AspectoJanela(log, x, y, w, h);
                    if (ok) ok = AspectoChamada(log, "mode:FullScreen", () => player_set_display_mode(player.Handle, PlayerDisplayMode.FullScreen));
                } else {
                    int major;
                    if (!int.TryParse((aspectoTizen ?? "").Split('.')[0], out major) || major < 5) { log.Append(" source-roi=unavailable:requires-Tizen5"); return; }
                    var mode = modo == 0 ? PlayerDisplayMode.LetterBox : PlayerDisplayMode.FullScreen;
                    ok = AspectoChamada(log, "mode:" + mode, () => player_set_display_mode(player.Handle, mode));
                    double sw = modo == 2 ? Math.Min(1.0, (double)telaW / w) : 1;
                    double sh = modo == 2 ? Math.Min(1.0, (double)telaH / h) : 1;
                    aspectoFonteSuja = true; // reset even if a driver partially applies then fails
                    ok &= AspectoChamada(log, "source-roi", () => player_set_video_roi_area(player.Handle, (1-sw)/2, (1-sh)/2, sw, sh));
                    log.AppendFormat(CultureInfo.InvariantCulture, " source={0:F6},{1:F6},{2:F6},{3:F6}", (1-sw)/2, (1-sh)/2, sw, sh);
                }
            } catch (Exception e) { log.Append(" exception=").Append(e.GetType().Name).Append(':').Append(e.Message.Replace('\n', ' ').Replace('\r', ' ')); }
            finally {
                log.Append(" accepted=").Append(ok ? "yes" : "no");
                Log(log.ToString());
                NvVid.Evento(9, pedido, ok ? 1 : 0);
            }
        }
    }
}
#endif
