package space.nuvio.nativelegacy

import org.junit.Assert.*
import org.junit.Test

class ThumbnailBlockProbeTest {
    private fun block(track: Int=0x81, flags: Int=0x80) = byteArrayOf(0xa3.toByte(),0x88.toByte(),track.toByte(),0,0,flags.toByte(),1,2,3,4)
    @Test fun boundsComeFromSizeNotDownloadingPayload() {
        val header=block().copyOf(6)
        assertEquals(ThumbnailBlockProbe.Step.Found(100,110),ThumbnailBlockProbe.block(header,100,1,1000))
    }
    @Test fun lacedOrNonKeyVideoIsRejected() {
        for(flags in listOf(0,0x82,0x84,0x86))
            assertEquals(ThumbnailBlockProbe.Step.Unknown,ThumbnailBlockProbe.block(block(flags=flags),100,1,1000))
    }
    @Test fun otherTrackSkipsAndMalformedSizeFailsClosed() {
        assertEquals(ThumbnailBlockProbe.Step.Skip(110),ThumbnailBlockProbe.block(block(track=0x82),100,1,1000))
        assertEquals(ThumbnailBlockProbe.Step.Unknown,ThumbnailBlockProbe.block(block(),995,1,1000))
        assertEquals(ThumbnailBlockProbe.Step.Unknown,ThumbnailBlockProbe.block(byteArrayOf(0xa3.toByte(),0xff.toByte()),100,1,1000))
    }
    @Test fun clusterHeaderHandlesUnknownLengthWithoutReadingCluster() {
        assertEquals(5,ThumbnailBlockProbe.clusterHeader(byteArrayOf(0x1f,0x43,0xb6.toByte(),0x75,0xff.toByte())))
        assertNull(ThumbnailBlockProbe.clusterHeader(byteArrayOf(0x1f,0x43)))
    }
    @Test fun targetsAreUniqueDistributedAndExcludeZero() {
        val targets=ThumbnailBlockProbe.targets(8360000)
        assertEquals(50,targets.size);assertEquals(50,targets.distinct().size)
        assertEquals(10000L,targets.first());assertEquals(8360000L,targets.last())
    }
}
