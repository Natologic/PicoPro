/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 * Copyright (c) 2024 Nato Logic
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "bsp/board_api.h"
#include "tusb.h"


#include <stdint.h>
#include <unistd.h>


//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF PROTYPES
//--------------------------------------------------------------------+

/* Blink pattern
 * - 250 ms  : device not mounted
 * - 1000 ms : device mounted
 * - 2500 ms : device is suspended
 */
enum  {
  BLINK_NOT_MOUNTED = 250,
  BLINK_MOUNTED = 1000,
  BLINK_SUSPENDED = 2500,
};

static uint32_t blink_interval_ms = BLINK_NOT_MOUNTED;

void led_blinking_task(void);
void counter_task(void);
void button_task(void);

/*------------- MAIN -------------*/
int main(void)
{
  board_init();

  // init device stack on configured roothub port
  tud_init(BOARD_TUD_RHPORT);

  if (board_init_after_tusb) {
    board_init_after_tusb();
  }

  while (1)
  {
    tud_task(); // tinyusb device task
    led_blinking_task();
    counter_task();
    button_task();
  }
}

//--------------------------------------------------------------------+
// Device callbacks
//--------------------------------------------------------------------+

// Invoked when device is mounted
void tud_mount_cb(void)
{
  blink_interval_ms = BLINK_MOUNTED;
}

// Invoked when device is unmounted
void tud_umount_cb(void)
{
  blink_interval_ms = BLINK_NOT_MOUNTED;
}

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us  to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en)
{
  (void) remote_wakeup_en;
  blink_interval_ms = BLINK_SUSPENDED;
}

// Invoked when usb bus is resumed
void tud_resume_cb(void)
{
  blink_interval_ms = tud_mounted() ? BLINK_MOUNTED : BLINK_NOT_MOUNTED;
}

//--------------------------------------------------------------------+
// USB HID
//--------------------------------------------------------------------+



// Thanks to MIZUNO Yuki for these https://www.mzyy94.com/blog/2020/03/20/nintendo-switch-pro-controller-usb-gadget/
uint8_t extended_mac_addr[] = { 0x00, 0x03, 0x00, 0x00, 0x5e, 0x00, 0x53, 0x5e };
uint8_t serial_number[] = { 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
uint8_t controller_color[] = { 0x29, 0xA9, 0xA9, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
uint8_t factory_sensor[] = { 0x50, 0xFD, 0x00, 0x00, 0xC6, 0x0F, 0x0F, 0x30, 0x61, 0x96, 0x30, 0xF3, 0xD4, 0x14, 0x54, 0x41, 0x15, 0x54, 0xC7, 0x79, 0x9C, 0x33, 0x36, 0x63 };
uint8_t factory_stick[] = { 0x0F, 0x30, 0x61, 0x96, 0x30, 0xF3, 0xD4, 0x14, 0x54, 0x41, 0x15, 0x54, 0xC7, 0x79, 0x9C, 0x33, 0x36, 0x63 };
uint8_t factory_config[] = { 0xBA, 0x15, 0x62, 0x11, 0xB8, 0x7F, 0x29, 0x06, 0x5B, 0xFF, 0xE7, 0x7E, 0x0E, 0x36, 0x56, 0x9E, 0x85, 0x60, 0xFF, 0x32, 0x32, 0x32, 0xFF, 0xFF, 0xFF };
uint8_t user_stick[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xB2, 0xA1 };
uint8_t user_motion[] = { 0xBE, 0xFF, 0x3E, 0x00, 0xF0, 0x01, 0x00, 0x40, 0x00, 0x40, 0x00, 0x40, 0xFE, 0xFF, 0xFE, 0xFF, 0x08, 0x00, 0xE7, 0x3B, 0xE7, 0x3B, 0xE7, 0x3B };
uint8_t nfc_ir[] = { 0x01, 0x00, 0xFF, 0x00, 0x03, 0x00, 0x05, 0x01 };
uint8_t initial_input[] = { 0x81, 0x00, 0x80, 0x00, 0xf8, 0xd7, 0x7a, 0x22, 0xc8, 0x7b, 0x0c };
uint8_t a_button_press_response[] = { 0x81, 0x08, 0x80, 0x00, 0xf8, 0xd7, 0x7a, 0x22, 0xc8, 0x7b, 0x0c };
uint8_t a_button_release_response[] =  { 0x81, 0x00, 0x80, 0x00, 0xf8, 0xd7, 0x7a, 0x22, 0xc8, 0x7b, 0x0c };
uint8_t info_from_device[] = {0x03,0x48,0x03,0x02,0xe5,0x35,0x00,0xe5,0x00,0x00,0x03,0x01 };

bool ok_to_send_presses = false;
int counter = 0;
bool mutex_held = false;
bool a_press = false;

void response(uint8_t command, uint8_t response, uint8_t *buffer, size_t buffer_len) {
    mutex_held = true;
    uint8_t report[64] = {0};
    report[0] = command;
    report[1] = response;
    memcpy(report + 2, buffer, buffer_len);
    printf("To Switch: ");
    for (int i = 0; i < 64; i++)
    {
        printf("%02X", report[i]);
    }
    printf("\n");
    tud_hid_report(0, report, 64);
    mutex_held = false;
}

void uart_response(uint8_t command, uint8_t subcommand, uint8_t *buffer, size_t buffer_len) {
    uint8_t buf[64] = {0};
    memcpy(buf, (uint8_t *)initial_input, sizeof(initial_input));
    buf[sizeof(initial_input)] = command;
    buf[sizeof(initial_input) + 1] = subcommand;
    memcpy(buf + 2 + sizeof(initial_input), (uint8_t *)buffer, buffer_len);
    response(0x21, counter, (uint8_t *)buf, sizeof(initial_input) + 2 + buffer_len);
}


void spi_response(uint8_t *addr, uint8_t *buffer, size_t buffer_len) {
    uint8_t buf[64] = {0};
    memcpy(buf, addr, 2);
    buf[2] = 0x00;
    buf[3] = 0x00;
    buf[4] = buffer_len;
    memcpy(buf + 5, buffer, buffer_len);
    uart_response(0x90, 0x10, (uint8_t *)buf, 5 + buffer_len);
}


// Invoked when received GET_REPORT control request
// Application must fill buffer report's content and return its length.
// Return zero will cause the stack to STALL request
uint16_t tud_hid_get_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen)
{
  // TODO not Implemented
  (void) itf;
  (void) report_id;
  (void) report_type;
  (void) buffer;
  (void) reqlen;

  return 0;
}

// Invoked when received SET_REPORT control request or
// received data on OUT endpoint ( Report ID = 0, Type = 0 )
void tud_hid_set_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize)
{
  // This example doesn't use multiple report and report ID
  (void) itf;
  (void) report_id;
  (void) report_type;

  //tud_hid_report(0, buffer, bufsize);
  printf("From Switch: ");
  for (int i = 0; i < bufsize; i++)
  {
      printf("%02X", buffer[i]);
  }

  // Thanks to MIZUNO Yuki for this code https://www.mzyy94.com/blog/2020/03/20/nintendo-switch-pro-controller-usb-gadget/
  printf("\n");
  if (buffer[0] == 0x80) {
      if (buffer[1] == 0x01) {
          printf("mac address\n");
          response(0x81, 0x01, (uint8_t *)extended_mac_addr, sizeof(extended_mac_addr));
      } 
      else if (buffer[1] == 0x02) {
          printf("handshake\n");
          response(0x81, 0x02, (uint8_t *)0x00, 1);
      }
      // else if (buffer[1] == 0x03) {
      //     printf("baud update\n");
      //     response(0x81, 0x03, (uint8_t *)0x00, 1);
      // }
      else if (buffer[1] == 0x04) {
          printf("start usb\n");
          ok_to_send_presses = true;
      }  
  }
  else if (buffer[0] == 0x01) {
      if (buffer[10] == 0x01) {
          uart_response(0x81, buffer[10], (uint8_t[]){0x03}, 1);
      } else if (buffer[10] == 0x02) {
          uart_response(0x82, buffer[10], (uint8_t *)info_from_device, sizeof(info_from_device));
      } else if (buffer[10] == 0x03 || buffer[10] == 0x08 || buffer[10] == 0x30 || buffer[10] == 0x38 || buffer[10] == 0x40 || buffer[10] == 0x48) {
          uart_response(0x80, buffer[10], NULL, 0);
      } else if (buffer[10] == 0x04) {
          uart_response(0x83, buffer[10], NULL, 0);
      } else if (buffer[10] == 0x21) { //NFC / IR communcation
          uart_response(0xa0, buffer[10], (uint8_t *)nfc_ir, 8);
      } 
      else if (buffer[10] == 0x10) {
          if (memcmp(buffer + 11, "\x00\x60", 2) == 0) { // Serial Number
              spi_response(buffer + 11, (uint8_t *)serial_number, 16);
          } else if (memcmp(buffer + 11, "\x50\x60", 2) == 0) { // Controller Color
              spi_response(buffer + 11, (uint8_t *)controller_color, 13);
          } else if (memcmp(buffer + 11, "\x80\x60", 2) == 0) { // Factory Sensor
              spi_response(buffer + 11, (uint8_t *)factory_sensor, 24);
          } else if (memcmp(buffer + 11, "\x98\x60", 2) == 0) { // Factory Stick
              spi_response(buffer + 11, (uint8_t *)factory_stick, 18);
          } else if (memcmp(buffer + 11, "\x3d\x60", 2) == 0) { // Factory configuration
              spi_response(buffer + 11, (uint8_t *)factory_config, 25);
          } else if (memcmp(buffer + 11, "\x10\x80", 2) == 0) { // User sticks calibration
              spi_response(buffer + 11, (uint8_t *)user_stick, 24);
          } else if (memcmp(buffer + 11, "\x28\x80", 2) == 0) { // User motion calibration
              spi_response(buffer + 11, (uint8_t *)user_motion, 24);
          } else {
              printf("Unknown SPI address: %s\n", buffer + 11);
          }
      }
      else if (buffer[0] == 0x10 && sizeof(buffer) == 10) {
              // Do nothing
      }
      else {
      printf("unhandled\n");
      }
  }
}

//--------------------------------------------------------------------+
// BLINKING TASK
//--------------------------------------------------------------------+
void led_blinking_task(void)
{
  static uint32_t start_ms = 0;
  static bool led_state = false;

  // Blink every interval ms
  if ( board_millis() - start_ms < blink_interval_ms) return; // not enough time
  start_ms += blink_interval_ms;

  board_led_write(led_state);
  led_state = 1 - led_state; // toggle
}

//--------------------------------------------------------------------+
// COUNTER AND BUTTON TASKS
//--------------------------------------------------------------------+
void counter_task(void)
{
  static uint32_t start_ms = 0;
  // Blink every interval ms
  if ( board_millis() - start_ms < 30) return; // not enough time
  start_ms += 30;
  counter = (counter + 3) % 256;
}

void button_task(void)
{
  uint32_t const button_pressed = board_button_read();
  static uint32_t start_ms = 0;
  // Blink every interval ms
  if (( board_millis() - start_ms < 30) || mutex_held == true || ok_to_send_presses == false) return; // not enough time
  start_ms += 30;
  if (button_pressed) {
    response(0x30, counter, (uint8_t *)a_button_press_response, sizeof(a_button_press_response));
  }
  else {
    response(0x30, counter, (uint8_t *)a_button_release_response, sizeof(a_button_release_response));
  }
}