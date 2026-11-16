package com.example.motonav

import android.app.Activity
import android.app.Dialog
import android.animation.ValueAnimator
import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.Path
import android.graphics.RadialGradient
import android.graphics.Shader
import android.graphics.Typeface
import android.graphics.drawable.GradientDrawable
import android.os.Handler
import android.os.Looper
import android.os.SystemClock
import android.view.Gravity
import android.view.View
import android.view.ViewGroup
import android.view.animation.AccelerateDecelerateInterpolator
import android.view.Window
import android.widget.LinearLayout
import android.widget.TextView

/**
 * AirPods 式连接动画（v8.3.27，同步自 iOS v0.15.10-tpms 的 ConnectSheetView）：
 * 底部白色圆角卡片 + 3D 铝合金圆盘 360° 旋转，连上后定格并显示圆屏电量。
 *
 *  - 首次 / 手动 / 启动自动连接都会弹出（后台静默重连不弹，与 iOS 行为一致）。
 *  - 「首次配对」标记存 SharedPreferences，设置里可重置。
 *  - 圆盘正面屏贴内容已按需求旋转 180°（路线图上下颠倒）。
 *  - 电量读 BleLink.screenBatteryPct（固件 v6.8+ 的 V 帧），没有就显示「—」。
 *
 * Android 无 SceneKit，圆盘用 Canvas 程序化绘制（径向渐变银圈 + 黑边屏 + 粉路线贴图），
 * ValueAnimator + AccelerateDecelerateInterpolator 转满一圈，观感对齐 iOS mock。
 */
object ConnectAnim {
    private const val TAG = "ConnectAnim"
    private const val PREFS = "connect_anim"
    private const val SPIN_MS = 3200L          // 与 iOS 一致：约 3.2s 转完再定格
    private const val HOLD_MS = 1200L          // 连上后停留，方便看清电量

    enum class Kind { FIRST, MANUAL, AUTO }

    private var dialog: Dialog? = null

    fun isFirstDone(ctx: Context): Boolean =
        ctx.getSharedPreferences(PREFS, Context.MODE_PRIVATE).getBoolean("first_done", false)

    /** 设置页可重置「首次」标记，方便反复看动画。 */
    fun resetFirstFlag(ctx: Context) {
        ctx.getSharedPreferences(PREFS, Context.MODE_PRIVATE).edit().putBoolean("first_done", false).apply()
    }

    /** 弹动画并发起 BLE 连接；已在展示中则不叠加。 */
    fun show(activity: Activity, auto: Boolean) {
        if (dialog?.isShowing == true) return
        val kind = when {
            !isFirstDone(activity) -> Kind.FIRST
            auto -> Kind.AUTO
            else -> Kind.MANUAL
        }
        val prefs = activity.getSharedPreferences(PREFS, Context.MODE_PRIVATE)
        val dp = activity.resources.displayMetrics.density
        fun d(v: Number) = (v.toFloat() * dp).toInt()

        // ---------- 卡片内容 ----------
        val root = LinearLayout(activity).apply {
            orientation = LinearLayout.VERTICAL
            gravity = Gravity.CENTER_HORIZONTAL
            setPadding(d(22), d(18), d(22), d(22))
            background = GradientDrawable().apply { setColor(Color.WHITE); cornerRadius = d(32f).toFloat() }
        }

        // 标题行：设备名 + 关闭
        val titleRow = LinearLayout(activity).apply {
            orientation = LinearLayout.HORIZONTAL; gravity = Gravity.CENTER_VERTICAL
        }
        val deviceName = BleLink.screenName.ifBlank { "MotoNav" }
        titleRow.addView(View(activity), LinearLayout.LayoutParams(0, 1, 1f))
        titleRow.addView(TextView(activity).apply {
            text = deviceName
            textSize = 20f; setTextColor(0xFF1A1A1A.toInt()); typeface = Typeface.DEFAULT_BOLD
        }, LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f).apply { gravity = Gravity.CENTER })
        val close = TextView(activity).apply {
            text = "✕"
            textSize = 13f; setTextColor(0xFF666666.toInt()); typeface = Typeface.DEFAULT_BOLD
            gravity = Gravity.CENTER
            background = GradientDrawable().apply { shape = GradientDrawable.OVAL; setColor(0xFFEBEBEB.toInt()) }
        }
        titleRow.addView(close, LinearLayout.LayoutParams(d(30), d(30)))
        root.addView(titleRow, LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT))

        // 3D 圆盘
        val puck = PuckView(activity)
        root.addView(puck, LinearLayout.LayoutParams(d(220), d(220)).apply { topMargin = d(8) })

        // 电量（连上后出现）
        val batLabel = TextView(activity).apply {
            textSize = 15f; setTextColor(0xFF1A1A1A.toInt()); typeface = Typeface.DEFAULT_BOLD
            gravity = Gravity.CENTER
        }
        val batBox = LinearLayout(activity).apply {
            orientation = LinearLayout.VERTICAL; gravity = Gravity.CENTER_HORIZONTAL
            visibility = View.GONE
            addView(batLabel)
        }
        root.addView(batBox, LinearLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT).apply { topMargin = d(2) })

        // 状态文字
        val status = TextView(activity).apply {
            textSize = 15f; setTextColor(0xFF595959.toInt())
            text = if (kind == Kind.AUTO) "正在重新连接…" else "正在连接…"
        }
        root.addView(status, LinearLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT).apply { topMargin = d(10); bottomMargin = d(8) })

        // 页脚：闪电 + 类型
        root.addView(TextView(activity).apply {
            text = "⚡ " + when (kind) {
                Kind.FIRST -> "首次配对 · BLE"
                Kind.MANUAL -> "手动连接 · BLE"
                Kind.AUTO -> "自动重连 · BLE"
            }
            textSize = 12f; setTextColor(0xFF8C8C8C.toInt())
        }, LinearLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT).apply { bottomMargin = d(6) })

        // ---------- Dialog ----------
        val dlg = Dialog(activity)
        dlg.requestWindowFeature(Window.FEATURE_NO_TITLE)
        dlg.setContentView(root)
        dlg.window?.apply {
            setBackgroundDrawable(android.graphics.drawable.ColorDrawable(Color.TRANSPARENT))
            setLayout(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT)
            setGravity(Gravity.BOTTOM)
            val pad = d(10)
            decorView.setPadding(pad, 0, pad, pad)
        }
        close.setOnClickListener { dlg.dismiss() }
        dialog = dlg

        // ---------- 相位机 ----------
        val h = Handler(Looper.getMainLooper())
        val startedAt = SystemClock.elapsedRealtime()
        val spin = ValueAnimator.ofFloat(0f, 360f).apply {
            duration = SPIN_MS
            interpolator = AccelerateDecelerateInterpolator()
            addUpdateListener { puck.angle = it.animatedValue as Float }
        }
        var settled = false
        val poll = object : Runnable {
            override fun run() {
                if (!dlg.isShowing) return
                val ready = BleLink.isConnected && BleLink.verified
                val elapsed = SystemClock.elapsedRealtime() - startedAt
                // 电量实时刷新（盒子连上后才显示，值随时可更新）
                val pct = BleLink.screenBatteryPct
                batLabel.text = "圆屏电量 " + (if (pct >= 0) "$pct%" else "—")
                if (ready && !settled && elapsed >= SPIN_MS) {
                    settled = true
                    status.text = if (kind == Kind.AUTO) "已重新连接" else "已连接"
                    status.setTextColor(0xFF33963F.toInt())
                    batBox.visibility = View.VISIBLE
                    if (kind == Kind.FIRST) prefs.edit().putBoolean("first_done", true).apply()
                    h.postDelayed({ if (dlg.isShowing) dlg.dismiss() }, HOLD_MS)
                    return
                }
                h.postDelayed(this, 100)
            }
        }
        dlg.setOnDismissListener {
            spin.cancel()
            h.removeCallbacks(poll)
            if (dialog === dlg) dialog = null
        }
        dlg.show()
        spin.start()
        BleLink.connect(activity)
        h.postDelayed(poll, 100)
    }

    /**
     * 铝合金圆盘：程序化绘制。旋转是圆盘整体（含侧键）；屏贴内容额外固定旋转 180°。
     * 旋转停止后保持当前角度定格（与 iOS「定格略抬正面」等效，2D 下即原位静止）。
     */
    private class PuckView(ctx: Context) : View(ctx) {
        var angle = 0f
            set(v) { field = v; invalidate() }
        private val dp = ctx.resources.displayMetrics.density

        override fun onDraw(c: Canvas) {
            super.onDraw(c)
            val cx = width / 2f
            val cy = height / 2f - 6 * dp
            val R = 62 * dp

            // 接触阴影（不随圆盘旋转）
            val shadow = Paint(Paint.ANTI_ALIAS_FLAG).apply { color = Color.argb(38, 0, 0, 0) }
            c.drawOval(cx - R * 0.88f, cy + R * 0.96f, cx + R * 0.88f, cy + R * 1.18f, shadow)

            c.save()
            c.rotate(angle, cx, cy)

            // 铝合金本体：径向渐变
            val body = Paint(Paint.ANTI_ALIAS_FLAG).apply {
                shader = RadialGradient(cx - R * 0.25f, cy - R * 0.35f, R * 1.25f,
                    intArrayOf(0xFFF2F5F7.toInt(), 0xFFD7DDE2.toInt(), 0xFFAEB6BD.toInt()),
                    floatArrayOf(0f, 0.55f, 1f), Shader.TileMode.CLAMP)
            }
            c.drawCircle(cx, cy, R, body)

            // 侧面按键示意（随盘转，转起来有实体感）
            c.drawRoundRect(cx + R * 0.96f, cy - 8 * dp, cx + R * 1.05f, cy + 4 * dp,
                3 * dp, 3 * dp, Paint(Paint.ANTI_ALIAS_FLAG).apply { color = 0xFFB7BFC5.toInt() })

            // 正面黑边 + 屏
            c.drawCircle(cx, cy, R * 0.90f, Paint(Paint.ANTI_ALIAS_FLAG).apply { color = 0xFF0A0A0C.toInt() })
            c.drawCircle(cx, cy, R * 0.82f, Paint(Paint.ANTI_ALIAS_FLAG).apply { color = 0xFF12101A.toInt() })

            // 屏贴内容：整体再转 180°（v8.3.27 按需求，内容上下颠倒）
            c.save()
            c.rotate(180f, cx, cy)
            val clip = Path().apply { addCircle(cx, cy, R * 0.80f, Path.Direction.CW) }
            c.clipPath(clip)
            // 粉色路线 + 青色起点 + 白色箭头（颠倒后的 MotoNav 小地图意象）
            c.drawPath(Path().apply {
                moveTo(cx - 26 * dp, cy + 20 * dp)
                cubicTo(cx - 6 * dp, cy + 30 * dp, cx + 8 * dp, cy + 2 * dp, cx + 24 * dp, cy - 18 * dp)
            }, Paint(Paint.ANTI_ALIAS_FLAG).apply {
                color = 0xFFFF4FB4.toInt(); style = Paint.Style.STROKE; strokeWidth = 4 * dp; strokeCap = Paint.Cap.ROUND
            })
            c.drawCircle(cx - 26 * dp, cy + 20 * dp, 5 * dp, Paint(Paint.ANTI_ALIAS_FLAG).apply { color = 0xFF43E1F0.toInt() })
            c.drawPath(Path().apply {
                moveTo(cx + 24 * dp, cy - 30 * dp)
                lineTo(cx + 32 * dp, cy - 12 * dp)
                lineTo(cx + 14 * dp, cy - 16 * dp)
                close()
            }, Paint(Paint.ANTI_ALIAS_FLAG).apply { color = 0xFFF3EEF8.toInt() })
            c.restore()

            // 屏上高光弧
            c.drawArc(cx - R * 0.86f, cy - R * 0.86f, cx + R * 0.86f, cy + R * 0.86f, 200f, 60f, false,
                Paint(Paint.ANTI_ALIAS_FLAG).apply { color = 0x30FFFFFF; style = Paint.Style.STROKE; strokeWidth = 2 * dp })

            c.restore()
        }
    }
}
