package space.nuvio.nativelegacy

// Shared only by the Android UI and SDL threads. Cancelling a profile's
// request invalidates tasks, queued tokens and late activity results together.
internal class DriveAuthState {
    data class Ticket(val request: Long, val generation: Long)
    data class Result(val request: Long, val status: String, val token: String = "")
    private var generation = 0L
    private var active: Ticket? = null
    private var resolution: Ticket? = null
    private var result: Result? = null
    private var closed = false

    @Synchronized fun begin(request: Long): Ticket? {
        if (closed || request <= 0 || active != null || resolution != null || result != null) return null
        return Ticket(request, ++generation).also { active = it }
    }

    @Synchronized fun current(ticket: Ticket) = !closed && active == ticket

    @Synchronized fun waitForResolution(ticket: Ticket): Boolean {
        if (!current(ticket) || resolution != null) return false
        resolution = ticket
        return true
    }

    @Synchronized fun takeResolution(): Ticket? = resolution.also { resolution = null }

    @Synchronized fun finish(ticket: Ticket, status: String, token: String = "") {
        if (!current(ticket)) return
        active = null
        result = Result(ticket.request, status, token)
    }

    @Synchronized fun poll(): Result? = result.also { result = null }

    @Synchronized fun cancel() {
        ++generation
        active = null
        result = null
        // Keep a cancelled resolution reserved until its Activity result
        // returns; that old result must never satisfy a new request.
    }

    @Synchronized fun close() { cancel(); closed = true }
}
