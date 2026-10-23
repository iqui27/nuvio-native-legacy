package space.nuvio.nativelegacy

import android.net.Uri
import androidx.media3.exoplayer.hls.playlist.HlsPlaylistTracker
import java.io.IOException
import androidx.media3.common.PlaybackException
import androidx.media3.common.Player
import org.junit.Assert.*
import org.junit.Test
import java.lang.reflect.Proxy

class LiveWindowRecoveryTest {
    private class Fixture {
        val calls = ArrayList<String>()
        val status = ArrayList<Long>()
        val scheduled = LinkedHashMap<Runnable, Long>()
        var active = true
        var isLive = true
        var failed = 0
        var throwOnPrepare = false
        var playbackState = Player.STATE_BUFFERING
        var afterPrepare: (() -> Unit)? = null
        // Strict Player boundary: stop is allowed only at terminal timeout.
        // URL replacement, release, play/pause and track changes remain forbidden.
        val player = Proxy.newProxyInstance(Player::class.java.classLoader,
            arrayOf(Player::class.java)) { _, method, _ ->
            when (method.name) {
                "isCurrentMediaItemLive" -> return@newProxyInstance isLive
                "getPlaybackState" -> return@newProxyInstance playbackState
                "stop" -> { calls.add("stop"); playbackState = Player.STATE_IDLE }
                "seekToDefaultPosition" -> calls.add("live position")
                "prepare" -> {
                    calls.add("prepare")
                    if (throwOnPrepare) throw IllegalStateException("stopped player")
                    afterPrepare?.invoke()
                }
                else -> error("Unexpected Player operation: ${method.name}")
            }
            null
        } as Player
        val recovery = LiveWindowRecovery(
            schedule = { task, delay -> scheduled[task] = delay },
            unschedule = { task -> scheduled.remove(task) },
            active = { active },
            status = { status.add(it) },
        )
        fun recover(now: Long = 0, code: Int = PlaybackException.ERROR_CODE_BEHIND_LIVE_WINDOW, timeoutMs: Long = 8_000L, cause: Throwable? = null) =
            recovery.recover(player, PlaybackException("fixture", cause, code), now, timeoutMs) { calls.add("failed"); failed++ }
        fun timeout(): Runnable = scheduled.keys.single()
    }

    @Test fun expiredLiveWindowSeeksAndPreparesSamePlayerWithoutReplacingSource() {
        val f = Fixture()
        assertTrue(f.recover())
        assertEquals(listOf("live position", "prepare"), f.calls)
        assertEquals(0, f.failed)
        assertTrue(f.recovery.ready())
        assertTrue(f.scheduled.isEmpty())
        assertFalse(f.recovery.ready())
        assertEquals(listOf(8_000L, 0L), f.status)
    }

    @Test fun ordinaryNetworkAuthenticationAndDecoderErrorsKeepNormalFallback() {
        val f = Fixture()
        for (code in listOf(2000, 2001, 2002, 2004, 4001, 4003)) assertFalse(f.recover(code = code))
        assertTrue(f.calls.isEmpty())
        assertTrue(f.status.isEmpty())
        assertTrue(f.scheduled.isEmpty())
    }

    @Test fun stalledRecoveryFallsBackOnceWithinNativeWatchdogDeadline() {
        val f = Fixture()
        assertTrue(f.recover())
        assertEquals(8_000L, f.scheduled.values.single())
        val task = f.timeout()
        task.run()
        task.run()
        assertEquals(1, f.failed)
        assertEquals(listOf("live position", "prepare", "stop", "failed"), f.calls)
        assertEquals(listOf(8_000L, 0L), f.status)
    }

    @Test fun readyInvalidatesAlreadyQueuedTimeout() {
        val f = Fixture()
        assertTrue(f.recover())
        val old = f.timeout()
        assertTrue(f.recovery.ready())
        old.run()
        assertEquals(0, f.failed)
    }

    @Test fun stoppedOrReplacedSessionCannotRecoverOrReportFailure() {
        val f = Fixture()
        assertTrue(f.recover())
        val task = f.timeout()
        f.active = false
        f.recovery.cancel()
        task.run()
        assertFalse(f.recover())
        assertEquals(0, f.failed)
        assertTrue(f.scheduled.isEmpty())
        assertEquals(listOf("live position", "prepare"), f.calls)
    }

    @Test fun sessionInvalidationAloneAlsoGuardsLateTimeout() {
        val f = Fixture()
        assertTrue(f.recover())
        f.active = false
        f.timeout().run()
        assertEquals(0, f.failed)
    }

    @Test fun repeatedErrorsBeforeReadyUseNormalFallbackInsteadOfLooping() {
        val f = Fixture()
        assertTrue(f.recover())
        assertFalse(f.recover(1))
        assertEquals(listOf("live position", "prepare"), f.calls)
    }

    @Test fun rapidSuccessAndFailureCyclesAreBoundedButLaterOutageCanRecover() {
        val f = Fixture()
        assertTrue(f.recover(0)); f.recovery.ready()
        assertTrue(f.recover(10_000)); f.recovery.ready()
        assertFalse(f.recover(59_999))
        assertEquals(4, f.calls.size)
        assertTrue(f.recover(60_000))
        assertEquals(6, f.calls.size)
    }

    @Test fun oldTimeoutCannotAbortNewRecovery() {
        val f = Fixture()
        assertTrue(f.recover())
        val old = f.timeout()
        f.recovery.ready()
        assertTrue(f.recover(1))
        old.run()
        assertEquals(0, f.failed)
        assertTrue(f.recovery.ready())
    }

    @Test fun synchronousPlayerFailureLeavesNoHiddenRetryPending() {
        val f = Fixture()
        f.throwOnPrepare = true
        assertFalse(f.recover())
        assertTrue(f.scheduled.isEmpty())
        assertEquals(0, f.failed)
        assertFalse(f.recovery.ready())
    }

    @Test fun synchronousReadyDoesNotLeaveWatchdogArmed() {
        val f = Fixture()
        f.afterPrepare = { f.recovery.ready() }
        assertTrue(f.recover())
        assertTrue(f.scheduled.isEmpty())
        assertEquals(0, f.failed)
    }
    @Test fun hlsDeadlineMatchesNativeWatchdogPolicy() {
        for ((segment, expected) in listOf(-1 to 8000L, 0 to 8000L, 6000 to 30000L,
            12000 to 36000L, 13000 to 39000L, Int.MAX_VALUE to 60000L))
            assertEquals(expected, LiveWindowRecovery.timeoutForSegment(segment))
    }

    @Test fun recoveryAfterOldEightSecondDeadlineDoesNotReportFailure() {
        val f = Fixture()
        val timeout = LiveWindowRecovery.timeoutForSegment(12000)
        assertTrue(f.recover(timeoutMs = timeout))
        val task = f.timeout()
        assertEquals(36000L, f.scheduled[task])
        assertEquals(listOf(36000L), f.status)
        // Logs70 reached READY after 8.9 seconds. It remains within this deadline.
        assertTrue(f.recovery.ready())
        task.run()
        assertEquals(0, f.failed)
        assertEquals(listOf("live position", "prepare"), f.calls)
        assertEquals(listOf(36000L, 0L), f.status)
    }

    @Test fun readyStateWinsWhenListenerCallbackHasNotArrived() {
        val f = Fixture()
        assertTrue(f.recover())
        f.playbackState = Player.STATE_READY
        f.timeout().run()
        assertEquals(0, f.failed)
        assertEquals(listOf("live position", "prepare"), f.calls)
        assertEquals(listOf(8000L, 0L), f.status)
    }

    @Test fun callerCannotRequestAnUnboundedRecovery() {
        val f = Fixture()
        assertTrue(f.recover(timeoutMs = Long.MAX_VALUE))
        assertEquals(60000L, f.scheduled.values.single())
        assertEquals(listOf(60000L), f.status)
        f.timeout().run()
        assertEquals(Player.STATE_IDLE, f.playbackState)
        assertEquals(listOf("live position", "prepare", "stop", "failed"), f.calls)
    }

    private fun playlistReset() = HlsPlaylistTracker.PlaylistResetException(Uri.parse("https://example.test/live.m3u8"))

    @Test fun loggedPlaylistResetRecoversSameLivePlayer() {
        val f = Fixture()
        assertTrue(f.recover(code = 2000, cause = IOException(playlistReset())))
        assertEquals(listOf("live position", "prepare"), f.calls)
        assertTrue(f.recovery.ready())
        assertEquals(0, f.failed)
        assertTrue(f.scheduled.isEmpty())
    }

    @Test fun playlistResetDoesNotChangeVodOrOtherErrorPolicies() {
        val f = Fixture()
        f.isLive = false
        assertFalse(f.recover(code = 2000, cause = playlistReset()))
        f.isLive = true
        assertFalse(f.recover(code = 2004, cause = playlistReset()))
        assertFalse(f.recover(code = 2000, cause = IOException("PlaylistResetException")))
        assertFalse(f.recover(code = 2000,
            cause = HlsPlaylistTracker.PlaylistStuckException(Uri.parse("https://example.test/live.m3u8"))))
        assertTrue(f.calls.isEmpty())
        assertTrue(f.status.isEmpty())
    }

    @Test fun resetAndExpiredWindowShareRetryBudget() {
        val f = Fixture()
        assertTrue(f.recover(code = 2000, cause = playlistReset()))
        assertFalse(f.recover(now = 1, code = 2000, cause = playlistReset()))
        f.recovery.ready()
        assertTrue(f.recover(now = 10_000))
        f.recovery.ready()
        assertFalse(f.recover(now = 59_999, code = 2000, cause = playlistReset()))
        assertTrue(f.recover(now = 60_000, code = 2000, cause = playlistReset()))
        assertEquals(6, f.calls.size)
    }

    @Test fun failedResetStopsBeforeReportingSourceFailure() {
        val f = Fixture()
        assertTrue(f.recover(code = 2000, cause = playlistReset(), timeoutMs = 36_000L))
        val task = f.timeout()
        assertEquals(36_000L, f.scheduled[task])
        task.run()
        task.run()
        assertEquals(listOf("live position", "prepare", "stop", "failed"), f.calls)
        assertEquals(1, f.failed)
    }

    @Test fun replacedSessionCannotRecoverResetOrFireItsOldTimeout() {
        val f = Fixture()
        assertTrue(f.recover(code = 2000, cause = playlistReset()))
        val task = f.timeout()
        f.active = false
        task.run()
        assertFalse(f.recover(now = 60_000, code = 2000, cause = playlistReset()))
        assertEquals(listOf("live position", "prepare"), f.calls)
        assertEquals(0, f.failed)
    }
}
