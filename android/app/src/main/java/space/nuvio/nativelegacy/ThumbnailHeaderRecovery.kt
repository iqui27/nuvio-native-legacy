package space.nuvio.nativelegacy

/** Only an EOF before any response headers on a reused connection is eligible. */
internal fun thumbnailCanRecoverHeaderEof(error: Throwable,acquired: Boolean,reused: Boolean,
    sent: Boolean,headers: Boolean,cancelled: Boolean): Boolean {
    if(!acquired || !reused || !sent || headers || cancelled) return false
    var cause: Throwable?=error
    repeat(8) {
        if(cause is java.io.EOFException) return true
        cause=cause?.cause
    }
    return false
}
