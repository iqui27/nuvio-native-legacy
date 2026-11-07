package space.nuvio.nativelegacy

import android.os.SystemClock
import android.util.Log
import java.io.File
import java.io.IOException
import java.util.concurrent.atomic.AtomicBoolean
import java.util.concurrent.atomic.AtomicLong

/** One-shot developer measurement after a source closes. Never decodes media. */
internal object ThumbnailSizeProbe {
    private val running = AtomicBoolean()
    fun launch(url: String, headers: List<Pair<String,String>>, durationMs: Long, total: Long,
               cues: LongArray?, track: Long, knownStarts: Map<Int,Long>, knownEnds: Map<Int,Long>) {
        val root = NvPlayer.contexto()?.getExternalFilesDir(null) ?: return
        val marker = File(root, "thumbnail-size-probe.enabled")
        if (!marker.isFile || cues == null || track <= 0 || durationMs !in 60000..86400000 ||
            cues.size / 3 > 20000 || !running.compareAndSet(false, true)) return
        if (!marker.delete()) { running.set(false); return }
        val index = cues.copyOf()
        val starts = HashMap(knownStarts); val ends = HashMap(knownEnds)
        val expectedEpoch = NvPlayer.thumbnailSourceEpoch
        Thread({
            android.os.Process.setThreadPriority(android.os.Process.THREAD_PRIORITY_BACKGROUND)
            val owner = "size-probe-${System.nanoTime()}"
            val begin = SystemClock.elapsedRealtime()
            val deadline = AtomicLong(begin+180000)
            fun admitted() = ThumbnailProbeAdmission.allowed(NvPlayer.thumbnailSourceOpen,
                NvPlayer.thumbnailPlayWhenReady,NvPlayer.thumbnailLoading,NvPlayer.thumbnailHealthAtMs,
                SystemClock.elapsedRealtime(),NvPlayer.thumbnailSourceEpoch,expectedEpoch)
            val finished = AtomicBoolean()
            val watchdog = Thread({
                while (!finished.get()) {
                    if (!admitted() || SystemClock.elapsedRealtime()>deadline.get())
                        MediaHttpBudget.cancelOwner(owner)
                    Thread.sleep(100)
                }
            }, "size-probe-watchdog").apply { isDaemon = true }
            val downloads = ArrayList<LongArray>()
            var requests = 0; var bytes = 0L; var lastStart = 0L
            var status = "complete"
            val targets = ThumbnailBlockProbe.targets(durationMs)
            val candidates = HashSet<Int>()
            for (target in targets) for (i in 0 until index.size / 3)
                if (kotlin.math.abs(index[i*3] - target*1000000) <= 5000000000L) candidates.add(i)
            fun check() {
                if (!admitted() || SystemClock.elapsedRealtime()>deadline.get() || bytes >= 4L.shl(20) || requests >= 96)
                    throw IOException("probe stopped by safety budget")
            }
            fun fetch(ranges: List<LongArray>): List<Pair<Long,ByteArray>> {
                check()
                require(ranges.size in 1..32 && ranges.all { it[0]>=0 && it[1]>=it[0] && it[1]<total && it[1]-it[0]<128 })
                while (SystemClock.elapsedRealtime()-lastStart < 1000) { check(); Thread.sleep(50) }
                var connection: java.net.HttpURLConnection? = null
                try {
                    connection = ThumbnailHttpRedirects.open(url, headers,
                        "bytes="+ranges.joinToString(",") { "${it[0]}-${it[1]}" }, true, {}, { address ->
                            check(); requests++; lastStart=SystemClock.elapsedRealtime()
                            MediaHttpBudget.open(address,owner,checkCancelled=::check)
                        })
                    val limit = minOf(4L.shl(20)-bytes, ranges.sumOf { it[1]-it[0]+1 }+1024)
                    val parts = ThumbnailPayloadExchange.readBounded(connection,ranges,maxBytes=limit,checkCancelled=::check)
                    for ((start,data) in parts) { bytes+=data.size; downloads.add(longArrayOf(start,start+data.size)) }
                    return parts
                } finally { MediaHttpBudget.close(connection) }
            }
            fun slice(parts: List<Pair<Long,ByteArray>>, at: Long, n: Int): ByteArray? {
                val part = parts.firstOrNull { at>=it.first && at+n<=it.first+it.second.size } ?: return null
                return part.second.copyOfRange((at-part.first).toInt(),(at-part.first).toInt()+n)
            }
            try {
                // A cached thumbnail job may close long before the user exits.
                // Wait for confirmed pause AND idle loading, or full teardown.
                Log.i("ThumbSizeProbe", "waiting for paused idle or closed playback; no requests")
                while (!admitted() && SystemClock.elapsedRealtime()<deadline.get() && NvPlayer.thumbnailSourceEpoch==expectedEpoch) Thread.sleep(100)
                check(); Thread.sleep(1000); check()
                deadline.set(SystemClock.elapsedRealtime()+180000); watchdog.start()
                Log.i("ThumbSizeProbe", "started targets=${targets.size} candidates=${candidates.size} requestCap=96 payloadCapMiB=4 wallCapSec=180 playbackStopped=true")
                val missing = candidates.filter { it !in starts || it !in ends }.sorted()
                val clusterHeaders = HashMap<Long,Int>()
                for (clusters in missing.map { index[it*3+1] }.distinct().chunked(32)) {
                    val parts = fetch(clusters.map { longArrayOf(it,minOf(total-1,it+11)) })
                    for (at in clusters) slice(parts,at,minOf(12L,total-at).toInt())?.let { data ->
                        ThumbnailBlockProbe.clusterHeader(data)?.let { clusterHeaders[at]=it }
                    }
                }
                var pending = missing.mapNotNull { i ->
                    val cluster=index[i*3+1]; val header=clusterHeaders[cluster] ?: return@mapNotNull null
                    val relative=index[i*3+2]
                    if (relative<0 || cluster>total-header-relative) null else i to cluster+header+relative
                }.toMap()
                repeat(8) {
                    val next = HashMap<Int,Long>()
                    for (batch in pending.entries.toList().chunked(32)) {
                        val ranges = batch.map { longArrayOf(it.value,minOf(total-1,it.value+63)) }.distinctBy { it[0] }
                        if (ranges.any { it[0]>=total }) continue
                        val parts=fetch(ranges)
                        for ((i,at) in batch) {
                            val data=slice(parts,at,minOf(64L,total-at).toInt()) ?: continue
                            when(val step=ThumbnailBlockProbe.block(data,at,track,total)) {
                                is ThumbnailBlockProbe.Step.Found -> { starts[i]=step.start; ends[i]=step.end }
                                is ThumbnailBlockProbe.Step.Skip -> if (step.next<total) next[i]=step.next
                                else -> {}
                            }
                        }
                    }
                    pending=next
                }
            } catch (e: Exception) { status="partial-${e.javaClass.simpleName}" }
            finally {
                finished.set(true); MediaHttpBudget.cancelOwner(owner)
                try {
                    val folder=File(root,"thumbnail-size-probe").apply { mkdirs() }
                    val output=File(folder,"${System.currentTimeMillis()}.csv")
                    val tmp=File(output.path+".tmp")
                    tmp.bufferedWriter().use { out ->
                        out.write("# version=1\n# duration_ms=$durationMs\n# source_bytes=$total\n# truncated=false\n")
                        out.write("# sample_targets_ms=${targets.joinToString(",")}\n# probe_status=$status\n")
                        out.write("# download_semantics=probe_only_successful_parsed_range_bytes\nkind,index,pts_ns,start,end_exclusive,selected\n")
                        for(i in 0 until index.size/3) {
                            val known=i in starts && i in ends
                            out.write("frame,$i,${index[i*3]},${if(known) starts[i] else -1},${if(known) ends[i] else -1},0\n")
                        }
                        for((i,r) in downloads.withIndex()) out.write("download,$i,0,${r[0]},${r[1]},0\n")
                    }
                    kotlin.check(tmp.renameTo(output))
                    Log.i("ThumbSizeProbe","saved=${output.name} status=$status targets=${targets.size} candidates=${candidates.size} candidatesKnown=${candidates.count { it in starts && it in ends }} requests=$requests parsedBytes=$bytes elapsedMs=${SystemClock.elapsedRealtime()-begin}")
                } catch (_: Exception) { Log.w("ThumbSizeProbe","export failed") }
                running.set(false)
            }
        },"thumbnail-size-probe").apply { isDaemon=true }.start()
    }
}
