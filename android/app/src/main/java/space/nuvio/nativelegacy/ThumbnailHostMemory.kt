package space.nuvio.nativelegacy

import android.util.Log
import java.io.File

// WHAT A SERVER TOLD US, REMEMBERED ACROSS SESSIONS (06/11/2026, after Nuvio Arc's MultiRangeSupport and
// ConnectionLedger). Keyed by service: the last two labels of the host name ("r2.cloudflarestorage.com" buckets share
// "cloudflarestorage.com"), three under a short country second level ("x.co.uk"). Each entry expires after its
// window, so a server that changes is tried again.
//   multi-range refusals (Cloudflare R2: HTTP 400 to a multi-range GET): a new title skips the first request that
//     used to find it out again; only refusals are stored, a host that serves multipart needs no memory.
//   thumbnail lane caps (R2: HTTP 403 once 8 thumbnail lanes + the player's 4 connections were open): the next title
//     starts at the cap instead of tripping the limit again. Caps only go DOWN from ThumbnailSingleLanes.LANES_OWN.
internal class ThumbnailHostMemory(private val file: File?, private val retryMs: Long = RETRY_MS) {
    private class Entry(val at: Long, val value: Int)
    private val entries = HashMap<String, Entry>()
    private var loaded = false

    /** The value remembered for [host]'s service, or null (none, or expired). */
    @Synchronized fun value(host: String?, now: Long): Int? {
        val k = key(host) ?: return null
        load()
        val e = entries[k] ?: return null
        if (now - e.at in 0 until retryMs) return e.value
        entries.remove(k); save()
        return null
    }

    @Synchronized fun record(host: String?, now: Long, value: Int = 0, what: String = "") {
        val k = key(host) ?: return
        load()
        val before = entries[k]
        entries[k] = Entry(now, value)
        if (before == null || before.value != value)
            Log.i("nuvio-miniatura", "[host-memory] $k $what: remembered for ${retryMs / 86_400_000} days")
        save()
    }

    fun refuses(host: String?, now: Long) = value(host, now) != null
    fun recordRefusal(host: String?, now: Long) = record(host, now, 0, "refuses multi-range")

    private fun load() {
        if (loaded) return
        loaded = true
        try {
            file?.takeIf { it.isFile }?.readLines()?.forEach { line ->
                val p = line.split('\t')
                val at = p.getOrNull(1)?.toLongOrNull() ?: return@forEach
                if (p.size == 2) entries[p[0]] = Entry(at, 0)
                else if (p.size == 3) p[2].toIntOrNull()?.let { entries[p[0]] = Entry(at, it) }
            }
        } catch (_: Exception) { }
    }

    private fun save() {
        val f = file ?: return
        try {
            val tmp = File(f.parentFile, f.name + ".tmp")
            tmp.writeText(entries.entries.joinToString("") { "${it.key}\t${it.value.at}\t${it.value.value}\n" })
            if (!tmp.renameTo(f)) { f.delete(); tmp.renameTo(f) }
        } catch (_: Exception) { }
    }

    companion object {
        const val RETRY_MS = 7L * 86_400_000L
        const val LANE_CAP_MS = 3L * 86_400_000L

        /** The service a host belongs to: its last two labels, or the whole name when it has fewer / is an address. */
        fun key(host: String?): String? {
            val h = host?.trim()?.lowercase()?.trimEnd('.')?.takeIf { it.isNotEmpty() } ?: return null
            if (h.all { it.isDigit() || it == '.' } || ':' in h) return h
            val labels = h.split('.')
            // "x.co.uk" / "x.com.br": a short second-level label under a country code takes three labels
            val n = if (labels.size >= 3 && labels.last().length == 2 && labels[labels.size - 2].length <= 3) 3 else 2
            return if (labels.size <= n) h else labels.takeLast(n).joinToString(".")
        }

        // Created once the app context exists (a memory-only instance before that would never persist).
        @Volatile private var refusals: ThumbnailHostMemory? = null
        @Volatile private var caps: ThumbnailHostMemory? = null
        private fun refusals(): ThumbnailHostMemory? = refusals ?: synchronized(this) {
            refusals ?: NvPlayer.contexto()?.filesDir?.let { ThumbnailHostMemory(File(it, "thumbnail-multirange-refused.txt")) }
                ?.also { refusals = it }
        }
        private fun caps(): ThumbnailHostMemory? = caps ?: synchronized(this) {
            caps ?: NvPlayer.contexto()?.filesDir?.let { ThumbnailHostMemory(File(it, "thumbnail-lane-caps.txt"), LANE_CAP_MS) }
                ?.also { caps = it }
        }
        fun refuses(host: String?) = refusals()?.refuses(host, System.currentTimeMillis()) == true
        fun recordRefusal(host: String?) { refusals()?.recordRefusal(host, System.currentTimeMillis()) }
        fun laneCap(host: String?): Int? = caps()?.value(host, System.currentTimeMillis())
        fun recordLaneCap(host: String?, cap: Int) {
            caps()?.record(host, System.currentTimeMillis(), cap, "allows $cap thumbnail lanes")
        }
    }
}
