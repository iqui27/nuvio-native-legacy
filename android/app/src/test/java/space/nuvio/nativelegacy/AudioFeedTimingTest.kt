package space.nuvio.nativelegacy

import org.junit.Assert.*
import org.junit.Test

class AudioFeedTimingTest {
    @Test fun separatesLateEntryFromBlockingSinkCall() {
        val t=AudioFeedTiming()
        t.record(1000000,2000000,3000000,100,true)
        t.record(901000000,902000000,903000000,100,true)
        val report=t.report()
        assertTrue(report.contains("entryGap{n=1,p99UpperMs=1024,maxUs=900000"))
        assertTrue(report.contains("sinkCall{n=2,p99UpperMs=1,maxUs=1000"))
    }
    @Test fun distinguishesBackpressureFromConsumption() {
        val t=AudioFeedTiming()
        t.record(1000000,1000000,2000000,100,true)
        t.record(2000000,2000000,12000000,0,false)
        t.record(12000000,12000000,13000000,100,true)
        val report=t.report()
        assertTrue(report.contains("calls=3 consumedBytes=200 accepted=2"))
        assertTrue(report.contains("consumptionReturnGap{n=1,p99UpperMs=16,maxUs=11000"))
        assertTrue(report.contains("sinkCall{n=3,p99UpperMs=16,maxUs=10000"))
    }
    @Test fun pauseResetExcludesIdleGapAndInvalidTimingDoesNotThrow() {
        val t=AudioFeedTiming()
        t.record(1000000,1000000,2000000,10,true)
        t.resetContinuity()
        t.record(9000000000,9000000000,9001000000,10,true)
        t.record(5,4,3,0,false)
        assertTrue(t.report().contains("entryGap{n=0"))
        assertTrue(t.report().contains("calls=2"))
    }
    @Test fun overflowBucketStillReportsAnUpperBound() {
        val t=AudioFeedTiming()
        t.record(1,1,40000000001,1,true)
        assertTrue(t.report().contains("p99UpperMs=40000,maxUs=40000000"))
    }
}
