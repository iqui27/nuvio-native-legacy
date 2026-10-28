package space.nuvio.nativelegacy

import androidx.media3.common.C
import androidx.media3.common.audio.BaseAudioProcessor
import androidx.media3.common.audio.AudioProcessor
import androidx.media3.common.audio.AudioMixingUtil
import java.nio.ByteBuffer
import androidx.media3.common.audio.ChannelMixingMatrix
import androidx.media3.common.util.UnstableApi
import kotlin.math.max

/** Canonical Media3 PCM channel order. LFE is omitted from stereo; dialogue is mixed to both sides. */
internal object StereoDownmixMatrix {
    fun coefficients(channels: Int): FloatArray {
        require(channels in 1..8)
        if(channels==1)return floatArrayOf(1f,1f)
        if(channels==2)return floatArrayOf(1f,0f,0f,1f)
        val q=0.70710677f
        val matrix=FloatArray(channels*2)
        fun channel(i: Int,left: Float,right: Float) {matrix[i*2]=left;matrix[i*2+1]=right}
        channel(0,1f,0f);channel(1,0f,1f)
        when(channels) {
            3 -> channel(2,q,q)
            4 -> {channel(2,q,0f);channel(3,0f,q)}
            5 -> {channel(2,q,q);channel(3,q,0f);channel(4,0f,q)}
            6 -> {channel(2,q,q);channel(4,q,0f);channel(5,0f,q)}
            7 -> {channel(2,q,q);channel(4,.5f,.5f);channel(5,q,0f);channel(6,0f,q)}
            8 -> {channel(2,q,q);channel(4,q,0f);channel(5,0f,q);channel(6,q,0f);channel(7,0f,q)}
        }
        var left=0f;var right=0f
        for(i in 0 until channels) {left+=matrix[i*2];right+=matrix[i*2+1]}
        val scale=1f/max(1f,max(left,right)) // coherent full-scale input cannot clip
        return FloatArray(matrix.size){matrix[it]*scale}
    }
}

/** Media3's PCM mixer with a bounded array of matrices, independent of Android SparseArray. */
@androidx.annotation.OptIn(UnstableApi::class)
internal class StereoDownmixProcessor : BaseAudioProcessor() {
    private val matrices=Array<ChannelMixingMatrix?>(9) {channels ->
        if(channels==0)null else ChannelMixingMatrix(channels,2,StereoDownmixMatrix.coefficients(channels))
    }
    override fun onConfigure(inputAudioFormat: AudioProcessor.AudioFormat): AudioProcessor.AudioFormat {
        if(inputAudioFormat.encoding!=C.ENCODING_PCM_16BIT || inputAudioFormat.channelCount !in 1..8)
            throw AudioProcessor.UnhandledAudioFormatException(inputAudioFormat)
        val matrix=checkNotNull(matrices[inputAudioFormat.channelCount])
        return if(matrix.isIdentity) AudioProcessor.AudioFormat.NOT_SET
            else AudioProcessor.AudioFormat(inputAudioFormat.sampleRate,2,C.ENCODING_PCM_16BIT)
    }
    override fun queueInput(inputBuffer: ByteBuffer) {
        val frames=inputBuffer.remaining()/inputAudioFormat.bytesPerFrame
        val output=replaceOutputBuffer(frames*outputAudioFormat.bytesPerFrame)
        AudioMixingUtil.mix(inputBuffer,inputAudioFormat,output,outputAudioFormat,
            checkNotNull(matrices[inputAudioFormat.channelCount]),frames,false,true)
        output.flip()
    }
}
@androidx.annotation.OptIn(UnstableApi::class)
internal fun stereoDownmixProcessor()=StereoDownmixProcessor()
