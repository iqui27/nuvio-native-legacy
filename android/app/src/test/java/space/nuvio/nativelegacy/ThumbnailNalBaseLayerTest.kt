package space.nuvio.nativelegacy

import org.junit.Assert.*
import org.junit.Test
import java.nio.ByteBuffer

class ThumbnailNalBaseLayerTest {
    private fun nal(type: Int,layer: Int,size: Int=3): ByteArray = ByteArray(size) { 42 }.also {
        it[0]=((type shl 1) or (layer shr 5)).toByte(); it[1]=((layer shl 3) or 1).toByte()
    }
    private fun packet(vararg units: ByteArray): ByteArray {
        val out=java.io.ByteArrayOutputStream()
        for(n in units) { out.write(ByteBuffer.allocate(4).putInt(n.size).array()); out.write(n) }
        return out.toByteArray()
    }
    @Test fun skipsLargeEnhancementPayloadWithoutRequiringOutputSpaceForIt() {
        val base=nal(19,0)
        val sample=packet(base,nal(19,1,2 shl 20),nal(62,0))
        val output=ByteBuffer.allocate(7)
        val result=ThumbnailNal.writeBaseLayer(sample,4,output)!!
        assertEquals(3,result.visitedNals)
        assertEquals(2,result.removedNals)
        assertArrayEquals(byteArrayOf(0,0,0,1)+base,output.array())
    }
    @Test fun matchesAnnexBReferenceAndHandlesAContainerSlice() {
        val sample=packet(nal(33,0),nal(19,1,1000),nal(19,0,2000),nal(63,0))
        val expected=ByteBuffer.allocate(sample.size)
        assertTrue(ThumbnailNal.write(sample,4,expected)); expected.flip()
        assertNotNull(ThumbnailHevcBaseLayer.filter(expected))
        val original=byteArrayOf(9,8,7)+sample+byteArrayOf(6,5)
        val actual=ByteBuffer.allocate(sample.size)
        val result=ThumbnailNal.writeBaseLayer(original,4,actual,3,3+sample.size)!!
        actual.flip()
        assertEquals(expected,actual)
        assertEquals(4,result.visitedNals)
    }
    @Test fun rejectsMalformedTailBeforeWritingAnyOutput() {
        val sample=packet(nal(19,0))+byteArrayOf(0,0,0,99,1,1)
        val output=ByteBuffer.allocate(64).apply { put(12); put(13) }
        val previous=output.array().copyOf()
        assertNull(ThumbnailNal.writeBaseLayer(sample,4,output))
        assertEquals(2,output.position())
        assertArrayEquals(previous,output.array())
    }
}
