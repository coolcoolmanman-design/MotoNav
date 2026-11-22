package com.example.motonav

import android.content.Context
import android.content.Intent
import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.Typeface
import android.graphics.drawable.GradientDrawable
import android.view.Gravity
import android.view.View
import android.widget.LinearLayout
import android.widget.TextView
import java.text.SimpleDateFormat
import java.util.Calendar
import java.util.Date
import java.util.Locale
import kotlin.math.max

/**
 * Trips tab overview (ported from iOS TripsOverviewView scheme A).
 * Week / month / year period chips + hero distance + metric grid + recent list.
 * On Fold8 inner unfolded (wideSplit), lays out overview | recent side-by-side.
 */
object TripsOverview {
    enum class Period(val label: String) { WEEK("周"), MONTH("月"), YEAR("年") }

    fun build(ctx: Context, wideSplit: Boolean, dark: Boolean): View {
        val density = ctx.resources.displayMetrics.density
        fun dp(v: Int) = (v * density).toInt()
        val root = LinearLayout(ctx).apply {
            orientation = if (wideSplit) LinearLayout.HORIZONTAL else LinearLayout.VERTICAL
            setPadding(dp(16), dp(8), dp(16), dp(24))
        }
        val left = LinearLayout(ctx).apply { orientation = LinearLayout.VERTICAL }
        val right = LinearLayout(ctx).apply { orientation = LinearLayout.VERTICAL }

        val title = TextView(ctx).apply {
            text = "行程"; textSize = 28f; typeface = Typeface.DEFAULT_BOLD
            setTextColor(CoolTheme.text(dark))
        }
        val subtitle = TextView(ctx).apply {
            text = "行程概述"; textSize = 13f; setTextColor(CoolTheme.sub(dark))
            setPadding(0, dp(2), 0, dp(12))
        }
        left.addView(title); left.addView(subtitle)

        var period = Period.WEEK
        val periodRow = LinearLayout(ctx).apply {
            orientation = LinearLayout.HORIZONTAL
            background = GradientDrawable().apply {
                setColor(CoolTheme.sunken(dark)); cornerRadius = dp(20).toFloat()
            }
            setPadding(dp(3), dp(3), dp(3), dp(3))
        }
        val chips = ArrayList<TextView>()
        val hero = TextView(ctx).apply {
            textSize = 34f; typeface = Typeface.DEFAULT_BOLD; setTextColor(CoolTheme.text(dark))
            setPadding(0, dp(16), 0, dp(4))
        }
        val heroSub = TextView(ctx).apply {
            textSize = 13f; setTextColor(CoolTheme.sub(dark)); setPadding(0, 0, 0, dp(12))
        }
        val metrics = LinearLayout(ctx).apply { orientation = LinearLayout.HORIZONTAL }
        val metricViews = (0 until 3).map {
            TextView(ctx).apply {
                textSize = 14f; setTextColor(CoolTheme.text(dark))
                setPadding(dp(10), dp(12), dp(10), dp(12))
                background = GradientDrawable().apply {
                    setColor(CoolTheme.card(dark)); cornerRadius = dp(14).toFloat()
                }
            }
        }
        metricViews.forEachIndexed { i, tv ->
            metrics.addView(tv, LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1f).apply {
                if (i < 2) rightMargin = dp(8)
            })
        }
        val chart = BarChartView(ctx, dark)
        val recentBox = LinearLayout(ctx).apply { orientation = LinearLayout.VERTICAL }
        val recentTitle = TextView(ctx).apply {
            text = "最近行程"; textSize = 17f; typeface = Typeface.DEFAULT_BOLD
            setTextColor(CoolTheme.text(dark)); setPadding(0, dp(16), 0, dp(8))
        }
        recentBox.addView(recentTitle)

        fun paintChips() {
            chips.forEachIndexed { i, tv ->
                val on = Period.entries[i] == period
                tv.setTextColor(CoolTheme.text(dark))
                tv.typeface = if (on) Typeface.DEFAULT_BOLD else Typeface.DEFAULT
                tv.background = if (on) GradientDrawable().apply {
                    setColor(CoolTheme.card(dark)); cornerRadius = dp(16).toFloat()
                } else null
            }
        }

        fun render(list: List<RideSummary>) {
            val snap = snapshot(period, list)
            hero.text = formatKm(snap.meters)
            heroSub.text = when (period) {
                Period.WEEK -> "本周骑行"
                Period.MONTH -> "本月骑行"
                Period.YEAR -> "本年骑行"
            } + " · ${snap.count} 次"
            metricViews[0].text = "时长\n${formatDur(snap.durationMs)}"
            metricViews[1].text = "最高速\n${"%.0f".format(snap.maxKmh)} km/h"
            metricViews[2].text = "日均\n${formatKm(snap.dayAvg)}"
            chart.setBars(snap.bars)
            recentBox.removeAllViews()
            recentBox.addView(recentTitle)
            val recent = list.sortedByDescending { it.start }.take(8)
            if (recent.isEmpty()) {
                recentBox.addView(TextView(ctx).apply {
                    text = "暂无行程；真实导航与巡航会自动记录（模拟不记）。"
                    textSize = 13f; setTextColor(CoolTheme.sub(dark))
                })
            } else recent.forEach { r ->
                recentBox.addView(TextView(ctx).apply {
                    val day = SimpleDateFormat("M月d日 HH:mm", Locale.CHINA).format(Date(r.start))
                    text = "$day · ${RideRecorder.distanceText(r.meters)} · ${r.mode}"
                    textSize = 14f; setTextColor(CoolTheme.text(dark))
                    setPadding(dp(12), dp(12), dp(12), dp(12))
                    background = GradientDrawable().apply {
                        setColor(CoolTheme.card(dark)); cornerRadius = dp(12).toFloat()
                    }
                    setOnClickListener {
                        ctx.startActivity(Intent(ctx, RideHistoryActivity::class.java))
                    }
                }, LinearLayout.LayoutParams(-1, -2).apply { bottomMargin = dp(8) })
            }
        }

        Period.entries.forEachIndexed { i, p ->
            val tv = TextView(ctx).apply {
                text = p.label; gravity = Gravity.CENTER; textSize = 13f
                setPadding(0, dp(8), 0, dp(8))
                setOnClickListener {
                    period = p; paintChips()
                    RideRecorder.history { list, _ -> render(list) }
                }
            }
            chips += tv
            periodRow.addView(tv, LinearLayout.LayoutParams(0, -2, 1f))
        }
        paintChips()

        left.addView(periodRow)
        left.addView(hero)
        left.addView(heroSub)
        left.addView(metrics)
        left.addView(TextView(ctx).apply {
            text = "里程分布"; textSize = 15f; typeface = Typeface.DEFAULT_BOLD
            setTextColor(CoolTheme.text(dark)); setPadding(0, dp(18), 0, dp(8))
        })
        left.addView(chart, LinearLayout.LayoutParams(-1, dp(120)))
        left.addView(TextView(ctx).apply {
            text = "全部轨迹 ›"; textSize = 15f; setTextColor(CoolTheme.accent(dark))
            setPadding(0, dp(16), 0, 0)
            setOnClickListener { ctx.startActivity(Intent(ctx, RideHistoryActivity::class.java)) }
        })

        if (wideSplit) {
            root.addView(left, LinearLayout.LayoutParams(0, -2, 1f).apply { rightMargin = dp(12) })
            root.addView(recentBox, LinearLayout.LayoutParams(0, -2, 1f))
        } else {
            left.addView(recentBox)
            root.addView(left)
        }

        RideRecorder.history { list, _ -> render(list) }
        return root
    }

    private data class Snap(
        val meters: Double, val durationMs: Long, val maxKmh: Double,
        val count: Int, val dayAvg: Double, val bars: List<Float>
    )

    private fun snapshot(period: Period, rides: List<RideSummary>): Snap {
        val cal = Calendar.getInstance()
        val now = System.currentTimeMillis()
        fun startBound(): Long {
            cal.timeInMillis = now
            when (period) {
                Period.WEEK -> {
                    cal.firstDayOfWeek = Calendar.MONDAY
                    cal.set(Calendar.DAY_OF_WEEK, Calendar.MONDAY)
                }
                Period.MONTH -> cal.set(Calendar.DAY_OF_MONTH, 1)
                Period.YEAR -> cal.set(Calendar.DAY_OF_YEAR, 1)
            }
            cal.set(Calendar.HOUR_OF_DAY, 0); cal.set(Calendar.MINUTE, 0)
            cal.set(Calendar.SECOND, 0); cal.set(Calendar.MILLISECOND, 0)
            return cal.timeInMillis
        }
        val from = startBound()
        val filtered = rides.filter { it.start >= from && !it.active }
        val meters = filtered.sumOf { it.meters }
        val dur = filtered.sumOf { max(0L, it.end - it.start) }
        val maxK = filtered.maxOfOrNull { it.maxKmh } ?: 0.0
        val days = max(1, ((now - from) / 86_400_000L).toInt() + 1)
        val barCount = when (period) {
            Period.WEEK -> 7
            Period.MONTH -> 6
            Period.YEAR -> 12
        }
        val bars = FloatArray(barCount)
        filtered.forEach { r ->
            val idx = when (period) {
                Period.WEEK -> {
                    cal.timeInMillis = r.start
                    val dow = (cal.get(Calendar.DAY_OF_WEEK) + 5) % 7
                    dow
                }
                Period.MONTH -> ((r.start - from) / (days * 86_400_000L / barCount)).toInt().coerceIn(0, barCount - 1)
                Period.YEAR -> {
                    cal.timeInMillis = r.start
                    cal.get(Calendar.MONTH)
                }
            }
            bars[idx] = bars[idx] + r.meters.toFloat()
        }
        val peak = bars.maxOrNull()?.takeIf { it > 0f } ?: 1f
        return Snap(meters, dur, maxK, filtered.size, meters / days, bars.map { it / peak })
    }

    private fun formatKm(m: Double) = when {
        m >= 100_000 -> "${(m / 1000).toInt()} 公里"
        m >= 1000 -> String.format(Locale.CHINA, "%.1f 公里", m / 1000)
        else -> "${m.toInt()} 米"
    }

    private fun formatDur(ms: Long): String {
        val min = ms / 60_000
        return if (min >= 60) "${min / 60}小时${min % 60}分" else "${min}分钟"
    }

    private class BarChartView(ctx: Context, private val dark: Boolean) : View(ctx) {
        private var bars: List<Float> = emptyList()
        private val paint = Paint(Paint.ANTI_ALIAS_FLAG)
        fun setBars(v: List<Float>) { bars = v; invalidate() }
        override fun onDraw(c: Canvas) {
            super.onDraw(c)
            if (bars.isEmpty()) return
            val n = bars.size
            val gap = width * 0.04f / n
            val w = (width - gap * (n + 1)) / n
            paint.color = CoolTheme.chartCyan(dark)
            bars.forEachIndexed { i, v ->
                val h = height * 0.85f * v.coerceIn(0f, 1f)
                val left = gap + i * (w + gap)
                c.drawRoundRect(left, height - h, left + w, height.toFloat(), 8f, 8f, paint)
            }
        }
    }
}
