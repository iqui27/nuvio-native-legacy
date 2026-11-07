package space.nuvio.nativelegacy

import org.junit.Assert.*
import org.junit.Test
import java.net.*
import java.util.concurrent.*
import java.util.concurrent.atomic.AtomicInteger

class ThumbnailTransportTest {
    @Test fun reusedConnectionClosedBeforeHeadersCanRecoverOnceOnFreshConnection() {
        for(recover in listOf(false,true)) {
            val server=ServerSocket(0,8,InetAddress.getByName("127.0.0.1"))
            val sockets=CopyOnWriteArrayList<Socket>()
            val count=AtomicInteger(); val accepts=AtomicInteger()
            val workers=Executors.newCachedThreadPool()
            val acceptor=Thread {
                while(!server.isClosed) try {
                    val socket=server.accept(); sockets.add(socket); accepts.incrementAndGet()
                    workers.submit {
                        try { socket.use {
                            val reader=it.getInputStream().bufferedReader()
                            while(reader.readLine()!=null) {
                                while(true) { if(reader.readLine().isNullOrEmpty()) break }
                                if(count.incrementAndGet()==2) break // peer closes reused socket without response headers
                                it.getOutputStream().apply {
                                    write("HTTP/1.1 206 Partial Content\r\nContent-Length: 1\r\nContent-Range: bytes 0-0/1\r\nConnection: keep-alive\r\n\r\nx".toByteArray()); flush()
                                }
                            }
                        } } catch(_: Exception) {}
                    }
                } catch(_: Exception) {}
            }.apply { isDaemon=true; start() }
            ThumbnailTransport.recoverStaleHeaders=recover
            fun fetch() {
                val c=MediaHttpBudget.open(URL("http://127.0.0.1:${server.localPort}/"),"thumbnail-test",ThumbnailTransport::open)
                try {
                    c.connectTimeout=3000; c.readTimeout=3000; c.setRequestProperty("Range","bytes=0-0")
                    assertEquals(206,c.responseCode)
                    assertEquals("x",c.inputStream.bufferedReader().use { it.readText() })
                } finally { MediaHttpBudget.close(c) }
            }
            try {
                fetch()
                if(recover) { fetch(); assertEquals(3,count.get()); assertEquals(2,accepts.get()) }
                else try { fetch(); fail("closed reused socket must fail without recovery") } catch(_: java.io.IOException) { assertEquals(2,count.get()) }
                assertEquals(4,MediaHttpBudget.available())
            } finally {
                ThumbnailTransport.recoverStaleHeaders=false
                server.close(); sockets.forEach { it.close() }; workers.shutdownNow(); acceptor.join(1000)
            }
        }
    }
    @Test fun exhaustedBodiesReuseOnePhysicalConnectionAndReturnLeases() {
        val server=ServerSocket(0,8,InetAddress.getByName("127.0.0.1"))
        val sockets=CopyOnWriteArrayList<Socket>()
        val accepts=AtomicInteger()
        val workers=Executors.newCachedThreadPool()
        val acceptor=Thread {
            while(!server.isClosed) try {
                val socket=server.accept(); sockets.add(socket); accepts.incrementAndGet()
                workers.submit {
                    try { socket.use {
                        val reader=it.getInputStream().bufferedReader()
                        while(true) {
                            if(reader.readLine()==null) break
                            while(true) { val line=reader.readLine() ?: break; if(line.isEmpty()) break }
                            it.getOutputStream().apply {
                                write("HTTP/1.1 206 Partial Content\r\nContent-Length: 1\r\nContent-Range: bytes 0-0/1\r\nConnection: keep-alive\r\n\r\nx".toByteArray()); flush()
                            }
                        }
                    } } catch(_: Exception) {}
                }
            } catch(_: Exception) { if(!server.isClosed) throw AssertionError("accept failed") }
        }.apply { isDaemon=true; start() }
        try {
            repeat(2) {
                val c=MediaHttpBudget.open(URL("http://127.0.0.1:${server.localPort}/"),"thumbnail-test",ThumbnailTransport::open)
                try {
                    c.connectTimeout=3000; c.readTimeout=3000
                    c.setRequestProperty("Range","bytes=0-0")
                    assertEquals(206,c.responseCode)
                    assertEquals("x",c.inputStream.bufferedReader().use { it.readText() })
                } finally { MediaHttpBudget.close(c) }
            }
            assertEquals(1,accepts.get())
            assertEquals(4,MediaHttpBudget.available())
        } finally {
            server.close(); sockets.forEach { it.close() }; workers.shutdownNow(); acceptor.join(1000)
        }
    }
}
