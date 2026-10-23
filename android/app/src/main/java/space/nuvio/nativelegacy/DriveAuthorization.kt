package space.nuvio.nativelegacy

import android.app.Activity
import android.content.Intent
import com.google.android.gms.auth.api.identity.AuthorizationRequest
import com.google.android.gms.auth.api.identity.AuthorizationResult
import com.google.android.gms.auth.api.identity.ClearTokenRequest
import com.google.android.gms.auth.api.identity.Identity
import com.google.android.gms.common.ConnectionResult
import com.google.android.gms.common.GoogleApiAvailability
import com.google.android.gms.common.api.Scope
import org.json.JSONObject

/** Phone authorization only. TV device authorization has its own worker path.
 * The Android client is identified by package + signer, not a web-client ID.
 * Tokens stay in process memory and are never logged or included in backups.
 */
internal class DriveAuthorization(private val activity: Activity) {
    private val state = DriveAuthState()
    private val client by lazy { Identity.getAuthorizationClient(activity) }

    fun begin(request: Long, chooseAccount: Boolean, rejectedToken: String): Boolean {
        if (rejectedToken.length > 4096 || rejectedToken.any { it.code !in 33..126 }) return false
        val ticket = state.begin(request) ?: return false
        activity.runOnUiThread {
            if (!state.current(ticket)) return@runOnUiThread
            if (activity.isFinishing || activity.isDestroyed ||
                GoogleApiAvailability.getInstance().isGooglePlayServicesAvailable(activity) != ConnectionResult.SUCCESS) {
                state.finish(ticket, "unavailable")
                return@runOnUiThread
            }
            if (rejectedToken.isNotEmpty()) {
                try {
                    client.clearToken(ClearTokenRequest.builder().setToken(rejectedToken).build())
                        .addOnSuccessListener { authorize(ticket, chooseAccount) }
                        .addOnFailureListener { state.finish(ticket, "failed") }
                } catch (_: Exception) { state.finish(ticket, "failed") }
            } else authorize(ticket, chooseAccount)
        }
        return true
    }

    private fun authorize(ticket: DriveAuthState.Ticket, chooseAccount: Boolean) {
        if (!state.current(ticket)) return
        if (activity.isFinishing || activity.isDestroyed) { state.finish(ticket, "unavailable"); return }
        try {
            val builder = AuthorizationRequest.builder()
                .setRequestedScopes(listOf(Scope(SCOPE)))
                .setOptOutIncludingGrantedScopes(true)
            if (chooseAccount) builder.setPrompt(AuthorizationRequest.Prompt.SELECT_ACCOUNT)
            client.authorize(builder.build())
                .addOnSuccessListener { result ->
                    if (!state.current(ticket)) return@addOnSuccessListener
                    if (result.hasResolution()) {
                        val pending = result.pendingIntent
                        if (pending == null) state.finish(ticket, "failed")
                        else if (state.waitForResolution(ticket)) {
                            try {
                                @Suppress("DEPRECATION")
                                activity.startIntentSenderForResult(pending.intentSender, REQUEST, null, 0, 0, 0)
                            } catch (_: Exception) {
                                state.takeResolution()
                                state.finish(ticket, "failed")
                            }
                        }
                    } else complete(ticket, result)
                }
                .addOnFailureListener { state.finish(ticket, "failed") }
        } catch (_: Exception) { state.finish(ticket, "failed") }
    }

    fun activityResult(request: Int, result: Int, data: Intent?): Boolean {
        if (request != REQUEST) return false
        val ticket = state.takeResolution() ?: return true
        if (!state.current(ticket)) return true
        if (result != Activity.RESULT_OK) state.finish(ticket, "cancelled")
        else try {
            complete(ticket, client.getAuthorizationResultFromIntent(data))
        } catch (_: Exception) { state.finish(ticket, "failed") }
        return true
    }

    private fun complete(ticket: DriveAuthState.Ticket, result: AuthorizationResult) {
        val token = result.accessToken
        if (result.grantedScopes.none { it == SCOPE } || token.isNullOrEmpty() ||
            token.length > 4096 || token.any { it.code !in 33..126 }) {
            state.finish(ticket, "denied")
        } else state.finish(ticket, "ok", token)
    }

    fun poll(): String? = state.poll()?.let {
        JSONObject().put("request", it.request).put("status", it.status)
            .also { json -> if (it.token.isNotEmpty()) json.put("token", it.token) }.toString()
    }
    fun cancel() = state.cancel()
    fun close() = state.close()

    companion object {
        const val REQUEST = 49119
        const val SCOPE = "https://www.googleapis.com/auth/drive.file"
    }
}
