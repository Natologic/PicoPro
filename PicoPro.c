/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *                    sekigon-gonnoc
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

// This example runs both host and device concurrently. The USB host receive
// reports from HID device and print it out over USB Device CDC interface.
// For TinyUSB roothub port0 is native usb controller, roothub port1 is
// pico-pio-usb.

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "hardware/clocks.h"
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/bootrom.h"

#include "pio_usb.h"
#include "tusb.h"
#include "pico/time.h"


typedef struct {
  char key;
  int byte;
  int shift;
} switchButtonMaps;

typedef struct {
  int byte;
  int shift;
} buttonLocation;

buttonLocation findSwitchMap(switchButtonMaps buttonMap[], int size, char key) {
  buttonLocation loc = { -1, -1 };
  for (int i=0; i < size; i++) {
    if (buttonMap[i].key == key) {
      loc.byte = buttonMap[i].byte;
      loc.shift = buttonMap[i].shift;
      break;
    }
  }
  return loc;
}

// IN ORDER:
// mapped character
// byte of button input report (starting from 0, which is byte 3 in the final report). 3
// bitshift count 
switchButtonMaps buttonMap[] = { \
  {'y', 0, 0}, /* Y button         */ \
  {'x', 0, 1}, /* X button         */ \
  {' ', 0, 2}, /* B button         */ \
  {'e', 0, 3}, /* A button         */ \
  {'r', 0, 6}, /* R button         */ \
  {'z', 0, 7}, /* ZR button        */ \
  {'p', 1, 1}, /* Plus button      */ \
  {'q', 2, 7}, /* ZL button        */ \
  {'w', 3, 0}, /* Left stick up    */ \
  {'s', 3, 1}, /* Left stick down  */ \
  {'a', 3, 2}, /* Left stick left  */ \
  {'d', 3, 3}, /* Left stick up    */ \
};


//--------------------------------------------------------------------+
// MACRO CONSTANT TYPEDEF PROTYPES
//--------------------------------------------------------------------+


void counter_task(void);
void button_task(void);

static uint8_t const keycode2ascii[128][2] =  { HID_KEYCODE_TO_ASCII };
uint32_t button_pressed = 0;
bool rotate = false;


/*------------- MAIN -------------*/

// core1: handle host events
void core1_main() {
  sleep_ms(10);

  stdio_uart_init ();
  // Use tuh_configure() to pass pio configuration to the host stack
  // Note: tuh_configure() must be called before
  pio_usb_configuration_t pio_cfg = PIO_USB_DEFAULT_CONFIG;
  tuh_configure(1, TUH_CFGID_RPI_PIO_USB_CONFIGURATION, &pio_cfg);

  // Set default protocol to enable additional buttons on mouse
  tuh_hid_set_default_protocol(HID_PROTOCOL_REPORT);
  // To run USB SOF interrupt in core1, init host stack for pio_usb (roothub
  // port1) on core1
  tuh_init(1);

  while (true) {
    tuh_task(); // tinyusb host task
  }
}

// core0: handle device events
int main(void) {
  stdio_init_all();
  // default 125MHz is not appropreate. Sysclock should be multiple of 12MHz.
  set_sys_clock_khz(120000, true);

  sleep_ms(10);

  multicore_reset_core1();
  // all USB task run in core1
  multicore_launch_core1(core1_main);

  // init device stack on native usb (roothub port0)
  tud_init(BOARD_TUD_RHPORT);

  while (true) {
    tud_task(); // tinyusb device task
    counter_task();
    button_task();
    fflush(stdout);
  }

  return 0;
}

//--------------------------------------------------------------------+
// USB HID
//--------------------------------------------------------------------+

// 3-byte package that holds the standard button report
uint8_t final_buttons[] = { 0x00, 0x00, 0x00 };
uint8_t imudata1a = 0x00;
uint8_t imudata1b = 0x00;
uint8_t imudata2a = 0x00;
uint8_t imudata2b = 0x00;
uint8_t imudata3a = 0x00;
uint8_t imudata3b = 0x00;



// neutral location for joystick?
uint8_t joystick_neutral[] = {0xFF, 0xF7, 0x7F};
uint8_t left_joystick[] = {0x00, 0x00, 0x00};

uint8_t imu_data1[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
uint8_t imu_data2[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
uint8_t imu_data3[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

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
    // printf("To Switch: ");
    // for (int i = 0; i < 64; i++)
    // {
    //     printf("%02X", report[i]);
    // }
    // printf("\n");
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

  // printf("From Switch: ");
  // for (int i = 0; i < bufsize; i++)
  // {
  //     printf("%02X", buffer[i]);
  // }
  // printf("\n");

  // Thanks to MIZUNO Yuki for this code https://www.mzyy94.com/blog/2020/03/20/nintendo-switch-pro-controller-usb-gadget/
  if (buffer[0] == 0x80) {
      if (buffer[1] == 0x01) {
          response(0x81, 0x01, (uint8_t *)extended_mac_addr, sizeof(extended_mac_addr));
      } 
      else if (buffer[1] == 0x02) {
          response(0x81, 0x02, (uint8_t *)0x00, 1);
      }
      else if (buffer[1] == 0x03) {
          printf("baud update\n");
          response(0x81, 0x03, (uint8_t *)0x00, 1);
      }
      else if (buffer[1] == 0x04) {
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
// Host HID
//--------------------------------------------------------------------+

// Invoked when device with hid interface is mounted
// Report descriptor is also available for use. tuh_hid_parse_report_descriptor()
// can be used to parse common/simple enough descriptor.
// Note: if report descriptor length > CFG_TUH_ENUMERATION_BUFSIZE, it will be skipped
// therefore report_desc = NULL, desc_len = 0
void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* desc_report, uint16_t desc_len)
{
  (void)desc_report;
  (void)desc_len;

  // Interface protocol (hid_interface_protocol_enum_t)
  const char* protocol_str[] = { "None", "Keyboard", "Mouse" };
  uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);

  uint16_t vid, pid;
  tuh_vid_pid_get(dev_addr, &vid, &pid);

  char tempbuf[256];
  int count = sprintf(tempbuf, "[%04x:%04x][%u] HID Interface%u, Protocol = %s\r\n", vid, pid, dev_addr, instance, protocol_str[itf_protocol]);
  printf(tempbuf);
  fflush(stdout);

  // Receive report from boot keyboard & mouse only
  // tuh_hid_report_received_cb() will be invoked when report is available
  if (itf_protocol == HID_ITF_PROTOCOL_KEYBOARD || itf_protocol == HID_ITF_PROTOCOL_MOUSE)
  {
    if ( !tuh_hid_receive_report(dev_addr, instance) )
    {
      printf("Error: cannot request report\r\n");
    }
  }
}

// Invoked when device with hid interface is un-mounted
void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance)
{
  char tempbuf[256];
  int count = sprintf(tempbuf, "[%u] HID Interface%u is unmounted\r\n", dev_addr, instance);
  printf(tempbuf);
  fflush(stdout);
}

// look up new key in previous keys
static inline bool find_key_in_report(hid_keyboard_report_t const *report, uint8_t keycode)
{
  for(uint8_t i=0; i<6; i++)
  {
    if (report->keycode[i] == keycode)  return true;
  }

  return false;
}

// Convert joystick values ranging from 0 to 2047 (neutral) to 4095 (max, higher numbers will overflow)
void to_joystick(int horiz, int vert, uint8_t *data) {
    uint8_t byte0 = horiz & 0x00FF; // mask out high byte to get low byte
    uint8_t byte1nibblelow = (horiz >> 8) & 0x000F; // bitshift high byte to low byte and mask out all but lowest nibble
    uint8_t byte1nibblehigh = (vert & 0x000F) << 4; // mask out all but lowest nibble and bitshift it to high nibble
    uint8_t byte1 = byte1nibblelow | byte1nibblehigh; // bitwise or together middle nibbles
    uint8_t byte2 = (vert >> 4) & 0x00FF; // bitshift all bytes by one nibble and mask out high byte to get low byte
    data[0] = byte0;
    data[1] = byte1;
    data[2] = byte2;
}

int vert = 2047;
int horiz = 2047;
int offset = 2047;

// convert hid keycode to ascii
static void process_kbd_report(uint8_t dev_addr, hid_keyboard_report_t const *report)
{
  (void) dev_addr;
  static hid_keyboard_report_t prev_report = { 0, 0, {0} }; // previous report to check key released
  // start by assuming the bytes are all zeros (all released)
  uint8_t buttons[] = { 0x00, 0x00, 0x00 };
  // container for change mask, where bit will be set to 1 if it _changes_ (this could be from 1 to 0 or from 0 to 1)
  uint8_t buttons_change_mask[] = { 0x00, 0x00, 0x00 };
  // neutral location for joystick?
  memcpy(left_joystick, joystick_neutral, sizeof(joystick_neutral));

  // check all 6 elements of the report to see if any of the corresponding keys are pressed
  for(uint8_t i=0; i<6; i++)
  {
    uint8_t keycode = report->keycode[i];
    if (keycode) {
      if (find_key_in_report(&prev_report, keycode)) {
        // exist in previous report means the current key is holding
      }
      else {
        //bool const is_shift = report->modifier & (KEYBOARD_MODIFIER_LEFTSHIFT | KEYBOARD_MODIFIER_RIGHTSHIFT);
        //char ch = is_shift ? keycode2ascii[keycode][1] : keycode2ascii[keycode][0];
        char ch = keycode2ascii[keycode][0];
        //printf("%c: ",ch);
        buttonLocation loc = findSwitchMap(buttonMap, sizeof(buttonMap) / sizeof(buttonMap[0]), ch);
        if (loc.byte != -1 && loc.shift != -1) {
          if (loc.byte == 3) {
            if (loc.shift == 0) {
              //vert = 4095;
              vert+=offset;
            }
            else if (loc.shift == 1) {
              //vert = 0;
              vert-=offset;
            }
            else if (loc.shift == 2) {
              //horiz = 0;
              horiz-=offset;
            }
            else if (loc.shift == 3) {
              //horiz = 4095;
              horiz+=offset;
            }
          }
          else {
            buttons[loc.byte] = buttons[loc.byte] | 1 << loc.shift;
            buttons_change_mask[loc.byte] = buttons_change_mask[loc.byte] | 1 << loc.shift;
          }
          //printf("%d\r\n",loc.shift);
        }
        else {
          //printf("not in mapping\r\n");
        }
        //fflush(stdout);
      }
    }
    // Check for key released
    uint8_t prev_keycode = prev_report.keycode[i];
    if ( prev_keycode && !find_key_in_report(report, prev_keycode) )
    {
      // key existed in previous report but not in current report means the key is released
      char ch = keycode2ascii[prev_keycode][0];
      buttonLocation loc = findSwitchMap(buttonMap, sizeof(buttonMap) / sizeof(buttonMap[0]), ch);
        if (loc.byte != -1 && loc.shift != -1) {
          if (loc.byte == 3) {
            if (loc.shift == 0) {
              //vert = 2047;
              vert-=offset;
            }
            else if (loc.shift == 1) {
              //vert = 2047;
              vert+=offset;
            }
            else if (loc.shift == 2) {
              //horiz = 2047;
              horiz+=offset;
            }
            else if (loc.shift == 3) {
              //horiz = 2047;
              horiz-=offset;
            }
          }
          else {
            buttons[loc.byte] = buttons[loc.byte] | 0 << loc.shift;
            buttons_change_mask[loc.byte] = buttons_change_mask[loc.byte] | 1 << loc.shift;
          }
          //printf("%d\r\n",loc.shift);
        }
        else {
          //printf("not in mapping\r\n");
        }
        //fflush(stdout);
      
    }
  }

  //printf("0x%02hhx\r\n", thirdbyte);
  //fflush(stdout);
  final_buttons[0] = (final_buttons[0] & ~buttons_change_mask[0]) | (buttons[0] & buttons_change_mask[0]);
  final_buttons[1] = (final_buttons[1] & ~buttons_change_mask[1]) | (buttons[1] & buttons_change_mask[1]);
  final_buttons[2] = (final_buttons[2] & ~buttons_change_mask[2]) | (buttons[2] & buttons_change_mask[2]);

  to_joystick(horiz, vert, left_joystick);
  prev_report = *report;
}

int16_t x_current_hid = 0;
int16_t y_current_hid = 0;
// send mouse report 
static void process_mouse_report(uint8_t dev_addr, hid_mouse_report_t const * report)
{
  uint8_t buttons[] = { 0x00, 0x00, 0x00 };
  uint8_t buttons_change_mask[] = { 0x00, 0x00, 0x00 };

  //------------- button state  -------------//
  //uint8_t button_changed_mask = report->buttons ^ prev_report.buttons;
  char l = report->buttons & MOUSE_BUTTON_LEFT   ? 'L' : '-';
  char m = report->buttons & MOUSE_BUTTON_MIDDLE ? 'M' : '-';
  char r = report->buttons & MOUSE_BUTTON_RIGHT  ? 'R' : '-';
  char b = report->buttons & MOUSE_BUTTON_BACKWARD  ? 'B' : '-';
  char f = report->buttons & MOUSE_BUTTON_FORWARD ? 'F' : '-';

  // char tempbuf[32];
  // int count = sprintf(tempbuf, "[%u] %c%c%c%c%c %d %d %d\r\n", dev_addr, l, m, r, b, f, report->x, report->y, report->wheel);  
  // printf(tempbuf);
  // fflush(stdout);

  //x is inverted
  x_current_hid += (report->x)*-1;
  y_current_hid += (report->y)*1;

  if (l == 'L') {
    char ch = 'z';
    buttonLocation loc = findSwitchMap(buttonMap, sizeof(buttonMap) / sizeof(buttonMap[0]), ch);
    buttons[loc.byte] = buttons[loc.byte] | 1 << loc.shift;
    buttons_change_mask[loc.byte] = buttons_change_mask[loc.byte] | 1 << loc.shift;
  }
  else {
    char ch = 'z';
    buttonLocation loc = findSwitchMap(buttonMap, sizeof(buttonMap) / sizeof(buttonMap[0]), ch);
    buttons[loc.byte] = buttons[loc.byte] | 0 << loc.shift;
    buttons_change_mask[loc.byte] = buttons_change_mask[loc.byte] | 1 << loc.shift;
  }
  final_buttons[0] = (final_buttons[0] & ~buttons_change_mask[0]) | (buttons[0] & buttons_change_mask[0]);
  final_buttons[1] = (final_buttons[1] & ~buttons_change_mask[1]) | (buttons[1] & buttons_change_mask[1]);
  final_buttons[2] = (final_buttons[2] & ~buttons_change_mask[2]) | (buttons[2] & buttons_change_mask[2]);
}

// Invoked when received report from device via interrupt endpoint
void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len)
{
  (void) len;
  uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);

  switch(itf_protocol)
  {
    case HID_ITF_PROTOCOL_KEYBOARD:
      process_kbd_report(dev_addr, (hid_keyboard_report_t const*) report );
    break;

    case HID_ITF_PROTOCOL_MOUSE:
      process_mouse_report(dev_addr, (hid_mouse_report_t const*) report );
    break;

    default: break;
  }

  // continue to request to receive report
  if ( !tuh_hid_receive_report(dev_addr, instance) )
  {
    printf("Error: cannot request report\r\n");
  }
}



//--------------------------------------------------------------------+
// COUNTER AND BUTTON TASKS
//--------------------------------------------------------------------+
void counter_task(void)
{
  static uint32_t start_ms = 0;
  // Blink every interval ms
  if ( to_ms_since_boot(get_absolute_time()) - start_ms < 30) return; // not enough time
  start_ms += 30;
  counter = (counter + 3) % 256;
}

int16_t x_delta = 0;
int16_t x_last = 0;
int16_t y_delta = 0;
int16_t y_last = 0;
void button_task(void)
{
  static uint32_t start_ms = 0;
  // Blink every interval ms
  if (( to_ms_since_boot(get_absolute_time()) - start_ms < 30) || mutex_held == true || ok_to_send_presses == false) return; // not enough time
  start_ms += 30;
  x_delta = (x_current_hid - x_last)*1;
  x_last = x_current_hid;
  y_delta = (y_current_hid - y_last)*0.1;
  y_last = y_current_hid;


  // convert all the deltas to little endian
  imu_data1[10] = (x_delta >> 8 & 0xFF);
  imu_data1[11] = (x_delta & 0xFF);
  imu_data2[10] = (x_delta >> 8 & 0xFF);
  imu_data2[11] = (x_delta & 0xFF);
  imu_data3[10] = (x_delta >> 8 & 0xFF);
  imu_data3[11] = (x_delta & 0xFF);

  // convert all the deltas to little endian
  imu_data1[8] = (y_delta >> 8 & 0xFF);
  imu_data1[9] = (y_delta & 0xFF);
  imu_data2[8] = (y_delta >> 8 & 0xFF);
  imu_data2[9] = (y_delta & 0xFF);
  imu_data3[8] = (y_delta >> 8 & 0xFF);
  imu_data3[9] = (y_delta & 0xFF);
  
  //uint8_t test_button_response[] = { 0x81, final_thirdbyte, 0x80, 0x00, 0xf8, 0xd7, 0x7a, 0x22, 0xc8, 0x7b, 0x0c };
  uint8_t test_button_response[] = { 0x81, final_buttons[0], final_buttons[1], final_buttons[2], left_joystick[0], left_joystick[1], left_joystick[2], 0x22, 0xc8, 0x7b, 0x0c };
  uint8_t buttons_and_joysticks[] = { 0x81, final_buttons[0], final_buttons[1], final_buttons[2], left_joystick[0], left_joystick[1], left_joystick[2], 0x22, 0xc8, 0x7b, 0x0c };
  uint8_t final_response[sizeof(buttons_and_joysticks) + sizeof(imu_data1) + sizeof(imu_data2) + sizeof(imu_data3)];
  memcpy(final_response,buttons_and_joysticks, sizeof(buttons_and_joysticks) * sizeof(uint8_t));
  memcpy(final_response+sizeof(buttons_and_joysticks),imu_data1, sizeof(imu_data1) * sizeof(uint8_t));
  memcpy(final_response+sizeof(buttons_and_joysticks)+sizeof(imu_data1),imu_data2, sizeof(imu_data2) * sizeof(uint8_t));
  memcpy(final_response+sizeof(buttons_and_joysticks)+sizeof(imu_data1)+sizeof(imu_data2),imu_data3, sizeof(imu_data3) * sizeof(uint8_t));
  response(0x30, counter, (uint8_t *)final_response, sizeof(final_response));
}