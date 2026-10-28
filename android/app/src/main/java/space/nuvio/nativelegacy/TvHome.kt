package space.nuvio.nativelegacy

import android.app.job.JobInfo
import android.app.job.JobParameters
import android.app.job.JobScheduler
import android.app.job.JobService
import android.content.ComponentName
import android.content.ContentUris
import android.content.ContentValues
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.graphics.Bitmap
import android.graphics.Canvas
import android.net.Uri
import android.os.Build
import androidx.tvprovider.media.tv.Channel
import androidx.tvprovider.media.tv.PreviewProgram
import androidx.tvprovider.media.tv.TvContractCompat
import androidx.tvprovider.media.tv.WatchNextProgram
import org.json.JSONArray
import org.json.JSONObject
import java.util.concurrent.Executors

internal object TvHome {
    private const val JOB = 2121
    private val executor = Executors.newSingleThreadExecutor()
    private val watch = TvContractCompat.WatchNextPrograms.CONTENT_URI
    private val preview = TvContractCompat.PreviewPrograms.CONTENT_URI
    private val channels = TvContractCompat.Channels.CONTENT_URI

    fun supported(context: Context): Boolean = try {
        Build.VERSION.SDK_INT >= 26 &&
            context.packageManager.hasSystemFeature(PackageManager.FEATURE_LEANBACK) &&
            context.packageManager.resolveContentProvider(TvContractCompat.AUTHORITY, 0) != null
    } catch (_: Exception) { false }

    // Public image CDNs only: custom poster providers may put API keys in paths.
    private fun artwork(value: String): String {
        val uri = Uri.parse(value)
        val host = uri.host ?: return ""
        return if (uri.scheme == "https" && uri.userInfo == null && uri.query == null && uri.fragment == null &&
            host in setOf("image.tmdb.org", "images.metahub.space", "images.cinemeta.space")) value else ""
    }

    fun publish(context: Context, json: String) {
        if (!supported(context)) return
        val app = context.applicationContext
        executor.execute {
            try {
                val snapshot = JSONObject(json)
                val clean = JSONArray()
                val source = snapshot.getJSONArray("items")
                for (i in 0 until source.length()) {
                    val item = source.getJSONObject(i)
                    if (!item.getString("id").matches(Regex("tt[0-9]{2,10}"))) continue
                    if (item.getString("type") !in setOf("movie", "series")) continue
                    item.put("poster", artwork(item.optString("poster")))
                    item.put("backdrop", artwork(item.optString("backdrop")))
                    if (item.getString("poster").isEmpty() && item.getString("backdrop").isEmpty()) continue
                    clean.put(item)
                }
                snapshot.put("items", clean)
                app.getSharedPreferences("tv-home", 0).edit().putString("snapshot", snapshot.toString()).commit()
                sync(app, snapshot)
                val scheduler = app.getSystemService(JobScheduler::class.java)
                if (snapshot.optInt("profile") > 0) {
                    scheduler.schedule(JobInfo.Builder(JOB, ComponentName(app, TvHomeJob::class.java))
                        .setPeriodic(6 * 60 * 60 * 1000L).build())
                } else scheduler.cancel(JOB)
            } catch (_: Exception) { /* Unsupported or denied provider: never break playback. */ }
        }
    }

    fun refresh(context: Context, finished: () -> Unit) {
        executor.execute {
            try {
                val json = context.getSharedPreferences("tv-home", 0).getString("snapshot", null)
                if (supported(context) && json != null) sync(context, JSONObject(json))
            } catch (_: Exception) { /* Retry on the next scheduled run or app refresh. */ }
            finally { finished() }
        }
    }

    private fun existing(context: Context, uri: Uri): MutableMap<String, Long> {
        val rows = mutableMapOf<String, Long>()
        context.contentResolver.query(uri, arrayOf("_id", "internal_provider_id", "package_name"), null, null, null)?.use { cursor ->
            while (cursor.moveToNext()) if (cursor.getString(2) == context.packageName)
                rows[cursor.getString(1) ?: ""] = cursor.getLong(0)
        }
        return rows
    }

    private fun sync(context: Context, snapshot: JSONObject) {
        if (!supported(context)) return
        val resolver = context.contentResolver
        val profile = snapshot.getInt("profile")
        val items = snapshot.getJSONArray("items")
        val oldWatch = existing(context, watch)
        val oldPreview = existing(context, preview)
        val oldChannels = existing(context, channels)
        if (profile <= 0) {
            for (id in oldWatch.values) resolver.delete(ContentUris.withAppendedId(watch, id), null, null)
            for (id in oldPreview.values) resolver.delete(ContentUris.withAppendedId(preview, id), null, null)
            return
        }
        val channelId = oldChannels["nuvio"] ?: resolver.insert(channels,
            Channel.Builder().setType(TvContractCompat.Channels.TYPE_PREVIEW)
                .setDisplayName("Nuvio").setInternalProviderId("nuvio")
                .setAppLinkIntentUri(Uri.parse(Intent(context, NuvioActivity::class.java)
                    .setAction(Intent.ACTION_MAIN).toUri(Intent.URI_INTENT_SCHEME)))
                .build().toContentValues())?.let(ContentUris::parseId) ?: return
        val prefs = context.getSharedPreferences("tv-home", 0)
        if (!prefs.getBoolean("requested", false)) {
            val drawable = context.packageManager.getApplicationIcon(context.packageName)
            val bitmap = Bitmap.createBitmap(128, 128, Bitmap.Config.ARGB_8888)
            drawable.setBounds(0, 0, 128, 128)
            drawable.draw(Canvas(bitmap))
            try { androidx.tvprovider.media.tv.ChannelLogoUtils.storeChannelLogo(context, channelId, bitmap) }
            finally { bitmap.recycle() }
            // Persist before asking: retries and restarts cannot spam the launcher.
            if (prefs.edit().putBoolean("requested", true).commit())
                TvContractCompat.requestChannelBrowsable(context, channelId)
        }
        for (i in 0 until items.length()) {
            val item = items.getJSONObject(i)
            val key = "$profile/${item.getString("id")}"
            val season = item.getInt("season")
            val episode = item.getInt("episode")
            val path = "nuvio-legacy://play/${item.getString("type")}/${item.getString("id")}" +
                (if (episode > 0 && season > 0) "/$season/$episode" else "") + "?tvprofile=$profile"
            val intent = Intent(context, NuvioActivity::class.java).setAction(Intent.ACTION_VIEW)
                .setData(Uri.parse(path)).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TOP)
            val watchProgram = WatchNextProgram.Builder()
                .setInternalProviderId(key).setTitle(item.getString("title"))
                .setType(if (episode > 0) TvContractCompat.PreviewProgramColumns.TYPE_TV_EPISODE else TvContractCompat.PreviewProgramColumns.TYPE_MOVIE)
                .setIntentUri(Uri.parse(intent.toUri(Intent.URI_INTENT_SCHEME)))
                .setWatchNextType(if (item.getBoolean("next")) TvContractCompat.WatchNextPrograms.WATCH_NEXT_TYPE_NEXT
                    else TvContractCompat.WatchNextPrograms.WATCH_NEXT_TYPE_CONTINUE)
                .setLastPlaybackPositionMillis(item.getInt("position"))
                .setDurationMillis(item.getInt("duration"))
                .setLastEngagementTimeUtcMillis(item.getLong("watched"))
            val previewProgram = PreviewProgram.Builder().setChannelId(channelId)
                .setInternalProviderId(key).setTitle(item.getString("title")).setWeight(items.length() - i)
                .setType(if (episode > 0) TvContractCompat.PreviewProgramColumns.TYPE_TV_EPISODE else TvContractCompat.PreviewProgramColumns.TYPE_MOVIE)
                .setIntentUri(Uri.parse(intent.toUri(Intent.URI_INTENT_SCHEME)))
            if (episode > 0) {
                watchProgram.setSeasonNumber(season).setEpisodeNumber(episode).setEpisodeTitle(item.optString("episodeTitle"))
                previewProgram.setSeasonNumber(season).setEpisodeNumber(episode).setEpisodeTitle(item.optString("episodeTitle"))
            }
            val poster = artwork(item.optString("poster")).ifEmpty { artwork(item.optString("backdrop")) }
            if (poster.isNotEmpty()) {
                watchProgram.setPosterArtUri(Uri.parse(poster))
                previewProgram.setPosterArtUri(Uri.parse(poster))
            }
            val backdrop = artwork(item.optString("backdrop"))
            if (backdrop.isNotEmpty()) {
                watchProgram.setThumbnailUri(Uri.parse(backdrop))
                previewProgram.setThumbnailUri(Uri.parse(backdrop))
            }
            upsert(context, watch, oldWatch.remove(key), watchProgram.build().toContentValues())
            upsert(context, preview, oldPreview.remove(key), previewProgram.build().toContentValues())
        }
        for (id in oldWatch.values) resolver.delete(ContentUris.withAppendedId(watch, id), null, null)
        for (id in oldPreview.values) resolver.delete(ContentUris.withAppendedId(preview, id), null, null)
    }

    private fun upsert(context: Context, uri: Uri, id: Long?, values: ContentValues) {
        if (id == null) context.contentResolver.insert(uri, values)
        else context.contentResolver.update(ContentUris.withAppendedId(uri, id), values, null, null)
    }
}

class TvHomeJob : JobService() {
    override fun onStartJob(params: JobParameters): Boolean {
        TvHome.refresh(this) { jobFinished(params, false) }
        return true
    }
    override fun onStopJob(params: JobParameters): Boolean = true
}
