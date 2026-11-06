package space.nuvio.nativelegacy

import org.junit.Assert.*
import org.junit.Test
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicReference
import java.nio.ByteBuffer

class ThumbnailBlockBufferTest {
    @Test fun repeatedSizesReuseStorageAndValidLengthExcludesOldTail() {
        val pool=ThumbnailBlockBuffer(1024)
        var first: ByteArray?=null
        pool.use(100) {b,n->first=b;b.fill(99);assertEquals(100,n)}
        pool.use(6) {b,n->
            assertSame(first,b);assertEquals(6,n)
            byteArrayOf(0,0,0,2,0x26,1).copyInto(b)
            val output=ByteBuffer.allocate(100)
            assertTrue(ThumbnailNal.write(b,4,output,0,n))
            assertArrayEquals(byteArrayOf(0,0,0,1,0x26,1),output.array().copyOf(output.position()))
        }
        assertTrue(pool.summary().contains("allocations=1 reuses=1"))
    }
    @Test fun ownershipLastsUntilConsumerCompletes() {
        val pool=ThumbnailBlockBuffer(1024)
        val entered=CountDownLatch(1);val release=CountDownLatch(1)
        val attempted=CountDownLatch(1);val secondEntered=CountDownLatch(1)
        val failure=AtomicReference<Throwable?>()
        val a=Thread {
            try {pool.use(32) {b,_->b[0]=7;entered.countDown();release.await(2,TimeUnit.SECONDS);assertEquals(7,b[0].toInt())}}
            catch(e: Throwable){failure.set(e)}
        }
        val b=Thread {attempted.countDown();pool.use(32) {bytes,_->secondEntered.countDown();bytes[0]=9}}
        a.start();assertTrue(entered.await(2,TimeUnit.SECONDS));b.start()
        try {assertTrue(attempted.await(2,TimeUnit.SECONDS));assertFalse(secondEntered.await(100,TimeUnit.MILLISECONDS))}
        finally {release.countDown();a.join(2000);b.join(2000)}
        assertNull(failure.get());assertEquals(0L,secondEntered.count)
    }
    @Test fun oversizeIsNotRetainedAndClearReleasesSourceScratch() {
        val pool=ThumbnailBlockBuffer(1024)
        pool.use(100) {_,_->}
        pool.use(2048) {b,n->assertEquals(2048,b.size);assertEquals(2048,n)}
        assertTrue(pool.summary().contains("retainedBytes=1024"))
        pool.clear();assertTrue(pool.summary().contains("retainedBytes=0"))
    }
}
