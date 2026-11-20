package space.nuvio.nativelegacy

import androidx.media3.common.C
import androidx.media3.common.MediaItem
import androidx.media3.common.MimeTypes
import androidx.media3.common.util.Util
import org.junit.Assert.*
import org.junit.Test

// #802: itemMidia() has to route a signed MPD nested in a query param
// (file=canal.mpd%3Ftoken%3Dabc) to DASH, the route the base already took.
// Media3's real Util.inferContentTypeForUriAndMimeType decides from the
// MediaItem the production builder emits.
class MpdUriRouteTest {
    private fun route(url: String): Int {
        val baixa = url.lowercase()
        val item = MediaItem.Builder().setUri(url)
        if (baixa.contains(".m3u8") || baixa.contains("m3u8?")) item.setMimeType(MimeTypes.APPLICATION_M3U8)
        else if (NvPlayer.isMpdUri(url)) item.setMimeType(MimeTypes.APPLICATION_MPD)
        val built = item.build()
        return Util.inferContentTypeForUriAndMimeType(built.localConfiguration!!.uri,
            built.localConfiguration!!.mimeType)
    }

    @Test fun signedMpdInFileParamRoutesDash() {
        for (url in listOf(
            "https://media.example/api/live?file=canal.mpd%3Ftoken%3Dabc",
            "https://media.example/api/live?file=https%3A%2F%2Fcdn.example%2Fx%2Fcanal.mpd%3Ft%3D1",
        )) assertEquals("esperava DASH: $url", C.CONTENT_TYPE_DASH, route(url))
    }

    @Test fun mpdInPathOrFileParamRoutesDash() {
        for (url in listOf(
            "https://media.example/vod/manifest.mpd",
            "https://media.example/live/sessao.MPD?token=abc",
            "https://media.example/api/live?file=canal.mpd",
            "https://media.example/api/live?file=canal.mpd&x=1",
            "https://media.example/film.mp4?next=x.mpd",
            "https://media.example/film.mp4?ref=https%3A%2F%2Fother.example%2Flive.mpd",
        )) assertEquals("esperava DASH: $url", C.CONTENT_TYPE_DASH, route(url))
    }

    @Test fun nonMpdStaysProgressive() {
        for (url in listOf(
            "https://media.example/film.mp4?title=Dash",
            "https://media.example/film.mp4#dash",
            "https://dash-cache.example/film.ts",
            "https://media.example/mp4/arquivo.mp4?sig=xyz",
            "https://media.example/api/live?file=canal.mpdx",
            "https://media.example/api/live?file=canal.mpdx%3Ftoken%3Dabc",
            "https://media.example/api/live?provider=abc",
        )) assertEquals("esperava progressivo: $url", C.CONTENT_TYPE_OTHER, route(url))
    }
}
