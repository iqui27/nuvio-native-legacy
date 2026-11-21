// Replay da ABI do OnUpdate(IntPtr,IntPtr) no TPK40 (#754).
//
// No runtime 5.5 (GLAppFW::GLApplication::Run) os dois argumentos do callback
// sao os ENDERECOS das variaveis de pilha que guardam EGLDisplay/EGLSurface —
// lea nos dois, nao mov. O host tem de desreferencia-los (Marshal.ReadIntPtr)
// e so chamar eglSwapBuffers quando os VALORES sao != EGL_NO_DISPLAY /
// EGL_NO_SURFACE (ambos zero); "ok first-frame" so vale se o swap devolver
// EGL_TRUE. Este runner simula exatamente isso contra o corpo real do
// OnUpdate extraido de tizen-tpk/NuvioTpk40/Program40.cs (ver Host.cs gerado).
using System;
using System.Runtime.InteropServices;

int falhas = 0;
void Checa(bool ok, string nome)
{
    Console.WriteLine((ok ? "ok   " : "FALHA") + " " + nome);
    if (!ok) falhas++;
}

var host = new Host();
IntPtr slotDisplay = Marshal.AllocHGlobal(IntPtr.Size);
IntPtr slotSurface = Marshal.AllocHGlobal(IntPtr.Size);
try
{
    Marshal.WriteIntPtr(slotDisplay, new IntPtr(0xD1));
    Marshal.WriteIntPtr(slotSurface, new IntPtr(0x51));

    // r=0: nada troca, callback segue vivo.
    NvLib.Quadro = () => 0;
    host.OnUpdate(slotDisplay, slotSurface);
    Checa(host.Swaps == 0, "r=0 nao chama eglSwapBuffers");

    // r=1 com display/superficie validos: swap com os VALORES, nao os ponteiros.
    NvLib.Quadro = () => 1;
    host.OnUpdate(slotDisplay, slotSurface);
    Checa(host.Swaps == 1, "r=1 chama eglSwapBuffers uma vez");
    Checa(host.InvalidSwaps == 0, "r=1 passa os handles EGL (0xd1/0x51), nao os enderecos");
    Checa(host.FirstFrameLogged, "primeiro swap com EGL_TRUE registra ok first-frame");

    // Superficie zero dentro de um endereco valido: nao chama swap.
    host.Swaps = 0; host.InvalidSwaps = 0;
    Marshal.WriteIntPtr(slotSurface, IntPtr.Zero);
    host.OnUpdate(slotDisplay, slotSurface);
    Checa(host.Swaps == 0, "EGLSurface=0 num endereco valido nao chama swap");

    // Display zero idem.
    Marshal.WriteIntPtr(slotSurface, new IntPtr(0x51));
    Marshal.WriteIntPtr(slotDisplay, IntPtr.Zero);
    host.OnUpdate(slotDisplay, slotSurface);
    Checa(host.Swaps == 0, "EGLDisplay=0 nao chama swap");
    Marshal.WriteIntPtr(slotDisplay, new IntPtr(0xD1));

    // Swap com EGL_FALSE nao grava ok first-frame.
    host.Swaps = 0; host.FirstFrameLogged = false; host.primeiroQuadro = false;
    Host.SwapRet = 0;
    host.OnUpdate(slotDisplay, slotSurface);
    Checa(host.Swaps == 1, "r=1 tenta o swap mesmo sem primeiroQuadro");
    Checa(!host.FirstFrameLogged, "EGL_FALSE nao registra ok first-frame");
}
finally
{
    Marshal.FreeHGlobal(slotSurface);
    Marshal.FreeHGlobal(slotDisplay);
}

Console.WriteLine(falhas == 0 ? "PASS: swap recebe os handles EGL e superficie zero nao apresenta"
                              : $"FALHOU: {falhas} verificacoes");
return falhas;
