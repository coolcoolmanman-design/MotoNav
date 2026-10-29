package com.example.motonav

import android.content.Context
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.media.MediaMetadata
import android.net.Uri
import java.io.InputStream
import java.util.concurrent.Executors
import java.util.zip.Deflater

/** 专辑封面推送到圆屏（bulk type 5）。
 *  来源：MediaSession 的 METADATA_KEY_ALBUM_ART 位图或 ALBUM_ART_URI（网易云/QQ 音乐均会带）。
 *  编码：居中裁方 → 128×128 RGB565（小端）→ zlib，头 'C''V''R' 版本 宽 高（与 iOS 端一致）。 */
object CoverBridge {
    private const val PX = 128
    private val exec = Executors.newSingleThreadExecutor()
    private var lastKey: String? = null
    private var seq = 0

    /** 每次歌曲文本变化时调用；meta 为 null 表示没有播放会话，清掉封面。 */
    fun push(context: Context, meta: MediaMetadata?) {
        val key = meta?.getString(MediaMetadata.METADATA_KEY_TITLE)
        if (key.isNullOrBlank()) {
            if (lastKey != null) { lastKey = null; send(header(0, 0, ByteArray(0))) }
            return
        }
        if (key == lastKey) return
        lastKey = key
        exec.execute {
            val bmp = meta.getBitmap(MediaMetadata.METADATA_KEY_ALBUM_ART)
                ?: meta.getString(MediaMetadata.METADATA_KEY_ALBUM_ART_URI)?.let { loadUri(context, it) }
            if (bmp == null) { send(header(0, 0, ByteArray(0))); return@execute }
            try {
                val small = scaledSquare(bmp, PX)
                send(header(PX, PX, zlib(rgb565(small))))
            } catch (_: Exception) {
                send(header(0, 0, ByteArray(0)))
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
