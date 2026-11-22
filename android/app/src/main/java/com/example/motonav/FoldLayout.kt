package com.example.motonav

import android.content.Context
import android.content.res.Configuration
import android.util.DisplayMetrics
import android.view.WindowManager

/**
 * Samsung Fold8 / foldable layout helpers (Android counterpart to iOS TabLayout Duo rules).
 *
 * - Cover (outer): compact phone-like; side rail preferred when short/wide cover aspect.
 * - Inner unfolded: wide dual-pane; landscape → side rail, portrait → bottom tabs.
 *
 * User preference: 0 auto, 1 always bottom, 2 always right rail.
 */
object FoldLayout {
    const val PLACE_AUTO = 0
    const val PLACE_BOTTOM = 1
    const val PLACE_RIGHT = 2

    private const val PREFS = "motonav"
    private const val KEY_PLACE = "tab_placement"

    @Volatile var placement: Int = PLACE_AUTO
        private set

    fun load(c: Context) {
        placement = c.applicationContext.getSharedPreferences(PREFS, 0)
            .getInt(KEY_PLACE, PLACE_AUTO)
    }

    fun setPlacement(c: Context, p: Int) {
        placement = p.coerceIn(0, 2)
        c.applicationContext.getSharedPreferences(PREFS, 0).edit().putInt(KEY_PLACE, placement).apply()
    }

    fun placementName(p: Int = placement) = when (p) {
        PLACE_BOTTOM -> "底部"
        PLACE_RIGHT -> "右侧"
        else -> "自动"
    }

    data class State(
        val widthDp: Float,
        val heightDp: Float,
        val wide: Boolean,
        val useSideRail: Boolean,
        /** True when window looks like Fold inner screen (unfolded). */
        val innerUnfolded: Boolean,
        /** True when window looks like outer cover. */
        val coverCompact: Boolean,
    )

    fun measure(ctx: Context): State {
        val dm = ctx.resources.displayMetrics
        val density = dm.density.coerceAtLeast(0.5f)
        val metrics = DisplayMetrics()
        @Suppress("DEPRECATION")
        (ctx.getSystemService(Context.WINDOW_SERVICE) as WindowManager).defaultDisplay.getRealMetrics(metrics)
        val winW = ctx.resources.configuration.screenWidthDp.toFloat().takeIf { it > 0 }
            ?: (dm.widthPixels / density)
        val winH = ctx.resources.configuration.screenHeightDp.toFloat().takeIf { it > 0 }
            ?: (dm.heightPixels / density)
        val panelW = metrics.widthPixels / density
        val landscape = winW > winH
        val smallest = minOf(winW, winH)
        val wide = smallest >= 600f ||
            ctx.resources.configuration.screenLayout and Configuration.SCREENLAYOUT_SIZE_MASK >=
            Configuration.SCREENLAYOUT_SIZE_LARGE
        // Fold8-ish cover: short-wide vertical panel (~430–500 × ≤700 dp)
        val coverCompact = winW >= 400f && winH <= 720f && winW / winH >= 0.58f && !wide
        // Inner unfolded: large smallest-width or window much larger than typical phone
        val innerUnfolded = wide || (smallest >= 580f && maxOf(winW, winH) >= 700f)
        val useRail = when (placement) {
            PLACE_BOTTOM -> false
            PLACE_RIGHT -> true
            else -> {
                when {
                    coverCompact -> true
                    // Outer morph: window narrower than physical panel (status strip)
                    panelW > 0 && winW > 0 && panelW / winW > 1.15f -> true
                    innerUnfolded && landscape -> true
                    else -> false
                }
            }
        }
        return State(winW, winH, wide, useRail, innerUnfolded, coverCompact)
    }
}
