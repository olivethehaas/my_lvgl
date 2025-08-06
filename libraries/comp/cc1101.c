#include <stdio.h>
#include "pico/stdlib.h"
#include "cc1101.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"

//spi_inst_t *spi = spi0;
bool cc1101_rdy;
uint8_t cc1101_state;
uint8_t csb;

#define CC1101_STATE_IDLE 0
#define CC1101_STATE_RX 1
#define CC1101_STATE_TX 2
#define CC1101_STATE_FSTXON 3
#define CC1101_STATE_CALIBRATE 4
#define CC1101_STATE_SETTLING 5
#define CC1101_STATE_RX_OVERFLOW 6
#define CC1101_STATE_TX_UNDERFLOW 7

void start_SPI(void) {
    // Initialize CS pin high
    gpio_init(CS);
    gpio_set_dir(CS, GPIO_OUT);
    gpio_put(CS, 1);

    gpio_init(CC1101_GDO0);
    gpio_init(CC1101_GDO2);
    gpio_set_function(CC1101_GDO0, GPIO_FUNC_SIO);
    gpio_set_function(CC1101_GDO2, GPIO_FUNC_SIO);
    gpio_set_dir(CC1101_GDO0, GPIO_IN);
    gpio_set_dir(CC1101_GDO2, GPIO_IN);

    // Initialize SPI port at 1 MHz

    // Initialize SPI pins
    gpio_init(CLK);
    gpio_set_dir(CLK, GPIO_OUT);
    gpio_set_function(CLK, GPIO_FUNC_SPI);
    gpio_init(MOSI);
    gpio_set_dir(MOSI, GPIO_OUT);
    gpio_set_function(MOSI, GPIO_FUNC_SPI);
    gpio_init(MISO);
    gpio_set_dir(MISO, GPIO_IN);
    gpio_set_function(MISO, GPIO_FUNC_SPI);

    //printf("CLK: %d   MOSI:  %d   MISO:  %d\n", CLK, MOSI, MISO);
    spi_init(CC1101_SPI_PORT, 1000 * 1000);

    // Set SPI format
    spi_set_format(CC1101_SPI_PORT,  // SPI instance
                   8,     // Number of bits per transfer
                   1,     // Polarity (CPOL)
                   1,     // Phase (CPHA)
                   SPI_MSB_FIRST);

    //printf("SPI initialized\n");
    sleep_ms(10);
    //return true;
}

/**
 * Macros
 */
// Select (SPI) CC1101 OK
void cc1101_chipSelect(void) {
    asm volatile("nop \n nop \n nop");
    gpio_put(CS, 0);
    asm volatile("nop \n nop \n nop");
}
// Deselect (SPI) CC1101 OK
void cc1101_chipDeselect() {
    asm volatile("nop \n nop \n nop");
    gpio_put(CS, 1);
    asm volatile("nop \n nop \n nop");
}
// Wait until SPI MISO line goes low
#define cc1101_waitMiso() while (gpio_get(MISO) == HIGH)
// Get GDO0 pin state
#define getGDO0state() gpio_get(CC1101_GDO0)
// Wait until GDO0 line goes high
#define wait_GDO0_high() while (gpio_get(CC1101_GDO0) == LOW)
// Wait until GDO0 line goes low
#define wait_GDO0_low() while (gpio_get(CC1101_GDO0) == HIGH)

bool bitRead(uint8_t *x, char n) { return (*x & (1 << n)) ? 1 : 0; }

/**
 * wakeUp
 *
 * Wake up CC1101 from Power Down state
 */
void cc1101_wakeUp(void) {
    cc1101_chipSelect();  // Select CC1101
    sleep_ms(10);
    cc1101_waitMiso();        // Wait until MISO goes low
    cc1101_chipDeselect();  // Deselect CC1101
    sleep_ms(10);
}

/**
 * writeReg
 *
 * Write single register into the CC1101 IC via SPI
 *
 * 'regAddr'	Register address
 * 'value'	Value to be writen
 */
void cc1101_writeReg(uint8_t regAddr, uint8_t value) {
    cc1101_chipSelect();  // Select CC1101
    cc1101_waitMiso();      // Wait until MISO goes low
/*
Write len bytes from src to SPI. Simultaneously read len bytes from SPI to dst. Blocks until all data is transferred
Parameters
    spi	SPI instance specifier, either spi0 or spi1
    src	Buffer of data to write
    dst	Buffer for read data
    len	Length of BOTH buffers

Returns
    Number of bytes written/read 
*/
    spi_write_read_blocking(CC1101_SPI_PORT, &regAddr, &csb, 1);

    spi_write_read_blocking(CC1101_SPI_PORT, &value, &csb, 1);
    //printf("writeReg: [0x%02X] csb: [0x%02X]\n", value, csb);

    cc1101_chipDeselect();  // Deselect CC1101
}

/**
 * writeBurstReg
 *
 * Write multiple registers into the CC1101 IC via SPI
 *
 * 'regAddr'	Register address
 * 'buffer'	Data to be writen
 * 'len'	Data length
 */
void cc1101_writeBurstReg(uint8_t regAddr, uint8_t *buffer, uint8_t len) {
    uint8_t addr, i;
    // printf("writeBurstReg: [0x%02X] len: %d\n", regAddr, len);

    addr = regAddr | WRITE_BURST;  // Enable burst transfer
    cc1101_chipSelect();               // Select CC1101
    cc1101_waitMiso();                   // Wait until MISO goes low
    spi_write_read_blocking(CC1101_SPI_PORT, &regAddr, &csb, 1);

    for (i = 0; i < len; i++) {
        // SPI.transfer(buffer[i]);  // Send value
        spi_write_read_blocking(CC1101_SPI_PORT, &(buffer[i]), &csb, 1);
        //printf("[%02X] ", buffer[i]);
    }
    cc1101_chipDeselect();  // Deselect CC1101
}

/**
 * cmdStrobe
 *
 * Send command strobe to the CC1101 IC via SPI
 *
 * 'cmd'	Command strobe
 */
void cc1101_cmdStrobe(uint8_t cmd) {
    cc1101_chipSelect();  // Select CC1101
    cc1101_waitMiso();      // Wait until MISO goes low
    spi_write_read_blocking(CC1101_SPI_PORT, &cmd, &csb, 1);
    cc1101_chipDeselect();  // Deselect CC1101
}

/**
 * readReg
 *
 * Read CC1101 register via SPI
 *
 * 'regAddr'	Register address
 * 'regType'	Type of register: CC1101_CONFIG_REGISTER or CC1101_STATUS_REGISTER
 *
 * Return:
 * 	Data uint8_t returned by the CC1101 IC
 */
uint8_t cc1101_readReg(uint8_t regAddr, uint8_t regType) {
    uint8_t addr;
    uint8_t val = 0xFF;

    //addr = regAddr | regType;
    addr = regType | regAddr;  // read register, no burst
    cc1101_chipSelect();  // Select CC1101
    cc1101_waitMiso();      // Wait until MISO goes low
    // SPI.transfer(addr);        // Send register address
    // val = SPI.transfer(0x00);  // Read result
    int n = spi_write_read_blocking(CC1101_SPI_PORT, &addr, &csb, 1);
    spi_read_blocking(CC1101_SPI_PORT, 0x00, &val, 1);
    cc1101_chipDeselect();
    //cc1101_waitMiso();
    return val;
}

/**
 * readBurstReg
 *
 * Read burst data from CC1101 via SPI
 *
 * 'buffer'	Buffer where to copy the result to
 * 'regAddr'	Register address
 * 'len'	Data length
 */
void cc1101_readBurstReg(uint8_t *buffer, uint8_t regAddr, uint8_t len) {
    uint8_t addr, i;

    addr = regAddr | READ_BURST;
    cc1101_chipSelect();  // Select CC1101
    cc1101_waitMiso();      // Wait until MISO goes low
    // SPI.transfer(addr);                                        // Send register address
    // for (i = 0; i < len; i++) buffer[i] = SPI.transfer(0x00);  // Read result uint8_t by uint8_t
    spi_write_blocking(CC1101_SPI_PORT, &addr, 1);
    for (i = 0; i < len; i++) {
        // buffer[i] = SPI.transfer(0x00);
        spi_read_blocking(CC1101_SPI_PORT, 0, &buffer[i], 1);
    }
    cc1101_chipDeselect();  // Deselect CC1101
}


/**
 * cc1101_Init_regs
 *
 * Configure CC1101 registers from wmbus_t_cc1101_config.h
 */
void cc1101_initRegisters(void){
    cc1101_writeReg(CC1101_IOCFG2, 0x06);   // GDO2 Output Pin Configuration
    cc1101_writeReg(CC1101_IOCFG1, 0x2E);   // GDO1 Output Pin Configuration
    cc1101_writeReg(CC1101_IOCFG0, 0x00);   // GDO0 Output Pin Configuration
    cc1101_writeReg(CC1101_FIFOTHR, 0x7);   // RX FIFO and TX FIFO Thresholds
    cc1101_writeReg(CC1101_SYNC1, 0x54);    // Sync Word, High Byte
    cc1101_writeReg(CC1101_SYNC0, 0x3D);    // Sync Word, Low Byte
    cc1101_writeReg(CC1101_PKTLEN, 0xFF);   // Packet Length
    cc1101_writeReg(CC1101_PKTCTRL1, 0x0);  // Packet Automation Control
    cc1101_writeReg(CC1101_PKTCTRL0, 0x0);  // Packet Automation Control
    cc1101_writeReg(CC1101_ADDR, 0x0);      // Device Address
    cc1101_writeReg(CC1101_CHANNR, 0x0);    // Channel Number
    cc1101_writeReg(CC1101_FSCTRL1, 0x8);   // Frequency Synthesizer Control
    cc1101_writeReg(CC1101_FSCTRL0, 0x0);   // Frequency Synthesizer Control
    cc1101_writeReg(CC1101_FREQ2, 0x21);    // Frequency Control Word, High Byte
    cc1101_writeReg(CC1101_FREQ1, 0x6B);    // Frequency Control Word, Middle Byte
    cc1101_writeReg(CC1101_FREQ0, 0xD0);    // Frequency Control Word, Low Byte
    cc1101_writeReg(CC1101_MDMCFG4, 0x5C);  // Modem Configuration
    cc1101_writeReg(CC1101_MDMCFG3, 0x4);   // Modem Configuration
    cc1101_writeReg(CC1101_MDMCFG2, 0x5);   // Modem Configuration
    cc1101_writeReg(CC1101_MDMCFG1, 0x22);  // Modem Configuration
    cc1101_writeReg(CC1101_MDMCFG0, 0xF8);  // Modem Configuration
    cc1101_writeReg(CC1101_DEVIATN, 0x44);  // Modem Deviation Setting
    cc1101_writeReg(CC1101_MCSM2, 0x7);     // Main Radio Control State Machine Configuration
    cc1101_writeReg(CC1101_MCSM1, 0x00);    // Main Radio Control State Machine Configuration
    cc1101_writeReg(CC1101_MCSM0, 0x18);    // Main Radio Control State Machine Configuration
    cc1101_writeReg(CC1101_FOCCFG, 0x2E);   // Frequency Offset Compensation Configuration
    cc1101_writeReg(CC1101_BSCFG, 0xBF);    // Bit Synchronization Configuration
    cc1101_writeReg(CC1101_AGCCTRL2, 0x43); // AGC Control
    cc1101_writeReg(CC1101_AGCCTRL1, 0x9);  // AGC Control
    cc1101_writeReg(CC1101_AGCCTRL0, 0xB5); // AGC Control
    cc1101_writeReg(CC1101_WOREVT1, 0x87);  // High Byte Event0 Timeout
    cc1101_writeReg(CC1101_WOREVT0, 0x6B);  // Low Byte Event0 Timeout
    cc1101_writeReg(CC1101_WORCTRL, 0xFB);  // Wake On Radio Control
    cc1101_writeReg(CC1101_FREND1, 0xB6);   // Front End RX Configuration
    cc1101_writeReg(CC1101_FREND0, 0x10);   // Front End TX Configuration
    cc1101_writeReg(CC1101_FSCAL3, 0xEA);   // Frequency Synthesizer Calibration
    cc1101_writeReg(CC1101_FSCAL2, 0x2A);   // Frequency Synthesizer Calibration
    cc1101_writeReg(CC1101_FSCAL1, 0x0);    // Frequency Synthesizer Calibration
    cc1101_writeReg(CC1101_FSCAL0, 0x1F);   // Frequency Synthesizer Calibration
    cc1101_writeReg(CC1101_RCCTRL1, 0x41);  // RC Oscillator Configuration
    cc1101_writeReg(CC1101_RCCTRL0, 0x0);   // RC Oscillator Configuration
    cc1101_writeReg(CC1101_FSTEST, 0x59);   // Frequency Synthesizer Calibration Control
    cc1101_writeReg(CC1101_PTEST, 0x7F);    // Production Test
    cc1101_writeReg(CC1101_AGCTEST, 0x3F);  // AGC Test
    cc1101_writeReg(CC1101_TEST2, 0x81);    // Various Test Settings
    cc1101_writeReg(CC1101_TEST1, 0x35);    // Various Test Settings
    cc1101_writeReg(CC1101_TEST0, 0x9);     // Various Test Settings
}
/**
 * reset
 *
 * Reset CC1101
 */
void cc1101_reset(void) {
    cc1101_chipDeselect();  // Deselect CC1101
    sleep_ms(250);
    cc1101_chipSelect();  // Select CC1101
    sleep_ms(250);
    cc1101_chipDeselect();  // Deselect CC1101
    sleep_ms(250);
    cc1101_chipSelect();  // Select CC1101

    cc1101_waitMiso();  // Wait until MISO goes low
                  // SPI.transfer(CC1101_SRES);  // Send reset command strobe
    cc1101_cmdStrobe(CC1101_SRES);
    cc1101_waitMiso();        // Wait until MISO goes low
    cc1101_chipDeselect();  // Deselect CC1101

    //uint8_t val = readStatusReg(CC1101_PARTNUM);
    //printf("Partnum: [0x%02X]\n", val);
}
