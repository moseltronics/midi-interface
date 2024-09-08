/*

ToDo
----
- pio-usb device ?
- multicore ?


 * Copyright (c) 2022 rppicomidi

 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 * 
 */
#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "hardware/clocks.h"
#include "midi_uart_lib.h"
#include "bsp/board_api.h"
#include "pio_usb.h"
#include "usbd_descriptors.c"
#include "tusb.h"
#include "usb_midi_host.h"
#include "MIDI-defs.c"
//
#ifndef MIDI_UART_NUM
#define MIDI_UART_NUM 1
#endif
#define MIDI_UART_NUM2 0
#ifndef MIDI_UART_TX_GPIO
#define MIDI_UART_TX_GPIO 4
#endif
#ifndef MIDI_UART_RX_GPIO
#define MIDI_UART_RX_GPIO 5
#endif
#define MIDI_UART_RX_GPIO2 1
#define midi_uart_A 0x0F        //virtual Cable_Num in
#define midi_uart_B 0x0E        //virtual Cable_Num in
#define midi_uart_C 0x0D        //virtual Cable_Num in
#define usbd2_cable_offset 3
#define usbd3_cable_offset 6
#define usbd4_cable_offset 8
#define usbh_cable_offset 10

#define rx_size 64
#define ry_size 96
#define tx_size 64
#define sysex_streambuf_size 16

#define PIO_USB1_DP_PIN 16      // USB 1/2
#define PIO_USB2_DP_PIN 26      // USB 1/2
#define PIO_USB3_DP_PIN 2       // USB 1/2/3
#define PIO_USB4_DP_PIN 6       // USB 1/2/3/4

int64_t delay_val = 1;          // no Wait state
uint8_t mode = 0x00;            // everything to U1
uint8_t m1_mode = 0x01;         //->M, ->C, ->U1
uint8_t m2_mode = 0x01;         //->M, ->C, ->U1
uint8_t c_mode = 0x1F;          // send to all
uint8_t u1_mode = 0x1E;         //->M, ->C, ->U2
uint8_t u2_mode = 0x01;         //->M, ->C, ->U1
uint8_t u3_mode = 0x01;         //->M, ->C, ->U1
uint8_t u4_mode = 0x01;         //->M, ->C, ->U1
uint8_t x_mode = 0x1F;          //->M, ->C, ->U1, ->U2

int8_t mode_index1 = 0;         // adjust U settings
int8_t mode_index2 = 0;         // adjust M/C settings

#include "WDC2704M_lib.c"

static void *midi_uart_instance;
static void *midi_uart_instance2;
static uint8_t midi_dev_addr1 = 0;   //USB
static uint8_t midi_dev_addr2 = 0;   //USB
static uint8_t midi_dev_addr3 = 0;   //USB
static uint8_t midi_dev_addr4 = 0;   //USB
bool UD1_connected = 0;
bool UD2_connected = 0;
bool UD3_connected = 0;
bool UD4_connected = 0;
bool UH_connected = 0;

typedef struct
{
    uint8_t buffer[3];
    uint8_t index;
    uint8_t remaining;
}midi_stream_info;

midi_stream_info midi_inf[16];

typedef struct
{
    uint8_t buffer[sysex_streambuf_size];
    uint8_t index;
}sysex_stream_info;

sysex_stream_info sysex_inf[2];

uint16_t m_sysblock = 0;

uint8_t last_status_out = 0;

uint8_t set_tab1[] = {0,0x32,1,0x33,2,0x34,3,0x41,4,0x42, 5,0x31,6,0x33,7,0x34,8,0x41,9,0x42, 10,0x31,11,0x32,12,0x34,13,0x41,14,0x42, 15,0x31,16,0x32,17,0x33,18,0x41,19,0x42};
uint8_t set_tab2[] = {0,0x31,1,0x32,2,0x33,3,0x34,4,0x41,5,0x42, 7,0x31,8,0x32,9,0x33,10,0x34,11,0x41,12,0x42, 14,0x31,15,0x32,16,0x33,17,0x34,18,0x4D};

void mod2info(void) {

  uint8_t cm = c_mode & 0xC0;

  strcpy(d_text,"                    ");

  if ((cm < 0x80) && !(mode & 0x04))      // routings to U1-U4
  {
    if (UD1_connected) strncpy(d_text+1,"U 1",3);
    if (UD2_connected) strncpy(d_text+6,"U 2",3);
    if (UD3_connected) strncpy(d_text+11,"U 3",3);
    if (UD4_connected) strncpy(d_text+16,"U 4",3);

    d_text[0] = 0x3C;       // [
    d_text[4] = 0x3E;       // ]
    d_text[5] = 0x3C;       // [
    d_text[9] = 0x3E;       // ]
    d_text[10] = 0x3C;       // [
    d_text[14] = 0x3E;       // ]
    d_text[15] = 0x3C;       // [
    d_text[19] = 0x3E;       // ]
 
    write_line(3);

    strcpy(d_text,"                    ");

// U1
    if (u2_mode & 0x01) d_text[0] = 0x32;           //2
    if (u3_mode & 0x01) d_text[1] = 0x33;           //3
    if (u4_mode & 0x01) d_text[2] = 0x34;           //4
    if (m1_mode & 0x01) d_text[3] = 0x41;           //A
    if (m2_mode & 0x01) d_text[4] = 0x42;           //B
// U2
    if (u1_mode & 0x02) d_text[5] = 0x31;           //1
    if (u3_mode & 0x02) d_text[6] = 0x33;           //3
    if (u4_mode & 0x02) d_text[7] = 0x34;           //4
    if (m1_mode & 0x02) d_text[8] = 0x41;           //A
    if (m2_mode & 0x02) d_text[9] = 0x42;           //B
// U3
    if (u1_mode & 0x04) d_text[10] = 0x31;           //1
    if (u2_mode & 0x04) d_text[11] = 0x32;           //2
    if (u4_mode & 0x04) d_text[12] = 0x34;           //4
    if (m1_mode & 0x04) d_text[13] = 0x41;           //A
    if (m2_mode & 0x04) d_text[14] = 0x42;           //B
// U4
    if (u1_mode & 0x08) d_text[15] = 0x31;           //1
    if (u2_mode & 0x08) d_text[16] = 0x32;           //2
    if (u3_mode & 0x08) d_text[17] = 0x33;           //3
    if (m1_mode & 0x08) d_text[18] = 0x41;           //A
    if (m2_mode & 0x08) d_text[19] = 0x42;           //B
    write_line(2);

    if (cm == 0x40)
    {
      uint8_t ch_pos = set_tab1[mode_index1];
      d_text[0] = set_tab1[mode_index1+1];
      write_pos(2, ch_pos, 1);
      display_adr(2, ch_pos);   // keep cursor in position
    }
  }
  else if ((cm == 0x80) || (mode & 0x04))      // routings to C / M
  {
    d_text[2] = 0x4D;                               //M
    d_text[9] = 0x43;                               //C

    if (c_mode & 0x01) d_text[14] = 0x31;           //1
    if (c_mode & 0x02) d_text[15] = 0x32;           //2
    if (c_mode & 0x04) d_text[16] = 0x33;           //3
    if (c_mode & 0x08) d_text[17] = 0x34;           //4
    if (c_mode & 0x10) d_text[18] = 0x4D;           //M

    d_text[19] = 0x20;                              //_

    write_line(3);

    strcpy(d_text,"                    ");

    if (u1_mode & 0x10) d_text[0] = 0x31;           //1
    if (u2_mode & 0x10) d_text[1] = 0x32;           //2
    if (u3_mode & 0x10) d_text[2] = 0x33;           //3
    if (u4_mode & 0x10) d_text[3] = 0x34;           //4
    if (m1_mode & 0x10) d_text[4] = 0x41;           //A
    if (m2_mode & 0x10) d_text[5] = 0x42;           //B

    d_text[6] = 0x20;                               //_

    if (u1_mode & 0x20) d_text[7] = 0x31;           //1
    if (u2_mode & 0x20) d_text[8] = 0x32;           //2
    if (u3_mode & 0x20) d_text[9] = 0x33;           //3
    if (u4_mode & 0x20) d_text[10] = 0x34;          //4
    if (m1_mode & 0x20) d_text[11] = 0x41;          //A
    if (m2_mode & 0x20) d_text[12] = 0x42;          //B

    strncpy(d_text+13,"   C   ",7);
    write_line(2);

    if (!(mode & 0x04))
    {
      uint8_t ch_pos = set_tab2[mode_index2];
      d_text[0] = set_tab2[mode_index2+1];

      uint8_t ln = 2;
      if (mode_index2 >= 24) ln = 3;
      write_pos(ln, ch_pos, 1);
      display_adr(ln, ch_pos);   // keep cursor in position
    }
  }
}

static void init_display(void)
{
    sleep_ms(50);
    gpio_put (rs, false);         //instruction mode
    write_byte(0x30);             //function_set: 8 bit interface
    sleep_us(200);
    write_byte(0x38);             //function_set
    sleep_us(100);

    write_byte(0x0C);             //Display on, Cursor + Blink off
    sleep_us(200);

    write_byte(0x01);             //clear_display
    sleep_ms(20);

    write_byte(0x06);             //cursor right, by 1
    sleep_us(100);

    mod2info();
}

uint8_t midi_filter (uint8_t midi_nr, uint8_t const* buffer_in, uint8_t *buffer_out, uint32_t i_bufsize, uint32_t o_bufsize)
{
  midi_nr &= 0x0F;
  midi_stream_info *midibuf = &midi_inf[midi_nr];
  uint32_t i = 0;
  uint32_t j = 0;
  uint32_t k;

  while ((i < i_bufsize) && ((j + sysex_streambuf_size) < o_bufsize))
  {
    uint8_t data = buffer_in[i++];

    if (data >= MIDI_STATUS_SYSREAL_TIMING_CLOCK)                // real-time messages
    {
      if (!(mode & 0x10))
      {
        if (data == MIDI_STATUS_SYSREAL_TIMING_CLOCK) continue;
        else if (data == MIDI_STATUS_SYSREAL_ACTIVE_SENSING) continue;
      }
      buffer_out[j++] = data;
    }
//#######################################
    else if ((m_sysblock & 0xFFF0) && ((m_sysblock & 0x00F) != midi_nr)) m_sysblock -= 0x10;   //do nothing, blocking sysex from different cable
//#######################################
    else if (midibuf->buffer[0] == MIDI_STATUS_SYSEX_START)       // still in a SysEx transmit
    {
      k = midibuf->index;
      sysex_stream_info *sysexbuf = &sysex_inf[k];
      k = sysexbuf->index;
      midibuf->remaining = 0;

      if (data < 0x80)
      {
        if (k == sysex_streambuf_size)
        {
          m_sysblock = 0xFF0 + midi_nr;

          for (uint8_t idx = 0; idx < k; idx++) buffer_out[j++] = sysexbuf->buffer[idx];//send buffered data

          sysexbuf->buffer[0] = 0;                            //free sysexbuf

          last_status_out = midibuf->buffer[0];

          k = 0;
        }

        sysexbuf->buffer[k++] = data;
        sysexbuf->index = k;
      }
      else if (data == MIDI_STATUS_SYSEX_END)
      {
        for (uint8_t idx = 0; idx < k; idx++) buffer_out[j++] = sysexbuf->buffer[idx];//send buffered data
        buffer_out[j++] = data;                             //send end byte

        last_status_out = midibuf->buffer[0];
        midibuf->buffer[0] = 0;
        sysexbuf->buffer[0] = 0;                            //free sysexbuf
        m_sysblock = 0;
      }
      else                                                    //new status message
      {
        midibuf->buffer[0] = 0;
        sysexbuf->buffer[0] = 0;                            //free sysexbuf
        m_sysblock = 0;
        i--;                                                //reload this byte next time
      }
    }
    else if (midibuf->remaining)                              // rest of a message
    {
      k = midibuf->index;

      if (data < 0x80)
      {
        uint8_t status = midibuf->buffer[0];
        midibuf->buffer[k++] = data;
        --(midibuf->remaining);

        if (!midibuf->remaining)
        {
          uint8_t idx = 0;

          if ((status & 0xF0) == 0x80)                        // note off ?
          {
            status |= 0x10;                                   // note on !
            midibuf->buffer[0] = status;
            midibuf->buffer[2] = 0;                           // velocity = 0
          }

          if (status == last_status_out) idx = 1;             //Running Status
          else last_status_out = status;

          for (; idx < k; idx++) buffer_out[j++] = midibuf->buffer[idx];//send buffered data
        }
        else midibuf->index = k;
      }
      else                                                    //new status message
      {                                                       //cancel former message
        midibuf->remaining = 0;                             
        //last_status_out = 0;
        //last_status[midi_nr] = 0;
        i--;                                                  //reload this byte next time
      }
    }
    else if (data < 0x80)                                     // new event message
    {
      uint8_t status = midibuf->buffer[0];
      k = 0;

      if ((status >= 0x80) && (status < 0xF0))                //Running Status ?
      {
        k = 1;

        midibuf->buffer[k++] = data;

        if (status >= 0xC0 && status < 0xE0)                  // Channel Voice two-byte variants
        {
          uint8_t idx = 0;

          if (status == last_status_out) idx = 1;
          else last_status_out = status;

          for (; idx < k; idx++) buffer_out[j++] = midibuf->buffer[idx];//send buffered data

        }
        else                                                  // Channel Voice Messages (3 bytes)
        {
          midibuf->remaining = 1;
          midibuf->index = k;
        }
      }
      else if (last_status[midi_nr] < 0xF0)                   //Running Status after cable change ?
      {       // #+#+#+#+#+ ?????????? #+#+#+#+#+
        midibuf->buffer[0] = last_status[midi_nr];
        i--;                                                //reload this byte next time
      }
    }
    else if (data < 0xF0)                                     // Channel Voice Messages
    {
      last_status[midi_nr] = data;
      midibuf->buffer[0] = data;
      midibuf->index = 1;
      if (data >= 0xC0 && data < 0xE0) midibuf->remaining = 1;// Channel Voice (two bytes)
      else midibuf->remaining = 2;                            // Channel (3 bytes)
    }
    else if ( data >= 0xF0 )                                    // System message
    {
      k = 0;
      last_status[midi_nr] = data;
      midibuf->buffer[0] = data;
      midibuf->index = 1;

      if (data == MIDI_STATUS_SYSEX_START)
      {
        k = 0;
        sysex_stream_info *sysexbuf = &sysex_inf[k];

        if (sysexbuf->buffer[k])                                // in use ?
        {
          k += 1;
          sysexbuf = &sysex_inf[k];
        }

        midibuf->index = k;
        sysexbuf->buffer[0] = data;
        sysexbuf->index = 1;
      }
      else if (data == MIDI_STATUS_SYSCOM_TIME_CODE_QUARTER_FRAME || data == MIDI_STATUS_SYSCOM_SONG_SELECT)
      {
        midibuf->remaining = 1;
      }
      else if ( data == MIDI_STATUS_SYSCOM_SONG_POSITION_POINTER )
      {
        midibuf->remaining = 2;
      }
      else                                                    //for example, MIDI_STATUS_SYSCOM_TUNE_REQUEST
      {
        buffer_out[j++] = data;                             //send data
        last_status_out = midibuf->buffer[0];
      }
    }
  }
  return j;
}

void insert_midi_data(uint8_t num)
{
  if (x_mode & 0x01)
  {
    if (UD1_connected && tuh_midih_get_num_tx_cables(midi_dev_addr1) >= 1)
    {
      tuh_midi_stream_write(midi_dev_addr1, midi_uart_C, stm, num, mode);
    }
  }

  if (x_mode & 0x2)
  {
    if (UD2_connected && tuh_midih_get_num_tx_cables(midi_dev_addr1) >= 1)
    {
      tuh_midi_stream_write(midi_dev_addr2, midi_uart_C, stm, num, mode);
    }
  }

  if (x_mode & 0x4)
  {
    if (UD3_connected && tuh_midih_get_num_tx_cables(midi_dev_addr3) >= 1)
    {
      tuh_midi_stream_write(midi_dev_addr3, midi_uart_C, stm, num, mode);
    }
  }

  if (x_mode & 0x8)
  {
    if (UD4_connected && tuh_midih_get_num_tx_cables(midi_dev_addr4) >= 1)
    {
      tuh_midi_stream_write(midi_dev_addr4, midi_uart_C, stm, num, mode);
    }
  }

  if (x_mode & 0x10)
  {
    uint8_t buffer[32];
    uint8_t nparsed = midi_filter (midi_uart_C, stm, buffer, num, 32);
    midi_uart_write_tx_buffer(midi_uart_instance, buffer, nparsed);
  }

  if ((x_mode & 0x20) && (UH_connected))        
  {
    tud_midi_stream_write (midi_uart_C, stm, num, mode);
  }
}

void all_sounds_off() {
    m_sysblock = 0;
    stm[0] = 0xB0 + channel; // #+#+#+#+ ???
    stm[1] = 0x78;  // all sounds off
    stm[2] = 0;
    insert_midi_data(3);
}

void all_notes_off() {
    m_sysblock = 0;
    stm[0] = 0xB0 + channel;
    stm[1] = 0x7B;  // all sounds off
    stm[2] = 0;
    insert_midi_data(3);

    sleep_ms(50);
}

void GM_reset() {
    m_sysblock = 0;

    stm[0] = 0xF0;
    stm[1] = 0x7E;
    stm[2] = 0x7F;
    stm[3] = 0x09;
    stm[4] = 0x01;
    stm[5] = 0xF7;

    insert_midi_data(6);

    sleep_ms(50);
}

void mod_change() {

    uint8_t cm = c_mode & 0xC0;

    if (!cm)                    // standard mode
    {
      x_mode = m1_mode | m2_mode | u1_mode | u2_mode | u3_mode | u4_mode;

      uint8_t m = mode & 0x07;
      if (swcheck == swm_2) m+=1;
      else if (swcheck == swm_1) m -= 1;
      m &= 0x07;
      mode &= 0xF8;
      mode |= m;

      if (m == 0)         // XXX <> U1
      {                   // c > XXX
        u1_mode = 0x1E;
        u2_mode = 0x01;
        u3_mode = 0x01;
        u4_mode = 0x01;
        m1_mode = 0x01;
        m2_mode = 0x01;
        c_mode |= 0x1F;
      }
      else if (m == 1)    // XXX <> U2
      {                   // c > XXX
        u1_mode = 0x02;
        u2_mode = 0x1D;
        u3_mode = 0x02;
        u4_mode = 0x02;
        m1_mode = 0x02;
        m2_mode = 0x02;
        c_mode |= 0x1F;
      }
      else if (m == 2)    // XXX <> U3
      {                   // c > XXX
        u1_mode = 0x04;
        u2_mode = 0x04;
        u3_mode = 0x1B;
        u4_mode = 0x04;
        m1_mode = 0x04;
        m2_mode = 0x04;
        c_mode |= 0x1F;
      }
      else if (m == 3)    // XXX <> U4
      {                   // c-> XXX
        u1_mode = 0x08;
        u2_mode = 0x08;
        u3_mode = 0x08;
        u4_mode = 0x17;
        m1_mode = 0x08;
        m2_mode = 0x08;
        c_mode |= 0x1F;
      }
      else if (m == 4)    // XXX <> C+U1+U4
      {
        u1_mode = 0x28;
        u2_mode = 0x29;
        u3_mode = 0x29;
        u4_mode = 0x21;
        m1_mode = 0x29;
        m2_mode = 0x29;
        c_mode |= 0x1F;
      }
      else if (m == 5)    // XXX <> M(A)
      {
        u1_mode = 0x10;
        u2_mode = 0x10;
        u3_mode = 0x10;
        u4_mode = 0x10;
        m1_mode = 0x1F;
        m2_mode = 0x10;
        c_mode |= 0x1F;
      }
      else if (m == 6)    // U1-4 <> M (A+B)
      {
        u1_mode = 0x10;
        u2_mode = 0x10;
        u3_mode = 0x10;
        u4_mode = 0x10;
        m1_mode = 0x0F;
        m2_mode = 0x0F;
        c_mode |= 0x1F;
      }
      else if (m == 7)    // XXX <> C+M+U2
      {
        u1_mode = 0x32;
        u2_mode = 0x30;
        u3_mode = 0x32;
        u4_mode = 0x32;
        m1_mode = 0x32;
        m2_mode = 0x32;
        c_mode |= 0x1F;
      }

      if (swcheck != swm_5) all_sounds_off();
    }
    else if (cm == 0x40)                    // U detail mode
    {
      if (swcheck == swm_2) mode_index1+=2;
      else if (swcheck == swm_1) mode_index1 -= 2;

      if (mode_index1 > 38) mode_index1 = 0;
      else if (mode_index1 < 0) mode_index1 = 38;

      if (swcheck == swm_5)
      {
        x_mode = m1_mode | m2_mode | u1_mode | u2_mode | u3_mode | u4_mode;
        x_mode &= 0x0F;             // only usb connections

        if (mode_index1 < 10)         // -> U1
        {
          if (mode_index1 == 0) u2_mode = u2_mode ^ 0x01;
          else if (mode_index1 == 2) u3_mode = u3_mode ^ 0x01;
          else if (mode_index1 == 4) u4_mode = u4_mode ^ 0x01;
          else if (mode_index1 == 6) m1_mode = m1_mode ^ 0x01;
          else if (mode_index1 == 8) m2_mode = m2_mode ^ 0x01;
        }
        else if (mode_index1 < 20) // -> U2
        {
          if (mode_index1 == 10) u1_mode = u1_mode ^ 0x02;
          else if (mode_index1 == 12) u3_mode = u3_mode ^ 0x02;
          else if (mode_index1 == 14) u4_mode = u4_mode ^ 0x02;
          else if (mode_index1 == 16) m1_mode = m1_mode ^ 0x02;
          else if (mode_index1 == 18) m2_mode = m2_mode ^ 0x02;
        }
        else if (mode_index1 < 30) // -> U3
        {
          if (mode_index1 == 20) u1_mode = u1_mode ^ 0x04;
          else if (mode_index1 == 22) u2_mode = u2_mode ^ 0x04;
          else if (mode_index1 == 24) u4_mode = u4_mode ^ 0x04;
          else if (mode_index1 == 26) m1_mode = m1_mode ^ 0x04;
          else if (mode_index1 == 28) m2_mode = m2_mode ^ 0x04;
        }
        else                        // -> U4
        {
          if (mode_index1 == 30) u1_mode = u1_mode ^ 0x08;
          else if (mode_index1 == 32) u2_mode = u2_mode ^ 0x08;
          else if (mode_index1 == 34) u3_mode = u3_mode ^ 0x08;
          else if (mode_index1 == 36) m1_mode = m1_mode ^ 0x08;
          else if (mode_index1 == 38) m2_mode = m2_mode ^ 0x08;
        }

        mode_index1+=2;
        if (mode_index1 > 38) mode_index1 = 0;

        all_sounds_off();
      }
    }
    else if (cm == 0x80)          // M / C detail mode
    {
      if (swcheck == swm_2) mode_index2+=2;
      else if (swcheck == swm_1) mode_index2 -= 2;

      if (mode_index2 > 32) mode_index2 = 0;
      else if (mode_index2 < 0) mode_index2 = 32;

      if (swcheck == swm_5)
      {
        x_mode = m1_mode | m2_mode | u1_mode | u2_mode | u3_mode | u4_mode;

        if (mode_index2 < 12)         // -> M
        {
          x_mode &= 0x10;             // only M

          if (mode_index2 == 0) u1_mode = u1_mode ^ 0x10;
          else if (mode_index2 == 2) u2_mode = u2_mode ^ 0x10;
          else if (mode_index2 == 4) u3_mode = u3_mode ^ 0x10;
          else if (mode_index2 == 6) u4_mode = u4_mode ^ 0x10;
          else if (mode_index2 == 8) m1_mode = m1_mode ^ 0x10;
          else if (mode_index2 == 10) m2_mode = m2_mode ^ 0x10;
        }
        else if (mode_index2 < 24) // -> C
        {
          x_mode &= 0x20;             // only C

          if (mode_index2 == 12) u1_mode = u1_mode ^ 0x20;
          else if (mode_index2 == 14) u2_mode = u2_mode ^ 0x20;
          else if (mode_index2 == 16) u3_mode = u3_mode ^ 0x20;
          else if (mode_index2 == 18) u4_mode = u4_mode ^ 0x20;
          else if (mode_index2 == 20) m1_mode = m1_mode ^ 0x20;
          else if (mode_index2 == 22) m2_mode = m2_mode ^ 0x20;
        }
        else                      // <- C
        {
          x_mode = c_mode;
          x_mode &= 0x1F;             // not C

          if (mode_index2 == 24) c_mode = c_mode ^ 0x01;
          else if (mode_index2 == 26) c_mode = c_mode ^ 0x02;
          else if (mode_index2 == 28) c_mode = c_mode ^ 0x04;
          else if (mode_index2 == 30) c_mode = c_mode ^ 0x08;
          else if (mode_index2 == 32) c_mode = c_mode ^ 0x10;
        }

        all_sounds_off();

        x_mode = m1_mode | m2_mode | u1_mode | u2_mode | u3_mode | u4_mode | c_mode;

        mode_index2+=2;
        if (mode_index2 > 32) mode_index2 = 0;
      }
    }

    mod2info();

    sleep_ms(2);

    delay_val = delay_time;
}

void hex2dez(uint16_t input) {

    uint16_t dez_x = input / 1000;
    input -= (dez_x * 1000);
    uint8_t ccc = (dez_x & 0x0F);
    if (ccc > 0x09) ccc += 0x37;
    else ccc += 0x30;
    d_text [1] = ccc;

    dez_x = input / 100;
    input -= (dez_x * 100);
    ccc = (dez_x & 0x0F);
    if (ccc > 0x09) ccc += 0x37;
    else ccc += 0x30;
    d_text [2] = ccc;

    dez_x = input / 10;
    input -= (dez_x * 10);
    ccc = (dez_x & 0x0F);
    if (ccc > 0x09) ccc += 0x37;
    else ccc += 0x30;
    d_text [3] = ccc;

    ccc = (input & 0x0F);
    if (ccc > 0x09) ccc += 0x37;
    else ccc += 0x30;
    d_text [4] = ccc;
}

void byte2char(uint8_t input) {
    uint8_t ccc = (input >> 4);
    if (ccc > 0x09) ccc += 0x37;
    else ccc += 0x30;
    d_text [0] = ccc;
    ccc = (input & 0x0F);
    if (ccc > 0x09) ccc += 0x37;
    else ccc += 0x30;
    d_text [1] = ccc;
    d_text [2] = 0;
}

void bytes2char(uint8_t input1, uint8_t input2) {
    uint8_t ccc = (input1 >> 4);
    if (ccc > 0x09) ccc += 0x37;
    else ccc += 0x30;
    d_text [0] = ccc;
    ccc = (input1 & 0x0F);
    if (ccc > 0x09) ccc += 0x37;
    else ccc += 0x30;
    d_text [1] = ccc;

    ccc = (input2 >> 4);
    if (ccc > 0x09) ccc += 0x37;
    else ccc += 0x30;
    d_text [2] = ccc;
    ccc = (input2 & 0x0F);
    if (ccc > 0x09) ccc += 0x37;
    else ccc += 0x30;
    d_text [3] = ccc;
    d_text [4] = 0;
}

void param_select() {
    uint8_t m = mode & 0xE0;
    uint32_t param;
    uint8_t step = 1;

    if (m == 0x20) {                                       //Channel
        if (swcheck == swm_3) channel += 1;                //+
        else if (swcheck == swm_4) channel -= 1;           //-
        channel &= 0x0F;
        byte2char (channel);
        write_pos(1,cursor,2);
    }
    else if (m == 0x40) {                                   //Voice
        if (swcheck == swm_3) voice += step;                //+
        else if (swcheck == swm_4) voice -= step;           //-
        voice &= 0x7F;
        stm[0] = 0xC0 + channel;
        stm[1] = voice;
        insert_midi_data(2);
        clear_space(1,0,16);
        strcpy(d_text, v_text[voice]);
        strcat(d_text,"     ");
        write_pos(1,cursor,8);
    }
    else if (m == 0x60) {                                  //Bank 
        if (swcheck == swm_3) bank += 1;                   //+
        else if (swcheck == swm_4) bank -= 1;              //-
        bank &= 0x7F;
        stm[0] = 0xB0 + channel;                            // + channel?
        stm[1] = 0x00;                                      //bank select
        stm[2] = 0x00;                                      //MSB
        stm[4] = 0x20;                                      //32 ?
        stm[5] = bank;                                      //LSB
        stm[6] = 0xC0 + channel;
        stm[7] = voice;
        insert_midi_data(8);
        clear_space(1,0,2);
        byte2char (bank);
        write_pos(1,cursor,2);
    }
    else if (m == 0x80) {                                   //Effect
        if ((sw_flag & 0x30) == 0x20) {                     //2. Level
            clear_space(0,0,16);
;
            stm[0] = 0xB0 + channel;
            if (effect < 0x0B) {                            //Reverb
                if (swcheck == swm_3) reverb += step;       //+
                else if (swcheck == swm_4) reverb -= step;  //-
                reverb &= 0x7F;
                stm[1] = 0x5B;                              //Reverb send
                stm[2] = reverb;
                byte2char (reverb);
            }
            else if (effect > 0x12) {                       //Flanger
                if (swcheck == swm_3) flanger += step;      //+
                else if (swcheck == swm_4) flanger -= step; //-
                flanger &= 0x7F;
                stm[1] = 0x5D;                              //Chorus send
                stm[2] = flanger;
                byte2char (flanger);
            }
            else if (effect > 0x0E) {                       //Celeste
                if (swcheck == swm_3) celeste += step;      //+
                else if (swcheck == swm_4) celeste -= step; //-
                celeste &= 0x7F;
                stm[1] = 0x5D;                              //Chorus send
                stm[2] = celeste;
                byte2char (celeste);
            }
            else {                                          //Chorus
                if (swcheck == swm_3) chorus += step;       //+
                else if (swcheck == swm_4) chorus -= step;  //-
                chorus &= 0x7F;
                stm[1] = 0x5D;                              //Chorus send
                stm[2] = chorus;
                byte2char (chorus);
            }

            insert_midi_data(3);
            write_pos(0,cursor,2);
        }
        else {                                              //select Effects
            if (swcheck == swm_3) effect += 1;              //+
            else if (swcheck == swm_4) effect -= 1;         //-
            if (effect > 0x15) effect = 0x15;
            stm[0] = 0xF0;                                  //System Message
            stm[1] = 0x43;
            stm[2] = 0x10;
            stm[3] = 0x4C;
            stm[4] = 0x02;
            stm[5] = 0x01;
            param = (eff_tab[effect] >> 16);
            stm[6] = param;
            param = (eff_tab[effect] >> 8);
            stm[7] = (param & 0x7F);
            param = eff_tab[effect];
            stm[8] = (param & 0x7F);
            stm[9] = 0xF7;
            insert_midi_data(10);
            clear_space(1,0,16);
            strcpy(d_text,eff_text[effect]);
            strcat(d_text,"     ");
            write_pos(1,cursor,9);
        }

        sleep_ms(10);
    }
    else if (m == 0xA0) {                                   //Misc
        if ((sw_flag & 0x30) == 0x20) {                     //2. Level
            clear_space(0,0,16);
            stm[0] = 0xF0;                                  //System Message
            stm[1] = 0x43;
            stm[2] = 0x10;

            if (misc == 0) {                                //Reset

              stm[0] = 0xF0;
              stm[1] = 0x7E;
              stm[2] = 0x7F;
              stm[3] = 0x09;
              stm[4] = 0x01;
              stm[5] = 0xF7;

              insert_midi_data(6);
              //d_text[0] = "*";
              //write_pos(0,cursor,1);
              sleep_ms(300);
              return;
            }
            else if (misc == 0x01) {                                //Transpose
                if (swcheck == swm_3) {                            //+
                    if (transp <= (0x58-step)) transp += step;
                    else transp = 0x58;
                }
                else if (transp >= (step + 0x28)) transp -= step;   //-
                else transp = 0x28;
                //stm[3] = 0x4C;
                //stm[4] = 0x00;
                //stm[5] = 0x00;
                //stm[6] = 0x06;
                //stm[7] = transp;
                //stm[8] = 0xF7;

              stm[0] = 0xF0;
              stm[1] = 0x7F;
              stm[2] = 0x7F;
              stm[3] = 0x04;
              stm[4] = 0x04;
              stm[5] = 0x00;
              stm[6] = transp;
              stm[7] = 0xF7;

                insert_midi_data(8);

                uint16_t transp2;

                if (transp < 0x40) {
                    transp2 = 0x40 - transp;                //show neg. Values pos.
                    d_text[0] = (char) 0x2D;                //"-"
                }
                else {
                    transp2 = transp - 0x40;
                    d_text[0] = (char) 0x2B;                // "+"
                }
                hex2dez(transp2);
                write_pos(0,cursor,5);
            }
            else if (misc == 0x02) {                        //Tune

                if (swcheck == swm_3) {                     //+
                    if (tune_l <= (0xFF-step)) {
                        tune_l += step;
                    }
                    else if (tune_m < 0x07) {
                        tune_l += step;
                        tune_m += 1;
                    }
                    else {
                        tune_l = 0xFF;
                        tune_m = 0x07;
                    }
                }
                else if (tune_l >= step){
                    tune_l -= step;                            //-
                }
                else if (tune_m > 0) {
                    tune_l -= step;                            //-
                    tune_m -= 1;
                }
                else {
                    tune_l = 0;
                    tune_m = 0;
                }

                stm[3] = 0x4C;
                stm[4] = 0x00;
                stm[5] = 0x00;
                stm[6] = 0x00;
                stm[7] = 0x00;                              //W
                stm[8] = tune_m;                            //X
                stm[9] = (tune_l >> 4);                     //Y
                stm[10] = (tune_l & 0x0F);                  //Z
                stm[11] = 0xF7;

                insert_midi_data(12);

                uint16_t tune16 = tune_m << 8;
                tune16 += tune_l;
                if (tune16 < 0x400) {
                    tune16 = 0x400 - tune16;
                    d_text[0] = (char) 0x2D;                //"-"
                }
                else {
                    tune16 -= 0x400;
                    d_text[0] = (char) 0x2B;                // "+"
                }

                hex2dez(tune16);
                write_pos(0,cursor,5);
                sleep_ms(300);
            }
        }
        else {                                              //Select Misc
            if (swcheck == swm_3) {
              misc += 1;               //+
              if (misc > 0x02) misc = 0x04;
            }
            else if (swcheck == swm_4) {
              misc -= 1;          //-
              if (misc > 0x10) misc = 0x02;
            }
            clear_space(1,0,16);
            strcpy(d_text,misc_text[misc]);
            strcat(d_text,"     ");
            write_pos(1,cursor,9);
        }

        sleep_ms(10);
    }

    delay_val = delay_time;
}

void title_text()
{
    strcpy(d_text,"USB    MIDI         ");
    d_text[4] = 0x7F;                             // <
    d_text[5] = 0x7E;                             // >

    if (mode & 0x10) strncpy(d_text+18,"Ck",2);

    if (mode & 0x08) strncpy(d_text+12,"Cable",5);

    write_line(0);
    strcpy(d_text,"HOST INTERFACE   1.0");
    write_line(1);
}

void set_params() {

    if (swcheck == swm_2) mode += 0x20;
    else if (swcheck == swm_1) mode -= 0x20;
    else
    {
      clear_space(2,0,16);           //first call
      mode &= 0x1F;
      mode |= 0x20;
    }

    uint8_t m = mode & 0xE0;

    if (m >= 0xC0) m = 0x20;
    else if (m < 0x20) m = 0xA0;

    mode &= 0x1F;
    mode |= m;

    m = m >> 5;

    clear_space(1,0,16);
    strcpy(d_text, menu_text[m-1]);
    strcat(d_text,"     ");
    write_pos(2,cursor,10);

    delay_val = delay_time;
}

void check_sw() {
    static absolute_time_t pre_time = {0};
    absolute_time_t now_time = get_absolute_time();
    int64_t diff = absolute_time_diff_us(pre_time, now_time);

    if (diff < delay_val) return;               //wait state after key press etc.

    swcheck = ~gpio_get_all();                  //invert first
    swcheck &= switch_in_mask;                  //then mask

    delay_val = 1;                              //reset wait state

    if (swcheck == 0) {
        sw_flag &= ~2;                          //key released
        return;
    }

                    //Taste wurde gedrückt !

    pre_time = now_time;                        //key pressed - restart counting

    delay_val = delay_time;                     //debounce time

    if (sw_flag & 2) {                          //key still pressed
        if (swcheck != sw_old) {                //same key ?
            sw_old = swcheck;
            delay_val = short_delay;            //debounce time
            return;                             //other key pressed
        }
        else delay_val = rep_delay;
    }
    else {
        sw_flag |= 2;                           //first key press
        sw_old = swcheck;
        delay_val = short_delay;                //debounce time
        return;
    }

// here: twice the same value

  switch(swcheck) {
    case (swm_1):                               // left
    case (swm_2):                               // right
      if (!(sw_flag & 0x30)) mod_change();
      else if ((sw_flag & 0x30) < 0x30)         // midi events
      {
        sw_flag &= 0x0F;
        sw_flag |= 0x10;                        // 2. switch off 2. Level
        set_params();
      }
      break;
    case (swm_3):                               // up
      if (!(sw_flag & 0x30))
      {
        sw_flag |= 0x10;                        // midi events
        set_params();
      }
      else if (((sw_flag & 0x30) < 0x30))
      {
        param_select();
      }
      break;
    case (swm_4):                               // down
      if (!(sw_flag & 0x30))                    //
      {
        c_mode += 0x40;

        if (c_mode >= 0xC0)
        {
          c_mode &= 0x3F;
          no_cursor();
        }
        else
        {
          mode &= 0xFB;
          set_blink();
        }

        mod2info();
      }
      else if (((sw_flag & 0x30) < 0x30))
      {
        param_select();
      }
      break;
    case (swm_5):                             // click (center)
      if (!(sw_flag & 0x30))
      {
        mod_change();
      }
      else if ((sw_flag & 0x30) == 0x10)
      {
        sw_flag += 0x10;                      //2. level on
      }
      else if ((sw_flag & 0x30) == 0x30)
      {
        ;// ???
      }
      break;
    case (swm_5 + swm_2):                     // cable mode
      mode = mode ^ 0x08;
      title_text();                           // restore text
      break;
    case (swm_5 + swm_1):                     // click + left: escape
      mode &= 0x1F;                           // base level
      sw_flag &= 0x0F;
      init_display();
      title_text();                           // restore text
      GM_reset();
      break;
    case (swm_5 + swm_3):                     // click + up: ck on/off
      mode = mode ^ 0x10;                     //clock on/off
      title_text();                           // restore text
      break;
    case (swm_5 + swm_4):                     // click + down: back
      if (sw_flag & 0x30) sw_flag -= 0x10;    // one level back
      if (((sw_flag & 0x30) >= 0x10))
      {
        param_select();
      }
      else
      {
        title_text();                         // restore text
        mod2info();
      }
  }
}

static void poll_midi_uart_rx()
{
    uint8_t rx[rx_size];
    uint8_t ry[ry_size];
    uint8_t nread;

///////////////////////////////////////// MIDI A ///////////////////////////////////////////////

    nread = midi_uart_poll_rx_buffer(midi_uart_instance, rx, rx_size);                    //midi_nr = 0x0F

    if (nread > 0)
    {
        if (m1_mode & 0x01)
        {
          if (UD1_connected && tuh_midih_get_num_tx_cables(midi_dev_addr1) >= 1)        
          {
            //&cable_inout: F0:out 0F:in
            //uint8_t cable_inout = midi_uart_A;                                          //0x0F
            uint32_t nwritten = tuh_midi_stream_write(midi_dev_addr1, midi_uart_A, rx, nread, mode);
          }
        }

        if (m1_mode & 0x02)
        {
          if (UD2_connected && tuh_midih_get_num_tx_cables(midi_dev_addr2) >= 1)        
          {
            //&cable_inout: F0:out 0F:in
            //uint8_t cable_inout = midi_uart_A;                                          //0x0F
            uint32_t nwritten = tuh_midi_stream_write(midi_dev_addr2, midi_uart_A, rx, nread, mode);
          }
        }

        if (m1_mode & 0x04)
        {
          if (UD3_connected && tuh_midih_get_num_tx_cables(midi_dev_addr3) >= 1)        
          {
            //&cable_inout: F0:out 0F:in
            //uint8_t cable_inout = midi_uart_A;                                          //0x0F
            uint32_t nwritten = tuh_midi_stream_write(midi_dev_addr3, midi_uart_A, rx, nread, mode);
          }
        }

        if (m1_mode & 0x08)
        {
          if (UD4_connected && tuh_midih_get_num_tx_cables(midi_dev_addr4) >= 1)        
          {
            //&cable_inout: F0:out 0F:in
            //uint8_t cable_inout = midi_uart_A;                                          //0x0F
            uint32_t nwritten = tuh_midi_stream_write(midi_dev_addr4, midi_uart_A, rx, nread, mode);
          }
        }

        if (m1_mode & 0x20)
        {
          if (UH_connected)        
          {
            //&cable_inout: F0:out 0F:in
            //uint8_t cable_inout = midi_uart_A;                                          //0x0F
            //uint32_t nwritten = tud_midi_stream_write (midi_uart_A, rx, nread);
            uint32_t nwritten = tud_midi_stream_write (midi_uart_A, rx, nread, mode);
          }
        }

        if (m1_mode & 0x10)
        {
            uint8_t nparsed = midi_filter (midi_uart_A, rx, ry, nread, ry_size);
            midi_uart_write_tx_buffer(midi_uart_instance, ry, nparsed);
        }
    }

///////////////////////////////////////// MIDI B ///////////////////////////////////////////////

    nread = midi_uart_poll_rx_buffer(midi_uart_instance2, rx, rx_size);                   //midi_nr = 0x0E

    if (nread > 0)
    {
        if (m2_mode & 0x01)
        {
          if (UD1_connected && tuh_midih_get_num_tx_cables(midi_dev_addr1) >= 1)        
          {
            //uint8_t cable_inout = midi_uart_B;                                          //0x0E
            uint32_t nwritten = tuh_midi_stream_write(midi_dev_addr1, midi_uart_B, rx, nread, mode);
          }
        }

        if (m2_mode & 0x02)
        {
          if (UD2_connected && tuh_midih_get_num_tx_cables(midi_dev_addr2) >= 1)        
          {
            //uint8_t cable_inout = midi_uart_B;                                          //0x0E
            uint32_t nwritten = tuh_midi_stream_write(midi_dev_addr2, midi_uart_B, rx, nread, mode);
          }
        }

        if (m2_mode & 0x04)
        {
          if (UD3_connected && tuh_midih_get_num_tx_cables(midi_dev_addr3) >= 1)        
          {
            //uint8_t cable_inout = midi_uart_B;                                          //0x0E
            uint32_t nwritten = tuh_midi_stream_write(midi_dev_addr3, midi_uart_B, rx, nread, mode);
          }
        }

        if (m2_mode & 0x08)
        {
          if (UD4_connected && tuh_midih_get_num_tx_cables(midi_dev_addr4) >= 1)        
          {
            //uint8_t cable_inout = midi_uart_B;                                          //0x0E
            uint32_t nwritten = tuh_midi_stream_write(midi_dev_addr4, midi_uart_B, rx, nread, mode);
          }
        }

        if (m2_mode & 0x20)
        {
          if (UH_connected)        
          {
            //&cable_inout: F0:out 0F:in
            //uint8_t cable_inout = midi_uart_A;                                          //0x0F
            //uint32_t nwritten = tud_midi_stream_write (midi_uart_B, rx, nread);
            uint32_t nwritten = tud_midi_stream_write (midi_uart_B, rx, nread, mode);
          }
        }

        if (m2_mode & 0x10)
        {
            uint8_t nparsed = midi_filter (midi_uart_B, rx, ry, nread, ry_size);
            midi_uart_write_tx_buffer(midi_uart_instance, ry, nparsed);
        }
    }
}

int main() {

  set_sys_clock_khz(120000, true);

  sleep_ms(10);

  //board_init();

  sleep_ms(10);

  init_display_pins();
  init_display();
  init_switches();

  midi_uart_instance = midi_uart_configure(MIDI_UART_NUM, MIDI_UART_TX_GPIO, MIDI_UART_RX_GPIO);
  midi_uart_instance2 = midi_rx_configure(MIDI_UART_NUM2, MIDI_UART_RX_GPIO2);

  // init device stack on configured roothub port
  tud_init(BOARD_TUD_RHPORT);

  // rp2040 use pico-pio-usb for host tuh_configure() can be used to passed pio configuration to the host stack
  // Note: tuh_configure() must be called before tuh_init()
  pio_usb_configuration_t pio_cfg = PIO_USB_DEFAULT_CONFIG;
  pio_cfg.pin_dp = PIO_USB1_DP_PIN;

  tuh_configure(BOARD_TUH_RHPORT, TUH_CFGID_RPI_PIO_USB_CONFIGURATION, &pio_cfg);

  tuh_init(BOARD_TUH_RHPORT);

  pio_usb_host_add_port(PIO_USB2_DP_PIN,PIO_USB_PINOUT_DPDM);
  pio_usb_host_add_port(PIO_USB3_DP_PIN,PIO_USB_PINOUT_DPDM);
  pio_usb_host_add_port(PIO_USB4_DP_PIN,PIO_USB_PINOUT_DPDM);

  if (board_init_after_tusb) {
    board_init_after_tusb();
  }

  title_text();

  while (1) {
    tuh_task(); // tinyusb host task

    tud_task(); // tinyusb device task

    check_sw();

    poll_midi_uart_rx();

    if (UD1_connected) tuh_midi_stream_flush(midi_dev_addr1);
    if (UD2_connected) tuh_midi_stream_flush(midi_dev_addr2);
    if (UD3_connected) tuh_midi_stream_flush(midi_dev_addr3);
    if (UD4_connected) tuh_midi_stream_flush(midi_dev_addr4);

    midi_uart_drain_tx_buffer(midi_uart_instance);
  }
}

//--------------------------------------------------------------------+
// TinyUSB H-Callbacks
//--------------------------------------------------------------------+

// Invoked when device with hid interface is mounted
// Report descriptor is also available for use. tuh_hid_parse_report_descriptor()
// can be used to parse common/simple enough descriptor.
// Note: if report descriptor length > CFG_TUH_ENUMERATION_BUFSIZE, it will be skipped
// therefore report_desc = NULL, desc_len = 0
void tuh_midi_mount_cb(uint8_t dev_addr, uint8_t in_ep, uint8_t out_ep, uint8_t num_cables_rx, uint16_t num_cables_tx)
{
  if (midi_dev_addr1 == 0) {
    sleep_ms(20);

    midi_dev_addr1 = dev_addr;

    UD1_connected = midi_dev_addr1 != 0 && tuh_midi_configured(midi_dev_addr1);

    mod2info();

    sleep_ms(20);
  }
  else if (midi_dev_addr2 == 0) {
    sleep_ms(50);

    midi_dev_addr2 = dev_addr;

    UD2_connected = midi_dev_addr2 != 0 && tuh_midi_configured(midi_dev_addr2);

    mod2info();

    sleep_ms(50);
  }
  else if (midi_dev_addr3 == 0) {
    sleep_ms(50);

    midi_dev_addr3 = dev_addr;

    UD3_connected = midi_dev_addr3 != 0 && tuh_midi_configured(midi_dev_addr3);

    mod2info();

    sleep_ms(50);
  }
  else if (midi_dev_addr4 == 0) {
    sleep_ms(50);

    midi_dev_addr4 = dev_addr;

    UD4_connected = midi_dev_addr4 != 0 && tuh_midi_configured(midi_dev_addr4);

    mod2info();

    sleep_ms(50);
  }
}

// Invoked when device with hid interface is un-mounted
void tuh_midi_umount_cb(uint8_t dev_addr, uint8_t instance)
{
  if (dev_addr == midi_dev_addr1) {
    midi_dev_addr1 = 0;

    UD1_connected = midi_dev_addr1 != 0 && tuh_midi_configured(midi_dev_addr1);

    mod2info();
  }
  else if (dev_addr == midi_dev_addr2) {
    midi_dev_addr2 = 0;

    UD2_connected = midi_dev_addr2 != 0 && tuh_midi_configured(midi_dev_addr2);

    mod2info();
  }
  else if (dev_addr == midi_dev_addr3) {
    midi_dev_addr3 = 0;

    UD3_connected = midi_dev_addr3 != 0 && tuh_midi_configured(midi_dev_addr3);

    mod2info();
  }
  else if (dev_addr == midi_dev_addr4) {
    midi_dev_addr4 = 0;

    UD4_connected = midi_dev_addr4 != 0 && tuh_midi_configured(midi_dev_addr4);

    mod2info();
  }
}

void tuh_midi_rx_cb(uint8_t dev_addr, uint32_t num_packets)
{
  if (num_packets != 0)
  {
    if (midi_dev_addr1 == dev_addr)
    {
      uint8_t cable_num;
      uint8_t buffer[rx_size];
      uint8_t buffer2[ry_size];
      while (1) {

        uint32_t bytes_read = tuh_midi_stream_read(dev_addr, &cable_num, buffer, rx_size);
        if (bytes_read == 0) return;

        if (mode & 0x08) cable_num += (cable_num << 4);   // cable out = cable in

        if (u1_mode & 0x10)
        {
          uint8_t nparsed = midi_filter (cable_num, buffer, buffer2, bytes_read, ry_size);
          uint8_t npushed = midi_uart_write_tx_buffer(midi_uart_instance, buffer2, nparsed);
        }

        if (u1_mode & 0x20)
        {

          if (UH_connected)
          {
            //&cable_inout: F0:out 0F:in
            uint32_t nwritten = tud_midi_stream_write (cable_num, buffer, bytes_read, mode);
          }
        }

        if (u1_mode & 0x02)
        {
          if (UD2_connected && tuh_midih_get_num_tx_cables(midi_dev_addr2) >= 1)        
          {
            //&cable_inout: F0:out 0F:in
            uint32_t nwritten = tuh_midi_stream_write(midi_dev_addr2, cable_num, buffer, bytes_read, mode);
          }
        }

        if (u1_mode & 0x04)
        {
          if (UD3_connected && tuh_midih_get_num_tx_cables(midi_dev_addr3) >= 1)        
          {
            //&cable_inout: F0:out 0F:in
            uint32_t nwritten = tuh_midi_stream_write(midi_dev_addr3, cable_num, buffer, bytes_read, mode);
          }
        }

        if (u1_mode & 0x08)
        {
          if (UD4_connected && tuh_midih_get_num_tx_cables(midi_dev_addr4) >= 1)        
          {
            //&cable_inout: F0:out 0F:in
            uint32_t nwritten = tuh_midi_stream_write(midi_dev_addr4, cable_num, buffer, bytes_read, mode);
          }
        }
      }
    }
    else if (midi_dev_addr2 == dev_addr)
    {
      uint8_t cable_num;
      uint8_t buffer[rx_size];
      uint8_t buffer2[ry_size];
      while (1) {

        uint32_t bytes_read = tuh_midi_stream_read(dev_addr, &cable_num, buffer, rx_size);
        if (bytes_read == 0) return;

        if (mode & 0x08) cable_num += (cable_num << 4);                             // cable out = cable in

        cable_num += usbd2_cable_offset;                                            // to distinguish it from midi_dev_addr1

        if (u2_mode & 0x10)
        {
          uint8_t nparsed = midi_filter (cable_num, buffer, buffer2, bytes_read, ry_size);
          uint8_t npushed = midi_uart_write_tx_buffer(midi_uart_instance, buffer2, nparsed);
        }
 
        if (u2_mode & 0x20)
        {
          if (UH_connected)
          {
            //&cable_inout: F0:out 0F:in
            uint32_t nwritten = tud_midi_stream_write (cable_num, buffer, bytes_read, mode);
          }
        }

        if (u2_mode & 0x01)
        {
          if (UD1_connected && tuh_midih_get_num_tx_cables(midi_dev_addr1) >= 1)        
          {
            //&cable_inout: F0:out 0F:in
            uint32_t nwritten = tuh_midi_stream_write(midi_dev_addr1, cable_num, buffer, bytes_read, mode);
          }
        }
 
        if (u2_mode & 0x04)
        {
          if (UD3_connected && tuh_midih_get_num_tx_cables(midi_dev_addr3) >= 1)        
          {
            //&cable_inout: F0:out 0F:in
            uint32_t nwritten = tuh_midi_stream_write(midi_dev_addr3, cable_num, buffer, bytes_read, mode);
          }
        }
 
        if (u2_mode & 0x08)
        {
          if (UD4_connected && tuh_midih_get_num_tx_cables(midi_dev_addr4) >= 1)        
          {
            //&cable_inout: F0:out 0F:in
            uint32_t nwritten = tuh_midi_stream_write(midi_dev_addr4, cable_num, buffer, bytes_read, mode);
          }
        }
       }
    }
    else if (midi_dev_addr3 == dev_addr)
    {
      uint8_t cable_num;
      uint8_t buffer[rx_size];
      uint8_t buffer2[ry_size];
      while (1) {

        uint32_t bytes_read = tuh_midi_stream_read(dev_addr, &cable_num, buffer, rx_size);
        if (bytes_read == 0) return;

        if (mode & 0x08) cable_num += (cable_num << 4);                             // cable out = cable in

        cable_num += usbd3_cable_offset;                                            // to distinguis it from midi_dev_addr1

        if (u3_mode & 0x10)
        {
          uint8_t nparsed = midi_filter (cable_num, buffer, buffer2, bytes_read, ry_size);
          uint8_t npushed = midi_uart_write_tx_buffer(midi_uart_instance, buffer2, nparsed);
        }
 
        if (u3_mode & 0x20)
        {
          if (UH_connected)
          {
            //&cable_inout: F0:out 0F:in
            uint32_t nwritten = tud_midi_stream_write (cable_num, buffer, bytes_read, mode);
          }
        }

        if (u3_mode & 0x01)
        {
          if (UD1_connected && tuh_midih_get_num_tx_cables(midi_dev_addr1) >= 1)        
          {
            //&cable_inout: F0:out 0F:in
            uint32_t nwritten = tuh_midi_stream_write(midi_dev_addr1, cable_num, buffer, bytes_read, mode);
          }
        }
 
        if (u3_mode & 0x02)
        {
          if (UD2_connected && tuh_midih_get_num_tx_cables(midi_dev_addr2) >= 1)        
          {
            //&cable_inout: F0:out 0F:in
            uint32_t nwritten = tuh_midi_stream_write(midi_dev_addr2, cable_num, buffer, bytes_read, mode);
          }
        }
 
        if (u3_mode & 0x08)
        {
          if (UD4_connected && tuh_midih_get_num_tx_cables(midi_dev_addr4) >= 1)        
          {
            //&cable_inout: F0:out 0F:in
            uint32_t nwritten = tuh_midi_stream_write(midi_dev_addr4, cable_num, buffer, bytes_read, mode);
          }
        }
       }
    }
    else if (midi_dev_addr4 == dev_addr)
    {
      uint8_t cable_num;
      uint8_t buffer[rx_size];
      uint8_t buffer2[ry_size];
      while (1) {

        uint32_t bytes_read = tuh_midi_stream_read(dev_addr, &cable_num, buffer, rx_size);
        if (bytes_read == 0) return;

        if (mode & 0x08) cable_num += (cable_num << 4);                             // cable out = cable in

        cable_num += usbd4_cable_offset;                                            // to distinguish it from midi_dev_addr1

        if (u4_mode & 0x10)
        {
          uint8_t nparsed = midi_filter (cable_num, buffer, buffer2, bytes_read, ry_size);
          uint8_t npushed = midi_uart_write_tx_buffer(midi_uart_instance, buffer2, nparsed);
        }
 
        if (u4_mode & 0x20)
        {
          if (UH_connected)
          {
            //&cable_inout: F0:out 0F:in
            uint32_t nwritten = tud_midi_stream_write (cable_num, buffer, bytes_read, mode);
          }
        }

        if (u4_mode & 0x01)
        {
          if (UD1_connected && tuh_midih_get_num_tx_cables(midi_dev_addr1) >= 1)        
          {
            //&cable_inout: F0:out 0F:in
            uint32_t nwritten = tuh_midi_stream_write(midi_dev_addr1, cable_num, buffer, bytes_read, mode);
          }
        }
 
        if (u4_mode & 0x02)
        {
          if (UD2_connected && tuh_midih_get_num_tx_cables(midi_dev_addr2) >= 1)        
          {
            //&cable_inout: F0:out 0F:in
            uint32_t nwritten = tuh_midi_stream_write(midi_dev_addr2, cable_num, buffer, bytes_read, mode);
          }
        }
 
        if (u4_mode & 0x04)
        {
          if (UD3_connected && tuh_midih_get_num_tx_cables(midi_dev_addr3) >= 1)        
          {
            //&cable_inout: F0:out 0F:in
            uint32_t nwritten = tuh_midi_stream_write(midi_dev_addr3, cable_num, buffer, bytes_read, mode);
          }
        }
       }
    }
  }
}

//--------------------------------------------------------------------+
// TinyUSB D-Callbacks
//--------------------------------------------------------------------+

// Invoked when received GET DEVICE DESCRIPTOR
// Application return pointer to descriptor
uint8_t const * tud_descriptor_device_cb(void)
{
  return (uint8_t const *) &desc_device;
}

// Invoked when received GET CONFIGURATION DESCRIPTOR
// Application return pointer to descriptor
// Descriptor contents must exist long enough for transfer to complete
uint8_t const * tud_descriptor_configuration_cb(uint8_t index)
{
  (void) index; // for multiple configurations

#if TUD_OPT_HIGH_SPEED
  // Although we are highspeed, host may be fullspeed.
  return (tud_speed_get() == TUSB_SPEED_HIGH) ?  desc_hs_configuration : desc_fs_configuration;
#else
  return desc_fs_configuration;
#endif
}

// Invoked when received GET STRING DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long enough for transfer to complete
uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
  (void) langid;

  uint8_t chr_count;

  if ( index == 0)
  {
    memcpy(&_desc_str[1], string_desc_arr[0], 2);
    chr_count = 1;
  }else
  {
    // Note: the 0xEE index string is a Microsoft OS 1.0 Descriptors.
    // https://docs.microsoft.com/en-us/windows-hardware/drivers/usbcon/microsoft-defined-usb-descriptors

    if ( !(index < sizeof(string_desc_arr)/sizeof(string_desc_arr[0])) ) return NULL;

    const char* str = string_desc_arr[index];

    // Cap at max char
    chr_count = (uint8_t) strlen(str);
    if ( chr_count > 31 ) chr_count = 31;

    // Convert ASCII string into UTF-16
    for(uint8_t i=0; i<chr_count; i++)
    {
      _desc_str[1+i] = str[i];
    }
  }

  // first byte is length (including header), second byte is string type
  _desc_str[0] = (uint16_t) ((TUSB_DESC_STRING << 8 ) | (2*chr_count + 2));

  return _desc_str;
}

// Invoked when device is mounted (configured)
void tud_mount_cb(void)
{
    sleep_ms(20);

    UH_connected = 1;

    //mode &= 0xFC;                       // == 0x00

    mod2info();

    sleep_ms(20);
}

// Invoked when device is unmounted
void tud_umount_cb(void)
{
    UH_connected = 0;

    mod2info();
}

void tud_midi_rx_cb(uint8_t itf)
{
  uint8_t cable_num;
  uint8_t buffer[rx_size];
  uint8_t buffer2[ry_size];
  while (1) {
    uint32_t bytes_read = tud_midi_n_stream_read(0, &cable_num, buffer, rx_size);

    if (mode & 0x08) cable_num += (cable_num << 4);

    cable_num += usbh_cable_offset;

    if (bytes_read == 0) return;

    if (c_mode & 0x10)
    {
      uint8_t nparsed = midi_filter (cable_num, buffer, buffer2, bytes_read, ry_size);
      uint8_t npushed = midi_uart_write_tx_buffer(midi_uart_instance, buffer2, nparsed);
    }

    if (c_mode & 0x01)
    {
      if (UD1_connected && tuh_midih_get_num_tx_cables(midi_dev_addr1) >= 1)        
      {
        //&cable_inout: F0:out 0F:in
        uint32_t nwritten = tuh_midi_stream_write(midi_dev_addr1, cable_num, buffer, bytes_read, mode);
      }
    }

    if (c_mode & 0x02)
    {
      if (UD2_connected && tuh_midih_get_num_tx_cables(midi_dev_addr2) >= 1)        
      {
        //&cable_inout: F0:out 0F:in
        uint32_t nwritten = tuh_midi_stream_write(midi_dev_addr2, cable_num, buffer, bytes_read, mode);
      }
    }

    if (c_mode & 0x04)
    {
      if (UD3_connected && tuh_midih_get_num_tx_cables(midi_dev_addr3) >= 1)        
      {
        //&cable_inout: F0:out 0F:in
        uint32_t nwritten = tuh_midi_stream_write(midi_dev_addr3, cable_num, buffer, bytes_read, mode);
      }
    }

    if (c_mode & 0x08)
    {
      if (UD4_connected && tuh_midih_get_num_tx_cables(midi_dev_addr4) >= 1)        
      {
        //&cable_inout: F0:out 0F:in
        uint32_t nwritten = tuh_midi_stream_write(midi_dev_addr4, cable_num, buffer, bytes_read, mode);
      }
    }
  }
}

// Invoked when received GET BOS DESCRIPTOR request
// Application return pointer to descriptor
uint8_t const * tud_descriptor_bos_cb(void) {
  return NULL;
}

// Invoked when received GET DEVICE QUALIFIER DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long enough for transfer to complete.
// device_qualifier descriptor describes information about a high-speed capable device that would
// change if the device were operating at the other speed. If not highspeed capable stall this request.
  uint8_t const* tud_descriptor_device_qualifier_cb(void) {
  return NULL;
}

// Invoked when received GET OTHER SPEED CONFIGURATION DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long enough for transfer to complete
// Configuration descriptor in the other speed e.g if high speed then this is for full speed and vice versa
  uint8_t const* tud_descriptor_other_speed_configuration_cb(uint8_t index) {
  return NULL;
}

// Invoked when usb bus is suspended
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en) {  // == umount ?
  UH_connected = 0;

  mod2info();
}

// Invoked when usb bus is resumed
void tud_resume_cb(void) {
}

// Invoked when received control request with VENDOR TYPE
bool tud_vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const * request) {
  return true;
}
