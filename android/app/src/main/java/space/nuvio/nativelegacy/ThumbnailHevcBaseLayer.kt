package space.nuvio.nativelegacy

import java.nio.ByteBuffer

/** Filter a validated Annex B access unit in place. Never rewrites SPS/DPB requirements. */
internal object ThumbnailHevcBaseLayer {
    data class Result(val inputBytes: Int,val outputBytes: Int,val removedBytes: Int,val removedNals: Int)
    fun filter(buffer: ByteBuffer): Result? {
        val end=buffer.limit()
        fun prefix(p: Int): Int = when {
            p+4<=end && buffer.get(p)==0.toByte() && buffer.get(p+1)==0.toByte() && buffer.get(p+2)==0.toByte() && buffer.get(p+3)==1.toByte() -> 4
            p+3<=end && buffer.get(p)==0.toByte() && buffer.get(p+1)==0.toByte() && buffer.get(p+2)==1.toByte() -> 3
            else -> 0
        }
        data class Nal(val start: Int,val end: Int,val keep: Boolean)
        val units=ArrayList<Nal>()
        var p=0; var kept=0
        while(p<end) {
            if(units.size>=4096) return null
            val n=prefix(p); if(n==0 || p+n+2>end) return null
            val h0=buffer.get(p+n).toInt() and 255; val h1=buffer.get(p+n+1).toInt() and 255
            if(h0 and 128 != 0 || h1 and 7 == 0) return null
            var next=p+n+2
            while(next<end && prefix(next)==0) next++
            val layer=((h0 and 1) shl 5) or (h1 shr 3)
            val type=(h0 shr 1) and 63
            val keep=layer==0 && type !in 62..63 // Dolby Vision private NALs are unused by the HDR base-layer decoder.
            units.add(Nal(p,next,keep)); if(keep) kept+=next-p
            p=next
        }
        if(kept==0) return null
        var dest=0; var removed=0
        for(unit in units) {
            if(!unit.keep) { removed++; continue }
            for(i in unit.start until unit.end) buffer.put(dest++,buffer.get(i))
        }
        buffer.position(0); buffer.limit(dest)
        return Result(end,dest,end-dest,removed)
    }
}
