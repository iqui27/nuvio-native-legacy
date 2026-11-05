package space.nuvio.nativelegacy.audiosync

import android.util.Log

/**
 * Audio subtitle sync's log (ported from NuvioTV-Reshaped): logcat, and the app's own log through
 * [sink] (NvPlayer hands it to the C log, so the lines land in nuvio-log next to [legsync]). Links
 * are reduced to their host, since stream and subtitle URLs can carry account tokens; the log holds
 * timings and decisions, never audio or text.
 */
internal object SyncLog {
    private const val TAG = "NuvioAudioSync"
    // Userinfo (user:pass@) is dropped with the rest: only the host stays.
    private val url = Regex("""https?://(?:[^@/\s?#]*@)?([^/\s?#]+)[^\s]*""")

    @Volatile
    var sink: ((String) -> Unit)? = null

    fun i(message: String) = add('I', message).also { Log.i(TAG, it) }

    fun w(message: String) = add('W', message).also { Log.w(TAG, it) }

    fun d(message: String) {
        // Per-alignment search lines: logcat only, not the app log.
        Log.d(TAG, redact(message))
    }

    private fun redact(message: String) = url.replace(message) { "<${it.groupValues[1]}>" }

    private fun add(level: Char, message: String): String {
        val redacted = redact(message)
        runCatching { sink?.invoke("[audiosync] $level $redacted") }
        return redacted
    }
}
