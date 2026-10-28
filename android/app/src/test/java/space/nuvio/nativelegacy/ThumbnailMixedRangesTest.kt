package space.nuvio.nativelegacy
import org.junit.Assert.*
import org.junit.Test

class ThumbnailMixedRangesTest {
    @Test fun retainsProbesAndFitsCompletePayloadsWithinBudget() {
        val probes = listOf(longArrayOf(0,99))
        val result = thumbnailMixedRanges(probes,listOf(longArrayOf(1000,8L shl 20),longArrayOf(10000,10999)))
        assertEquals(2,result.size)
        assertTrue(result.any { it.contentEquals(probes[0]) })
        assertTrue(result.sumOf { it[1]-it[0]+1 } <= (8L shl 20))
    }
    @Test fun capsRangeCountAndAvoidsContainedDuplicates() {
        val probes = (0 until 31).map { longArrayOf(it*1000L,it*1000L+99) }
        val result = thumbnailMixedRanges(probes,listOf(longArrayOf(0,50),longArrayOf(50000,50099),longArrayOf(60000,60099)))
        assertEquals(32,result.size)
        assertTrue(result.any { it[0]==50000L })
    }
}
