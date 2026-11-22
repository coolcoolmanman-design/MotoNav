package com.example.motonav

import android.app.Activity
import android.content.Context
import android.content.res.Configuration
import android.graphics.Color
import androidx.appcompat.app.AppCompatDelegate

/**
 * Cool gray + system blue appearance (ported from iOS AppTheme).
 * Mode: 0 auto (follow system), 1 force light, 2 force dark.
 */
object AppAppearance {
    private const val PREFS = "motonav"
    private const val KEY = "appearance_mode"

    const val MODE_SYSTEM = 0
    const val MODE_LIGHT = 1
    const val MODE_DARK = 2

    @Volatile var mode: Int = MODE_SYSTEM
        private set

    fun load(c: Context) {
        mode = c.applicationContext.getSharedPreferences(PREFS, 0).getInt(KEY, MODE_SYSTEM)
        applyNightMode()
    }

    fun setMode(c: Context, m: Int) {
        mode = m.coerceIn(0, 2)
        c.applicationContext.getSharedPreferences(PREFS, 0).edit().putInt(KEY, mode).apply()
        applyNightMode()
    }

    fun modeName(m: Int = mode) = when (m) {
        MODE_LIGHT -> "手动白"
        MODE_DARK -> "手动黑"
        else -> "自动"
    }

    private fun applyNightMode() {
        AppCompatDelegate.setDefaultNightMode(
            when (mode) {
                MODE_LIGHT -> AppCompatDelegate.MODE_NIGHT_NO
                MODE_DARK -> AppCompatDelegate.MODE_NIGHT_YES
                else -> AppCompatDelegate.MODE_NIGHT_FOLLOW_SYSTEM
            }
        )
    }

    fun isDark(ctx: Context): Boolean = when (mode) {
        MODE_LIGHT -> false
        MODE_DARK -> true
        else -> (ctx.resources.configuration.uiMode and Configuration.UI_MODE_NIGHT_MASK) ==
            Configuration.UI_MODE_NIGHT_YES
    }

    fun applyWindow(activity: Activity) {
        val dark = isDark(activity)
        activity.window.statusBarColor = if (dark) Color.BLACK else Color.parseColor("#F3F3F8")
        activity.window.navigationBarColor = if (dark) Color.BLACK else Color.parseColor("#F3F3F8")
        val decor = activity.window.decorView
        var flags = decor.systemUiVisibility
        flags = if (dark) {
            flags and android.view.View.SYSTEM_UI_FLAG_LIGHT_STATUS_BAR.inv()
        } else {
            flags or android.view.View.SYSTEM_UI_FLAG_LIGHT_STATUS_BAR
        }
        decor.systemUiVisibility = flags
    }
}

/** Semantic cool-gray palette shared by shell + trips (light/dark). */
object CoolTheme {
    fun bg(dark: Boolean) = if (dark) Color.BLACK else Color.parseColor("#F3F3F8")
    fun card(dark: Boolean) = if (dark) Color.parseColor("#1C1C1E") else Color.WHITE
    fun sunken(dark: Boolean) = if (dark) Color.parseColor("#2C2C2E") else Color.parseColor("#F2F2F7")
    fun stroke(dark: Boolean) = if (dark) Color.parseColor("#3C3C43") else Color.parseColor("#C7C7CC")
    fun text(dark: Boolean) = if (dark) Color.WHITE else Color.BLACK
    fun sub(dark: Boolean) = Color.parseColor("#8E8E93")
    fun accent(dark: Boolean) = if (dark) Color.parseColor("#0A84FF") else Color.parseColor("#007AFF")
    fun accentSoft(dark: Boolean) = if (dark) 0x380A84FF else 0x24007AFF
    fun chartCyan(dark: Boolean) = if (dark) Color.parseColor("#64D2FF") else Color.parseColor("#5AC8FA")
    fun red(dark: Boolean) = if (dark) Color.parseColor("#FF453A") else Color.parseColor("#FF3B30")
    fun green(dark: Boolean) = if (dark) Color.parseColor("#30D158") else Color.parseColor("#34C759")
    fun amber(dark: Boolean) = if (dark) Color.parseColor("#FF9F0A") else Color.parseColor("#FF9500")
}
