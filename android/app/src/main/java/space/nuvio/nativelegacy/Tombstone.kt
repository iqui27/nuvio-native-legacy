package space.nuvio.nativelegacy

// ONDE O PROCESSO ANTERIOR MORREU, a partir do tombstone que o Android 12+
// guarda junto do ApplicationExitInfo de um crash nativo
// (getTraceInputStream, formato protobuf de system/core/debuggerd/proto/
// tombstone.proto). A 2.0.1 tinha 21 sessoes "crash-nativo(5) status=11" no
// D1 (Skyworth e Formuler com Mali-G57, Xiaomi com Mali-G310) e nenhuma linha
// dizendo onde: o log do app para no meio de um envio de textura e so. O
// queda.c da LG nao serve aqui (o processo e da ART, que usa SIGSEGV por conta
// propria); o tombstone e o relato do proprio debuggerd, com a pilha resolvida.
//
// Le so o que interessa e devolve linhas "[queda] ..." prontas para o log (o
// C as imprime, android.c). Um leitor de protobuf minimo, sem dependencia: um
// campo desconhecido ou um arquivo truncado so encurta o relato.
object Tombstone {
    private const val MAX_BYTES = 8 * 1024 * 1024
    private const val MAX_QUADROS = 20

    // Campo -> valores, na ordem em que aparecem. Varint e fixos viram Long;
    // os delimitados ficam como ByteArray (string ou mensagem, quem le decide).
    private class Msg(val campos: Map<Int, List<Any>>) {
        fun long(n: Int): Long? = campos[n]?.firstOrNull() as? Long
        fun bytes(n: Int): ByteArray? = campos[n]?.firstOrNull() as? ByteArray
        fun str(n: Int): String = bytes(n)?.let { String(it, Charsets.UTF_8) } ?: ""
        fun msg(n: Int): Msg? = bytes(n)?.let { ler(it) }
        fun todas(n: Int): List<ByteArray> = campos[n]?.filterIsInstance<ByteArray>() ?: emptyList()
    }

    private fun ler(b: ByteArray): Msg {
        val campos = HashMap<Int, MutableList<Any>>()
        var i = 0
        fun varint(): Long {
            var r = 0L; var s = 0
            while (i < b.size && s < 64) {
                val x = b[i++].toInt() and 0xff
                r = r or ((x and 0x7f).toLong() shl s)
                if (x and 0x80 == 0) return r
                s += 7
            }
            throw IllegalStateException("varint")
        }
        try {
            while (i < b.size) {
                val chave = varint()
                val campo = (chave ushr 3).toInt()
                val v: Any = when ((chave and 7).toInt()) {
                    0 -> varint()
                    1 -> { if (i + 8 > b.size) break; var r = 0L; for (k in 0 until 8) r = r or ((b[i + k].toLong() and 0xff) shl (8 * k)); i += 8; r }
                    2 -> { val n = varint(); if (n < 0 || i + n > b.size) break; val c = b.copyOfRange(i, i + n.toInt()); i += n.toInt(); c }
                    5 -> { if (i + 4 > b.size) break; var r = 0L; for (k in 0 until 4) r = r or ((b[i + k].toLong() and 0xff) shl (8 * k)); i += 4; r }
                    else -> break
                }
                campos.getOrPut(campo) { ArrayList() }.add(v)
            }
        } catch (_: Exception) { }
        return Msg(campos)
    }

    private fun hex(v: Long) = "0x" + java.lang.Long.toHexString(v)
    private fun base(caminho: String) = caminho.substringAfterLast('/')

    // Tombstone: tid=6, process_uptime=20, signal_info=10, abort_message=14,
    // causes=15, threads=16 (map<uint32, Thread>: entrada com chave 1, valor 2).
    // Signal: name=2, code_name=4, has_fault_address=8, fault_address=9.
    // Thread: id=1, name=2, current_backtrace=4.
    // BacktraceFrame: rel_pc=1, function_name=4, function_offset=5, file_name=6.
    fun resumo(bruto: ByteArray): List<String> {
        val t = ler(bruto)
        val out = ArrayList<String>()
        val tid = t.long(6)
        val sinal = t.msg(10)
        val partes = StringBuilder("[queda] tombstone:")
        if (sinal != null) {
            partes.append(" sinal ").append(sinal.str(2).ifEmpty { "?" })
            sinal.str(4).takeIf { it.isNotEmpty() }?.let { partes.append(" (").append(it).append(')') }
            if ((sinal.long(8) ?: 0L) != 0L) partes.append(" endereco ").append(hex(sinal.long(9) ?: 0L))
        }
        t.long(20)?.let { partes.append(" aberto ha ").append(it).append(" s") }
        out.add(partes.toString())
        t.str(14).takeIf { it.isNotBlank() }?.let { out.add("[queda] abort: " + it.replace('\n', ' ').take(300)) }
        for (c in t.todas(15).take(3)) {
            ler(c).str(1).takeIf { it.isNotBlank() }?.let { out.add("[queda] causa: " + it.replace('\n', ' ').take(300)) }
        }
        // O fio que caiu e o de id == tid; sem tid (ou sem casar), o primeiro.
        var fio: Msg? = null
        var casou = false
        for (e in t.todas(16)) {
            val ent = ler(e)
            val th = ent.msg(2) ?: continue
            if (fio == null) fio = th
            if (tid != null && (th.long(1) ?: -1L) == tid) { fio = th; casou = true; break }
        }
        if (fio != null) {
            out.add("[queda] fio \"" + fio.str(2) + "\" tid=" + (fio.long(1) ?: 0L) +
                (if (casou) "" else " (nao e o fio que caiu: tombstone sem ele)"))
            fio.todas(4).take(MAX_QUADROS).forEachIndexed { k, q ->
                val f = ler(q)
                val nome = f.str(4)
                val func = if (nome.isNotEmpty()) " ($nome+${f.long(5) ?: 0L})" else ""
                out.add("[queda] #%02d %s+%s%s".format(k, base(f.str(6)).ifEmpty { "?" }, hex(f.long(1) ?: 0L), func))
            }
        } else out.add("[queda] tombstone sem a pilha do fio que caiu")
        return out
    }

    // O ApplicationExitInfo de um crash nativo -> linhas, ou vazio.
    fun doExitInfo(r: android.app.ApplicationExitInfo): List<String> {
        if (android.os.Build.VERSION.SDK_INT < 31) return emptyList()
        return try {
            val ent = r.traceInputStream ?: return listOf("[queda] o Android nao guardou tombstone deste crash")
            val bruto = ent.use { s ->
                val buf = java.io.ByteArrayOutputStream()
                val b = ByteArray(16384)
                while (buf.size() < MAX_BYTES) { val n = s.read(b); if (n < 0) break; buf.write(b, 0, n) }
                buf.toByteArray()
            }
            // Alguns fabricantes gravam o tombstone comprimido.
            val dados = if (bruto.size > 2 && bruto[0] == 0x1f.toByte() && bruto[1] == 0x8b.toByte())
                java.util.zip.GZIPInputStream(bruto.inputStream()).use { it.readBytes() } else bruto
            resumo(dados)
        } catch (e: Exception) {
            listOf("[queda] tombstone ilegivel: " + e.javaClass.simpleName)
        }
    }
}
