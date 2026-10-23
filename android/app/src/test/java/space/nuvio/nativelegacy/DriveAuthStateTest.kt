package space.nuvio.nativelegacy

import org.junit.Assert.*
import org.junit.Test

class DriveAuthStateTest {
    @Test fun authorizationResultDoesNotRouteToDocumentPickers() {
        assertNotEquals(FontImport.PEDIDO, DriveAuthorization.REQUEST)
        assertNotEquals(RegistroDownload.PEDIDO, DriveAuthorization.REQUEST)
    }

    @Test fun cancelledProfileCannotReceiveALateToken() {
        val state = DriveAuthState()
        val first = state.begin(10)!!
        state.cancel()
        val second = state.begin(20)!!
        state.finish(first, "ok", "old-profile-token")
        assertNull(state.poll())
        assertTrue(state.current(second))
        state.finish(second, "ok", "current-profile-token")
        assertEquals(DriveAuthState.Result(20, "ok", "current-profile-token"), state.poll())
        assertNull(state.poll())
    }

    @Test fun lateActivityResultCannotCompleteANewRequest() {
        val state = DriveAuthState()
        val old = state.begin(1)!!
        assertTrue(state.waitForResolution(old))
        state.cancel()
        assertNull(state.begin(2))
        assertEquals(old, state.takeResolution())
        assertFalse(state.current(old))
        val current = state.begin(2)!!
        state.finish(old, "ok", "stale-token")
        assertNull(state.poll())
        state.finish(current, "cancelled")
        assertEquals(DriveAuthState.Result(2, "cancelled"), state.poll())
    }

    @Test fun cancelClearsAnAlreadyQueuedToken() {
        val state = DriveAuthState()
        state.finish(state.begin(1)!!, "ok", "private-token")
        state.cancel()
        assertNull(state.poll())
        assertNotNull(state.begin(2))
    }

    @Test fun repeatedRequestNumberStillHasANewGeneration() {
        val state = DriveAuthState()
        val first = state.begin(1)!!
        state.cancel()
        val second = state.begin(1)!!
        assertNotEquals(first, second)
        state.finish(first, "failed")
        assertNull(state.poll())
        assertTrue(state.current(second))
    }

    @Test fun noOverlappingRequestsOrTokenQueueGrowth() {
        val state = DriveAuthState()
        assertNull(state.begin(0))
        assertNull(state.begin(-1))
        val ticket = state.begin(1)!!
        assertNull(state.begin(2))
        state.finish(ticket, "ok", "one-token")
        state.finish(ticket, "ok", "duplicate-token")
        assertNull(state.begin(2))
        assertEquals("one-token", state.poll()!!.token)
        assertNotNull(state.begin(2))
    }

    @Test fun closedActivityDropsTasksAndQueuedTokens() {
        val state = DriveAuthState()
        val ticket = state.begin(1)!!
        state.close()
        state.finish(ticket, "ok", "destroyed-activity-token")
        assertNull(state.poll())
        assertNull(state.begin(2))
    }
}
