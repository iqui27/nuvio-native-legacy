package space.nuvio.nativelegacy

import java.io.InputStream

// Times actual underlying stream reads, including EOF/failed reads. Wrap before
// buffering so multipart header parsing does not time every byte separately.
internal class ThumbnailReadTiming(
    private val input: InputStream,
    private val now: () -> Long = System::nanoTime,
) : InputStream() {
    var readNanos = 0L
        private set
    var bytesRead = 0L
        private set
    var firstReadNanos: Long? = null
        private set
    private fun measured(read: () -> Int, single: Boolean): Int {
        val start = now()
        try {
            val count = read()
            val end = now()
            if (count >= 0 && (single || count > 0)) {
                bytesRead += if (single) 1 else count
                if (firstReadNanos == null) firstReadNanos = end
            }
            return count
        } finally {
            readNanos += now() - start
        }
    }
    override fun read(): Int = measured({ input.read() }, true)
    override fun read(buffer: ByteArray, offset: Int, length: Int): Int =
        measured({ input.read(buffer, offset, length) }, false)
    override fun close() = input.close()
}
