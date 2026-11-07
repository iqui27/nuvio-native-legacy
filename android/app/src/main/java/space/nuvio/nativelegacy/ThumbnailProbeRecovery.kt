package space.nuvio.nativelegacy

internal class ThumbnailProbeRecovery {
    private var failures = 0
    private var successes = 0
    private var until = 0L
    private var disabled = false
    fun timeout(now: Long) {
        failures++; successes = 0; disabled = true
        until = now + minOf(240_000L, 60_000L shl minOf(2, failures - 1))
    }
    fun success() { if (disabled) successes++ }
    fun allowed(now: Long): Boolean {
        // At most three recoveries per source; repeated failure cannot probe forever.
        if (disabled && failures <= 3 && now >= until && successes >= 5) disabled = false
        return !disabled
    }
}
