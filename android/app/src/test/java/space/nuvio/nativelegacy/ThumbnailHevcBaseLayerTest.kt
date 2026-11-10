package space.nuvio.nativelegacy
import org.junit.Assert.*
import org.junit.Test
import java.nio.ByteBuffer

class ThumbnailHevcBaseLayerTest {
    private fun nal(type: Int,layer: Int)=byteArrayOf(0,0,0,1,((type shl 1) or (layer shr 5)).toByte(),((layer shl 3) or 1).toByte(),42)
    @Test fun keepsBaseParametersAndPictureWhileDroppingEnhancementAndPrivateNals() {
        val base=nal(33,0)+nal(19,0)
        val b=ByteBuffer.wrap(nal(33,0)+nal(19,1)+nal(62,0)+nal(19,0))
        val r=ThumbnailHevcBaseLayer.filter(b)!!
        val out=ByteArray(b.remaining()); b.get(out)
        assertArrayEquals(base,out)
        assertEquals(2,r.removedNals)
    }
    @Test fun malformedOrEnhancementOnlyInputIsNotModified() {
        for(bytes in listOf(nal(19,1),nal(19,0)+byteArrayOf(0,0,1,2))) {
            val b=ByteBuffer.wrap(bytes.copyOf())
            assertNull(ThumbnailHevcBaseLayer.filter(b))
            assertArrayEquals(bytes,b.array())
        }
    }
}
