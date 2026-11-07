package space.nuvio.nativelegacy

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import java.net.HttpURLConnection
import java.net.URL
import java.util.concurrent.Executors
import java.util.concurrent.TimeUnit

class MediaHttpBudgetBorrowTest {
    private val host = "bucket.borrow-test.example"
    private fun conn(): HttpURLConnection = URL("http://$host/f").openConnection() as HttpURLConnection
    private fun openThumb(): HttpURLConnection = MediaHttpBudget.open(URL("http://$host/f"), "thumbnail-test", { conn() })
    private fun openPlayer(): HttpURLConnection = MediaHttpBudget.open(URL("http://player.borrow-test.example/f"), "player", { conn() })

    @Test fun ownSlotsFirstThenAtMostThreeBorrowedAlwaysLeavingOneForThePlayer() {
        MediaHttpBudget.allowThumbnailLanes(host)
        val open = ArrayList<HttpURLConnection>()
        try {
            repeat(MediaHttpBudget.EXTRA) { open += openThumb() }
            assertEquals(MediaHttpBudget.LIMIT, MediaHttpBudget.available())          // own slots used first
            repeat(MediaHttpBudget.BORROW_MAX) { open += openThumb() }
            assertEquals(MediaHttpBudget.LIMIT - MediaHttpBudget.BORROW_MAX, MediaHttpBudget.available())
            assertTrue(MediaHttpBudget.available() >= 1)
            // a further thumbnail request waits for an own slot instead of taking the player's last one
            val exec = Executors.newSingleThreadExecutor()
            try {
                val waiting = exec.submit<HttpURLConnection> { openThumb() }
                Thread.sleep(300)
                assertTrue(!waiting.isDone)
                assertEquals(MediaHttpBudget.LIMIT - MediaHttpBudget.BORROW_MAX, MediaHttpBudget.available())
                MediaHttpBudget.close(open.removeAt(0))                                  // an own slot frees
                open += waiting.get(5, TimeUnit.SECONDS)
            } finally { exec.shutdownNow() }
            open += openPlayer()                                                       // the player still gets in
        } finally { open.forEach { MediaHttpBudget.close(it) } }
        assertEquals(MediaHttpBudget.LIMIT, MediaHttpBudget.available())               // borrowed slots returned
    }

    @Test fun nothingIsBorrowedWhileThePlayerWaits() {
        MediaHttpBudget.allowThumbnailLanes(host)
        val open = ArrayList<HttpURLConnection>()
        val exec = Executors.newSingleThreadExecutor()
        try {
            repeat(MediaHttpBudget.LIMIT) { open += openPlayer() }                     // player holds all 4
            repeat(MediaHttpBudget.EXTRA) { open += openThumb() }
            val thumb = exec.submit<HttpURLConnection> { openThumb() }
            Thread.sleep(300)
            assertTrue("no shared slot free, so nothing to borrow", !thumb.isDone)
            thumb.cancel(true)
        } finally { exec.shutdownNow(); open.forEach { MediaHttpBudget.close(it) } }
        assertEquals(MediaHttpBudget.LIMIT, MediaHttpBudget.available())
    }
}
