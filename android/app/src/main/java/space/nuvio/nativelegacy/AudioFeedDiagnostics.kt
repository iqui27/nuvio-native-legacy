package space.nuvio.nativelegacy

import android.os.SystemClock
import android.util.Log

/** Marker-gated diagnostics only; no codec, pacing or audio-buffer policy. */
internal object AudioFeedDiagnostics {
    @Volatile var enabled=false
    private var lastReport=0L
    @Volatile private var timing: AudioFeedTiming?=null
    @Volatile private var tid=0
    fun select(value: AudioFeedTiming) {
        if(timing===value)return
        timing=value;tid=android.os.Process.myTid();lastReport=0
        val tracing=android.os.Build.VERSION.SDK_INT>=29 && android.os.Trace.isEnabled()
        Log.i("AudioFeedTrace","sinkBoundary thread=${Thread.currentThread().name} tid=$tid traceEnabled=$tracing binUpperMs=1:2:4:8:16:32:64:128:256:512:1024:2048:4096:8192:16384:32768plus")
    }
    fun report(force: Boolean=false) {
        if(!enabled)return
        val now=SystemClock.elapsedRealtime()
        if(!force && now-lastReport<15000)return
        lastReport=now
        val root=NvPlayer.contexto()?.getExternalFilesDir(null)
        if(root!=null && !java.io.File(root,"audio-feed-trace.enabled").isFile) {
            enabled=false;timing=null;Log.i("AudioFeedTrace","disabled: marker removed");return
        }
        timing?.let {Log.i("AudioFeedTrace","elapsedMs=$now tid=$tid ${it.report()}")}
    }
    fun resetContinuity() {if(enabled) timing?.resetContinuity()}
}
