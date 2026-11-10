package space.nuvio.nativelegacy

import org.junit.Assert.*
import org.junit.Test
import java.io.*

class ProgressiveBodyReaderTest {
    @Test fun fullKnownBodyReturnsItsOnlyAllocatedArray() {
        val expected=ByteArray(150000) {it.toByte()}
        var allocated: ByteArray?=null;var total=0
        val actual=ProgressiveBodyReader.read(ByteArrayInputStream(expected),expected.size.toLong(),-1,4096,{}, {total+=it}, {
            ByteArray(it).also {allocated=it}
        })
        assertSame(allocated,actual);assertArrayEquals(expected,actual);assertEquals(expected.size,total)
    }
    @Test fun limitedReadLeavesRestAndKnownShortBodyPreservesActualLength() {
        val input=ByteArrayInputStream(byteArrayOf(1,2,3,4))
        assertArrayEquals(byteArrayOf(1,2),ProgressiveBodyReader.read(input,4,2,4096,{},{}))
        assertEquals(2,input.available())
        assertArrayEquals(byteArrayOf(9),ProgressiveBodyReader.read(ByteArrayInputStream(byteArrayOf(9)),8,-1,4096,{},{}))
    }
    @Test fun unknownLengthAndSeparateAsyncOwnersKeepTheirBytes() {
        val first=ProgressiveBodyReader.read(ByteArrayInputStream(byteArrayOf(1,2)),-1,-1,16,{},{})
        val second=ProgressiveBodyReader.read(ByteArrayInputStream(byteArrayOf(3,4)),2,-1,16,{},{})
        second[0]=99
        assertArrayEquals(byteArrayOf(1,2),first)
    }
    @Test fun cancellationClosesCallerOwnedStreamAndStopsBeforeNextChunk() {
        var cancel=false;var closed=false;var count=0
        val input=object: ByteArrayInputStream(ByteArray(200000)) {
            override fun close(){closed=true;super.close()}
        }
        try {
            input.use {ProgressiveBodyReader.read(it,200000,-1,4096,{
                if(cancel)throw InterruptedIOException("cancelled")
            },{count+=it;cancel=true})}
            fail("cancel ignored")
        } catch (_: InterruptedIOException) {}
        assertTrue(closed);assertEquals(65536,count)
    }
    @Test fun impossibleDeclaredSizeFailsBeforeAllocation() {
        var allocated=false
        try {
            ProgressiveBodyReader.read(ByteArrayInputStream(byteArrayOf()),Int.MAX_VALUE.toLong()+1,-1,16,{}, {}, {
                allocated=true;ByteArray(it)
            });fail("oversized declaration accepted")
        } catch (_: IOException) {}
        assertFalse(allocated)
    }
}
