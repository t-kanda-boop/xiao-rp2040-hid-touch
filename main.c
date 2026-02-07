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
#define TOUCH_INTERVAL  600000   // ms 10分
#define TOUCH_DURATION  50     // ms
#define SWIPE_STEPS     20     // 分割数（滑らかさ）
#define SWIPE_DELAY     10     // 各ステップ間隔(ms)

#define HID_REPORT_SIZE 6

/*------------------------------------------------------------------*/
/*  HID Report Descriptor (Single Touch Digitizer)
/*------------------------------------------------------------------*/
static const uint8_t desc_hid_report[] =
{
  0x05, 0x0D,        // Usage Page (Digitizer)
  0x09, 0x04,        // Usage (Touch Screen)
  0xA1, 0x01,        // Collection (Application)

  0x85, 0x01,        //   Report ID (1)

  0x09, 0x22,        //   Usage (Finger)
  0xA1, 0x02,        //   Collection (Logical)

  0x09, 0x42,        //     Tip Switch
  0x09, 0x32,        //     In Range
  0x15, 0x00,
  0x25, 0x01,
  0x75, 0x01,
  0x95, 0x02,
  0x81, 0x02,

  0x75, 0x06,
  0x95, 0x01,
  0x81, 0x03,

  0x05, 0x01,
  0x09, 0x30,        // X
  0x09, 0x31,        // Y
  0x16, 0x00, 0x00,
  0x26, 0xFF, 0x7F,
  0x75, 0x10,
  0x95, 0x02,
  0x81, 0x02,

  0xC0,
  0xC0
};

/*------------------------------------------------------------------*/
/*  Device Descriptor
/*------------------------------------------------------------------*/
static const tusb_desc_device_t desc_device =
{
  .bLength            = sizeof(tusb_desc_device_t),
  .bDescriptorType    = TUSB_DESC_DEVICE,
  .bcdUSB             = 0x0200,
  .bDeviceClass       = 0x00,
  .bDeviceSubClass    = 0x00,
  .bDeviceProtocol    = 0x00,
  .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
  .idVendor           = 0xCafe,
  .idProduct          = 0x4000,
  .bcdDevice          = 0x0100,
  .iManufacturer      = 0x01,
  .iProduct           = 0x02,
  .iSerialNumber      = 0x03,
  .bNumConfigurations = 0x01
};

uint8_t const* tud_descriptor_device_cb(void)
{
  return (uint8_t const*) &desc_device;
}

/*------------------------------------------------------------------*/
/*  Configuration Descriptor
/*------------------------------------------------------------------*/
#define CONFIG_TOTAL_LEN  (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN)

static const uint8_t desc_configuration[] =
{
  TUD_CONFIG_DESCRIPTOR(1, 1, 0, CONFIG_TOTAL_LEN,
                        TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP,
                        100),

  TUD_HID_DESCRIPTOR(0,        // Interface number
                     0,        // String index
                     HID_ITF_PROTOCOL_NONE,
                     sizeof(desc_hid_report),
                     0x81,     // EP IN
                     64,       // EP size
                     10)       // Poll interval
};

uint8_t const* tud_descriptor_configuration_cb(uint8_t index)
{
  (void) index;
  return desc_configuration;
}

/*------------------------------------------------------------------*/
/*  HID Callbacks
/*------------------------------------------------------------------*/
uint8_t const* tud_hid_descriptor_report_cb(uint8_t instance)
{
  (void) instance;
  return desc_hid_report;
}

void tud_hid_set_report_cb(uint8_t instance,
                           uint8_t report_id,
                           hid_report_type_t report_type,
                           uint8_t const* buffer,
                           uint16_t bufsize)
{
  (void) instance;
  (void) report_id;
  (void) report_type;
  (void) buffer;
  (void) bufsize;
}

uint16_t tud_hid_get_report_cb(uint8_t instance,
                               uint8_t report_id,
                               hid_report_type_t report_type,
                               uint8_t* buffer,
                               uint16_t reqlen)
{
  (void) instance;
  (void) report_id;
  (void) report_type;
  (void) buffer;
  (void) reqlen;
  return 0;
}

/*------------------------------------------------------------------*/
/*  String Descriptors
/*------------------------------------------------------------------*/
static const char* string_desc_arr[] =
{
  (const char[]){ 0x09, 0x04 }, // English
  "XIAO",
  "HID Touch",
  "123456",
};

static uint16_t _desc_str[32];

uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
  (void) langid;
  uint8_t chr_count;

  if (index == 0)
  {
    _desc_str[1] = 0x0409;
    chr_count = 1;
  }
  else
  {
    if (index >= sizeof(string_desc_arr)/sizeof(string_desc_arr[0]))
      return NULL;

    const char* str = string_desc_arr[index];
    chr_count = strlen(str);

    for (uint8_t i = 0; i < chr_count; i++)
      _desc_str[1 + i] = str[i];
  }

  _desc_str[0] = (TUSB_DESC_STRING << 8) | (2 * chr_count + 2);
  return _desc_str;
}

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

    report[0] = 0x01;                 // Report ID
    report[1] = pressed ? 0x03 : 0x00; // Tip + InRange
    report[2] = x & 0xFF;
    report[3] = (x >> 8) & 0xFF;
    report[4] = y & 0xFF;
    report[5] = (y >> 8) & 0xFF;

    tud_hid_report(1, report, sizeof(report));
}

static void tap(uint16_t px, uint16_t py)
{
  uint16_t x = convert_x(px);
  uint16_t y = convert_y(py);

  send_touch(x, y, true);
  sleep_ms(TOUCH_DURATION);
  send_touch(x, y, false);
  sleep_ms(TOUCH_DURATION);
}

static void swipe(uint16_t x1, uint16_t y1,
                  uint16_t x2, uint16_t y2)
{
    uint16_t start_x = convert_x(x1);
    uint16_t start_y = convert_y(y1);
    uint16_t end_x   = convert_x(x2);
    uint16_t end_y   = convert_y(y2);

    int32_t dx = (int32_t)end_x - start_x;
    int32_t dy = (int32_t)end_y - start_y;

    // 押す
    send_touch(start_x, start_y, true);
    sleep_ms(SWIPE_DELAY);

    for (int i = 1; i <= SWIPE_STEPS; i++)
    {
        uint16_t move_x = start_x + (dx * i) / SWIPE_STEPS;
        uint16_t move_y = start_y + (dy * i) / SWIPE_STEPS;

        send_touch(move_x, move_y, true);
        sleep_ms(SWIPE_DELAY);
    }

    // 離す
    send_touch(end_x, end_y, false);
}

static inline uint32_t millis(void)
{
    return to_ms_since_boot(get_absolute_time());
}

/*------------------------------------------------------------------*/
/*  Main
/*------------------------------------------------------------------*/
int main(void)
{
  board_init();
  stdio_init_all);
  tusb_init();

  while (!tud_mounted())
  {
      tud_task();
  }

  uint32_t timer = millis();

  while (1)
  {
    tud_task();

    if (millis() - timer > TOUCH_INTERVAL)
    {
      timer = millis();

      swipe(500, 1800, 500, 600);
      sleep_ms(300);

      tap(900, 2350);
      sleep_ms(500);
      tap(800, 1700);
      sleep_ms(500);
      tap(500, 2000);
      sleep_ms(500);
      tap(300, 2000);
      sleep_ms(500);
      tap(500, 2550);
      sleep_ms(500);
    }
    else
    {
      tap(900, 2050);
      sleep_ms(200);
      tap(420, 2550);
      sleep_ms(200);
      tap(900, 2050);
      sleep_ms(200);
      tap(90, 2550);
      sleep_ms(200);
    }

    sleep_ms(10);
  }
}
