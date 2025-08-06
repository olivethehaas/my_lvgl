/*drive for ILI9488
4-Line Serial Interface 
color mode 65K RGB 565
*/
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/dma.h"
#include "hardware/pio.h"
#include "hardware/pwm.h"
#include "ili9488.h"



uint slice_num;

typedef struct {
        uint8_t cmd;
        uint8_t dat[16];
        uint datLen;
        uint32_t sleep;
} ili9488_ini_str_t;

ili9488_ini_str_t lcd_ini_str[] = {
        {ILI9488_CMD_SOFTWARE_RESET, {0x00}, 0, 200}, 
                               /* software reset */
 /*       {ILI9488_CMD_POSITIVE_GAMMA_CORRECTION, {
        0X00, 0x03, 0x09, 0x08, 0x16, 
        0x0A, 0x3F, 0x78, 0x4C, 0x09,
        0x0A, 0x08, 0x16, 0x1A, 0x0F}, 15, 0},
        {ILI9488_CMD_NEGATIVE_GAMMA_CORRECTION, {
        0x00, 0x16, 0x19, 0x03, 0x0F,
        0x05, 0x32, 0x45, 0x46, 0x04,
        0x0E, 0x0D, 0x35, 0x37, 0x0F}, 15, 0},
        {ILI9488_CMD_POWER_CONTROL_1, {0x17, 0x15}, 2, 0},
        {ILI9488_CMD_POWER_CONTROL_2, {0x41}, 1, 0},
        {ILI9488_CMD_VCOM_CONTROL_1, {0x00, 0x12, 0x80}, 3, 0},
        {ILI9488_CMD_MEMORY_ACCESS_CONTROL, {0x40}, 1, 0},*/
    #if defined (TFT_PARALLEL_8_BIT) || defined (TFT_PARALLEL_16_BIT) || defined (RPI_DISPLAY_TYPE)
        {ILI9488_CMD_COLMOD_PIXEL_FORMAT_SET, {0x55}, 1, 0},
    #else 
        {ILI9488_CMD_COLMOD_PIXEL_FORMAT_SET, {0x66}, 1, 0},
    #endif
/*
        {ILI9488_CMD_INTERFACE_MODE_CONTROL, {0x00}, 1, 0},
        {ILI9488_CMD_FRAME_RATE_CONTROL_NORMAL, {0xA0}, 1, 0},
//        {ILI9488_CMD_DISPLAY_INVERSION_CONTROL, {0x02}, 1, 0},

        {ILI9488_CMD_DISPLAY_FUNCTION_CONTROL, {0x02, 0x02, 0x38}, 3, 0},
        {ILI9488_CMD_ENTRY_MODE_SET, {0xC6}, 1, 0},
        {ILI9488_CMD_ADJUST_CONTROL_3, {0xA9, 0x51, 0x2C, 0x82}, 4 , 0},
        */
        {ILI9488_CMD_DISP_INVERSION_ON, {0x00}, 0, 0},
        
        {ILI9488_CMD_SLEEP_OUT, {0x00}, 0, 120},
        {ILI9488_CMD_DISPLAY_ON, {0x00}, 0, 25},
        {0x00, {0x00}, 0, 0}                            /* EOL */
};

struct
{
        uint width;
        uint height;
} ili9488_resolution;




void ili9488_Init(uint rot)
{
 
        // SPI initialisation. This example will use SPI at 1MHz.
        spi_init(LCD_SPI_PORT, 1 * 1000 * 1000);
        spi_set_baudrate(LCD_SPI_PORT, 46000000);
        gpio_set_function(LCD_MISO, GPIO_FUNC_SPI);
        gpio_set_function(LCD_CS, GPIO_FUNC_SIO);
        gpio_set_function(LCD_SCK, GPIO_FUNC_SPI);
        gpio_set_function(LCD_MOSI, GPIO_FUNC_SPI);

        // Chip select is active-low, so we'll initialise it to a driven-high state
        gpio_init(LCD_CS);
        gpio_set_dir(LCD_CS, GPIO_OUT);
        gpio_put(LCD_CS, 1);
        gpio_init(LCD_DC);
        gpio_set_dir(LCD_DC, GPIO_OUT);
        gpio_put(LCD_DC, 1);
        gpio_init(LCD_RESET);
        gpio_set_dir(LCD_RESET, GPIO_OUT);
        gpio_put(LCD_RESET, 1);

        // LCD BackLight PWM control
        gpio_set_function(LCD_LED, GPIO_FUNC_PWM);
        slice_num = pwm_gpio_to_slice_num(LCD_LED);
        pwm_set_wrap(slice_num, (LCD_LED_PWM_MAX - 1));
        pwm_set_chan_level(slice_num, LCD_LED, 0);
        pwm_set_enabled(slice_num, true);

        // initialize LCD
        ili9488_HardReset();
        ili9488_SstLED(10);
        ili9488_SendInitStr();

        ili9488_setRotate(rot);
        ili9488_SetWindow(0, 0, ili9488_resolution.width, ili9488_resolution.width);
        ili9488_SstLED(100);
}

void ili9488_HardReset()
{
        gpio_put(LCD_RESET, 1);
        sleep_ms(10);
        gpio_put(LCD_RESET, 0);
        sleep_ms(100);
        gpio_put(LCD_RESET, 1);
        sleep_ms(100);
}

void ili9488_SstLED(uint parcent)
{
        if (parcent > 100)
        {
                parcent = 100;
        }
        pwm_set_chan_level(slice_num, LCD_LED, parcent * 20);
}

void ili9488_SendInitStr()
{
        ili9488_SetCS(0);
        uint i = 0;
        while(lcd_ini_str[i].cmd != 0x00)
        {
                uint8_t cmd = lcd_ini_str[i].cmd;
                uint datLen = lcd_ini_str[i].datLen;
                uint8_t *dat;
                dat = &(lcd_ini_str[i].dat[0]);
                uint32_t slp = lcd_ini_str[i].sleep;

                ili9488_SetDC(0);
                spi_write_blocking(LCD_SPI_PORT, &cmd, 1);

                if(datLen > 0)
                {
                        ili9488_SetDC(1);
                        spi_write_blocking(LCD_SPI_PORT, dat, datLen);
                }
                if(slp > 0)
                {
                        sleep_ms(slp);
                }
                i++;
        }
        ili9488_SetCS(1);
}

void ili9488_SetCS(bool val)
{
        asm volatile("nop\n");
        asm volatile("nop\n");
        gpio_put(LCD_CS, val);
        asm volatile("nop\n");
        asm volatile("nop\n");
}

void ili9488_SetDC(bool val)
{
        asm volatile("nop\n");
        asm volatile("nop\n");
        gpio_put(LCD_DC, val);
        asm volatile("nop\n");
        asm volatile("nop\n");
}

void ili9488_Send_Cmd(uint8_t cmd)
{
        ili9488_SetCS(0);   
        ili9488_SetDC(0);
        spi_write_blocking(LCD_SPI_PORT, &cmd, 1);
        ili9488_SetDC(1);
}



void ili9488_SendData(uint8_t cmd, uint8_t *data, uint length)
{
        ili9488_SetCS(0);
        ili9488_SetDC(0);
        spi_write_blocking(LCD_SPI_PORT, &cmd, 1);
        ili9488_SetDC(1);
        spi_write_blocking(LCD_SPI_PORT, data, length);
        ili9488_SetCS(1);
}

void ili9488_setRotate(uint rot)
{
        uint8_t cmd = ILI9488_CMD_MEMORY_ACCESS_CONTROL;
        uint8_t r;
        switch (rot)
        {
                case LCD_PORTRAIT:
                        r = TFT_MAD_MX;
                        ili9488_resolution.width = TFT_WIDTH;
                        ili9488_resolution.height = TFT_HEIGHT;
                        break;
                case LCD_LANDSCAPE:
                        r = TFT_MAD_MV;
                        ili9488_resolution.width = TFT_HEIGHT;
                        ili9488_resolution.height = TFT_WIDTH;
                        break;
                case LCD_INV_PORTRAIT:
                        r = TFT_MAD_MY;
                        ili9488_resolution.width = TFT_WIDTH;
                        ili9488_resolution.height = TFT_HEIGHT;
                        break;
                case LCD_INV_LANDSCAPE:
                        r = (TFT_MAD_MX | TFT_MAD_MY | TFT_MAD_MV);
                        ili9488_resolution.width = TFT_HEIGHT;
                        ili9488_resolution.height = TFT_WIDTH;
                        break;
                default:
                        r = TFT_MAD_MX;
                        ili9488_resolution.width = TFT_WIDTH;
                        ili9488_resolution.height = TFT_HEIGHT;
        }

        r |= 0x08;
        ili9488_SendData(cmd, &r, 1);
}

void ili9488_SetWindow(uint x, uint y, uint w, uint h)
{
        /* CASET */
        uint8_t cmd = 0x2A;
        uint8_t buf4[4];
        buf4[0] = (x >> 8) & 0xFF;
        buf4[1] = x & 0xFF;
        buf4[2] = ((x + w - 1) >> 8) & 0xFF;
        buf4[3] = (x + w - 1) & 0xFF;
        ili9488_SendData(cmd, buf4, 4);

        /* RASET */
        cmd = 0x2B;
        buf4[0] = (y >> 8) & 0xFF;
        buf4[1] = y & 0xFF;
        buf4[2] = ((y + h - 1) >> 8) & 0xFF;
        buf4[3] = (y + h - 1) & 0xFF;
        ili9488_SendData(cmd, buf4, 4);
}


uint lcd_Get_Width()
{
        return(ili9488_resolution.width);
}


uint lcd_Get_height()
{
        return(ili9488_resolution.height);
}
