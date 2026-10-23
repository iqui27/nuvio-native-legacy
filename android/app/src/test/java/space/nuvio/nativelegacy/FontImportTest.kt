package space.nuvio.nativelegacy

import org.junit.Assert.assertEquals
import org.junit.Assert.assertThrows
import org.junit.Test
import java.io.ByteArrayInputStream
import java.io.ByteArrayOutputStream
import java.io.IOException
import java.io.InputStream
import java.io.OutputStream

class FontImportTest {
    private class Generated(private var remaining: Int) : InputStream() {
        override fun read(): Int = if (remaining-- > 0) 65 else -1
        override fun read(buffer: ByteArray, offset: Int, length: Int): Int {
            if (remaining <= 0) return -1
            val n = minOf(remaining, length)
            java.util.Arrays.fill(buffer, offset, offset + n, 65.toByte())
            remaining -= n
            return n
        }
    }
    private class Counted : OutputStream() {
        var size = 0
        override fun write(value: Int) { size++ }
        override fun write(buffer: ByteArray, offset: Int, length: Int) { size += length }
    }
    @Test fun exactLimitAndOversize() {
        val exact = Counted()
        FontImport.copiar(Generated(FontImport.MAX_BYTES), exact)
        assertEquals(FontImport.MAX_BYTES, exact.size)
        val over = Counted()
        assertThrows(IOException::class.java) {
            FontImport.copiar(Generated(FontImport.MAX_BYTES + 1), over)
        }
        assertEquals(FontImport.MAX_BYTES, over.size)
    }
    @Test fun shortAndZeroLengthReads() {
        assertThrows(IOException::class.java) {
            FontImport.copiar(ByteArrayInputStream(ByteArray(11)), Counted())
        }
        val input = object : InputStream() {
            var first = true
            val rest = ByteArrayInputStream(ByteArray(12) { 65 })
            override fun read(): Int = rest.read()
            override fun read(buffer: ByteArray, offset: Int, length: Int): Int {
                if (first) { first = false; return 0 }
                return rest.read(buffer, offset, length)
            }
        }
        val output = ByteArrayOutputStream()
        FontImport.copiar(input, output)
        assertEquals("AAAAAAAAAAAA", output.toString("UTF-8"))
    }
    @Test fun readAndWriteFailuresPropagate() {
        assertThrows(IOException::class.java) {
            FontImport.copiar(object : InputStream() {
                override fun read(): Int = throw IOException("provider denied")
            }, Counted())
        }
        assertThrows(IOException::class.java) {
            FontImport.copiar(Generated(100), object : OutputStream() {
                override fun write(value: Int) { throw IOException("storage full") }
            })
        }
    }
}
