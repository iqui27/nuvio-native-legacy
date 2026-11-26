package space.nuvio.nativelegacy

/** Preallocated boundary histograms. Bin upper bounds, not exact percentiles. */
internal class AudioFeedTiming {
    private class Histogram {
        private val bins=LongArray(16)
        private var count=0L
        private var max=0L
        fun add(ns: Long) {
            val value=ns.coerceAtLeast(0);var i=0
            while(i<15 && value>(1L shl i)*1000000) i++
            bins[i]++;count++;max=maxOf(max,value)
        }
        fun report(): String {
            var sum=0L;var p99=0L
            for(i in bins.indices) {sum+=bins[i];if(sum*100>=count*99 && count>0) {p99=if(i==15) maxOf(32768,(max+999999)/1000000) else 1L shl i;break}}
            return "n=$count,p99UpperMs=$p99,maxUs=${max/1000},bins=${bins.joinToString(":")}"
        }
    }
    private val gaps=Histogram();private val progressGaps=Histogram()
    private val tap=Histogram();private val sink=Histogram();private val total=Histogram()
    private var previousStart=0L;private var previousProgress=0L
    private var calls=0L;private var progressed=0L;private var accepted=0L
    @Synchronized fun record(start: Long, afterTap: Long, end: Long, bytes: Int, allAccepted: Boolean) {
        if(start>afterTap || afterTap>end)return
        if(previousStart>0) gaps.add(start-previousStart)
        previousStart=start
        if(bytes>0) {
            if(previousProgress>0) progressGaps.add(end-previousProgress)
            previousProgress=end;progressed+=bytes
        }
        calls++;if(allAccepted) accepted++
        tap.add(afterTap-start);sink.add(end-afterTap);total.add(end-start)
    }
    @Synchronized fun resetContinuity() {previousStart=0;previousProgress=0}
    @Synchronized fun report() = "calls=$calls consumedBytes=$progressed accepted=$accepted " +
        "entryGap{${gaps.report()}} consumptionReturnGap{${progressGaps.report()}} " +
        "tap{${tap.report()}} sinkCall{${sink.report()}} wholeCall{${total.report()}}"
}
