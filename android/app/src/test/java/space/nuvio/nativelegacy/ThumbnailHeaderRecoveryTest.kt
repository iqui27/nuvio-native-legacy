package space.nuvio.nativelegacy
import org.junit.Assert.*
import org.junit.Test
import java.io.*
class ThumbnailHeaderRecoveryTest {
    private val eof=IOException("closed",EOFException())
    @Test fun onlyReusedPreHeaderEofIsEligible() {
        assertTrue(thumbnailCanRecoverHeaderEof(eof,true,true,true,false,false))
        assertFalse(thumbnailCanRecoverHeaderEof(eof,true,false,true,false,false))
        assertFalse(thumbnailCanRecoverHeaderEof(eof,true,true,true,true,false))
        assertFalse(thumbnailCanRecoverHeaderEof(eof,true,true,true,false,true))
        assertFalse(thumbnailCanRecoverHeaderEof(eof,false,true,true,false,false))
        assertFalse(thumbnailCanRecoverHeaderEof(eof,true,true,false,false,false))
        assertFalse(thumbnailCanRecoverHeaderEof(java.net.SocketTimeoutException(),true,true,true,false,false))
        assertFalse(thumbnailCanRecoverHeaderEof(IOException("HTTP 429"),true,true,true,false,false))
    }
}
