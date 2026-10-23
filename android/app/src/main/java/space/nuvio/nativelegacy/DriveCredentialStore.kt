package space.nuvio.nativelegacy

import android.content.Context
import android.security.keystore.KeyGenParameterSpec
import android.security.keystore.KeyProperties
import android.util.Base64
import java.security.KeyStore
import javax.crypto.Cipher
import javax.crypto.KeyGenerator
import javax.crypto.SecretKey
import javax.crypto.spec.GCMParameterSpec

// TV refresh credentials belong to this installation + Google registration +
// Nuvio account/profile. They are never settings-backup content. Phones leave
// token caching to Google Play Services. Android app backup is disabled.
internal class DriveCredentialStore(context: Context) {
    private val preferences = context.getSharedPreferences("nuvio-drive", Context.MODE_PRIVATE)
    private fun name(owner: String, profile: Int): String? =
        if (profile > 0 && owner.matches(Regex("[A-Za-z0-9_-]{1,80}"))) "$owner:$profile" else null
    private fun aad(name: String) = (BuildConfig.GOOGLE_TV_CLIENT_ID + ":" + name).toByteArray(Charsets.UTF_8)

    private fun key(create: Boolean): SecretKey? {
        val store = KeyStore.getInstance("AndroidKeyStore").apply { load(null) }
        (store.getKey(ALIAS, null) as? SecretKey)?.let { return it }
        if (!create) return null
        return KeyGenerator.getInstance(KeyProperties.KEY_ALGORITHM_AES, "AndroidKeyStore").apply {
            init(KeyGenParameterSpec.Builder(ALIAS, KeyProperties.PURPOSE_ENCRYPT or KeyProperties.PURPOSE_DECRYPT)
                .setBlockModes(KeyProperties.BLOCK_MODE_GCM)
                .setEncryptionPaddings(KeyProperties.ENCRYPTION_PADDING_NONE)
                .setRandomizedEncryptionRequired(true).build())
        }.generateKey()
    }

    @Synchronized fun read(owner: String, profile: Int): String? {
        val name = name(owner, profile) ?: return null
        return try {
            val value = preferences.getString(name, null) ?: return null
            if (value.length > 12000) return null
            val bytes = Base64.decode(value, Base64.NO_WRAP)
            if (bytes.size !in 29..8220) return null
            val cipher = Cipher.getInstance("AES/GCM/NoPadding")
            cipher.init(Cipher.DECRYPT_MODE, key(false) ?: return null, GCMParameterSpec(128, bytes, 0, 12))
            cipher.updateAAD(aad(name))
            val plain = cipher.doFinal(bytes, 12, bytes.size - 12)
            try { String(plain, Charsets.UTF_8).takeIf(::validToken) }
            finally { plain.fill(0) }
        } catch (_: Exception) { null }
    }

    @Synchronized fun write(owner: String, profile: Int, token: String): Boolean {
        val name = name(owner, profile) ?: return false
        if (token.isEmpty()) return preferences.edit().remove(name).commit()
        if (!validToken(token)) return false
        return try {
            val cipher = Cipher.getInstance("AES/GCM/NoPadding")
            cipher.init(Cipher.ENCRYPT_MODE, key(true))
            cipher.updateAAD(aad(name))
            val plain = token.toByteArray(Charsets.UTF_8)
            val encrypted = try { cipher.doFinal(plain) } finally { plain.fill(0) }
            if (cipher.iv.size != 12) return false
            preferences.edit().putString(name, Base64.encodeToString(cipher.iv + encrypted, Base64.NO_WRAP)).commit()
        } catch (_: Exception) { false }
    }

    companion object {
        private const val ALIAS = "nuvio-drive-refresh-v1"
        private fun validToken(token: String) = token.length in 1..4096 && token.all { it.code in 33..126 }
    }
}
