package com.example.motonav

import android.content.Context
import android.net.ConnectivityManager
import android.net.Network
import android.net.NetworkCapabilities
import android.net.NetworkRequest
import android.net.wifi.WifiNetworkSpecifier
import android.os.Build
import android.os.Handler
import android.os.Looper
import java.io.OutputStream
import java.net.InetSocketAddress
import java.net.Socket
import java.util.concurrent.atomic.AtomicBoolean

/**
 * SoftAP / Wi‑Fi map acceleration (ported from iOS WifiMapLink on tip 0.16.2.108).
 * Phone joins round-screen SoftAP and can PUT whole basemap packets on TCP:7443.
 * Feature stays optional; BLE remains the fallback path.
 */
object WifiMapBoost {
    private const val PREFS = "motonav"
    private const val KEY = "wifi_map_boost"
    private val main = Handler(Looper.getMainLooper())

    @Volatile var enabled = false
        private set
    @Volatile var softApOn = false
        private set
    @Volatile var reachable = false
        private set
    @Volatile var status = "未启用"
        private set
    @Volatile var ssid = ""
        private set
    @Volatile var host = "192.168.4.1"
        private set
    @Volatile var tcpPort: Int = 7443
        private set

    private var ctx: Context? = null
    private var openRetryLeft = 0
    private val sending = AtomicBoolean(false)
    private var boundNetwork: Network? = null

    fun load(c: Context) {
        ctx = c.applicationContext
        enabled = c.applicationContext.getSharedPreferences(PREFS, 0).getBoolean(KEY, false)
        status = if (enabled) "已开 · 等待圆屏热点" else "未启用"
        if (enabled) requestSoftAP("load", 4)
    }

    fun setEnabled(c: Context, on: Boolean) {
        enabled = on
        c.applicationContext.getSharedPreferences(PREFS, 0).edit().putBoolean(KEY, on).apply()
        if (on) {
            status = "开启中…"
            requestSoftAP("manual", 6)
        } else {
            BleLink.send("#WIFI,0")
            softApOn = false
            reachable = false
            ssid = ""
            status = "未启用"
            releaseNetwork()
        }
    }

    /** Parse BLE `WIFI,…` status lines from the round screen. */
    fun onBleWifiLine(line: String) {
        val parts = line.split(',')
        if (parts.isEmpty() || !parts[0].equals("WIFI", true)) return
        val on = parts.getOrNull(1)?.trim() == "1"
        softApOn = on
        if (!on) {
            reachable = false
            status = if (enabled) "圆屏热点已关" else "未启用"
            return
        }
        // WIFI,1,ssid,psk,host,port (psk never shown in UI)
        ssid = parts.getOrNull(2)?.trim().orEmpty()
        if (parts.size > 4) host = parts[4].trim().ifEmpty { host }
        parts.getOrNull(5)?.toIntOrNull()?.let { tcpPort = it }
        status = if (ssid.isNotEmpty()) "热点 $ssid · 连接中" else "热点已开 · 连接中"
        joinSoftAP(parts.getOrNull(3)?.trim().orEmpty())
    }

    fun requestSoftAP(reason: String, retries: Int) {
        if (!enabled) return
        openRetryLeft = retries
        BleLink.send("#WIFI,1")
        status = "请求热点（$reason）…"
        scheduleOpenRetry()
    }

    private fun scheduleOpenRetry() {
        main.postDelayed({
            if (!enabled || softApOn || openRetryLeft <= 0) return@postDelayed
            openRetryLeft--
            BleLink.send("#WIFI,1")
            if (openRetryLeft == 2) BleLink.send("#WIFI?")
            scheduleOpenRetry()
        }, 1200)
    }

    private fun joinSoftAP(psk: String) {
        val c = ctx ?: return
        if (Build.VERSION.SDK_INT < 29 || ssid.isEmpty()) {
            // Fall back to probing gateway; user may join SoftAP manually.
            probeReachable()
            return
        }
        try {
            val specifier = WifiNetworkSpecifier.Builder()
                .setSsid(ssid)
                .apply { if (psk.isNotEmpty()) setWpa2Passphrase(psk) }
                .build()
            val request = NetworkRequest.Builder()
                .addTransportType(NetworkCapabilities.TRANSPORT_WIFI)
                .removeCapability(NetworkCapabilities.NET_CAPABILITY_INTERNET)
                .setNetworkSpecifier(specifier)
                .build()
            val cm = c.getSystemService(Context.CONNECTIVITY_SERVICE) as ConnectivityManager
            releaseNetwork()
            cm.requestNetwork(request, object : ConnectivityManager.NetworkCallback() {
                override fun onAvailable(network: Network) {
                    boundNetwork = network
                    cm.bindProcessToNetwork(network)
                    probeReachable()
                }
                override fun onLost(network: Network) {
                    if (boundNetwork == network) {
                        reachable = false
                        status = "热点断开"
                        releaseNetwork()
                    }
                }
            })
        } catch (e: Exception) {
            status = "加入热点失败：${e.javaClass.simpleName}"
            probeReachable()
        }
    }

    private fun probeReachable() {
        Thread {
            try {
                Socket().use { s ->
                    s.soTimeout = 1500
                    s.connect(InetSocketAddress(host, tcpPort), 1500)
                }
                main.post {
                    reachable = true
                    status = if (ssid.isNotEmpty()) "热点 $ssid · 已连通" else "WiFi 已通"
                }
            } catch (_: Exception) {
                main.post {
                    reachable = false
                    status = if (ssid.isNotEmpty()) "热点 $ssid · 未连通" else "热点未连通"
                }
            }
        }.start()
    }

    /** Prefer Wi‑Fi whole-packet PUT when SoftAP is up; returns true if queued. */
    fun preferWifi(): Boolean = enabled && softApOn && reachable && host.isNotEmpty()

    fun putMap(data: ByteArray, onDone: (Boolean) -> Unit) {
        if (!preferWifi() || !sending.compareAndSet(false, true)) {
            onDone(false); return
        }
        Thread {
            var ok = false
            try {
                val net = boundNetwork
                val socket = if (net != null) net.socketFactory.createSocket() else Socket()
                socket.use { s ->
                    s.soTimeout = 8000
                    s.connect(InetSocketAddress(host, tcpPort), 2500)
                    val out: OutputStream = s.getOutputStream()
                    // Simple length-prefixed blob (matches iOS TCP whole-packet convention)
                    val header = "PUT ${data.size}\n".toByteArray(Charsets.US_ASCII)
                    out.write(header)
                    out.write(data)
                    out.flush()
                    ok = true
                }
            } catch (_: Exception) {
                ok = false
            } finally {
                sending.set(false)
                main.post { onDone(ok) }
            }
        }.start()
    }

    private fun releaseNetwork() {
        val c = ctx ?: return
        try {
            val cm = c.getSystemService(Context.CONNECTIVITY_SERVICE) as ConnectivityManager
            cm.bindProcessToNetwork(null)
        } catch (_: Exception) { }
        boundNetwork = null
    }
}
