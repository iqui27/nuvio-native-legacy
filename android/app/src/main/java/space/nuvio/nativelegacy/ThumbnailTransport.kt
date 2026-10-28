package space.nuvio.nativelegacy

import android.util.Log
import okhttp3.*
import java.io.InputStream
import java.io.IOException
import java.net.*
import java.util.concurrent.TimeUnit

/** Thumbnail-only transport. Redirects and active exchange permits remain owned by existing code. */
internal object ThumbnailTransport {
    private val pool = ConnectionPool(2,30,TimeUnit.SECONDS)
    private val client = OkHttpClient.Builder().connectionPool(pool)
        .protocols(listOf(Protocol.HTTP_2, Protocol.HTTP_1_1)).followRedirects(false).followSslRedirects(false)
        .retryOnConnectionFailure(false).eventListenerFactory { Trace() }.build()
    @Volatile var recoverStaleHeaders=false // armed diagnostic only
    fun open(url: URL): HttpURLConnection = Connection(url)
    private class Trace : EventListener() {
        private var dnsAt=0L; private var connectAt=0L; private var tlsAt=0L; private var sentAt=0L
        private var dns=0L; private var connect=0L; private var tls=0L
        private var fresh=false
        @Volatile var acquired=false
        @Volatile var headers=false
        private var protocol="unknown"
        fun canRecover(error: Throwable,cancelled: Boolean)=thumbnailCanRecoverHeaderEof(error,acquired,!fresh,sentAt>0,headers,cancelled)
        override fun connectionAcquired(call: Call, connection: okhttp3.Connection) {
            acquired=true; protocol=connection.protocol().toString()
        }
        override fun callFailed(call: Call, ioe: IOException) {
            Log.w("nuvio-miniatura","[transport-failure] host=${call.request().url.host} acquired=$acquired reused=${!fresh} protocol=$protocol requestSent=${sentAt>0} responseHeaders=$headers type=${ioe.javaClass.simpleName} cause=${ioe.cause?.javaClass?.simpleName} cancelled=${call.isCanceled()}")
        }
        override fun dnsStart(call: Call, domainName: String) { dnsAt=System.nanoTime() }
        override fun dnsEnd(call: Call, domainName: String, inetAddressList: List<InetAddress>) { dns+=System.nanoTime()-dnsAt }
        override fun connectStart(call: Call, inetSocketAddress: InetSocketAddress, proxy: Proxy) { fresh=true; connectAt=System.nanoTime() }
        override fun secureConnectStart(call: Call) { tlsAt=System.nanoTime() }
        override fun secureConnectEnd(call: Call, handshake: Handshake?) { tls+=System.nanoTime()-tlsAt }
        override fun connectEnd(call: Call, inetSocketAddress: InetSocketAddress, proxy: Proxy, protocol: Protocol?) { connect+=System.nanoTime()-connectAt }
        override fun requestHeadersEnd(call: Call, request: Request) { sentAt=System.nanoTime() }
        override fun responseHeadersEnd(call: Call, response: Response) {
            headers=true
            Log.i("nuvio-miniatura","[transport] host=${call.request().url.host} protocol=${response.protocol} reused=${!fresh} dnsMs=${dns/1_000_000} tcpMs=${(connect-tls).coerceAtLeast(0)/1_000_000} tlsMs=${tls/1_000_000} serverHeadersMs=${if(sentAt>0)(System.nanoTime()-sentAt)/1_000_000 else -1} code=${response.code}")
        }
    }
    private class Connection(url: URL) : HttpURLConnection(url) {
        private val requestHeaders = linkedMapOf<String,String>()
        @Volatile private var call: Call?=null
        @Volatile private var response: Response?=null
        @Volatile private var cancelled=false
        private var stream: InputStream?=null
        @Volatile private var bodyFinished=false
        override fun setRequestProperty(key: String,value: String) { check(!connected); requestHeaders[key]=value }
        override fun getRequestProperty(key: String): String? = requestHeaders.entries.firstOrNull { it.key.equals(key,true) }?.value
        @Synchronized override fun connect() {
            if (connected) return
            if(cancelled) throw IOException("thumbnail connection cancelled")
            val request=Request.Builder().url(url).get().apply { requestHeaders.forEach { (k,v)->header(k,v) } }.build()
            val begin=System.nanoTime()
            var attempt=0
            while(true) {
                val trace=Trace()
                val elapsed=(System.nanoTime()-begin)/1_000_000
                val remaining=if(readTimeout>0) (readTimeout-elapsed).coerceAtLeast(1) else 0
                val builder=client.newBuilder().eventListener(trace)
                    .connectTimeout(if(connectTimeout>0) minOf(connectTimeout.toLong(),remaining.coerceAtLeast(1)) else 0,TimeUnit.MILLISECONDS)
                    .readTimeout(remaining,TimeUnit.MILLISECONDS)
                if(attempt>0) builder.connectionPool(ConnectionPool(0,1,TimeUnit.SECONDS))
                val c=builder.build().newCall(request)
                call=c
                if(cancelled) { c.cancel(); throw IOException("thumbnail connection cancelled") }
                try { response=c.execute(); connected=true; break }
                catch(e: IOException) {
                    val eligible=recoverStaleHeaders && attempt==0 && request.header("Range")!=null &&
                        trace.canRecover(e,cancelled || c.isCanceled()) && (readTimeout==0 || (System.nanoTime()-begin)/1_000_000<readTimeout)
                    c.cancel()
                    if(!eligible) throw e
                    attempt++
                    Log.w("nuvio-miniatura","[transport-recovery] host=${url.host} reusedHeaderEof=true attempt=1 freshConnection=true")
                }
            }
        }
        override fun getResponseCode(): Int { connect(); return response!!.code }
        override fun getHeaderField(name: String?): String? { connect(); return if(name==null) null else response!!.header(name) }
        override fun getContentType(): String? = getHeaderField("Content-Type")
        override fun getContentLength(): Int = getHeaderField("Content-Length")?.toIntOrNull() ?: -1
        override fun getContentLengthLong(): Long = getHeaderField("Content-Length")?.toLongOrNull() ?: -1
        override fun getInputStream(): InputStream {
            connect()
            if(response!!.code>=400) throw IOException("HTTP ${response!!.code}")
            return stream ?: object : java.io.FilterInputStream(response!!.body!!.byteStream()) {
                private var consumed=0L
                private val expected=response!!.body!!.contentLength()
                private fun record(n: Int): Int {
                    if(n<0) bodyFinished=true
                    else { consumed+=n; if(expected>=0 && consumed>=expected) bodyFinished=true }
                    return n
                }
                override fun read(): Int { val n=`in`.read(); record(if(n<0) -1 else 1); return n }
                override fun read(b: ByteArray,off: Int,len: Int): Int = record(`in`.read(b,off,len))
            }.also { stream=it }
        }
        override fun disconnect() {
            cancelled=true
            // Closing an exhausted response returns its connection to the bounded pool.
            // Cancel unfinished calls so source cancellation also interrupts a blocking read.
            if(!bodyFinished) call?.cancel()
            response?.close()
        }
        override fun usingProxy()=false
    }
}
