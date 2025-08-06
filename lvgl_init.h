#ifndef _LVGL_INIT_H_
#define _LVGL_INIT_H_
#include <stdio.h>
#include "pico/stdlib.h"
#include "libraries/lvgl/lvgl.h"
#include "hardware/timer.h"
#include "hardware/spi.h"

#define DISP_HOR_RES 480
#define DISP_VER_RES 320

static lv_draw_buf_t *buf1;
static lv_draw_buf_t *buf2;

static lv_display_t *disp;

static struct repeating_timer lvgl_timer;

void lvgl_init(void);
static void lcd_Flush_cb(lv_disp_t * disp, const lv_area_t * area, unsigned char * buf);
static void dma_handler(void);
static bool repeating_lvgl_timer_cb(struct repeating_timer *t);


#endif