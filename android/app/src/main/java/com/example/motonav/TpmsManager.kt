package com.example.motonav

import android.annotation.SuppressLint
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
import android.content.Context
import android.os.Handler
import android.os.Looper
import android.util.Log
import java.util.UUID

/**
 * 路线 A：手机作为 BLE 主机连胎压气嘴，读出前/后轮 kPa / °C，经 BleLink.send("#TP,...") 透传圆屏。
 *
 * 协议覆盖两类通用气嘴（参考 m-gracia/esp32-bike-computer 的 BLE-TPMS 架构）：
 *  - 被动广播型（ZEEPIN/TP630，服务 0xFBB0 或 0xFD2D）：数据直接塞在广播厂商数据/服务数据里，扫到即解，无需连接。
 *  - GATT 型（如 E9 "Hi-L-120YV00"，服务 0xFFB0）：广播里只有型号名+MAC，真实数据在 GATT 特征值里；
 *    气嘴只在插电池/行驶时短暂广播，所以"扫到立即连接"才能在它还在线的窗口里抓到特征值。
 *
 * 前/后轮按 MAC 绑定（frontMac/rearMac），对应固件 #TP,<前kPa>,<前°C>,<后kPa>,<后°C>。
 * 变化才发送，并每 RESEND_MS 续命（固件 LIVE 超时 5 分钟）。
 *
 * ⚠️ E9(0xFFB0) 的字节布局尚未破译，parseFfb0Gatt() 目前只打原始字节并返回 null（不污染圆屏）。
 *    抓到特征值后在此实现真实映射即可（见 TpmsSniffer 抓包 + 官方 App 读数校准）。
 */
object TpmsManager {
    private const val TAG = "TpmsManager"

    private val FFB0 = UUID.fromString("0000ffb0-0000-1000-8000-00805f9b34fb") // GATT 型（E9）
    private val FBB0 = UUID.fromString("0000fbb0-0000-1000-8000-00805f9b34fb") // 被动型 ZEEPIN/TP630
    private val FD2D = UUID.fromString("0000fd2d-0000-1000-8000-00805f9b34fb") // 部分通用型
    private val CCCD = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")

    // 前/后轮 MAC 绑定：构建时经 Gradle 属性注入（脱敏，真实 MAC 不入库）：
    //   ./gradlew assembleRelease -PtpmsFrontMac=AA:BB:CC:DD:EE:FF -PtpmsRearMac=AA:BB:CC:DD:EE:FF
    // 未注入时为 null：首个广播合法（0x00AC 15 字节帧）的气嘴按"先前轮、后后轮"自动绑定。
    private val knownMacs = buildSet {
        BuildConfig.TPMS_FRONT_MAC.takeIf { it.isNotBlank() }?.let { add(it.uppercase()) }
        BuildConfig.TPMS_REAR_MAC.takeIf { it.isNotBlank() }?.let { add(it.uppercase()) }
    }
    private val nameKw = listOf("HI-L", "TPMS", "ZEEPIN", "PRESSURE", "TYRE", "TIRE")

    /** 用户设置：前/后轮 MAC（adb 可经 TPMS_ASSIGN 覆盖）。 */
    @Volatile var frontMac: String? = BuildConfig.TPMS_FRONT_MAC.ifBlank { null }
    @Volatile var rearMac: String? = BuildConfig.TPMS_REAR_MAC.ifBlank { null }

    private val handler = Handler(Looper.getMainLooper())
    private var appCtx: Context? = null
    private var scanner: android.bluetooth.le.BluetoothLeScanner? = null
    private var scanning = false
    private val gatts = mutableListOf<BluetoothGatt>()
    private val gattStarted = mutableSetOf<String>()
    private val retryCnt = mutableMapOf<String, Int>()
    private var advCount = 0L               // 30s 心跳内的广播计数（诊断扫描是否漏帧）
    private var tickN = 0
    private val advLastLog = mutableMapOf<String, Pair<String, Long>>()  // addr -> (payloadKey, ts) 去重
    private const val TICK_MS = 30000L

    data class Reading(val kpa: Int, val tempC: Int, val battPct: Int = -1)

    @Volatile var front = Reading(0, 0, -1)
    @Volatile var rear = Reading(0, 0, -1)
    private var frontHas = false
    private var rearHas = false
    private var frontAt = 0L      // 最后一次收到该侧广播的时刻（诊断用）
    private var rearAt = 0L
    private var lastSendAt = 0L
    private const val RESEND_MS = 30000L
    private const val MAX_GATT = 4
    private const val MAX_RETRY = 6
    private const val GATT_ENABLED = false // LyTPMS 气嘴天生拒连（ra6070 文档确认），被动广播即数据，GATT 只耗电

    val isScanning: Boolean get() = scanning

    // ================= 胎压卡片（v8.3.27 同步自 iOS tpms 版） =================
    val hasData: Boolean get() = frontHas || rearHas
    val frontLive: Boolean get() = frontHas
    val rearLive: Boolean get() = rearHas
    val pairedCount: Int get() = listOfNotNull(frontMac, rearMac).distinct().size
    val frontAddr: String? get() = frontMac
    val rearAddr: String? get() = rearMac

    private val uiPrefs: android.content.SharedPreferences?
        get() = appCtx?.getSharedPreferences("tpms", Context.MODE_PRIVATE)

    /** 低压报警阈值 kPa（卡片步进器 + 状态胶囊，存 SharedPreferences）。 */
    @Volatile var lowKpa: Int = 220
        private set
    /** 高温报警阈值 °C。 */
    @Volatile var highTempC: Int = 70
        private set

    /** 启动时恢复阈值与（公开构建时的）上次自动绑定。 */
    fun loadUiPrefs() {
        val p = uiPrefs ?: return
        lowKpa = p.getInt("low_kpa", 220)
        highTempC = p.getInt("high_temp_c", 70)
        // BuildConfig 未注入 MAC 的公开构建：恢复上次自动绑定的前后轮，App 重启不重排
        if (BuildConfig.TPMS_FRONT_MAC.isBlank()) p.getString("front_mac", null)?.let { frontMac = it }
        if (BuildConfig.TPMS_REAR_MAC.isBlank()) p.getString("rear_mac", null)?.let { rearMac = it }
    }

    fun setLowKpa(v: Int) {
        lowKpa = v.coerceIn(100, 350)
        uiPrefs?.edit()?.putInt("low_kpa", lowKpa)?.apply()
    }

    fun setHighTempC(v: Int) {
        highTempC = v.coerceIn(40, 110)
        uiPrefs?.edit()?.putInt("high_temp_c", highTempC)?.apply()
    }

    /** 清空前后轮绑定：下一颗广播按「先前轮、后后轮」重新绑定。 */
    fun clearPairing() {
        frontMac = null; rearMac = null
        frontHas = false; rearHas = false
        front = Reading(0, 0, -1); rear = Reading(0, 0, -1)
        uiPrefs?.edit()?.remove("front_mac")?.remove("rear_mac")?.apply()
        Log.i(TAG, "TPMS pairing cleared")
    }

    @SuppressLint("MissingPermission")
    fun start(ctx: Context) {
        val c = ctx.applicationContext
        appCtx = c
        loadUiPrefs()
        val ad = (c.getSystemService(Context.BLUETOOTH_SERVICE) as? BluetoothManager)?.adapter
        if (ad == null || !ad.isEnabled) { Log.i(TAG, "BT off"); return }
        if (scanning) return
        scanner = ad.bluetoothLeScanner
        scanning = true
        val settings = ScanSettings.Builder().setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY).build()
        try { scanner?.stopScan(scanCb) } catch (_: Exception) {}
        try {
            // mMaxScanFilters=0 设备上 null 会被底层丢弃，用空列表更稳（与 TpmsSniffer 一致）
            scanner?.startScan(emptyList<ScanFilter>(), settings, scanCb)
            Log.i(TAG, "TpmsManager scan start")
            tickN = 0
            handler.removeCallbacks(tick)
            handler.postDelayed(tick, TICK_MS)
        } catch (e: Exception) {
            Log.i(TAG, "scan start FAILED ${e.message}")
        }
    }

    @SuppressLint("MissingPermission")
    fun stop() {
        scanning = false
        handler.removeCallbacks(tick)
        try { scanner?.stopScan(scanCb) } catch (_: Exception) {}
        gatts.forEach { try { it.disconnect() } catch (_: Exception) {}; try { it.close() } catch (_: Exception) {} }
        gatts.clear()
        gattStarted.clear()
        Log.i(TAG, "TpmsManager stop")
    }

    /** 30s 心跳：广播计数（诊断漏帧）/ 每 4 分钟重启扫描（对抗 MIUI 限流）。 */
    private val tick = object : Runnable {
        override fun run() {
            if (!scanning) return
            tickN++
            Log.i(TAG, "scan alive: $advCount advs/30s")
            advCount = 0
            if (tickN % 8 == 0) restartScan()
            handler.postDelayed(this, TICK_MS)
        }
    }

    /** BLE 通道就绪（首连 / 圆屏重启重连）：立即补发当前读数，圆屏页面马上显示实际数字。 */
    fun onLinkReady() {
        handler.post {
            if (frontHas || rearHas) {
                Log.i(TAG, "link ready -> resend current TPMS")
                maybeSend(true)
            }
        }
    }

    @SuppressLint("MissingPermission")
    private fun restartScan() {
        try { scanner?.stopScan(scanCb) } catch (_: Exception) {}
        val settings = ScanSettings.Builder().setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY).build()
        try {
            scanner?.startScan(emptyList<ScanFilter>(), settings, scanCb)
            Log.i(TAG, "scan restarted (anti-throttle)")
        } catch (e: Exception) {
            Log.i(TAG, "scan restart FAILED ${e.message}")
        }
    }

    private val scanCb = object : ScanCallback() {
        @SuppressLint("MissingPermission")
        override fun onScanResult(type: Int, r: ScanResult) {
            val dev = r.device
            val name = r.scanRecord?.deviceName ?: dev.name ?: "?"
            val addr = dev.address
            if (name.startsWith("MotoNav")) return           // 跳过圆屏本身
            val rec = r.scanRecord ?: return
            val suuids = rec.serviceUuids?.map { it.toString().lowercase() } ?: emptyList()
            // 诊断：所有非 MotoNav 广播去重落盘（排查"插拔电池后收不到气嘴帧"）
            advCount++
            run {
                val sb0 = StringBuilder()
                val md = rec.manufacturerSpecificData
                if (md != null) for (i in 0 until md.size()) {
                    val v = md.valueAt(i) ?: continue
                    sb0.append(String.format("[mfr 0x%04X]", md.keyAt(i)))
                    for (b in v) sb0.append(String.format("%02X", b))
                }
                val sd = rec.serviceData
                if (sd != null) for ((k, v) in sd) {
                    sb0.append(String.format("[svc %s]", k))
                    if (v != null) for (b in v) sb0.append(String.format("%02X", b))
                }
                val key = "$name|$suuids|$sb0"
                val nowMs = android.os.SystemClock.elapsedRealtime()
                val last = advLastLog[addr]
                if (last == null || last.first != key || nowMs - last.second > 60000) {
                    advLastLog[addr] = key to nowMs
                    Log.i(TAG, "ADV $addr name=$name suuids=$suuids $sb0")
                }
            }
            val why = isCandidate(name, suuids, addr)
            if (why == null) return
            Log.i(TAG, "candidate $addr name=$name why=$why suuids=$suuids")
            // 原始字节全 dump：破译广播布局用
            run {
                val sb = StringBuilder()
                val md = rec.manufacturerSpecificData
                if (md != null) for (i in 0 until md.size()) {
                    val v = md.valueAt(i) ?: continue
                    sb.append(String.format("[mfr 0x%04X] ", md.keyAt(i)))
                    for (b in v) sb.append(String.format("%02X", b))
                    sb.append(' ')
                }
                val sd = rec.serviceData
                if (sd != null) for ((k, v) in sd) {
                    sb.append("[svc $k] ")
                    if (v != null) for (b in v) sb.append(String.format("%02X", b))
                    sb.append(' ')
                }
                if (sb.isNotEmpty()) Log.i(TAG, "RAW $addr $sb")
            }

            // 被动广播型：直接从广告解
            val passive = parsePassive(rec)
            if (passive != null) {
                assign(addr, passive)
                maybeSend(false)
                return
            }
            // GATT 型：理论上存在，但实测 LyTPMS 拒连（E9 全部 133），默认关闭
            if (GATT_ENABLED && suuids.any { it.contains("ffb0") }) maybeConnect(addr)
        }
        override fun onScanFailed(err: Int) { Log.i(TAG, "scan failed $err") }
    }

    private fun isCandidate(name: String, suuids: List<String>, addr: String): String? {
        val up = name.uppercase()
        if (knownMacs.contains(addr.uppercase())) return "KNOWN_MAC"
        if (suuids.any { it.contains("ffb0") || it.contains("fbb0") || it.contains("fd2d") }) return "SVC_UUID"
        if (nameKw.any { up.contains(it) }) return "NAME_KW"
        return null
    }

    /** 被动广播型解码：LyTPMS(0x00AC/0xFBB0，15字节) / ZEEPIN/TP630(0xFBB0 厂商数据18字节) / 0xFD2D(服务数据)。 */
    @SuppressLint("MissingPermission")
    private fun parsePassive(rec: android.bluetooth.le.ScanRecord): Reading? {
        val mdata = rec.manufacturerSpecificData
        if (mdata != null) {
            // LyTPMS（用户这批通用气嘴，2026-10-30 破译并经官方 App 锚点验证）：
            // 公司ID 0x00AC，其后 15 字节：HH PP TT SS VV VV RR UU KK MM×6
            //  b1=压力（1/32 bar 表压，×100/32 = kPa）  b2=温度（值−55 = °C）
            //  b3=趋势(0稳/1降/2升)  b4-5=电池电压 mV（大端）  b6=保留恒0
            for (i in 0 until mdata.size()) {
                if (mdata.keyAt(i) != 0x00AC) continue
                val v = mdata.valueAt(i) ?: continue
                if (v.size != 15 || (v[6].toInt() and 0xFF) != 0x00) continue  // 保留位校验防误匹配
                val pCnt = v[1].toInt() and 0xFF
                val tRaw = v[2].toInt() and 0xFF
                val trend = v[3].toInt() and 0xFF
                val mv = ((v[4].toInt() and 0xFF) shl 8) or (v[5].toInt() and 0xFF)
                val kpa = (pCnt * 100 + 16) / 32          // 四舍五入到整数 kPa
                val tempC = tRaw - 55
                val battPct = (((mv - 2400) * 100) / 600).coerceIn(0, 100)  // CR1632 粗略 2.4-3.0V 映射
                Log.i(TAG, "LyTPMS pCnt=$pCnt tRaw=$tRaw trend=$trend ${mv}mV -> $kpa kPa / ${tempC}C batt~$battPct%")
                if (kpa in 0..900 && tempC in -40..110) return Reading(kpa, tempC, battPct)
            }
            // ZEEPIN/TP630：厂商数据 18 字节：bytes8-11=压力(uint32 LE)，bytes12-15=温度(uint32 LE)，byte16=电量，byte17=报警
            for (i in 0 until mdata.size()) {
                val v = mdata.valueAt(i) ?: continue
                if (v.size >= 18) {
                    val p = (v[8].toInt() and 0xFF) or ((v[9].toInt() and 0xFF) shl 8) or
                            ((v[10].toInt() and 0xFF) shl 16) or ((v[11].toInt() and 0xFF) shl 24)
                    val t = (v[12].toInt() and 0xFF) or ((v[13].toInt() and 0xFF) shl 8) or
                            ((v[14].toInt() and 0xFF) shl 16) or ((v[15].toInt() and 0xFF) shl 24)
                    val batt = v[16].toInt() and 0xFF
                    // 刻度歧义：ra6070 原库用 /1000(kPa)，esp32-bike-computer 项目用 /10000。待实测校准。
                    val kpa = (p / 10000f).toInt().coerceIn(0, 999)
                    val tempC = (t / 100f).toInt().coerceIn(-40, 125)
                    if (kpa in 100..400 && tempC in -20..90) {  // 合理性过滤
                        Log.i(TAG, "ZEEPIN mfr=${mdata.keyAt(i)} p_raw=$p t_raw=$t batt=$batt -> $kpa kPa / $tempC C")
                        return Reading(kpa, tempC, batt)
                    }
                }
            }
        }
        return null
    }

    /** GATT 型(E9 0xFFB0)特征值解析——⚠️ 待抓包校准，目前仅记录原始字节，不返回读数。 */
    private fun parseFfb0Gatt(value: ByteArray): Reading? {
        val hex = value.joinToString("") { "%02X".format(it) }
        Log.i(TAG, "FFB0 GATT raw hex=$hex")
        if (value.size < 4) return null
        // 候选解释（校准时择一）：
        //  方案A: u16LE@0 = 压力(0.1kPa?), i8@2 = 温度(°C,可能带偏移), u8@3 = 电量%
        //  方案B: u16LE@0 = 压力(kPa), i8@1 = 温度(°C), u8@2 = 电量%
        val pA = (value[0].toInt() and 0xFF) or ((value[1].toInt() and 0xFF) shl 8)
        val tA = value[2].toInt()            // 可能带 +40°C 或 +50°C 偏移
        val bA = value[3].toInt() and 0xFF
        Log.i(TAG, "FFB0 candidate A: p=$pA t=$tA b=$bA")
        // TODO(calibrate): 对照官方 App 读数确定真实映射后取消 return null
        return null
    }

    @SuppressLint("MissingPermission")
    private fun maybeConnect(addr: String) {
        val c = appCtx ?: return
        if (gattStarted.contains(addr) || gattStarted.size >= MAX_GATT) return
        val dev = try { (c.getSystemService(Context.BLUETOOTH_SERVICE) as? BluetoothManager)?.adapter?.getRemoteDevice(addr) } catch (_: Exception) { null }
        if (dev == null) return
        gattStarted.add(addr)
        Log.i(TAG, "GATT connect $addr (${gattStarted.size}/$MAX_GATT)")
        try { dev.connectGatt(c, false, gattCb, BluetoothDevice.TRANSPORT_LE) }
        catch (e: Exception) { Log.i(TAG, "GATT connect FAIL $addr ${e.message}"); gattStarted.remove(addr) }
    }

    private val gattCb = object : BluetoothGattCallback() {
        @SuppressLint("MissingPermission")
        override fun onConnectionStateChange(g: BluetoothGatt, status: Int, newState: Int) {
            val addr = g.device.address
            if (newState == BluetoothProfile.STATE_CONNECTED) {
                gatts.add(g); retryCnt.remove(addr)
                g.discoverServices()
            } else {
                Log.i(TAG, "GATT disc $addr status=$status")
                gatts.remove(g); try { g.close() } catch (_: Exception) {}; gattStarted.remove(addr)
                // status=133：气嘴广播窗口短/连接易断，短延迟自动重连（有限次）
                if (status == 133) {
                    val n = retryCnt[addr] ?: 0
                    if (n < MAX_RETRY) {
                        retryCnt[addr] = n + 1
                        Log.i(TAG, "GATT retry #$n $addr in 1.5s")
                        handler.postDelayed({ maybeConnect(addr) }, 1500)
                    } else {
                        Log.i(TAG, "GATT give up $addr after $n retries")
                    }
                }
            }
        }
        @SuppressLint("MissingPermission")
        override fun onServicesDiscovered(g: BluetoothGatt, status: Int) {
            val addr = g.device.address
            for (svc in g.services) {
                if (svc.uuid != FFB0) continue
                for (ch in svc.characteristics) {
                    val p = ch.properties
                    if (p and BluetoothGattCharacteristic.PROPERTY_READ != 0) g.readCharacteristic(ch)
                    if (p and BluetoothGattCharacteristic.PROPERTY_NOTIFY != 0) {
                        g.setCharacteristicNotification(ch, true)
                        ch.getDescriptor(CCCD)?.let { d ->
                            d.value = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
                            g.writeDescriptor(d)
                        }
                    }
                }
            }
        }
        @SuppressLint("MissingPermission")
        override fun onCharacteristicRead(g: BluetoothGatt, ch: BluetoothGattCharacteristic, value: ByteArray, status: Int) {
            handleGatt(addr = g.device.address, value = value)
        }
        @SuppressLint("MissingPermission")
        override fun onCharacteristicChanged(g: BluetoothGatt, ch: BluetoothGattCharacteristic, value: ByteArray) {
            handleGatt(addr = g.device.address, value = value)
        }
        @SuppressLint("MissingPermission")
        override fun onDescriptorWrite(g: BluetoothGatt, d: BluetoothGattDescriptor, status: Int) {}
    }

    private fun handleGatt(addr: String, value: ByteArray) {
        val r = parseFfb0Gatt(value) ?: return     // 校准前返回 null，不污染圆屏
        assign(addr, r)
        maybeSend(true)
    }

    private fun assign(addr: String, r: Reading) {
        val up = addr.uppercase()
        val now = System.currentTimeMillis()
        when {
            frontMac?.uppercase() == up -> { front = r; frontHas = true; frontAt = now; Log.i(TAG, "FRONT $addr = $r") }
            rearMac?.uppercase() == up  -> { rear = r; rearHas = true; rearAt = now; Log.i(TAG, "REAR $addr = $r") }
            else -> {
                // 自动绑定：前轮未定→绑前轮；否则→后轮（持久化，App 重启不重排）
                if (!frontHas && frontMac == null) { front = r; frontHas = true; frontMac = addr
                    uiPrefs?.edit()?.putString("front_mac", addr)?.apply()
                    Log.i(TAG, "AUTO FRONT $addr = $r") }
                else if (!rearHas) { rear = r; rearHas = true; rearMac = addr
                    uiPrefs?.edit()?.putString("rear_mac", addr)?.apply()
                    Log.i(TAG, "AUTO REAR $addr = $r") }
                else { Log.i(TAG, "extra sensor $addr = $r (ignored)") }
            }
        }
    }

    /** 变化或到期续命时，向圆屏发送 #TP。 */
    /** adb 测试注入（action TPMS_TEST）：伪造前后轮读数发一次 #TP，验证 App→圆屏→TPMS 页 LIVE 显示链路。 */
    fun sendTestTp() {
        front = Reading(250, 32, 87); frontHas = true; frontAt = System.currentTimeMillis()
        rear = Reading(243, 41, 64); rearHas = true; rearAt = System.currentTimeMillis()
        Log.i(TAG, "TEST #TP injected (250/32/87% 243/41/64%)")
        maybeSend(true)
    }

    private fun maybeSend(force: Boolean) {
        val now = System.currentTimeMillis()
        if (!frontHas && !rearHas) return
        val changed = front != lastSendFront || rear != lastSendRear
        if (!force && !changed && now - lastSendAt < RESEND_MS) return
        // v7.12.10：数据不过期——气嘴静止不广播时保持最后一次读数，直到下次变化
        val msg = String.format("#TP,%d,%d,%d,%d,%d,%d", front.kpa, front.tempC, rear.kpa, rear.tempC, front.battPct, rear.battPct)
        Log.i(TAG, "SEND $msg")
        BleLink.send(msg)
        lastSendFront = front
        lastSendRear = rear
        lastSendAt = now
    }

    private var lastSendFront = Reading(0, 0, -1)
    private var lastSendRear = Reading(0, 0, -1)
}
