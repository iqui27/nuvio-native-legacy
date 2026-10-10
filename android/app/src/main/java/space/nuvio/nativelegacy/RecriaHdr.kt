package space.nuvio.nativelegacy

// QUEM NAO RECRIA A SUPERFICIE NO HDR. A recriacao do primeiro quadro HDR (e a
// segunda, 1,5 s depois) existe por causa do painel da TCL (MediaTek), que so
// liga o modo HDR quando a Surface nasce com o decoder ja em HDR. Em outras
// familias ela so custa:
// - MStar (OMX.MS.*, caixas Shinon): o hwcomposer refaz o overlay a cada
//   GONE/VISIBLE e a tela pisca/trava.
// - Amlogic (OMX.amlogic.*, c2.amlogic.*: Mecool, caixas de 2 GB): o HDR e da
//   saida HDMI, nao do painel. Na KM7 SE (Reddit, 2.0.3) as duas sessoes HDR10
//   recriaram a superficie duas vezes no inicio e cairam dentro da ART (GC e
//   AudioTrack.getTimestamp no fio do ExoPlayer) com memoria critica; a sessao
//   SDR, sem recriacao, nao caiu (registros D1 66008, 66017, 65445).
// Devolve o nome da familia dispensada, ou null para recriar.
object RecriaHdr {
    fun dispensa(decoders: List<String>): String? {
        if (decoders.any { it.startsWith("OMX.MS.") }) return "MStar"
        if (decoders.any { it.startsWith("OMX.amlogic.") || it.startsWith("c2.amlogic.") }) return "Amlogic"
        return null
    }
}
