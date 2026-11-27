package space.nuvio.nativelegacy

import android.util.Log
import java.io.File
import java.util.concurrent.atomic.AtomicLong

/** Opt-in numeric metadata only. Never reads source bytes or changes selection. */
internal class ThumbnailRangeAudit private constructor(private val output: File) {
    private val downloads = ArrayList<LongArray>()
    private val selected = HashSet<Int>()
    private var truncated = false
    private var dumped = false
    @Synchronized fun downloaded(start: Long, size: Int) {
        if (downloads.size < 50_000) downloads.add(longArrayOf(start, start + size)) else truncated = true
    }
    @Synchronized fun selected(indices: List<Int>) { selected.addAll(indices.take(20_000)) }
    @Synchronized fun dump(durationMs: Long, total: Long, cues: LongArray?, starts: Map<Int, Long>,
                           ends: Map<Int, Long>, mp4Sizes: IntArray?) {
        if (dumped || cues == null) return
        dumped = true
        if (cues.size / 3 > 20_000) truncated = true
        try {
            output.parentFile?.mkdirs()
            val tmp = File(output.path + ".tmp")
            tmp.bufferedWriter().use { out ->
                out.write("# version=1\n# duration_ms=$durationMs\n# source_bytes=$total\n# truncated=$truncated\n")
                out.write("# download_semantics=successful_parsed_range_bytes_excludes_partial_failures_and_multipart_framing\n")
                out.write("kind,index,pts_ns,start,end_exclusive,selected\n")
                for (i in 0 until minOf(cues.size / 3, 20_000)) {
                    val start = if (mp4Sizes != null) cues[i * 3 + 1] else starts[i] ?: -1
                    val end = if (mp4Sizes != null) start + mp4Sizes[i] else ends[i] ?: -1
                    out.write("frame,$i,${cues[i * 3]},$start,$end,${if (i in selected) 1 else 0}\n")
                }
                for ((i, range) in downloads.withIndex()) out.write("download,$i,0,${range[0]},${range[1]},0\n")
            }
            check(tmp.renameTo(output))
            Log.i("ThumbRangeAudit", "saved=${output.name} frames=${cues.size / 3} downloads=${downloads.size} truncated=$truncated")
        } catch (_: Exception) { Log.w("ThumbRangeAudit", "export failed; playback unaffected") }
    }
    companion object {
        private val sequence = AtomicLong()
        fun create(): ThumbnailRangeAudit? = try {
            val root = NvPlayer.contexto()?.getExternalFilesDir(null)
            if (root == null || !File(root, "thumbnail-range-audit.enabled").isFile) null
            else ThumbnailRangeAudit(File(root, "thumbnail-range-audit/${System.currentTimeMillis()}-${sequence.incrementAndGet()}.csv")).also {
                Log.i("ThumbRangeAudit", "enabled: numeric ranges only; no extra source reads; exports on source close")
            }
        } catch (_: Exception) { null }
    }
}
