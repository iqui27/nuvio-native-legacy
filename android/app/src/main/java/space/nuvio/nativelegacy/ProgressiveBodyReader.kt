package space.nuvio.nativelegacy

import java.io.ByteArrayOutputStream
import java.io.IOException
import java.io.InputStream

/** Returned playback chunks own their arrays. No pooling across async consumers. */
internal object ProgressiveBodyReader {
    fun read(input: InputStream, declaredLength: Long, limit: Long, initialCapacity: Int,
             checkCancelled: () -> Unit, transferred: (Int) -> Unit,
             allocate: (Int) -> ByteArray = { ByteArray(it) }): ByteArray {
        checkCancelled()
        val length = when {
            limit >= 0 && declaredLength >= 0 -> minOf(limit,declaredLength)
            limit >= 0 -> limit
            declaredLength >= 0 -> declaredLength
            else -> -1L
        }
        if(length >= 0) {
            if(length>Int.MAX_VALUE) throw IOException("HTTP chunk exceeds array bounds")
            val result=allocate(length.toInt())
            var used=0
            while(used<result.size) {
                checkCancelled()
                val n=input.read(result,used,minOf(65536,result.size-used))
                checkCancelled()
                if(n<0)break
                if(n==0)throw IOException("HTTP body made no progress")
                used+=n;transferred(n)
            }
            // Preserve short-body behaviour; callers decide whether to retry.
            return if(used==result.size) result else result.copyOf(used)
        }
        // Unknown-length bodies retain the existing growable-buffer path.
        val output=ByteArrayOutputStream(initialCapacity)
        val temporary=ByteArray(65536)
        while(true) {
            checkCancelled()
            val n=input.read(temporary)
            checkCancelled()
            if(n<0)break
            if(n==0)throw IOException("HTTP body made no progress")
            output.write(temporary,0,n);transferred(n)
        }
        return output.toByteArray()
    }
}
