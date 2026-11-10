package space.nuvio.nativelegacy

import android.util.Log
import java.io.ByteArrayInputStream
import java.io.ByteArrayOutputStream
import java.io.DataInputStream
import java.io.DataOutputStream
import java.io.File
import java.security.MessageDigest
import java.security.SecureRandom
import java.util.zip.GZIPInputStream
import java.util.zip.GZIPOutputStream
import javax.crypto.Cipher
import javax.crypto.spec.GCMParameterSpec
import javax.crypto.spec.SecretKeySpec

// THUMBNAIL MAP (06/11/2026): where a file's keyframes are, so the next play of the SAME file skips the index read
// and fetches exact keyframe blocks (no guessed windows, no header probes). Only numbers: byte offsets, sizes and
// times inside the file; never a picture, never server facts (those stay in ThumbnailHostMemory).
//
// Same file = same bytes: the fingerprint is the file size plus a SHA-256 of its first 64 KB, which for Matroska
// holds the random SegmentUID written when the file was muxed. Offsets belong to the file, not the server, so a map
// made on TorBox serves an identical R2 mirror and the other way round.
//
// Anonymous by construction, ready for a shared store (phase 2): the map is AES-256-GCM encrypted with a key derived
// from the fingerprint, and stored under a different hash of it. Without the file itself a map can be neither read
// nor tied to a film; bucket(id) is the short prefix a shared store would be asked for (k-anonymity).
internal object ThumbnailMap {
    const val VERSION = 1
    const val HEAD_BYTES = 64 * 1024
    private const val MAX_MAPS = 400

    class Data(
        val size: Long, val trackV: Long, val nalLength: Int, val cuesIni: Long, val cuesFim: Long,
        /** [time ns, absolute cluster position, relative block position] x n, as Fonte.cues */
        val cues: LongArray,
        /** cue index -> [block start, block end) */
        val blocks: Map<Int, LongArray>,
    )

    fun fingerprint(size: Long, head: ByteArray): ByteArray {
        val md = MessageDigest.getInstance("SHA-256")
        md.update(ByteArray(8) { (size shr (8 * (7 - it))).toByte() })
        md.update(head, 0, minOf(HEAD_BYTES, head.size))
        return md.digest()
    }
    private fun derive(label: String, fp: ByteArray): ByteArray =
        MessageDigest.getInstance("SHA-256").run { update(label.toByteArray()); update(fp); digest() }
    fun id(fp: ByteArray): String = derive("nuvio-thumb-map-id-v1", fp).joinToString("") { "%02x".format(it) }
    /** What a shared store would be asked for: thousands of files share each bucket. */
    fun bucket(id: String): String = id.take(4)

    // ---- compact encoding: varints, deltas, gzip ----
    private fun DataOutputStream.varint(v0: Long) { var v = v0; while (v and 0x7fL.inv() != 0L) { writeByte(((v and 0x7f) or 0x80).toInt()); v = v ushr 7 }; writeByte(v.toInt()) }
    private fun DataOutputStream.signed(v: Long) = varint((v shl 1) xor (v shr 63))
    private fun DataInputStream.varint(): Long { var r = 0L; var s = 0; while (true) { val b = readUnsignedByte(); r = r or ((b and 0x7f).toLong() shl s); if (b and 0x80 == 0) return r; s += 7; if (s > 63) throw java.io.IOException("varint") } }
    private fun DataInputStream.signed(): Long { val v = varint(); return (v ushr 1) xor -(v and 1) }

    fun encode(d: Data): ByteArray {
        val bo = ByteArrayOutputStream()
        DataOutputStream(GZIPOutputStream(bo)).use { o ->
            o.writeInt(VERSION)
            o.varint(d.size); o.signed(d.trackV); o.varint(d.nalLength.toLong()); o.varint(d.cuesIni); o.varint(d.cuesFim)
            val n = d.cues.size / 3
            o.varint(n.toLong())
            var t = 0L; var p = 0L
            for (k in 0 until n) {
                o.signed(d.cues[k * 3] - t); t = d.cues[k * 3]
                o.signed(d.cues[k * 3 + 1] - p); p = d.cues[k * 3 + 1]
                o.varint(d.cues[k * 3 + 2])
            }
            val ks = d.blocks.keys.filter { it in 0 until n }.sorted()
            o.varint(ks.size.toLong())
            var last = 0
            for (k in ks) {
                val b = d.blocks.getValue(k)
                o.varint((k - last).toLong()); last = k
                o.signed(b[0] - d.cues[k * 3 + 1]); o.varint(b[1] - b[0])
            }
        }
        return bo.toByteArray()
    }

    fun decode(bytes: ByteArray): Data? = try {
        DataInputStream(GZIPInputStream(ByteArrayInputStream(bytes))).use { i ->
            if (i.readInt() != VERSION) return null
            val size = i.varint(); val trackV = i.signed(); val nal = i.varint().toInt(); val ci = i.varint(); val cf = i.varint()
            val n = i.varint().toInt()
            if (n <= 0 || n > 2_000_000) return null
            val cues = LongArray(n * 3)
            var t = 0L; var p = 0L
            for (k in 0 until n) { t += i.signed(); p += i.signed(); cues[k * 3] = t; cues[k * 3 + 1] = p; cues[k * 3 + 2] = i.varint() }
            val nb = i.varint().toInt()
            if (nb < 0 || nb > n) return null
            val blocks = HashMap<Int, LongArray>(nb * 2)
            var k = 0
            repeat(nb) {
                k += i.varint().toInt()
                if (k !in 0 until n) return null
                val ini = cues[k * 3 + 1] + i.signed(); val len = i.varint()
                if (ini < 0 || len <= 0 || ini + len > size) return null
                blocks[k] = longArrayOf(ini, ini + len)
            }
            Data(size, trackV, nal, ci, cf, cues, blocks)
        }
    } catch (_: Exception) { null }

    // ---- sealing: AES-256-GCM, key from the fingerprint, the id as associated data ----
    fun seal(fp: ByteArray, plain: ByteArray): ByteArray {
        val iv = ByteArray(12).also { SecureRandom().nextBytes(it) }
        val c = Cipher.getInstance("AES/GCM/NoPadding")
        c.init(Cipher.ENCRYPT_MODE, SecretKeySpec(derive("nuvio-thumb-map-key-v1", fp), "AES"), GCMParameterSpec(128, iv))
        c.updateAAD(id(fp).toByteArray())
        return iv + c.doFinal(plain)
    }
    fun open(fp: ByteArray, sealed: ByteArray): ByteArray? = try {
        if (sealed.size < 12 + 16) null else {
            val c = Cipher.getInstance("AES/GCM/NoPadding")
            c.init(Cipher.DECRYPT_MODE, SecretKeySpec(derive("nuvio-thumb-map-key-v1", fp), "AES"), GCMParameterSpec(128, sealed, 0, 12))
            c.updateAAD(id(fp).toByteArray())
            c.doFinal(sealed, 12, sealed.size - 12)
        }
    } catch (_: Exception) { null }

    // ---- the on-device store (phase 1) ----
    private fun dir(): File? = NvPlayer.contexto()?.filesDir?.let { File(it, "thumbnail-maps") }

    fun load(fp: ByteArray, base: File? = dir()): Data? {
        val f = File(base ?: return null, id(fp) + ".map")
        if (!f.isFile) return null
        val d = open(fp, try { f.readBytes() } catch (_: Exception) { return null })?.let { decode(it) }
        if (d == null) { f.delete(); return null }
        f.setLastModified(System.currentTimeMillis())
        return d
    }

    fun save(fp: ByteArray, d: Data, base: File? = dir()) {
        val b = base ?: return
        try {
            b.mkdirs()
            val f = File(b, id(fp) + ".map")
            val tmp = File(b, f.name + ".tmp")
            tmp.writeBytes(seal(fp, encode(d)))
            if (!tmp.renameTo(f)) { f.delete(); tmp.renameTo(f) }
            b.listFiles { x -> x.name.endsWith(".map") }?.sortedByDescending { it.lastModified() }?.drop(MAX_MAPS)?.forEach { it.delete() }
            Log.i("nuvio-miniatura", "[thumb-map] saved: ${d.cues.size / 3} cues, ${d.blocks.size} located blocks, ${f.length() / 1024} KB")
        } catch (_: Exception) { }
    }

    fun delete(fp: ByteArray, base: File? = dir()) { base?.let { File(it, id(fp) + ".map").delete() } }
}
