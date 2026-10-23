package space.nuvio.nativelegacy

import android.net.Uri
import androidx.media3.datasource.DataSource
import androidx.media3.datasource.DataSpec
import androidx.media3.datasource.TransferListener
import java.io.IOException
import java.util.concurrent.Semaphore
import java.util.concurrent.TimeUnit

/** One upstream HTTP request, including a previous player's outstanding close. */
internal class LimitedDataSource private constructor(
    private val upstream: DataSource, private val verify: () -> Int, private val cancelled: () -> Boolean
) : DataSource {
    companion object {
        private val connection = Semaphore(1, true)
    }
    class Factory(private val upstream: DataSource.Factory, private val cancelled: () -> Boolean = { false },
                  private val verify: () -> Int = { 0 }) : DataSource.Factory {
        override fun createDataSource(): DataSource = LimitedDataSource(upstream.createDataSource(), verify, cancelled)
    }
    private var acquired = false
    override fun open(dataSpec: DataSpec): Long {
        try {
            if (!connection.tryAcquire(25, TimeUnit.SECONDS)) throw IOException("Previous stream is still closing")
        } catch (e: InterruptedException) {
            Thread.currentThread().interrupt()
            throw IOException("Stream cancelled", e)
        }
        acquired = true
        try {
            if (cancelled()) throw IOException("Stream cancelled")
            val status = verify()
            if (cancelled()) throw IOException("Stream cancelled")
            if (status != 0) throw androidx.media3.datasource.HttpDataSource.InvalidResponseCodeException(
                status, "Line in use", null, emptyMap(), dataSpec, ByteArray(0))
            return upstream.open(dataSpec)
        } catch (e: Throwable) { close(); throw e }
    }
    override fun read(buffer: ByteArray, offset: Int, length: Int): Int = upstream.read(buffer, offset, length)
    override fun getUri(): Uri? = upstream.uri
    override fun getResponseHeaders(): Map<String, List<String>> = upstream.responseHeaders
    override fun addTransferListener(listener: TransferListener) = upstream.addTransferListener(listener)
    override fun close() {
        if (!acquired) return
        try { upstream.close() }
        finally { acquired = false; connection.release() }
    }
}
