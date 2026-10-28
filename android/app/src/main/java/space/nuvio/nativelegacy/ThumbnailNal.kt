package space.nuvio.nativelegacy

import java.nio.ByteBuffer

internal object ThumbnailNal {
    data class Result(val inputBytes: Int,val outputBytes: Int,val removedBytes: Int,val removedNals: Int,val visitedNals: Int)
    fun write(sample: ByteArray, lengthBytes: Int, out: ByteBuffer): Boolean =
        writeUnit(sample,lengthBytes,out,0,sample.size,false)!=null
    fun writeBaseLayer(sample: ByteArray,lengthBytes: Int,out: ByteBuffer,start: Int=0,end: Int=sample.size): Result? =
        writeUnit(sample,lengthBytes,out,start,end,true)
    /** Validate by lengths and headers only, then bulk-copy retained NALs. No payload-byte scan. */
    private fun writeUnit(sample: ByteArray,lengthBytes: Int,out: ByteBuffer,start: Int,end: Int,base: Boolean): Result? {
        if(lengthBytes !in intArrayOf(1,2,4) || start<0 || end>sample.size || start>=end) return null
        fun length(p: Int): Long {
            var n=0L
            repeat(lengthBytes) { n=(n shl 8) or (sample[p+it].toLong() and 255) }
            return n
        }
        fun keep(p: Int): Boolean {
            if(!base) return true
            val h0=sample[p].toInt() and 255; val h1=sample[p+1].toInt() and 255
            val layer=((h0 and 1) shl 5) or (h1 shr 3)
            return layer==0 && ((h0 shr 1) and 63) !in 62..63
        }
        var p=start; var needed=0L; var removed=0; var removedBytes=0; var count=0
        while(p<end) {
            if(end-p<lengthBytes) return null
            val n=length(p); p+=lengthBytes
            if(n<=0 || n>end-p || (base && (++count>4096 || n<2))) return null
            if(base && ((sample[p].toInt() and 128)!=0 || (sample[p+1].toInt() and 7)==0)) return null
            if(keep(p)) needed+=4+n else { removed++; removedBytes+=lengthBytes+n.toInt() }
            if(needed>out.remaining()) return null
            p+=n.toInt()
        }
        if(needed==0L) return null
        p=start
        while(p<end) {
            val n=length(p).toInt(); p+=lengthBytes
            if(keep(p)) { out.put(0).put(0).put(0).put(1); out.put(sample,p,n) }
            p+=n
        }
        return Result(end-start,needed.toInt(),removedBytes,removed,count)
    }
}
