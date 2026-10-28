package space.nuvio.nativelegacy

import java.io.IOException
import java.io.InputStream
import java.net.HttpURLConnection
import java.util.concurrent.*
import java.util.concurrent.atomic.AtomicReference

internal object ThumbnailPayloadExchange {
    private val currentConnection = ThreadLocal<AtomicReference<HttpURLConnection?>?>()
    fun track(c: HttpURLConnection) { currentConnection.get()?.set(c) }
    data class Result(val parts: List<Pair<Long,ByteArray>>, val bytes: Int, val elapsedMs: Long,
                      val headersMs: Long, val bodyMs: Long, val permitMs: Long,
                      val bodyStartNs: Long=0, val bodyEndNs: Long=0)
    class Limited(val status: Int): IOException("thumbnail HTTP $status")
    fun read(c: HttpURLConnection, requested: List<LongArray>, publish: (Long,ByteArray)->Unit = { _,_-> }): List<Pair<Long,ByteArray>> {
        val limit=8L shl 20
        if(c.responseCode in listOf(401,403,429,503,509)) throw Limited(c.responseCode)
        if(c.responseCode!=206) throw IOException("thumbnail HTTP ${c.responseCode}")
        val result=ArrayList<Pair<Long,ByteArray>>()
        var bytes=0L
        fun range(value: String): Pair<Long,Long> {
            val m=Regex("bytes\\s+(\\d+)-(\\d+)/",RegexOption.IGNORE_CASE).find(value) ?: throw IOException("missing content range")
            val a=m.groupValues[1].toLong(); val b=m.groupValues[2].toLong()
            if(a<0 || b<a || b-a+1>limit-bytes) throw IOException("payload response exceeds budget")
            return a to b
        }
        fun take(input: InputStream,a: Long,b: Long) {
            if(requested.none { a<=it[0] && b>=it[1] }) throw IOException("unexpected payload range")
            val data=ByteArray((b-a+1).toInt()); var n=0
            while(n<data.size) { val count=input.read(data,n,data.size-n); if(count<0) throw IOException("short payload"); n+=count }
            result.add(a to data); bytes+=data.size; publish(a,data)
        }
        c.inputStream.buffered(16384).use { input ->
            val type=c.contentType ?: ""
            if(!type.lowercase().startsWith("multipart/byteranges")) {
                val (a,b)=range(c.getHeaderField("Content-Range") ?: ""); take(input,a,b)
            } else {
                val boundary=Regex("boundary=\"?([^\";]+)").find(type)?.groupValues?.get(1) ?: throw IOException("missing boundary")
                if(boundary.length>200) throw IOException("oversized boundary")
                val marker="--$boundary"
                fun line(): String {
                    val text=StringBuilder()
                    while(text.length<=4096) {
                        val x=input.read(); if(x<0) throw IOException("short multipart headers")
                        if(x==10) return text.toString().trimEnd('\r')
                        text.append(x.toChar())
                    }
                    throw IOException("oversized multipart header")
                }
                fun boundaryLine(preamble: Boolean): String {
                    var total=0
                    repeat(if(preamble) 64 else 8) {
                        val value=line().trimEnd(' ','\t')
                        total+=value.length
                        if(total>16384) throw IOException("oversized multipart preamble")
                        if(value==marker || value=="$marker--") return value
                        if(!preamble && value.isNotEmpty()) throw IOException("invalid multipart separator")
                    }
                    throw IOException("missing multipart boundary")
                }
                var line=boundaryLine(true); var parts=0
                while(line!= "$marker--") {
                    if(line!=marker || ++parts>32) throw IOException("invalid multipart framing")
                    var contentRange=""; var headers=0
                    while(true) {
                        val h=line(); if(h.isEmpty()) break
                        if(++headers>64) throw IOException("too many multipart headers")
                        if(h.startsWith("Content-Range:",true)) contentRange=h.substringAfter(':').trim()
                    }
                    val (a,b)=range(contentRange); take(input,a,b)
                    if(line().isNotEmpty()) throw IOException("invalid multipart separator")
                    line=boundaryLine(false)
                }
            }
        }
        if(requested.any { r -> result.none { r[0]>=it.first && r[1]<it.first+it.second.size } })
            throw IOException("missing requested payload range")
        return result
    }

    /** Each worker owns only its response. Source/cache/counters are updated by the caller after joining. */
    fun fetch(ranges: List<LongArray>, dispatch: (List<LongArray>)->HttpURLConnection,
              lowBuffer: ()->Boolean, publish: (Long,ByteArray)->Unit = { _,_-> }): List<Result> {
        val groups=listOf(ranges.take(ranges.size/2),ranges.drop(ranges.size/2))
        val executor=Executors.newFixedThreadPool(2) { r -> Thread(r,"nuvio-thumb-payload").apply { isDaemon=true } }
        val connections=List(2) { AtomicReference<HttpURLConnection?>() }
        fun one(index: Int): Result {
            val t=System.nanoTime(); val permit=MediaHttpBudget.threadWaitNanos()
            var c: HttpURLConnection?=null
            try {
                currentConnection.set(connections[index])
                c=dispatch(groups[index]); connections[index].set(c)
                if(Thread.currentThread().isInterrupted) throw java.io.InterruptedIOException("payload cancelled")
                val headers=System.nanoTime()
                val parts=read(c,groups[index],publish); val end=System.nanoTime()
                return Result(parts,parts.sumOf { it.second.size },(end-t)/1_000_000,
                    (headers-t)/1_000_000,(end-headers)/1_000_000,
                    (MediaHttpBudget.threadWaitNanos()-permit)/1_000_000,headers,end)
            } finally { currentConnection.remove(); MediaHttpBudget.close(c); connections[index].set(null) }
        }
        val futures=(0..1).map { i->executor.submit<Result> { one(i) } }
        var reduced=false
        try {
            while(futures.any { !it.isDone }) {
                if(!reduced && lowBuffer() && !futures[1].isDone) {
                    reduced=true; futures[1].cancel(true); MediaHttpBudget.close(connections[1].get())
                }
                try { futures.firstOrNull { !it.isDone }?.get(100,TimeUnit.MILLISECONDS) }
                catch(_: TimeoutException) {} catch(_: CancellationException) {}
            }
            val result=mutableListOf(futures[0].get())
            // Wait for the cancelled worker to relinquish its exchange before retrying sequentially.
            executor.shutdown()
            if(!executor.awaitTermination(8,TimeUnit.SECONDS)) throw IOException("payload worker did not stop")
            result.add(if(reduced) one(1) else futures[1].get())
            return result
        } catch(e: ExecutionException) { throw (e.cause as? IOException ?: IOException("payload worker failed",e.cause)) }
        finally {
            futures.forEach { it.cancel(true) }; connections.forEach { MediaHttpBudget.close(it.get()) }
            executor.shutdownNow()
            if(!executor.awaitTermination(8,TimeUnit.SECONDS)) throw IOException("payload cleanup pending")
        }
    }
}
