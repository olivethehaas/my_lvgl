#include "lvgl_init.h"
#include "ili9488.h"


int dma_tx;
dma_channel_config c;


void lvgl_init()
{

    // Get a free channel, panic() if there are none
    int chan = dma_claim_unused_channel(true);
    c = dma_channel_get_default_config(dma_tx);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_8);
    channel_config_set_dreq(&c, DREQ_SPI1_TX);
    channel_config_set_read_increment(&c, true);
    channel_config_set_write_increment(&c, false);

    add_repeating_timer_ms(5, repeating_lvgl_timer_cb, NULL, &lvgl_timer);
    lv_init();
    disp = lv_display_create(DISP_HOR_RES, DISP_VER_RES);
    lv_display_set_flush_cb(disp, lcd_Flush_cb);
    buf1 = lv_draw_buf_create(240, 60, LV_COLOR_FORMAT_RGB565, LV_STRIDE_AUTO);
    buf2 = lv_draw_buf_create(240, 60, LV_COLOR_FORMAT_RGB565, LV_STRIDE_AUTO);
    lv_display_set_draw_buffers(disp, buf1, buf2);
    // lv_display_set_buffers(disp, buf1, buf2, sizeof(buf1), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_render_mode(disp, LV_DISPLAY_RENDER_MODE_PARTIAL);

    dma_channel_set_irq0_enabled(dma_tx, true);
    irq_set_exclusive_handler(DMA_IRQ_0, dma_handler);
    irq_set_enabled(DMA_IRQ_0, true);

    // screen = lv_obj_create(NULL);
    // lv_screen_load(screen);
    // ui_init();
}
/********************************************************************************
brief:	Refresh image by transferring the color data to the SPI bus by DMA
parameter:
********************************************************************************/
static void lcd_Flush_cb(lv_disp_t * disp, const lv_area_t * area, unsigned char * buf)
{
        uint8_t cmd_memwr = ILI9488_CMD_MEMORY_WRITE;
        uint x1, y1;
        x1 = area->x1;
        y1 = area->y1;
        uint32_t size = lv_area_get_width(area) * lv_area_get_height(area);
        /*
         * Reverse buffer data 
         */
        lv_draw_sw_rgb565_swap((void *) buf, size);

        /* 
         *  transfer pixel data via DMA function
         */
        /*while(1)
        {
                bool a = dma_channel_is_busy(dmaChannel);
                if(a == false)  break;
        }*/
        ili9488_SetWindow(x1, y1, lv_area_get_width(area), lv_area_get_height(area));
        ili9488_Send_Cmd(cmd_memwr); //RAMWR
        /* 
         *  transfer pixel data via DMA function
         */
         dma_channel_configure(
                dma_tx,             // Channel to be configured
                &c,                     // The configuration we just created
                &spi_get_hw(LCD_SPI_PORT)->dr,                    // The initial write address
                buf,                    // The initial read address
                size*2,                 // Number of transfers; in this case each is 1 byte.
                true);                    // Start immediately.
}


/********************************************************************************
brief:   Indicate ready with the flushing when DMA complete transmission
parameter:
********************************************************************************/
static void dma_handler(void)
{
    if (dma_channel_get_irq0_status(dma_tx)) {
        dma_channel_acknowledge_irq0(dma_tx);
        lv_disp_flush_ready(disp); // Indicate you are ready with the flushing
    }
}


/********************************************************************************
brief:   Report the elapsed time to LVGL each 5ms
parameter:
********************************************************************************/
static bool repeating_lvgl_timer_cb(struct repeating_timer *t) 
{
    lv_tick_inc(5);
    return true;
}
