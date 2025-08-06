#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/dma.h"
#include "libraries/comp/interrupt.h"
#include "libraries/comp/cc1101.h"
#include "libraries/comp/3outof6.h"
#include "libraries/comp/mbus_packet.h"
#include "libraries/comp/aes.h"

#include "ui/screens.h"
#include "ui/ui.h"
#include "libraries/comp/ili9488.h"
#include "libraries/lvgl/lvgl.h"
#include "lvgl_init.h"

// #include "ui/ui.h"
// #include "demos/lv_demos.h"
// #include "examples/lv_examples.h"

uint8_t aes_default_key[16] = {0x51, 0x72, 0x89, 0x10, 0xE6, 0x6D, 0x83, 0xF8, 0x51, 0x72, 0x89, 0x10, 0xE6, 0x6D, 0x83, 0xF8};
uint8_t aes_iv[16];

// RX - Buffers
uint8_t RXpacket[291];
uint8_t RXbytes[584];

RXinfoDescr RXinfo;

bool receiving = false;

// gd0 rxFifoISR, rising
// gd2 rxPacketRecvdISR falling

void rxFifoISR(void)
{

  uint8_t fixedLength;
  uint8_t bytesDecoded[2];

  // - RX FIFO 4 bytes detected -
  // Calculate the total length of the packet, and set fixed mode if less
  // than 255 bytes to receive
  if (RXinfo.start == true)
  {
    // Read the 3 first bytes
    cc1101_readBurstReg(RXinfo.pByteIndex, CC1101_RXFIFO, 3);

    // - Calculate the total number of bytes to receive -
    // Possible improvment: Check the return value from the deocding function,
    // and abort RX if coding error.
    decode3outof6(RXinfo.pByteIndex, bytesDecoded, 0);
    RXinfo.lengthField = bytesDecoded[0];
    RXinfo.length = byteSize((packetSize(RXinfo.lengthField)));

    // - Length mode -
    // Set fixed packet length mode is less than 256 bytes
    if (RXinfo.length < (MAX_FIXED_LENGTH))
    {
      cc1101_writeReg(CC1101_PKTLEN, (uint8_t)(RXinfo.length));
      cc1101_writeReg(CC1101_PKTCTRL0, FIXED_PACKET_LENGTH);
      RXinfo.format = FIXED;
    }

    // Infinite packet length mode is more than 255 bytes
    // Calculate the PKTLEN value
    else
    {
      fixedLength = RXinfo.length % (MAX_FIXED_LENGTH);
      cc1101_writeReg(CC1101_PKTLEN, (uint8_t)(fixedLength));
    }

    RXinfo.pByteIndex += 3;
    RXinfo.bytesLeft = RXinfo.length - 3;

    // Set RX FIFO threshold to 32 bytes
    RXinfo.start = false;
    cc1101_writeReg(CC1101_FIFOTHR, RX_FIFO_THRESHOLD);
  }

  // - RX FIFO Half Full detected -
  // Read out the RX FIFO and set fixed mode if less
  // than 255 bytes to receive
  else
  {
    // - Length mode -
    // Set fixed packet length mode is less than 256 bytes
    if (((RXinfo.bytesLeft) < (MAX_FIXED_LENGTH)) && (RXinfo.format == INFINITE))
    {
      cc1101_writeReg(CC1101_PKTCTRL0, FIXED_PACKET_LENGTH);
      RXinfo.format = FIXED;
    }

    // Read out the RX FIFO
    // Do not empty the FIFO (See the CC110x or 2500 Errata Note)
    cc1101_readBurstReg(RXinfo.pByteIndex, CC1101_RXFIFO, RX_AVAILABLE_FIFO - 1);

    RXinfo.bytesLeft -= (RX_AVAILABLE_FIFO - 1);
    RXinfo.pByteIndex += (RX_AVAILABLE_FIFO - 1);
  }
}
/********************************************************************************
brief:  This function is called when the complete packet has been received.
The remaining bytes in the RX FIFO are read out, and packet complete signalized
parameter:
********************************************************************************/
void rxPacketRecvdISR(void)
{

  // Read remaining bytes in RX FIFO
  cc1101_readBurstReg(RXinfo.pByteIndex, CC1101_RXFIFO, (uint8_t)RXinfo.bytesLeft);
  RXinfo.complete = true;
}

uint16_t startReceiving(uint8_t *packet, uint8_t *bytes)
{
  uint16_t rxStatus;

  // Initialize RX info variable
  RXinfo.lengthField = 0;    // Length Field in the wireless MBUS packet
  RXinfo.length = 0;         // Total length of bytes to receive packet
  RXinfo.bytesLeft = 0;      // Bytes left to to be read from the RX FIFO
  RXinfo.pByteIndex = bytes; // Pointer to current position in the byte array
  RXinfo.format = INFINITE;  // Infinite or fixed packet mode
  RXinfo.start = true;       // Sync or End of Packet
  RXinfo.complete = false;   // Packet Received

  // Set RX FIFO threshold to 4 bytes
  cc1101_writeReg(CC1101_FIFOTHR, RX_FIFO_START_THRESHOLD);

  // Set infinite length
  cc1101_writeReg(CC1101_PKTCTRL0, INFINITE_PACKET_LENGTH);

  // Check RX Status
  rxStatus = cc1101_readReg(CC1101_SNOP, READ_SINGLE);
  if ((rxStatus & 0x70) != 0)
  {
    // Abort if not in IDLE
    cc1101_cmdStrobe(CC1101_SIDLE); // Enter IDLE state
    return (RX_STATE_ERROR);
  }

  // Flush RX FIFO
  // Ensure that FIFO is empty before reception is started
  cc1101_cmdStrobe(CC1101_SFRX); // flush receive queue

  attachInterrupt(CC1101_GDO0, rxFifoISR, RISING);
  attachInterrupt(CC1101_GDO2, rxPacketRecvdISR, FALLING);

  // Strobe RX
  cc1101_cmdStrobe(CC1101_SRX); // Enter RX state

  // Wait for FIFO being filled
  // while (RXinfo.complete != true)
  //{
  // delay(1);
  //}

  return (PACKET_OK);
}

uint16_t stopReceiving(uint8_t *packet, uint8_t *bytes)
{

  uint16_t rxStatus;

  detachInterrupt(CC1101_GDO0);
  detachInterrupt(CC1101_GDO2);

  // Check that transceiver is in IDLE
  rxStatus = cc1101_readReg(CC1101_SNOP, READ_SINGLE);
  if ((rxStatus & 0x70) != 0)
  {
    cc1101_cmdStrobe(CC1101_SIDLE); // Enter IDLE state
    return (RX_STATE_ERROR);
  }

  rxStatus = decodeRXBytesTmode(bytes, packet, packetSize(RXinfo.lengthField));

  return (rxStatus);
}

/**
 * @brief  Extract a 32 bit unsigned integer encoded into a stream of data in
 *         little-endian format.
 * @param  uint8_t *data Location of where to extract the int from.
 * @param  int offset Offset from the start of the location.
 * @retval uint32_t
 */
uint32_t read_uint32_le(uint8_t *data, int offset)
{
  uint32_t result = *(data + offset + 3) << 24;
  result |= *(data + offset + 2) << 16;
  result |= *(data + offset + 1) << 8;
  result |= *(data + offset);
  return result;
}
/********************************************************************************
brief: this function the DIEHL packet from WMBUS
parameter:
********************************************************************************/
void decodeDiehlWaterMeters()
{
  uint8_t AES_in[16];
  uint32_t result;

  struct AES_ctx ctx;

  // The right packet size
  if (RXpacket[0] == 0x1E)
  {
    // manufacturer
    if (RXpacket[2] == 0xA5 && RXpacket[3] == 0x11)
    {


      // You can use int instead of char array
      uint32_t device_id = (RXpacket[7] << 24) | (RXpacket[6] << 16) | (RXpacket[5] << 8) | RXpacket[4];



      // parse IV AES
      // 2bytes(fab) + 4bytes(meter-id) + 1bytes(version) + 1bytes(types)
      //
      for (int i = 0; i < 8; i++)
      {
        aes_iv[i] = RXpacket[i + 2];
      }
      for (int i = 8; i < 16; i++)
      {
        aes_iv[i] = RXpacket[13];
      }
      for (int i = 0; i < 11; i++)
      {
        AES_in[i] = RXpacket[i + 17];
      }
      for (int i = 0; i < 5; i++)
      {
        AES_in[i + 11] = RXpacket[i + 30];
      }

      AES_init_ctx_iv(&ctx, aes_default_key, aes_iv);
      AES_CBC_decrypt_buffer(&ctx, AES_in, 16);


      result = read_uint32_le(AES_in, 4);

      /*
      to get "lv_label_set_text_fmt" working
      don't use  * - LV_STDLIB_BUILTIN in lv_conf.h
      use instead LV_STDLIB_CLIB (pico sdk printf)
      */

      lv_label_set_text_fmt(objects.meter_id, "%x", device_id);
      lv_obj_set_style_text_align(objects.meter_conso, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);

      lv_label_set_text_fmt(objects.meter_conso, "%ld", result);
      lv_obj_set_style_text_align(objects.meter_conso, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);

    }
  }
}

int main()
{
  stdio_init_all();
  start_SPI();
  cc1101_initRegisters();
  ili9488_Init(LCD_INV_LANDSCAPE);
  lvgl_init();
  lv_obj_t *screen;
  screen = lv_obj_create(NULL);
  lv_screen_load(screen);
  ui_init();
  while (true)
  {

    if (!receiving)
    {
      // Await packet received
      uint16_t status = startReceiving(RXpacket, RXbytes);
      if (status == PACKET_OK)
      {
        receiving = true;
      }
    }
    else if (RXinfo.complete)
    {

      receiving = false;

      uint16_t status = stopReceiving(RXpacket, RXbytes);

      if (status == PACKET_OK)
      {
        // Send the received Wireless MBUS packet to the UART
        // for (int i = 0; i < packetSize(RXpacket[0]); i++)
        //{
        //  if (RXpacket[i] < 16)
        //  {
        //    printf("0");
        //  }
        //  printf("%x\n", RXpacket[i]);
        //}
        decodeDiehlWaterMeters();
      }
    }
    lv_timer_handler();
    sleep_ms(5);
  }
}
