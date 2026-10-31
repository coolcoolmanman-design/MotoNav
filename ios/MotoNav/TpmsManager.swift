import Foundation
import CoreBluetooth
import SwiftUI

/// 版本：跟随 App 版本（见 Config.swift）
///
/// 胎压（iOS 移植自安卓 TpmsManager，路线 A：手机作为 BLE 主机被动听气嘴广播）：
/// - LyTPMS 被动广播型（协议 2026-10-30 破译并经官方 App 锚点验证，与安卓端一致）：
///   厂商数据公司 ID 0x00AC，其后 15 字节：
///     b0=0xC7 头  b1=压力（1/32 bar 表压，×100÷32=kPa）  b2=温度（值−55=°C）
///     b3=趋势（0稳/1降/2升）  b4-5=电池电压 mV（大端）  b6=保留恒 0（防误匹配）
/// - 气嘴天生拒 GATT 连接，只广播；且只在"上电瞬间 + 压力变化/移动"时发，静止静默——
///   圆屏端固件保持最后一次读数，这里也保持最后读数不变。
/// - 前后轮绑定用 CBPeripheral.identifier（iOS 不暴露真实 MAC），存 UserDefaults；
///   未绑定时按"先前轮、后后轮"自动绑定，「重新配对」清空重来。
/// - 发圆屏：#TP,<前kPa>,<前°C>,<后kPa>,<后°C>,<前电量>,<后电量>；变化才发 + 30s 续命；
///   BleLink 的 lastState 机制会在圆屏重连后自动补发，无需手动 resend。
/// - 扫描节流防护：30s 心跳计数 + 每 4 分钟重启扫描；连续空心跳时降级为无过滤扫描一轮
///   （默认按服务 0xFBB0/0xFD2D 过滤，后台也稳）。
final class TpmsManager: NSObject, ObservableObject {
    static let shared = TpmsManager()

    struct Reading: Equatable {
        let kpa: Int
        let tempC: Int
        let battPct: Int
    }

    // MARK: 发布状态

    @Published private(set) var front: Reading?
    @Published private(set) var rear: Reading?
    @Published private(set) var scanning = false
    /// 最近一个 30s 心跳收到的广播条数（诊断扫描是否漏帧）
    @Published private(set) var lastAdvs = 0
    /// 前/后轮绑定（CBPeripheral.identifier UUID 串；空 = 未绑定）
    @Published var frontId: String { didSet { ud.set(frontId, forKey: "tpms_front_id") } }
    @Published var rearId: String { didSet { ud.set(rearId, forKey: "tpms_rear_id") } }
    /// 低压报警阈值（kPa）与高温报警阈值（°C）
    @Published var lowKpa: Int { didSet { ud.set(lowKpa, forKey: "tpms_low_kpa") } }
    @Published var highTemp: Int { didSet { ud.set(highTemp, forKey: "tpms_high_temp") } }

    var pairedCount: Int { (frontId.isEmpty ? 0 : 1) + (rearId.isEmpty ? 0 : 1) }

    /// 读数状态：（文案，颜色）——低压/高温越限标红，正常绿；nil = 还没数据
    func status(_ r: Reading?) -> (String, Color)? {
        guard let r else { return nil }
        if r.kpa <= lowKpa { return ("偏低", AppTheme.orange) }   // 冷淡科技：低压用系统橙
        if r.tempC >= highTemp { return ("高温", AppTheme.red) }
        return ("正常", AppTheme.green)
    }

    // MARK: 私有状态

    private let ud = UserDefaults.standard
    private var central: CBCentralManager?
    private var timer: Timer?
    private var tickN = 0
    private var emptyBeats = 0
    private var advCount = 0
    private var lastSendAt = TimeInterval(0)
    private var lastSentFront = Reading(kpa: -1, tempC: 0, battPct: 0)
    private var lastSentRear = Reading(kpa: -1, tempC: 0, battPct: 0)
    private let svcFBB0 = CBUUID(string: "FBB0")   // 被动广播型气嘴
    private let svcFD2D = CBUUID(string: "FD2D")   // 部分通用型
    private let RESEND_S = TimeInterval(30)

    private override init() {
        frontId = ud.string(forKey: "tpms_front_id") ?? ""
        rearId = ud.string(forKey: "tpms_rear_id") ?? ""
        lowKpa = ud.object(forKey: "tpms_low_kpa") as? Int ?? 220
        highTemp = ud.object(forKey: "tpms_high_temp") as? Int ?? 70
        super.init()
    }

    // MARK: 生命周期

    /// App 启动即开扫（ContentView onAppear 调）；蓝牙权限走既有 NSBluetoothAlwaysUsageDescription
    func start() {
        guard central == nil else {
            if central?.state == .poweredOn { beginScan() }
            return
        }
        central = CBCentralManager(delegate: self, queue: .main)
    }

    func stop() {
        central?.stopScan()
        timer?.invalidate()
        timer = nil
        scanning = false
    }

    /// 清空前后轮绑定，下两个广播到的气嘴按序自动重绑
    func clearPairing() {
        frontId = ""
        rearId = ""
        front = nil
        rear = nil
    }

    /// adb/界面注入样例帧（250kPa/32°C/87% + 243kPa/41°C/64%），验证 App→圆屏→胎压页链路
    func sendTest() {
        front = Reading(kpa: 250, tempC: 32, battPct: 87)
        rear = Reading(kpa: 243, tempC: 41, battPct: 64)
        maybeSend(force: true)
    }

    // MARK: 扫描

    private func beginScan() {
        guard let central, central.state == .poweredOn else { return }
        central.stopScan()
        let filtered = emptyBeats < 2
        let services: [CBUUID]? = filtered ? [svcFBB0, svcFD2D] : nil
        central.scanForPeripherals(withServices: services)
        scanning = true
        if timer == nil {
            timer = Timer.scheduledTimer(withTimeInterval: 30, repeats: true) { [weak self] _ in
                self?.heartbeat()
            }
        }
    }

    private func heartbeat() {
        tickN += 1
        lastAdvs = advCount
        advCount = 0
        emptyBeats = lastAdvs == 0 ? emptyBeats + 1 : 0
        if tickN % 8 == 0 { beginScan() }   // 每 4 分钟重启扫描，对抗系统扫描节流
    }

    // MARK: 解码（与安卓 TpmsManager.parsePassive 一致）

    static func decode(manufacturerData d: Data) -> Reading? {
        guard d.count >= 17 else { return nil }
        let company = UInt16(d[d.startIndex]) | (UInt16(d[d.startIndex + 1]) << 8)
        guard company == 0x00AC else { return nil }
        var b = [UInt8](repeating: 0, count: 15)
        (d as NSData).getBytes(&b, range: NSRange(location: d.startIndex + 2, length: 15))
        guard b[6] == 0 else { return nil }                       // 保留位恒 0
        let pCnt = Int(b[1])
        let tRaw = Int(b[2])
        let mv = (Int(b[4]) << 8) | Int(b[5])                     // 大端电压 mV
        let kpa = (pCnt * 100 + 16) / 32                          // 1/32 bar 表压 → kPa（四舍五入）
        let tempC = tRaw - 55
        let batt = min(max((mv - 2400) * 100 / 600, 0), 100)      // CR1632 2.4-3.0V 粗映射
        guard (0...900).contains(kpa), (-40...110).contains(tempC) else { return nil }
        return Reading(kpa: kpa, tempC: tempC, battPct: batt)
    }

    private func assign(id: String, reading r: Reading) {
        if id == frontId {
            front = r
        } else if id == rearId {
            rear = r
        } else if frontId.isEmpty {
            frontId = id
            front = r
        } else if rearId.isEmpty {
            rearId = id
            rear = r
        } else {
            return                                                // 已绑满：额外气嘴忽略
        }
    }

    private func maybeSend(force: Bool = false) {
        guard front != nil || rear != nil else { return }
        let f = front ?? Reading(kpa: 0, tempC: 0, battPct: 0)
        let r = rear ?? Reading(kpa: 0, tempC: 0, battPct: 0)
        let changed = f != lastSentFront || r != lastSentRear
        guard force || changed || Date().timeIntervalSinceReferenceDate - lastSendAt >= RESEND_S else { return }
        BleLink.shared.send("#TP,\(f.kpa),\(f.tempC),\(r.kpa),\(r.tempC),\(f.battPct),\(r.battPct)")
        lastSentFront = f
        lastSentRear = r
        lastSendAt = Date().timeIntervalSinceReferenceDate
    }
}

// MARK: - CBCentralManagerDelegate

extension TpmsManager: CBCentralManagerDelegate {
    func centralManagerDidUpdateState(_ central: CBCentralManager) {
        if central.state == .poweredOn { beginScan() }
    }

    func centralManager(_ central: CBCentralManager,
                        didDiscover peripheral: CBPeripheral,
                        advertisementData: [String: Any],
                        rssi RSSI: NSNumber) {
        advCount += 1
        guard let md = advertisementData[CBAdvertisementDataManufacturerDataKey] as? Data,
              let r = Self.decode(manufacturerData: md) else { return }
        assign(id: peripheral.identifier.uuidString, reading: r)
        maybeSend()
    }
}
