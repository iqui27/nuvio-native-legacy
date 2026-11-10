package space.nuvio.nativelegacy

import java.util.concurrent.ArrayBlockingQueue
import java.util.concurrent.ThreadPoolExecutor
import java.util.concurrent.TimeUnit
import java.util.concurrent.RejectedExecutionException
import java.util.concurrent.atomic.AtomicBoolean

/** One speculative batch, one worker, no decoder or image ownership. */
internal class ThumbnailPrefetch(
    private val allowed: () -> Boolean,
    private val prepare: (LongArray) -> Int,
    private val abort: () -> Unit,
    private val report: (String) -> Unit = {},
    private val prepareFurther: ((LongArray,LongArray)->Int)? = null,
) : AutoCloseable {
    private val busy = AtomicBoolean()
    private val closed = AtomicBoolean()
    private val executor = ThreadPoolExecutor(1, 1, 0, TimeUnit.MILLISECONDS, ArrayBlockingQueue(1),
        { task -> Thread(task, "nuvio-thumb-prefetch").apply { isDaemon = true } })

    fun offer(times: LongArray, further: LongArray = longArrayOf()): Boolean {
        if (times.isEmpty() || closed.get() || !allowed() || !busy.compareAndSet(false, true)) return false
        val copy = times.copyOf(minOf(16, times.size))
        val hints = further.copyOf(minOf(16,further.size))
        try {
            executor.execute {
                val start = System.nanoTime()
                try {
                    if (!closed.get() && allowed()) {
                        report("start targets=${copy.size}")
                        val result = prepareFurther?.invoke(copy,hints) ?: prepare(copy)
                        report("done result=$result elapsedMs=${(System.nanoTime() - start) / 1_000_000}")
                    }
                } catch (e: Exception) {
                    report("stopped type=${e.javaClass.simpleName}")
                } finally { busy.set(false) }
            }
        } catch (_: RejectedExecutionException) { busy.set(false); return false }
        return true
    }

    override fun close() {
        if (!closed.compareAndSet(false, true)) return
        executor.shutdownNow()
        abort() // disconnect an active HTTP body as well as interrupting permit/sleep waits
        try {
            if (!executor.awaitTermination(8, TimeUnit.SECONDS)) report("close pending: worker has not terminated")
        } catch (_: InterruptedException) { Thread.currentThread().interrupt() }
    }
}
