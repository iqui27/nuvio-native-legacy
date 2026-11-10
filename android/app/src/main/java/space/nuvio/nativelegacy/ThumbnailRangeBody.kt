package space.nuvio.nativelegacy

import java.io.IOException
import java.io.InputStream

internal object ThumbnailRangeBody {
    fun read(input: InputStream, contentLength: Long, contentRange: String?, start: Long, end: Long): ByteArray {
        val match = Regex("^bytes\\s+(\\d+)-(\\d+)/(?:\\d+|\\*)$", RegexOption.IGNORE_CASE)
            .matchEntire(contentRange?.trim() ?: "") ?: throw IOException("invalid content range")
        val first = match.groupValues[1].toLongOrNull() ?: throw IOException("invalid content range")
        val last = match.groupValues[2].toLongOrNull() ?: throw IOException("invalid content range")
        if (first != start || last < first || last > end || last - first >= (16L shl 20))
            throw IOException("range not honoured")
        val size = (last - first + 1).toInt()
        if (contentLength >= 0 && contentLength != size.toLong()) throw IOException("invalid content length")
        val data = ByteArray(size)
        var got = 0
        while (got < size) {
            val n = input.read(data, got, size - got)
            if (n <= 0) throw IOException("short read")
            got += n
        }
        if (contentLength < 0 && input.read() != -1) throw IOException("oversized response")
        return data
    }
}
