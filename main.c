#include <string.h>
#include "pico/stdlib.h"
#include "bsp/board.h"
#include "tusb.h"
#include "pico/time.h"

/*------------------------------------------------------------------*/
/*  設定
/*------------------------------------------------------------------*/
#define SCREEN_WIDTH    1080
#define SCREEN_HEIGHT   2640
#define TOUCH_INTERVAL  600000   // 10分(ms)
#define TOUCH_DURATION  50       // タッチ時間(ms)
#define HID_REPORT_SIZE 6

/*------------------------------------------------------------------*/
/*  HID Report Descriptor
/*------------------------------------------------------------------*/
static const uint8_t desc_hid_report[] =
{
  0x05,0x0D, 0x09,0x04, 0xA1,0x01,
  0x85,0x01,
  0x09,0x22, 0xA1,0x02,
  0x09,0x42, 0x09,0x32,
  0x15,0x00, 0x25,0x01,
  0x75,0x01, 0x95,0x02,
  0x81,0x02,
  0x75,0x06, 0x95,0x01,
  0x81,0x03,
  0x05,0x01,
  0x09,0x30, 0x09,0x31,
  0x16,0x00,0x00,
  0x26,0xFF,0x7F,
  0x75,0x10, 0x95,0x02,
  0x81,0x02,
  0xC0,0xC0
};

/*------------------------------------------------------------------*/
/*  Device Descriptor
/*------------------------------------------------------------------*/
static const tusb_desc_device_t desc_device =
{
  .bLength = sizeof(tusb_desc_device_t),
  .bDescriptorType = TUSB_DESC_DEVICE,
  .bcdUSB = 0x0200,
  .bDeviceClass = 0x00,
  .bDeviceSubClass = 0x00,
  .bDeviceProtocol = 0x00,
  .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
  .idVendor = 0xCafe,
  .idProduct = 0x4000,
  .bcdDevice = 0x0100,
  .iManufacturer = 0x01,
  .iProduct = 0x02,
  .iSerialNumber = 0x03,
  .bNumConfigurations = 0x01
};

uint8_t const* tud_descriptor_device_cb(void){ return (uint8_t const*)&desc_device; }

/*------------------------------------------------------------------*/
/*  Configuration Descriptor
/*------------------------------------------------------------------*/
#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN)

static const uint8_t desc_configuration[] =
{
  TUD_CONFIG_DESCRIPTOR(1,1,0,CONFIG_TOTAL_LEN,
                        TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP,100),

  TUD_HID_DESCRIPTOR(0,0,HID_ITF_PROTOCOL_NONE,
                     sizeof(desc_hid_report),
                     0x81,64,5)
};

uint8_t const* tud_descriptor_configuration_cb(uint8_t index)
{
  (void)index;
  return desc_configuration;
}

uint8_t const* tud_hid_descriptor_report_cb(uint8_t instance)
{
  (void)instance;
  return desc_hid_report;
}

void tud_hid_set_report_cb(uint8_t a,uint8_t b,hid_report_type_t c,uint8_t const*d,uint16_t e){}
uint16_t tud_hid_get_report_cb(uint8_t a,uint8_t b,hid_report_type_t c,uint8_t*d,uint16_t e){return 0;}

/*------------------------------------------------------------------*/
/*  座標変換
/*------------------------------------------------------------------*/
static inline uint16_t convert_x(uint16_t px)
{
  return (uint32_t)px * 32767 / SCREEN_WIDTH;
}

static inline uint16_t convert_y(uint16_t py)
{
  return (uint32_t)py * 32767 / SCREEN_HEIGHT;
}

/*------------------------------------------------------------------*/
/*  タッチ送信
/*------------------------------------------------------------------*/
static void send_touch(uint16_t x, uint16_t y, bool pressed)
{
  if (!tud_mounted()) return;
  if (!tud_hid_ready()) return;

  uint8_t report[HID_REPORT_SIZE];

  report[0] = 0x01;
  report[1] = pressed ? 0x03 : 0x00;
  report[2] = x & 0xFF;
  report[3] = (x >> 8) & 0xFF;
  report[4] = y & 0xFF;
  report[5] = (y >> 8) & 0xFF;

  tud_hid_report(1, report, sizeof(report));
}

/*------------------------------------------------------------------*/
/*  状態管理
/*------------------------------------------------------------------*/
typedef enum {
  STATE_IDLE,
  STATE_PRESS,
  STATE_RELEASE
} touch_state_t;

static touch_state_t state = STATE_IDLE;

static uint32_t touch_timer = 0;
static uint32_t interval_timer = 0;

static uint16_t current_x = 0;
static uint16_t current_y = 0;

static inline uint32_t millis(void)
{
  return to_ms_since_boot(get_absolute_time());
}

static void tap_start(uint16_t px, uint16_t py)
{
  current_x = convert_x(px);
  current_y = convert_y(py);

  send_touch(current_x, current_y, true);

  state = STATE_PRESS;
  touch_timer = millis();
}

/*------------------------------------------------------------------*/
/*  Main
/*------------------------------------------------------------------*/
int main(void)
{
  board_init();
  tusb_init();

  while (!tud_mounted())
  {
    tud_task();
  }

  interval_timer = millis();

  while (1)
  {
    tud_task();
    uint32_t now = millis();

    /* タップ状態制御 */
    if (state == STATE_PRESS)
    {
      if (now - touch_timer >= TOUCH_DURATION)
      {
        send_touch(current_x, current_y, false);
        state = STATE_RELEASE;
        touch_timer = now;
      }
    }
    else if (state == STATE_RELEASE)
    {
      if (now - touch_timer >= TOUCH_DURATION)
      {
        state = STATE_IDLE;
      }
    }

    /* 10分処理 */
    if (now - interval_timer >= TOUCH_INTERVAL)
    {
      if (state == STATE_IDLE)
      {
        interval_timer = now;
        tap_start(900, 2350);
      }
    }
    else
    {
      if (state == STATE_IDLE)
      {
        tap_start(900, 2050);
      }
    }
  }
}
