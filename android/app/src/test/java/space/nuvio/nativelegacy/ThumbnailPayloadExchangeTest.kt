package space.nuvio.nativelegacy

import org.junit.Assert.*
import org.junit.Test
import java.io.*
import java.net.*
import java.util.concurrent.*

class ThumbnailPayloadExchangeTest {
    private class Response(val range: String, val data: ByteArray, val type: String="application/octet-stream",
                           val barrier: CountDownLatch?=null, val block: Boolean=false,
                           val cancelled: CountDownLatch?=null, val waitFor: CountDownLatch?=null): HttpURLConnection(URL("http://example.test/")) {
        override fun connect() {}
        override fun disconnect() {}
        override fun usingProxy()=false
        override fun getResponseCode()=206
        override fun getContentType()=type
        override fun getHeaderField(name: String?)=if(name=="Content-Range") range else null
        override fun getInputStream(): InputStream {
            barrier?.countDown()
            check(barrier?.await(3,TimeUnit.SECONDS) != false) { "two payload bodies did not overlap" }
            if(block) try { Thread.sleep(30000) } catch(_: InterruptedException) {
                throw java.io.InterruptedIOException("cancelled second body")
            } finally { cancelled?.countDown() }
            check(waitFor?.await(3,TimeUnit.SECONDS) != false)
            return ByteArrayInputStream(data)
        }
    }
    @Test fun completePartPublishedEvenIfLaterBodyIsTruncated() {
        val body="--x\r\nContent-Range: bytes 0-1/8\r\n\r\nab\r\n--x\r\nContent-Range: bytes 4-5/8\r\n\r\nc"
        val published=mutableListOf<Long>()
        val c=Response("",body.toByteArray(),"multipart/byteranges; boundary=x")
        try {
            ThumbnailPayloadExchange.read(c,listOf(longArrayOf(0,1),longArrayOf(4,5))) { a,_ -> published.add(a) }
            fail("truncated second body must fail")
        } catch(expected: IOException) { assertEquals(listOf(0L),published) }
    }
    @Test fun twoBodiesOverlapAndReleaseAllSharedPermits() {
        val barrier=CountDownLatch(2)
        val result=ThumbnailPayloadExchange.fetch((0..3).map { longArrayOf(it.toLong(),it.toLong()) }, { ranges ->
            val a=ranges.first()[0]; val b=ranges.last()[1]
            MediaHttpBudget.open(URL("http://example.test/"),"thumbnail-test", {
                Response("bytes $a-$b/4",ByteArray((b-a+1).toInt()),barrier=barrier)
            })
        },{ false })
        assertEquals(4,result.sumOf { it.bytes })
        assertEquals(4,MediaHttpBudget.available())
    }
    @Test fun lowBufferCancelsSecondBodyThenRetriesItsRangesSequentially() {
        val barrier=CountDownLatch(2)
        val cancelled=CountDownLatch(1)
        val calls=java.util.concurrent.atomic.AtomicInteger()
        val result=ThumbnailPayloadExchange.fetch((0..3).map { longArrayOf(it.toLong(),it.toLong()) }, { ranges ->
            val n=calls.incrementAndGet(); val a=ranges.first()[0]; val b=ranges.last()[1]
            MediaHttpBudget.open(URL("http://example.test/"),"thumbnail-test", {
                Response("bytes $a-$b/4",ByteArray((b-a+1).toInt()),
                    barrier=if(n<=2) barrier else null, block=a==2L && n<=2,
                    cancelled=cancelled,waitFor=if(a==0L) cancelled else null)
            })
        },{ barrier.count==0L })
        assertEquals(4,result.sumOf { it.bytes })
        assertEquals(3,calls.get())
        assertEquals(4,MediaHttpBudget.available())
    }
    @Test fun malformedOrMissingRangesDoNotPublishPartialResults() {
        try {
            ThumbnailPayloadExchange.read(Response("bytes 0-8388608/9999999",byteArrayOf()),listOf(longArrayOf(0,0)))
            fail("oversized response accepted")
        } catch(_: IOException) {}
        try {
            ThumbnailPayloadExchange.read(Response("bytes 0-0/10",byteArrayOf(1)),listOf(longArrayOf(0,0),longArrayOf(5,5)))
            fail("missing range accepted")
        } catch(_: IOException) {}
    }
    @Test fun multipartReorderedRangesAreStoredAtTheirActualOffsets() {
        val bytes="--b\r\nContent-Range: bytes 5-5/10\r\n\r\ny\r\n--b\r\nContent-Range: bytes 0-0/10\r\n\r\nx\r\n--b--\r\n".toByteArray()
        val result=ThumbnailPayloadExchange.read(Response("",bytes,"multipart/byteranges; boundary=b"),listOf(longArrayOf(0,0),longArrayOf(5,5)))
        assertEquals(listOf(5L,0L),result.map { it.first })
        assertEquals('y'.code,result[0].second[0].toInt())
    }
    @Test fun multipartMayStartWithCrLfAndMimePreamble() {
        val bytes="\r\nMIME preamble\r\n--b\r\nContent-Range: bytes 0-0/1\r\n\r\nx\r\n--b--\r\n".toByteArray()
        val result=ThumbnailPayloadExchange.read(Response("",bytes,"multipart/byteranges; boundary=b"),listOf(longArrayOf(0,0)))
        assertEquals('x'.code,result.single().second.single().toInt())
    }
}
