package com.example.motonav

import android.content.Context
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.media.MediaMetadata
import android.net.Uri
import android.util.Log
import java.io.InputStream
import java.util.concurrent.ConcurrentHashMap
import java.util.concurrent.Executors
import java.util.zip.Deflater

/** 专辑封面推送到圆屏（bulk type 5）。
 *  来源依次尝试：MediaSession ALBUM_ART 位图 → ART 位图 → ALBUM_ART_URI → 通知 largeIcon
 *  （网易云/QQ 音乐实测常把封面只放通知 largeIcon，MediaSession 里没有）。
 *  编码：居中裁方 → 128×128 RGB565（小端）→ zlib，头 'C''V''R' 版本 宽 高（与 iOS 端一致）。 */
object CoverBridge {
    private const val PX = 128
    private const val TAG = "MotoNavCover"
    private val exec = Executors.newSingleThreadExecutor()
    private var lastKey: String? = null
    private var seq = 0

    /** 各音乐包名最近一次通知里的封面（由 AmapNotifyListener 喂入），仅兜底用，不 recycle。 */
    val notifyArt = ConcurrentHashMap<String, Bitmap>()

    /** 每次歌曲文本变化时调用；meta 为 null 表示没有播放会话，清掉封面。 */
    fun push(context: Context, pkg: String?, meta: MediaMetadata?) {
        val key = meta?.getString(MediaMetadata.METADATA_KEY_TITLE)
        if (key.isNullOrBlank()) {
            if (lastKey != null) { lastKey = null; send(header(0, 0, ByteArray(0))) }
            return
        }
        val bmp = meta.getBitmap(MediaMetadata.METADATA_KEY_ALBUM_ART)
            ?: meta.getBitmap(MediaMetadata.METADATA_KEY_ART)
        val uri = meta.getString(MediaMetadata.METADATA_KEY_ALBUM_ART_URI)
        val notif = pkg?.let { notifyArt[it] }
        // 去重键带上"封面来源是否可用"：封面比文本晚到（同歌名）时也能补发
        val dedupe = "$key\u0001${bmp != null}\u0001${!uri.isNullOrBlank()}\u0001${notif != null}"
        Log.d(TAG, "push pkg=$pkg title=$key bmp=${bmp != null} uri=$uri notifArt=${notif != null}")
        if (dedupe == lastKey) return
        lastKey = dedupe
        exec.execute {
            val src = bmp
                ?: uri?.takeIf { it.isNotBlank() }?.let { loadUri(context, it) }
                ?: notif
            if (src == null) { Log.d(TAG, "no art source -> send empty"); send(header(0, 0, ByteArray(0))); return@execute }
            try {
                val small = scaledSquare(src, PX)
                val data = zlib(rgb565(small))
                Log.d(TAG, "send cover ${data.size + 6}B from=${src.width}x${src.height}")
                send(header(PX, PX, data))
            } catch (e: Exception) {
                Log.d(TAG, "encode fail: $e"); send(header(0, 0, ByteArray(0)))
            }
        }
    }

    private fun loadUri(context: Context, uriStr: String): Bitmap? = try {
        val stream: InputStream? = when {
            uriStr.startsWith("content:") -> context.contentResolver.openInputStream(Uri.parse(uriStr))
            uriStr.startsWith("file:") -> context.contentResolver.openInputStream(Uri.parse(uriStr))
            uriStr.startsWith("http") -> java.net.URL(uriStr).openStream()
            else -> null
        }
        stream?.use { BitmapFactory.decodeStream(it) }
    } catch (_: Exception) { null }

    private fun scaledSquare(b: Bitmap, px: Int): Bitmap {
        val s = minOf(b.width, b.height)
        val square = if (s == b.width && s == b.height) b
                     else Bitmap.createBitmap(b, (b.width - s) / 2, (b.height - s) / 2, s, s)
        return if (square.width == px && square.height == px) square
               else Bitmap.createScaledBitmap(square, px, px, true)
    }

    private fun rgb565(b: Bitmap): ByteArray {
        val px = PX
        val px32 = IntArray(px * px)
        b.getPixels(px32, 0, px, 0, 0, px, px)
        val out = ByteArray(px * px * 2)
        for (i in px32.indices) {
            val c = px32[i]
            val v = ((c shr 16) and 0xF8) shl 8 or (((c shr 8) and 0xFC) shl 3) or ((c and 0xFF) shr 3)
            out[i * 2] = (v and 0xFF).toByte()
            out[i * 2 + 1] = ((v shr 8) and 0xFF).toByte()
        }
        return out
    }

    private fun zlib(data: ByteArray): ByteArray {
        val d = Deflater()
        d.setInput(data); d.finish()
        val buf = ByteArray(65536)
        var n = 0
        while (!d.finished() && n < buf.size) n += d.deflate(buf, n, buf.size - n)
        d.end()
        return buf.copyOf(n)
    }

    private fun header(w: Int, h: Int, data: ByteArray): ByteArray {
        val head = byteArrayOf('C'.code.toByte(), 'V'.code.toByte(), 'R'.code.toByte(), 1, w.toByte(), h.toByte())
        return head + data
    }

    private fun send(data: ByteArray) {
        BleLink.sendBulk(5, seq and 0xFF, data)
        seq = (seq + 1) and 0xFF
    }
}
