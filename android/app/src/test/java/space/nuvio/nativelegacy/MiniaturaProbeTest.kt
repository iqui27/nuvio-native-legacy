package space.nuvio.nativelegacy

import org.junit.Assert.*
import org.junit.Test
import java.io.ByteArrayOutputStream
import java.net.InetAddress
import java.net.ServerSocket
import java.util.Collections

class MiniaturaProbeTest {
    @Test fun constructorRefusalReportsIOExceptionAndReleasesLease() {
        MiniaturaMotor.configurar(0,480)
        try {
            fixture(limitRequest=1) { _,_,_ -> fail("refusal must not construct a source") }
            fail("expected refusal")
        } catch(e: java.lang.reflect.InvocationTargetException) {
            assertTrue("network error must not be replaced by initialization NPE",e.cause is java.io.IOException)
        }
        assertEquals(4,MediaHttpBudget.available())
    }
    @Test fun constructorTimeoutRecoversWithoutInitializationNpe() {
        MiniaturaMotor.configurar(0,480)
        fixture(timeoutRequests=setOf(1)) { source,requests,_ ->
            assertEquals("one transient retry",2,requests.size)
            source.javaClass.getDeclaredMethod("close").apply { isAccessible=true }.invoke(source)
        }
        assertEquals(4,MediaHttpBudget.available())
    }
    private fun fixture(maxRanges: Int = 100, timeoutExpanded: Boolean = false, timeoutRequests: Set<Int> = emptySet(), limitRequest: Int = -1, mp4: ByteArray? = null, test: (Any, List<List<LongRange>>, LongArray) -> Unit) {
        val bytes = mp4 ?: ByteArray(64 * 2048)
        val cues = LongArray(64 * 3)
        if (mp4 == null) for (i in 0 until 64) {
            val base = i * 2048 + 100
            cues[i * 3] = (i + 1) * 10_000_000_000L
            cues[i * 3 + 1] = base.toLong()
            val body = byteArrayOf(0x81.toByte(), 0, 0, 0x80.toByte(), 1, 0x65) + ByteArray(500)
            val cluster = byteArrayOf(0x1f, 0x43, 0xb6.toByte(), 0x75, 0xff.toByte(), 0xa3.toByte(),
                (0x40 or (body.size shr 8)).toByte(), body.size.toByte()) + body
            cluster.copyInto(bytes, base)
        }
        val requests = Collections.synchronizedList(mutableListOf<List<LongRange>>())
        val server = ServerSocket(0, 1, InetAddress.getByName("127.0.0.1"))
        val responder = Thread {
            while (!server.isClosed) try {
                server.accept().use { socket ->
                    socket.soTimeout = 3000
                    val reader = socket.getInputStream().bufferedReader()
                    var range = ""
                    while (true) {
                        val line = reader.readLine() ?: break
                        if (line.isEmpty()) break
                        if (line.startsWith("Range:", true)) range = line.substringAfter("bytes=")
                    }
                    val ranges = range.split(',').map { value ->
                        val ends = value.trim().split('-')
                        ends[0].toLong()..ends[1].toLong()
                    }
                    requests += ranges
                    val response = socket.getOutputStream()
                    if ((timeoutExpanded && ranges.size > 2) || requests.size in timeoutRequests) {
                        Thread.sleep(6500) // exceed the production six-second header timeout
                        return@use
                    }
                    if (requests.size == limitRequest) {
                        response.write("HTTP/1.1 429 Too Many Requests\r\nContent-Length: 0\r\nConnection: close\r\n\r\n".toByteArray())
                    } else if (ranges.size > maxRanges) {
                        response.write("HTTP/1.1 416 Range Not Satisfiable\r\nContent-Length: 0\r\nConnection: close\r\n\r\n".toByteArray())
                    } else if (ranges.size == 1) {
                        val r = ranges.single()
                        val body = bytes.copyOfRange(r.first.toInt(), r.last.toInt() + 1)
                        response.write("HTTP/1.1 206 Partial Content\r\nContent-Range: bytes ${r.first}-${r.last}/${bytes.size}\r\nContent-Length: ${body.size}\r\nConnection: close\r\n\r\n".toByteArray())
                        response.write(body)
                    } else {
                        val body = ByteArrayOutputStream()
                        for (r in ranges) {
                            body.write("--sample\r\nContent-Range: bytes ${r.first}-${r.last}/${bytes.size}\r\n\r\n".toByteArray())
                            body.write(bytes, r.first.toInt(), (r.last - r.first + 1).toInt())
                            body.write("\r\n".toByteArray())
                        }
                        body.write("--sample--\r\n".toByteArray())
                        response.write("HTTP/1.1 206 Partial Content\r\nContent-Type: multipart/byteranges; boundary=sample\r\nContent-Length: ${body.size()}\r\nConnection: close\r\n\r\n".toByteArray())
                        response.write(body.toByteArray())
                    }
                    response.flush()
                }
            } catch (e: java.io.IOException) { if (!server.isClosed) throw e }
        }.apply { isDaemon = true; start() }
        try {
            val type = Class.forName("space.nuvio.nativelegacy.MiniaturaMotor\$Fonte")
            val source = type.declaredConstructors.single { it.parameterCount == 3 }.apply { isAccessible = true }
                .newInstance("http://127.0.0.1:${server.localPort}/", "", false)
            fun set(name: String, value: Any) { type.getDeclaredField(name).apply { isAccessible = true }.set(source, value) }
            if (mp4 == null) { set("cues", cues); set("nalTam", 1); set("trilhaV", 1L) }
            test(source, requests, cues)
        } finally { server.close(); responder.join(2000) }
    }
    private fun fetch(source: Any, cues: LongArray, current: LongArray, ahead: LongArray): Int =
        source.javaClass.getDeclaredMethod("prefetchExato", LongArray::class.java, LongArray::class.java, LongArray::class.java)
            .apply { isAccessible = true }.invoke(source, cues, current, ahead) as Int

    private fun flag(source: Any, name: String) = source.javaClass.getDeclaredField(name)
        .apply { isAccessible = true }.getBoolean(source)

    @Test fun requiredProbeTimeoutRetriesAndKeepsExactFrames() = fixture(timeoutRequests = setOf(2)) { source, requests, cues ->
        assertTrue(fetch(source, cues, longArrayOf(10000,20000), longArrayOf()) >= 0)
        assertEquals(4, requests.size) // size, failed probe, retry, payload
        assertEquals(requests[1], requests[2])
        assertFalse(flag(source, "recusado"))
        assertFalse((source.javaClass.getDeclaredField("probeRecovery").apply { isAccessible=true }.get(source) as ThumbnailProbeRecovery).allowed(0))
        assertTrue(requests.last().all { it.last - it.first + 1 == 509L })
    }
    @Test fun persistentTimeoutStopsAfterOneRetry() = fixture(timeoutRequests = setOf(2,3)) { source, requests, cues ->
        try {
            fetch(source, cues, longArrayOf(10000,20000), longArrayOf())
            fail("persistent timeout must stop")
        } catch (e: java.lang.reflect.InvocationTargetException) {
            assertTrue(e.cause is java.net.SocketTimeoutException)
        }
        assertEquals(3, requests.size)
        assertTrue(flag(source, "recusado"))
    }
    @Test fun explicitRateLimitStopsWithoutRetry() = fixture(limitRequest = 2) { source, requests, cues ->
        try {
            fetch(source, cues, longArrayOf(10000,20000), longArrayOf())
            fail("HTTP 429 must stop")
        } catch (e: java.lang.reflect.InvocationTargetException) {
            assertTrue(e.cause is java.io.IOException)
        }
        assertEquals(2, requests.size)
        assertTrue(flag(source, "recusado"))
    }
    @Test fun mp4HttpIndexFetchAndCpuPacketKeepExactPtsAndBytes() = fixture(mp4 = ThumbnailMp4IndexTest().mediaForHttp()) { source, requests, _ ->
        fun call(name: String, vararg args: Any): Any? {
            val method = source.javaClass.declaredMethods.single { it.name == name && it.parameterCount == args.size }
            method.isAccessible = true
            return method.invoke(source, *args)
        }
        call("modoDireto")
        assertEquals(2000L, call("keyframeMs", 2500L))
        call("soltarCache") // force the data request, not just a hit in the tiny fixture's metadata window
        assertTrue((call("prefetch", longArrayOf(0,2500), longArrayOf()) as Int) >= 0)
        assertEquals(16, call("loteIdeal")) // exact MP4 sizes must update CPU-route batching too
        val output = java.nio.ByteBuffer.allocate(32)
        val packet = call("blocoChave", 2500L, output) as Pair<*, *>
        assertEquals(2_000_000L, packet.first) // microseconds, not nominal 2500 ms cursor
        assertEquals(6, packet.second)
        assertArrayEquals(byteArrayOf(0,0,0,1,0x26,1), output.array().copyOf(6))
        assertEquals(listOf(24L..29L,37L..42L), requests.last())
        val count = requests.size
        assertEquals(0, call("prefetch", longArrayOf(0,2500), longArrayOf()))
        assertEquals(count, requests.size) // already available samples need no newly written bytes
    }
    @Test fun firstBatchStaysSmallAndFutureBatchSkipsItsProbe() = fixture { source, requests, cues ->
        assertTrue(fetch(source, cues, longArrayOf(10000,20000), longArrayOf(30000,40000)) >= 0)
        assertEquals(3, requests.size) // size probe, current-only probe, exact payload
        assertEquals(2, requests[1].size)
        assertTrue(fetch(source, cues, longArrayOf(30000,40000), longArrayOf(50000,60000)) >= 0)
        assertEquals(5, requests.size)
        assertEquals(4, requests[3].size)
        assertTrue(fetch(source, cues, longArrayOf(50000,60000), longArrayOf(70000,80000)) >= 0)
        assertEquals(6, requests.size) // no future-only request on an already-known batch
        for (index in listOf(2,4,5)) for (r in requests[index]) {
            assertEquals(509L, r.last - r.first + 1) // exact block, never guessed oversized windows
        }
    }
    @Test fun probeLookaheadIsBounded() = fixture { source, requests, cues ->
        fetch(source, cues, longArrayOf(10000,20000), longArrayOf())
        fetch(source, cues, longArrayOf(30000,40000), (5L..64L).map { it*10000 }.toLongArray())
        assertEquals(32, requests[3].size)
        assertTrue(requests.all { it.size <= 32 })
    }
    @Test fun expandedProbeTimeoutDowngradesOnceAndKeepsCurrentFrames() = fixture(timeoutExpanded = true) { source, requests, cues ->
        fetch(source, cues, longArrayOf(10000,20000), longArrayOf())
        assertTrue(fetch(source, cues, longArrayOf(30000,40000), longArrayOf(50000,60000)) >= 0)
        assertFalse(source.javaClass.getDeclaredField("recusado").apply { isAccessible=true }.getBoolean(source))
        assertFalse((source.javaClass.getDeclaredField("probeRecovery").apply { isAccessible=true }.get(source) as ThumbnailProbeRecovery).allowed(0))
        assertTrue(fetch(source, cues, longArrayOf(50000,60000), longArrayOf(70000,80000)) >= 0)
        assertEquals(listOf(4,2,2,2,2), requests.drop(3).map { it.size })
    }
    @Test fun rangeLimitedServerFallsBackToCurrentOnly() = fixture(2) { source, requests, cues ->
        fetch(source, cues, longArrayOf(10000,20000), longArrayOf())
        assertTrue(fetch(source, cues, longArrayOf(30000,40000), longArrayOf(50000,60000)) >= 0)
        assertEquals(listOf(4,2,2), requests.drop(3).map { it.size })
        assertEquals(509L, requests.last().first().let { it.last-it.first+1 })
    }
    @Test fun knownPayloadAndFutureProbesShareOneHttpRoundTrip() = fixture { source, requests, cues ->
        val bits = MiniaturaMotor::class.java.getDeclaredField("perfilBits").apply { isAccessible = true }
        val previous = bits.getInt(null)
        try {
            bits.setInt(null,8192)
            fetch(source,cues,longArrayOf(10000,20000),longArrayOf())
            fetch(source,cues,longArrayOf(30000,40000),longArrayOf(50000,60000))
            val before = requests.size
            assertTrue(fetch(source,cues,longArrayOf(50000,60000),longArrayOf(70000,80000)) >= 0)
            assertEquals(before+1,requests.size)
            assertEquals(4,requests.last().size)
            assertEquals(2,requests.last().count { it.last-it.first+1 == 509L })
        } finally { bits.setInt(null,previous) }
    }
}
