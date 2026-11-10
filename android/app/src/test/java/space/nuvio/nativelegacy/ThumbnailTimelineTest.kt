package space.nuvio.nativelegacy
import org.junit.Assert.*
import org.junit.Test

class ThumbnailTimelineTest {
    @Test fun overlapIsExclusiveAndScopesCloseExactlyOnce() {
        var t = 0L
        val clock = ThumbnailTimeline { t }
        clock.start()
        t=10; val decode=clock.enter(ThumbnailTimeline.State.DECODE_ONLY)
        t=20; val header=clock.enter(ThumbnailTimeline.State.HEADER)
        t=30; val body=clock.enter(ThumbnailTimeline.State.BODY)
        t=50; header.close()
        t=60; body.close(); body.close()
        t=70; decode.close()
        t=80
        val v=clock.snapshot()
        assertEquals(30,v[0]); assertEquals(10,v[1]); assertEquals(20,v[5]); assertEquals(20,v[7])
        assertEquals(80,v.last()); assertEquals(v.last(),v.dropLast(1).sum())
    }
    @Test fun alreadyActivePrewarmAndResetPreserveCoverage() {
        var t=0L
        val clock=ThumbnailTimeline { t }
        val header=clock.enter(ThumbnailTimeline.State.HEADER)
        clock.start()
        t=100; header.close()
        assertEquals(100,clock.snapshot()[1])
        clock.start(); t=150
        val v=clock.snapshot()
        assertEquals(50,v.last()); assertEquals(v.last(),v.dropLast(1).sum())
    }
}
