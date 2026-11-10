package space.nuvio.nativelegacy

import java.io.File
import java.io.RandomAccessFile
import java.net.ServerSocket
import java.net.InetAddress
import java.nio.ByteBuffer
import java.util.concurrent.CountDownLatch
import java.util.concurrent.Executors
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicInteger
import org.junit.Assert.*
import org.junit.Test

class MiniaturaBlocoTest {
    private fun withBlock(bytes: ByteArray, check: (Any, AtomicInteger) -> Unit) {
        val requests = AtomicInteger()
        val server = ServerSocket(0, 1, InetAddress.getByName("127.0.0.1"))
        val responder = Thread {
            while (!server.isClosed) try {
                server.accept().use { socket ->
                    socket.soTimeout = 2000
                    val input = socket.getInputStream().bufferedReader()
                    while (!input.readLine().isNullOrEmpty()) { /* request headers */ }
                    requests.incrementAndGet()
                    socket.getOutputStream().apply {
                        write("HTTP/1.1 206 Partial Content\r\nContent-Range: bytes 0-0/1000\r\nContent-Length: 1\r\nConnection: close\r\n\r\n".toByteArray())
                        write(0); flush()
                    }
                }
            } catch (e: java.io.IOException) { if (!server.isClosed) throw e }
        }
        responder.isDaemon = true; responder.start()
        val file = File.createTempFile("nuvio-block-", ".bin")
        val disk = RandomAccessFile(file, "rw")
        try {
            val type = Class.forName("space.nuvio.nativelegacy.MiniaturaMotor\$Fonte")
            val ctor = type.declaredConstructors.single { it.parameterCount == 3 }.apply { isAccessible = true }
            val source = ctor.newInstance("http://127.0.0.1:${server.localPort}/", "", false)
            fun set(name: String, value: Any) { type.getDeclaredField(name).apply { isAccessible = true }.set(source, value) }
            disk.write(bytes)
            set("disco", disk)
            set("cues", longArrayOf(10_000_000_000, 0, 0))
            set("nalTam", 1)
            set("trilhaV", 1L)
            @Suppress("UNCHECKED_CAST")
            val entries = type.getDeclaredField("noDisco").apply { isAccessible = true }.get(source) as MutableMap<Int, LongArray>
            entries[0] = longArrayOf(0, bytes.size.toLong())
            check(source, requests)
        } finally {
            disk.close(); file.delete(); server.close(); responder.join(2000)
        }
    }

    private fun read(source: Any, out: ByteBuffer): Any? = source.javaClass
        .getDeclaredMethod("blocoChave", java.lang.Long.TYPE, ByteBuffer::class.java)
        .apply { isAccessible = true }.invoke(source, 10_000L, out)

    private fun block(flags: Int = 0x80, length: Int = 1) =
        byteArrayOf(0xA3.toByte(), 0x86.toByte(), 0x81.toByte(), 0, 0, flags.toByte(), length.toByte(), 0x65)

    @Test fun completedBlockDecodesWhileNetworkMonitorIsOccupied() = withBlock(block()) { source, requests ->
        val occupied = CountDownLatch(1)
        val release = CountDownLatch(1)
        val holder = Thread { synchronized(source) { occupied.countDown(); release.await(5, TimeUnit.SECONDS) } }
        val executor = Executors.newSingleThreadExecutor()
        holder.start()
        try {
            assertTrue(occupied.await(1, TimeUnit.SECONDS))
            val out = ByteBuffer.allocateDirect(32)
            val value = executor.submit<Any?> { read(source, out) }.get(1, TimeUnit.SECONDS) as Pair<*, *>
            assertEquals(10_000_000L, value.first)
            val annexB = ByteArray(out.remaining()); out.get(annexB)
            assertArrayEquals(byteArrayOf(0, 0, 0, 1, 0x65), annexB)
            assertEquals(1, requests.get()) // constructor's size probe only
        } finally {
            release.countDown(); holder.join(2000); executor.shutdownNow()
        }
    }

    @Test fun lacedBlocksAreDeclined() = withBlock(block(flags = 0x82)) { source, _ ->
        assertNull(read(source, ByteBuffer.allocateDirect(32)))
    }

    @Test fun malformedNalLengthIsDeclined() = withBlock(block(length = 10)) { source, _ ->
        assertNull(read(source, ByteBuffer.allocateDirect(32)))
    }
}
