package com.example.motonav

import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.Path
import android.graphics.RectF
import android.os.Bundle
import android.view.Gravity
import android.view.View
import android.view.ViewGroup
import android.widget.FrameLayout
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.ViewCompat
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat

/**
 * Four tabs matching iOS tip IA: 导航 / 行程 / 圆屏 / 我的.
 * Fold8: bottom bar on phone/portrait-inner; right side rail on cover / landscape-inner.
 */
class MainPagesShell(
    private val activity: AppCompatActivity,
    saved: Bundle?,
    private val build: (Int) -> View,
    private val onSelected: (Int) -> Unit
) : LinearLayout(activity) {
    companion object {
        val TITLES = listOf("导航", "行程", "圆屏", "我的")
        const val COUNT = 4
    }

    private fun dp(v: Int) = (v * resources.displayMetrics.density).toInt()
    private val host = FrameLayout(activity)
    private val pages = arrayOfNulls<ScrollView>(COUNT)
    private val restoreY = saved?.getIntArray("main_page_scroll")?.takeIf { it.size == COUNT } ?: IntArray(COUNT)
    private val tabs = ArrayList<LinearLayout>()
    private val icons = ArrayList<PageIcon>()
    private val labels = ArrayList<TextView>()
    private val indicators = ArrayList<FrameLayout>()
    private lateinit var navigation: LinearLayout
    var selectedPage = 0; private set
    private var dark = AppAppearance.isDark(activity)
    private var fold = FoldLayout.measure(activity)

    init {
        orientation = if (fold.useSideRail) HORIZONTAL else VERTICAL
        setBackgroundColor(CoolTheme.bg(dark))
        WindowCompat.setDecorFitsSystemWindows(activity.window, false)
        AppAppearance.applyWindow(activity)

        navigation = LinearLayout(activity).apply {
            orientation = if (fold.useSideRail) VERTICAL else HORIZONTAL
            setPadding(dp(5), dp(5), dp(5), dp(5))
            setBackgroundColor(CoolTheme.card(dark))
            elevation = dp(10).toFloat()
        }
        TITLES.forEachIndexed { index, title ->
            val tab = LinearLayout(activity).apply {
                orientation = VERTICAL; gravity = Gravity.CENTER; isFocusable = true
                contentDescription = title; minimumHeight = dp(58)
                setOnClickListener { showPage(index) }
            }
            val indicator = FrameLayout(activity)
            val icon = PageIcon(index)
            indicator.addView(icon, FrameLayout.LayoutParams(dp(23), dp(23), Gravity.CENTER))
            tab.addView(indicator, LayoutParams(dp(44), dp(29)))
            val label = TextView(activity).apply {
                text = title; textSize = 11.5f; setTextColor(CoolTheme.sub(dark)); gravity = Gravity.CENTER
                isSingleLine = true; includeFontPadding = false; setPadding(0, dp(4), 0, 0)
            }
            tab.addView(label, LayoutParams(-1, -2))
            if (fold.useSideRail) {
                navigation.addView(tab, LayoutParams(dp(72), dp(72)))
            } else {
                navigation.addView(tab, LayoutParams(0, dp(58), 1f))
            }
            tabs += tab; icons += icon; labels += label; indicators += indicator
        }

        if (fold.useSideRail) {
            // Right rail (Duo/Fold cover + landscape inner): content | rail
            addView(host, LayoutParams(0, -1, 1f))
            addView(navigation, LayoutParams(-2, -1))
        } else {
            addView(host, LayoutParams(-1, 0, 1f))
            addView(navigation, LayoutParams(-1, -2))
        }

        ViewCompat.setOnApplyWindowInsetsListener(this) { view, insets ->
            val bars = insets.getInsets(WindowInsetsCompat.Type.systemBars() or WindowInsetsCompat.Type.displayCutout())
            val ime = insets.getInsets(WindowInsetsCompat.Type.ime())
            view.setPadding(bars.left, bars.top, bars.right, maxOf(bars.bottom, ime.bottom)); insets
        }
    }

    fun isWideSplit(): Boolean = fold.wide || fold.innerUnfolded
    fun foldState(): FoldLayout.State = fold

    fun refreshChrome() {
        dark = AppAppearance.isDark(activity)
        fold = FoldLayout.measure(activity)
        setBackgroundColor(CoolTheme.bg(dark))
        navigation.setBackgroundColor(CoolTheme.card(dark))
        paintTabs()
    }

    fun showPage(index: Int) {
        if (index !in 0 until COUNT) return
        pages[selectedPage]?.visibility = GONE
        selectedPage = index
        val page = pages[index] ?: ScrollView(activity).apply {
            id = View.generateViewId(); isFillViewport = true; clipToPadding = true
            addView(build(index), FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT))
            host.addView(this, FrameLayout.LayoutParams(-1, -1)); pages[index] = this
            post { scrollTo(0, restoreY[index]) }
        }
        page.visibility = VISIBLE
        paintTabs()
        onSelected(index)
    }

    private fun paintTabs() {
        tabs.forEachIndexed { i, tab ->
            val selected = i == selectedPage; tab.isSelected = selected
            labels[i].setTextColor(if (selected) CoolTheme.accent(dark) else CoolTheme.sub(dark))
            labels[i].typeface = if (selected) android.graphics.Typeface.DEFAULT_BOLD else android.graphics.Typeface.DEFAULT
            indicators[i].background = if (selected) {
                android.graphics.drawable.GradientDrawable().apply {
                    setColor(CoolTheme.accentSoft(dark)); cornerRadius = dp(14).toFloat()
                }
            } else null
            icons[i].active = selected
            icons[i].invalidate()
        }
    }

    fun save(out: Bundle) {
        out.putInt("main_page_selected", selectedPage)
        out.putIntArray("main_page_scroll", IntArray(COUNT) { pages[it]?.scrollY ?: restoreY[it] })
    }

    private inner class PageIcon(private val index: Int) : View(activity) {
        var active = false
            set(value) { if (field != value) { field = value; invalidate() } }
        private val paint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
            style = Paint.Style.STROKE; strokeWidth = 1.8f
            strokeCap = Paint.Cap.ROUND; strokeJoin = Paint.Join.ROUND
        }
        override fun onDraw(canvas: Canvas) {
            super.onDraw(canvas)
            canvas.save(); canvas.scale(width / 24f, height / 24f)
            paint.color = if (active) CoolTheme.accent(dark) else CoolTheme.sub(dark)
            when (index) {
                0 -> { // 导航 · north arrow
                    canvas.drawPath(Path().apply {
                        moveTo(12f, 3f); lineTo(19f, 20f); lineTo(12f, 16f); lineTo(5f, 20f); close()
                    }, paint)
                }
                1 -> { // 行程 · map
                    canvas.drawPath(Path().apply {
                        moveTo(4f, 6f); lineTo(10f, 4f); lineTo(14f, 7f); lineTo(20f, 5f)
                        lineTo(20f, 18f); lineTo(14f, 20f); lineTo(10f, 17f); lineTo(4f, 19f); close()
                    }, paint)
                }
                2 -> { // 圆屏
                    canvas.drawCircle(12f, 12f, 9f, paint)
                    canvas.drawCircle(12f, 12f, 3f, paint)
                }
                else -> { // 我的
                    canvas.drawCircle(12f, 8f, 3.5f, paint)
                    canvas.drawArc(RectF(5f, 13f, 19f, 23f), 200f, 140f, false, paint)
                }
            }
            canvas.restore()
        }
    }
}
