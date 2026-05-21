/*
* Copyright 2026 NXP
* NXP Proprietary. This software is owned or controlled by NXP and may only be used strictly in
* accordance with the applicable license terms. By expressly accepting such terms or by downloading, installing,
* activating and/or otherwise using the software, you are agreeing that you have read, and that you agree to
* comply with and are bound by, such license terms.  If you do not agree to be bound by the applicable license
* terms, then you may not retain, install, activate or otherwise use the software.
*/

#ifndef WIDGET_INIT_H
#define WIDGET_INIT_H
#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"
#include "gui_guider.h"
/* Pull lv_analogclock.h here so every translation unit that includes
 * widgets_init.h (e.g. setup_scr_screen.c) sees lv_analogclock_create and
 * gets LV_USE_ANALOGCLOCK predefined to 1 by the analogclock header. */
#include "lv_analogclock.h"

__attribute__((unused)) void kb_event_cb(lv_event_t *e);
__attribute__((unused)) void ta_event_cb(lv_event_t *e);
#if LV_USE_ANALOGCLOCK != 0
void clock_count(int *hour, int *min, int *sec);
#endif


void screen_analog_clock_1_timer(lv_timer_t *timer);

#ifdef __cplusplus
}
#endif
#endif
