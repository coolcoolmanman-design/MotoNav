package com.example.motonav

import android.annotation.SuppressLint
import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCallback
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattDescriptor
import android.bluetooth.BluetoothManager
import android.bluetooth.BluetoothProfile
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanFilter
import android.bluetooth.le.ScanResult
import android.bluetooth.le.ScanSettings
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.os.Handler
import android.os.IBinder
import android.os.Looper
import android.util.Log
import androidx.core.app.NotificationCompat
import java.io.BufferedWriter
import java.io.File
import java.io.FileWriter
import java.util.UUID

/**
 * 临时 BLE 抓包探针：前台 Service 承载扫描（Android 12+ 后台扫描会被静默丢弃结果）。
 * 通过广播启动，不干扰圆屏连接（自动跳过 MotoNav 前缀设备）。
 *   adb shell am broadcast -a com.example.motonav.SNIFF_TPMS
 *   adb shell am broadcast -a com.example.motonav.SNIFF_TPMS_STOP
 * 日志过滤：adb logcat -s TpmsSniffer:*
 */
const val TPMS_CHAN = "tpms_sniff"

/** 前台 Service：持有扫描生命周期，保证 Android 12+ 能持续收到扫描结果。 */
class TpmsSnifferService : Service() {
    override fun onBind(intent: Intent?): IBinder? = null

    @SuppressLint("MissingPermission")
    override fun onCreate() {
        super.onCreate()
        val nm = getSystemService(NotificationManager::class.java)
        if (nm.getNotificationChannel(TPMS_CHAN) == null) {
            val ch = NotificationChannel(TPMS_CHAN, "TPMS 抓包", NotificationManager.IMPORTANCE_LOW)
            nm.createNotificationChannel(ch)
        }
    }

    @SuppressLint("MissingPermission")
    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        when (intent?.action) {
            TpmsSniffer.ACTION_START -> {
                val pi = PendingIntent.getActivity(
                    this, 0,
                    packageManager.getLaunchIntentForPackage(packageName),
                    PendingIntent.FLAG_IMMUTABLE
                )
                val n = NotificationCompat.Builder(this, TPMS_CHAN)
                    .setContentTitle("TPMS 抓包中")
                    .setContentText("正在扫描胎压气嘴…（开车/触发气嘴）")
                    .setSmallIcon(android.R.drawable.ic_menu_compass)
                    .setContentIntent(pi)
                    .setOngoing(true)
                    .build()
                startForeground(1, n)
                TpmsSniffer.start(this)
            }
            TpmsSniffer.ACTION_STOP -> {
                TpmsSniffer.stopAll()
                stopForeground(STOP_FOREGROUND_REMOVE)
                stopSelf()
            }
        }
        return START_STICKY
    }
}

/** 广播接收器：仅负责把 intent 转成前台 Service 启动（类名不含 $，便于 adb 显式触发）。 */
class TpmsSnifferReceiver : BroadcastReceiver() {
    override fun onReceive(ctx: Context, intent: Intent) {
        if (intent.action == "com.example.motonav.TPMS_TEST") {
            TpmsManager.sendTestTp()      // adb 触发的 #TP 显示链路测试注入
            return
        }
        val i = Intent(ctx, TpmsSnifferService::class.java).setAction(intent.action)
        ctx.startForegroundService(i)
    }
}

object TpmsSniffer {
    private const val TAG = "TpmsSniffer"
    const val ACTION_START = "com.example.motonav.SNIFF_TPMS"
    const val ACTION_STOP = "com.example.motonav.SNIFF_TPMS_STOP"
    private const val SCAN_MS = 45000L
    private const val GATT_PER = 15000L
    private const val CCCD = "00002902-0000-1000-8000-00805f9b34fb"
    private val handler = Handler(Looper.getMainLooper())
    private var scanner: android.bluetooth.le.BluetoothLeScanner? = null
    private var scanning = false
    private val watchPrefix = "MotoNav"
    private val pending = LinkedHashSet<String>()
    private val gatts = mutableListOf<BluetoothGatt>()
    private var out: BufferedWriter? = null
    private var appCtx: Context? = null
    private val gattStarted = mutableSetOf<String>()
    // 抓包目标 MAC（按需本地填写，脱敏不入库；留空 = 不过滤，抓全部 TPMS 特征广播）
    private val targetMacs = emptySet<String>()
    // 胎压相关服务 UUID：0xFFB0(GATT 型, 如 Hi-L-120YV00) / 0xFBB0(ZEEPIN/TP630 被动广播型) / 0xFD2D(部分通用型)
    private val tpmsSvc = setOf(
        UUID.fromString("0000ffb0-0000-1000-8000-00805f9b34fb"),
        UUID.fromString("0000fbb0-0000-1000-8000-00805f9b34fb"),
        UUID.fromString("0000fd2d-0000-1000-8000-00805f9b34fb")
    )
    private val tpmsNameKw = listOf("HI-L", "TPMS", "ZEEPIN", "PRESSURE", "TYRE", "TIRE")
    private const val MAX_GATT = 4

    /** 同时写 logcat 和手机文件（/sdcard/Android/data/<pkg>/files/tpms_scan.txt），绕开 logcat 环形缓冲丢失 */
    private fun emit(line: String) {
        Log.i(TAG, line)
        try { out?.append(line); out?.append("\n"); out?.flush() } catch (_: Exception) {}
    }

    @SuppressLint("MissingPermission")
    fun start(ctx: Service) {
        val c = ctx.applicationContext
        appCtx = c
        gattStarted.clear()
        val mgr = c.getSystemService(Context.BLUETOOTH_SERVICE) as? BluetoothManager
        val ad = mgr?.adapter
        if (ad == null || !ad.isEnabled) { Log.i(TAG, "=== BT off / null, abort ==="); return }
        scanner = ad.bluetoothLeScanner
        scanning = true
        try {
            val f = ctx.getExternalFilesDir(null)?.resolve("tpms_scan.txt")
            out = f?.let { BufferedWriter(FileWriter(it, false)) }
            emit("FILE=${f?.absolutePath}")
        } catch (_: Exception) { out = null }
        emit("=== TPMS SNIFF START (scan ${SCAN_MS}ms) scanner=${scanner != null} btName=${ad.name} ===")
        val settings = ScanSettings.Builder().setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY).build()
        try {
            scanner?.stopScan(scanCb)
        } catch (_: Exception) {}
        try {
            // 部分设备 mMaxScanFilters=0，传 null 会被底层静默丢弃；用空列表更稳
            scanner?.startScan(emptyList<ScanFilter>(), settings, scanCb)
            Log.i(TAG, "startScan(emptyList) issued")
        } catch (e: Exception) {
            Log.i(TAG, "startScan FAILED: ${e.javaClass.simpleName}: ${e.message}")
            try {
                @Suppress("UNCHECKED_CAST")
                scanner?.startScan(null as List<ScanFilter>?, settings, scanCb)
                Log.i(TAG, "startScan(null) fallback issued")
            } catch (e2: Exception) {
                Log.i(TAG, "startScan(null) ALSO FAILED: ${e2.javaClass.simpleName}: ${e2.message}")
            }
        }
        handler.postDelayed(object : Runnable {
            override fun run() {
                if (scanning) {
                    Log.i(TAG, "HEARTBEAT scanning alive, candidates=${pending.size}")
                    handler.postDelayed(this, 10000)
                }
            }
        }, 10000)
        handler.postDelayed({ finishScan(c) }, SCAN_MS)
    }

    private val scanCb = object : ScanCallback() {
        @SuppressLint("MissingPermission")
        override fun onScanResult(type: Int, r: ScanResult) {
            val dev = r.device
            val name = r.scanRecord?.deviceName ?: dev.name ?: "?"
            val addr = dev.address
            val bytes = r.scanRecord?.bytes
            val hex = bytes?.joinToString("") { "%02X".format(it) } ?: ""
            val msb = StringBuilder()
            val mdata = r.scanRecord?.manufacturerSpecificData
            if (mdata != null) {
                for (i in 0 until mdata.size()) {
                    val k = mdata.keyAt(i)
                    val v = mdata.valueAt(i)
                    msb.append(" [mfr 0x%04X]=".format(k)).append(v?.joinToString("") { "%02X".format(it) } ?: "")
                }
            }
            val suuids = r.scanRecord?.serviceUuids?.joinToString(",") { it.toString() } ?: ""
            val sdata = r.scanRecord?.serviceData
            val sd = StringBuilder()
            if (sdata != null) {
                for ((u, v) in sdata) sd.append(" [svcData $u=").append(v.joinToString("") { "%02X".format(it) }).append("]")
            }
            emit("SCAN name=$name addr=$addr rssi=${r.rssi} svcUuids=[$suuids]$msb$sd")
            emit("SCAN_RAW addr=$addr hex=$hex")
            if (!name.startsWith(watchPrefix) && pending.size < 12) pending.add(addr)
            // 扫到胎压候选立即 GATT 连接：气嘴只在插电池瞬间广播，扫描结束后再连已停广播(status=133)
            val reason = isTpmsCandidate(name, suuids, sd.toString(), addr)
            if (reason != null) {
                emit("TPMS_CANDIDATE addr=$addr name=$name reason=$reason")
                maybeConnect(addr)
            }
        }
        override fun onScanFailed(err: Int) { emit("SCAN_FAILED err=$err") }
    }

    /** 判定是否为胎压气嘴：已知 MAC / 胎压服务 UUID / 名称关键字。返回原因或 null。 */
    private fun isTpmsCandidate(name: String, suuids: String, svcData: String, addr: String): String? {
        val up = name.uppercase()
        if (targetMacs.contains(addr.uppercase())) return "KNOWN_MAC"
        if (suuids.contains("ffb0", true) || suuids.contains("fbb0", true) || suuids.contains("fd2d", true)) return "SVC_UUID"
        if (svcData.contains("fd2d", true)) return "SVC_DATA_0xFD2D"
        if (tpmsNameKw.any { up.contains(it) }) return "NAME_KW"
        return null
    }

    /** 立即对候选发起 GATT 连接（限制并发数，避免资源耗尽）。 */
    @SuppressLint("MissingPermission")
    private fun maybeConnect(addr: String) {
        val c = appCtx ?: return
        if (gattStarted.contains(addr) || gattStarted.size >= MAX_GATT) return
        gattStarted.add(addr)
        val dev = try { (c.getSystemService(Context.BLUETOOTH_SERVICE) as? BluetoothManager)?.adapter?.getRemoteDevice(addr) } catch (_: Exception) { null }
        if (dev == null) { gattStarted.remove(addr); return }
        emit("GATT_CONNECTING $addr (started=${gattStarted.size}/$MAX_GATT)")
        try {
            dev.connectGatt(c, false, gattCb, BluetoothDevice.TRANSPORT_LE)
        } catch (e: Exception) {
            emit("GATT_CONNECT_FAIL $addr ${e.javaClass.simpleName}: ${e.message}")
            gattStarted.remove(addr)
        }
    }

    @SuppressLint("MissingPermission")
    private fun finishScan(c: Context) {
        if (scanning) {
            scanning = false
            try { scanner?.stopScan(scanCb) } catch (_: Exception) {}
        }
        emit("=== SCAN DONE, ${pending.size} non-MotoNav device(s) seen; GATT started=${gattStarted.size} ===")
        if (gattStarted.isEmpty()) {
            emit("=== NO TPMS CANDIDATE: 扫描窗内未匹配到胎压气嘴（已知MAC/服务UUID/名称）。请确认气嘴在广播（插拔电池/开车）===")
        } else {
            emit("=== 已对 ${gattStarted.size} 个胎压候选发起 GATT 连接，等待特征值读取/通知（保持连接 ~${GATT_PER}ms）===")
        }
        handler.postDelayed({ stopAll() }, GATT_PER + 6000L)
    }

    private val gattCb = object : BluetoothGattCallback() {
        @SuppressLint("MissingPermission")
        override fun onConnectionStateChange(g: BluetoothGatt, status: Int, newState: Int) {
            val addr = g.device.address
            if (newState == BluetoothProfile.STATE_CONNECTED) {
                emit("GATT_CONNECTED $addr")
                gatts.add(g)
                g.discoverServices()
            } else {
                emit("GATT_DISCONNECTED $addr status=$status")
            }
        }
        @SuppressLint("MissingPermission")
        override fun onServicesDiscovered(g: BluetoothGatt, status: Int) {
            val addr = g.device.address
            emit("GATT_SERVICES $addr count=${g.services.size} status=$status")
            for (svc in g.services) {
                emit("  SVC ${svc.uuid}")
                for (ch in svc.characteristics) {
                    val p = ch.properties
                    val f = mutableListOf<String>()
                    if (p and BluetoothGattCharacteristic.PROPERTY_READ != 0) f.add("R")
                    if (p and BluetoothGattCharacteristic.PROPERTY_NOTIFY != 0) f.add("N")
                    if (p and BluetoothGattCharacteristic.PROPERTY_INDICATE != 0) f.add("I")
                    if (p and BluetoothGattCharacteristic.PROPERTY_WRITE != 0) f.add("W")
                    if (p and BluetoothGattCharacteristic.PROPERTY_WRITE_NO_RESPONSE != 0) f.add("WNR")
                    emit("    CHR ${ch.uuid} [${f.joinToString("")}]")
                    if (f.contains("R")) g.readCharacteristic(ch)
                    if (f.contains("N") || f.contains("I")) {
                        g.setCharacteristicNotification(ch, true)
                        val d = ch.getDescriptor(UUID.fromString(CCCD))
                        d?.value = if (f.contains("I")) BluetoothGattDescriptor.ENABLE_INDICATION_VALUE
                        else BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
                        g.writeDescriptor(d)
                    }
                }
            }
        }
        @SuppressLint("MissingPermission")
        override fun onCharacteristicRead(g: BluetoothGatt, ch: BluetoothGattCharacteristic, value: ByteArray, status: Int) {
            emit("READ ${g.device.address} ${ch.uuid} status=$status hex=${value.joinToString("") { "%02X".format(it) }}")
        }
        @SuppressLint("MissingPermission")
        override fun onCharacteristicChanged(g: BluetoothGatt, ch: BluetoothGattCharacteristic, value: ByteArray) {
            emit("NOTIFY ${g.device.address} ${ch.uuid} hex=${value.joinToString("") { "%02X".format(it) }}")
        }
        @SuppressLint("MissingPermission")
        override fun onDescriptorWrite(g: BluetoothGatt, d: BluetoothGattDescriptor, status: Int) {}
    }

    @SuppressLint("MissingPermission")
    fun stopAll() {
        scanning = false
        try { scanner?.stopScan(scanCb) } catch (_: Exception) {}
        gatts.forEach { try { it.disconnect() } catch (_: Exception) {}; try { it.close() } catch (_: Exception) {} }
        gatts.clear()
        handler.removeCallbacksAndMessages(null)
        emit("=== TPMS SNIFF STOP ===")
        try { out?.close() } catch (_: Exception) {}
        out = null
    }
}
