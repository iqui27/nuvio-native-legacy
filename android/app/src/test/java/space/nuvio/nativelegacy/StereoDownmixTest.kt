package space.nuvio.nativelegacy

import androidx.media3.common.C
import androidx.media3.common.audio.AudioProcessor
import org.junit.Assert.*
import org.junit.Test
import java.nio.ByteBuffer
import java.nio.ByteOrder

@androidx.annotation.OptIn(androidx.media3.common.util.UnstableApi::class)
class StereoDownmixTest {
    private fun mix(vararg input: Short): ShortArray {
        val processor=stereoDownmixProcessor()
        val format=processor.configure(AudioProcessor.AudioFormat(48000,input.size,C.ENCODING_PCM_16BIT))
        assertEquals(2,format.channelCount);assertEquals(48000,format.sampleRate)
        processor.flush()
        val source=ByteBuffer.allocateDirect(input.size*2).order(ByteOrder.nativeOrder())
        input.forEach {source.putShort(it)};source.flip();processor.queueInput(source)
        val output=processor.output.order(ByteOrder.nativeOrder())
        assertEquals(0,source.remaining());assertEquals(4,output.remaining())
        return shortArrayOf(output.short,output.short)
    }
    @Test fun fivePointOneDialogueIsAudibleOnBothStereoChannels() {
        val result=mix(0,0,10000,0,0,0)
        assertTrue(result[0]>2000);assertEquals(result[0],result[1])
    }
    @Test fun fivePointOneSurroundsStayOnTheirOwnSide() {
        val left=mix(0,0,0,0,10000,0);assertTrue(left[0]>2000);assertEquals(0,left[1].toInt())
        val right=mix(0,0,0,0,0,10000);assertEquals(0,right[0].toInt());assertTrue(right[1]>2000)
    }
    @Test fun sevenPointOneSideChannelsAndDialogueAreRetained() {
        val side=mix(0,0,0,0,0,0,10000,0);assertTrue(side[0]>1000);assertEquals(0,side[1].toInt())
        val centre=mix(0,0,10000,0,0,0,0,0);assertTrue(centre[0]>1000);assertEquals(centre[0],centre[1])
    }
    @Test fun lfeIsNotMistakenForDialogue() {assertArrayEquals(shortArrayOf(0,0),mix(0,0,0,10000,0,0))}
    @Test fun allSupportedLayoutsHaveHeadroomAndStereoIsIdentity() {
        for(n in 1..8) {
            val c=StereoDownmixMatrix.coefficients(n)
            for(side in 0..1) {var total=0f;for(i in 0 until n) {assertTrue(c[i*2+side]>=0);total+=c[i*2+side]};assertTrue(total<=1.000001f)}
        }
        assertArrayEquals(floatArrayOf(1f,0f,0f,1f),StereoDownmixMatrix.coefficients(2),0f)
    }
    @Test fun fullScaleSurroundDoesNotWrapOrClip() {
        val positive=mix(32767,32767,32767,32767,32767,32767)
        assertTrue(positive.all {it>32000})
        val negative=mix(-32768,-32768,-32768,-32768,-32768,-32768)
        assertTrue(negative.all {it< -32000})
    }
}
