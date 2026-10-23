"""Offline Kotlin compile and localhost HTTP regression; uses cached dependencies only."""
from pathlib import Path
import http.server
import json
import os
import subprocess
import tempfile
import threading

root = Path(__file__).resolve().parent.parent
kt = root / "android/app/src/main/java/space/nuvio/nativelegacy"
# Reuse cached Kotlin/Media3 lookup helpers without running another test.
prelude = (root / "tests/streamfit_passiva_android.py").read_text().split("with tempfile.TemporaryDirectory", 1)[0]
exec(compile(prelude, "streamfit_passiva_android.py", "exec"))

class Server(http.server.ThreadingHTTPServer):
    daemon_threads = True
    active = peak = 0
    channels = []
    lock = threading.Lock()

class Handler(http.server.BaseHTTPRequestHandler):
    def log_message(self, *args):
        pass

    def do_GET(self):
        if self.path == "/stats":
            with self.server.lock:
                payload = json.dumps([self.server.peak, self.server.channels]).encode()
            self.send_response(200)
            self.send_header("Content-Length", str(len(payload)))
            self.end_headers()
            self.wfile.write(payload)
            return
        if self.path in ("/403", "/458", "/509"):
            self.send_response(int(self.path[1:]))
            self.send_header("Content-Length", "0")
            self.end_headers()
            return
        with self.server.lock:
            self.server.active += 1
            self.server.peak = max(self.server.peak, self.server.active)
            self.server.channels.append(self.path)
        try:
            self.send_response(200)
            self.end_headers()
            self.wfile.write(b"X")
            self.wfile.flush()
            self.connection.recv(1)  # Real stream stays open until client closes.
        finally:
            with self.server.lock:
                self.server.active -= 1

fixture = r'''
package space.nuvio.nativelegacy
import android.net.Uri
import androidx.media3.datasource.*
import java.net.HttpURLConnection
import java.net.URL
import java.util.concurrent.CountDownLatch
import kotlin.concurrent.thread

class HttpStream : DataSource {
    var c: HttpURLConnection? = null
    override fun open(s: DataSpec): Long {
        val x = URL(s.uri.toString()).openConnection() as HttpURLConnection
        c = x
        val code = x.responseCode
        if (code >= 400) throw HttpDataSource.InvalidResponseCodeException(code, "fixture", null, emptyMap(), s, ByteArray(0))
        check(x.inputStream.read() == 'X'.code)
        return -1
    }
    override fun read(b: ByteArray, o: Int, n: Int) = c!!.inputStream.read(b, o, n)
    override fun getUri(): Uri? = null
    override fun addTransferListener(t: TransferListener) {}
    override fun close() { c?.disconnect(); c = null }
}
fun main(args: Array<String>) {
    val base = args[0]
    val upstream = object : DataSource.Factory {
        override fun createDataSource(): DataSource = HttpStream()
    }
    val factory: DataSource.Factory = if (args.getOrNull(1) == "baseline") upstream
        else LimitedDataSource.Factory(upstream)
    fun spec(s: String) = DataSpec(Uri.parse(base + s))
    val a = factory.createDataSource()
    a.open(spec("/A"))
    // The production C debounce selects C, not intermediate B.
    val closed = CountDownLatch(1)
    val closer = thread {
        Thread.sleep(180) // Simulate delayed player/HTTP teardown.
        a.close()
        closed.countDown()
    }
    val c = factory.createDataSource()
    c.open(spec("/C"))
    check(closed.count == 0L) { "new stream opened before old HTTP close" }
    c.close(); closer.join()
    for (status in listOf(403, 458, 509)) {
        try { factory.createDataSource().open(spec("/$status")); error("expected HTTP $status") }
        catch (e: HttpDataSource.InvalidResponseCodeException) { check(e.responseCode == status) }
    }
    // Preflight blocks a provider's HTTP-200 placeholder before reading media.
    val blocked = LimitedDataSource.Factory(object : DataSource.Factory {
        override fun createDataSource(): DataSource = HttpStream()
    }) { 458 }
    try { blocked.createDataSource().open(spec("/placeholder")); error("expected account refusal") }
    catch (e: HttpDataSource.InvalidResponseCodeException) { check(e.responseCode == 458) }
    val cancelled = LimitedDataSource.Factory(object : DataSource.Factory {
        override fun createDataSource(): DataSource = HttpStream()
    }, { true })
    try { cancelled.createDataSource().open(spec("/stale")); error("cancelled source opened") }
    catch (_: java.io.IOException) {}
    val after = factory.createDataSource(); after.open(spec("/retry")); after.close()
    println("PASS: delayed HTTP close, denied account, 403/458/509, Retry releases permit")
}
'''

with tempfile.TemporaryDirectory(prefix="nuvio-conexlim-") as work:
    work = Path(work)
    deps = [sdk, stdlib]
    for name in ("media3-common", "media3-datasource", "media3-exoplayer", "media3-decoder",
                 "media3-extractor", "media3-container", "media3-database", "media3-exoplayer-hls"):
        deps.append(aar_jar("androidx.media3", name, "1.8.0", work))
    deps += [aar_jar("androidx.core", "core", "1.13.1", work),
             jar("androidx.annotation", "annotation-jvm", "1.6.0"),
             aar_jar("androidx.annotation", "annotation-experimental", "1.4.0", work),
             jar("com.google.guava", "guava", "33.3.1-android")]
    activity = work / "NuvioActivity.kt"
    activity.write_text('package space.nuvio.nativelegacy\nclass NuvioActivity : android.app.Activity() { fun devolverFoco() {}; fun nativeQuadros(): Long = -1; fun nativeEtapa() = ""; fun estadoSdl() = ""; fun nativeRecUrl() = "" }')
    sources = [p for p in kt.glob("*.kt") if p.name not in ("NuvioActivity.kt", "RegistroDownload.kt", "Tombstone.kt")]
    kotlinc(sources + [activity], work / "compiled", os.pathsep.join(map(str, deps)))
    print("PASS: real NvPlayer and LimitedDataSource compile against cached Android/Media3")

    replay = work / "Replay.kt"
    replay.write_text(fixture)
    # Use real Media3 interfaces; DataSpec needs Android TextUtils at construction.
    textutils = work / "TextUtils.kt"
    textutils.write_text('package android.text\nobject TextUtils { @JvmStatic fun isEmpty(s: CharSequence?) = s == null || s.isEmpty() }')
    # Compile replay against actual Media3, with executable Java URI substitute.
    # Java interfaces expose Kotlin synthetic properties without duplicate JVM methods.
    uri_java = work / "Uri.java"
    uri_java.write_text('package android.net; public class Uri { private final String s; private Uri(String s){this.s=s;} public static Uri parse(String s){return new Uri(s);} public String toString(){return s;} }')
    stubs = work / "stub"
    stubs.mkdir()
    subprocess.run([str(Path(java).with_name("javac")), "-d", str(stubs), str(uri_java)], check=True)
    # Run against real Media3 DataSpec; its Android dependencies are supplied below.
    kotlinc([kt / "LimitedDataSource.kt", replay, textutils], work / "replay", os.pathsep.join(map(str, [stubs, *deps])))
    server = Server(("127.0.0.1", 0), Handler)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    try:
        command = [java, "-cp", os.pathsep.join(map(str, [stubs, work / "replay", *deps])),
                   "space.nuvio.nativelegacy.ReplayKt", f"http://127.0.0.1:{server.server_port}"]
        before = subprocess.run(command + ["baseline"], capture_output=True, text=True, timeout=30)
        assert before.returncode != 0 and "new stream opened before old HTTP close" in before.stderr
        assert server.peak == 2, server.peak
        print("EXPECTED FAIL before: old factory opens C before A closes; peak=2")
        with server.lock:
            server.peak = 0
            server.channels = []
        subprocess.run(command, check=True, timeout=30)
        assert server.peak == 1, server.peak
        assert server.channels[:2] == ["/A", "/C"], server.channels
        assert "/placeholder" not in server.channels and "/stale" not in server.channels
        print(f"PASS: localhost fake Xtream peak={server.peak}, channels={server.channels}")
    finally:
        server.shutdown()
        server.server_close()
