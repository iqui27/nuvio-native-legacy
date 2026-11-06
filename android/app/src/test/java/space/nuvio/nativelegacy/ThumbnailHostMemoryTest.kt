package space.nuvio.nativelegacy

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder
import java.io.File

class ThumbnailHostMemoryTest {
    @get:Rule val tmp = TemporaryFolder()

    @Test fun keyGroupsAServiceButNotACountry() {
        assertEquals("cloudflarestorage.com", ThumbnailHostMemory.key("1c8e.r2.cloudflarestorage.com"))
        assertEquals("pengu.uk", ThumbnailHostMemory.key("pengu.uk"))
        assertEquals("example.co.uk", ThumbnailHostMemory.key("cdn.example.co.uk"))
        assertEquals("192.168.0.10", ThumbnailHostMemory.key("192.168.0.10"))
        assertEquals(null, ThumbnailHostMemory.key(""))
    }

    @Test fun refusalSurvivesANewInstanceAndCoversSiblingHosts() {
        val f = File(tmp.root, "hosts.txt")
        ThumbnailHostMemory(f).recordRefusal("aaa.r2.cloudflarestorage.com", 1_000)
        val again = ThumbnailHostMemory(f)
        assertTrue(again.refuses("bbb.r2.cloudflarestorage.com", 2_000))
        assertFalse(again.refuses("nexus-272.neur.tb-cdn.st", 2_000))
    }

    @Test fun refusalIsRetriedAfterItsWindow() {
        val f = File(tmp.root, "hosts.txt")
        val m = ThumbnailHostMemory(f, retryMs = 10_000)
        m.recordRefusal("a.r2.cloudflarestorage.com", 0)
        assertTrue(m.refuses("a.r2.cloudflarestorage.com", 9_999))
        assertFalse(m.refuses("a.r2.cloudflarestorage.com", 10_000))
        assertFalse(ThumbnailHostMemory(f, retryMs = 10_000).refuses("a.r2.cloudflarestorage.com", 10_001))
    }

    @Test fun laneCapIsStoredWithItsValueAndOldLinesStillLoad() {
        val f = File(tmp.root, "caps.txt")
        ThumbnailHostMemory(f, retryMs = 1_000).record("a.r2.cloudflarestorage.com", 0, 4)
        assertEquals(4, ThumbnailHostMemory(f, retryMs = 1_000).value("b.r2.cloudflarestorage.com", 10))
        assertEquals(null, ThumbnailHostMemory(f, retryMs = 1_000).value("b.r2.cloudflarestorage.com", 1_000))
        val old = File(tmp.root, "old.txt"); old.writeText("cloudflarestorage.com\t100\n")
        assertTrue(ThumbnailHostMemory(old).refuses("x.r2.cloudflarestorage.com", 200))
    }

    @Test fun missingOrCorruptFileIsHarmless() {
        val f = File(tmp.root, "hosts.txt")
        f.writeText("garbage\nno-tab-here\nx.com\tnot-a-number\n")
        val m = ThumbnailHostMemory(f)
        assertFalse(m.refuses("x.com", 5))
        assertFalse(ThumbnailHostMemory(null).refuses("x.com", 5))
        ThumbnailHostMemory(null).recordRefusal("x.com", 5)   // no file: no crash
    }
}
