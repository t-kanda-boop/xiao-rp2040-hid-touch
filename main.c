#include "pico/stdlib.h"
#include "bsp/board.h"
#include "tusb.h"

#define SCREEN_WIDTH 1080
#define SCREEN_HEIGHT 2640

// ======================
// HID Report Descriptor
// ======================

uint8_t const desc_hid_report[] =
{
  0x05, 0x0D,
  0x09, 0x04,
  0xA1, 0x01,
  0x09, 0x22,
  0xA1, 0x02,
  0x09, 0x42,
  0x15, 0x00,
  0x25, 0x01,
  0x75, 0x01,
  0x95, 0x01,
  0x81, 0x02,
  0x75, 0x07,
  0x95, 0x01,
  0x81, 0x03,
  0x05, 0x01,
  0x09, 0x30,
  0x09, 0x31,
  0x16, 0x00, 0x00,
  0x26, 0xFF, 0x7F,
  0x75, 0x10,
  0x95, 0x02,
  0x81, 0x02,
  0xC0,
  0xC0
};

// =====================
// Device Descriptor
// =====================

tusb_desc_device_t const desc_device =
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

uint8_t const * tud_descriptor_device_cb(void)
{
  return (uint8_t const *) &desc_device;
}

#define CONFIG_TOTAL_LEN  (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN)

uint8_t const desc_configuration[] =
{
  TUD_CONFIG_DESCRIPTOR(1, 1, 0, CONFIG_TOTAL_LEN, 0x00, 100),
  TUD_HID_DESCRIPTOR(0, 0, HID_ITF_PROTOCOL_NONE,
                     sizeof(desc_hid_report),
                     0x81, 16, 10)
};

uint8_t const * tud_descriptor_configuration_cb(uint8_t index)
{
  (void) index;
  return desc_configuration;
}

// HID Set Report Callback（必須）
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

// HID Get Report Callback（必須）
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

  // 何も返さない（タッチでは不要）
  return 0;
}

// ======================
// USB Descriptors
// ======================

uint8_t const * tud_hid_descriptor_report_cb(uint8_t instance)
{
  (void) instance;  // 未使用警告防止
  return desc_hid_report;
}

// -----------------------
// String Descriptors
// -----------------------

char const* string_desc_arr[] =
{
  (const char[]){ 0x09, 0x04 }, // 0: English (0x0409)
  "XIAO",                       // 1: Manufacturer
  "HID Touch",                  // 2: Product
  "123456",                     // 3: Serial
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
    if (!(index < sizeof(string_desc_arr)/sizeof(string_desc_arr[0])))
      return NULL;

    const char* str = string_desc_arr[index];
    chr_count = strlen(str);

    for (uint8_t i = 0; i < chr_count; i++)
    {
      _desc_str[1 + i] = str[i];
    }
  }

  _desc_str[0] = (TUSB_DESC_STRING << 8) | (2*chr_count + 2);

  return _desc_str;
}

// ======================
// 座標変換
// ======================

uint16_t convert_x(int px)
{
    return (px * 32767) / SCREEN_WIDTH;
}

uint16_t convert_y(int py)
{
    return (py * 32767) / SCREEN_HEIGHT;
}

// ======================
// タッチ送信
// ======================

void send_touch(uint16_t x, uint16_t y, bool pressed)
{
    uint8_t report[5];

    report[0] = pressed ? 1 : 0;
    report[1] = x & 0xFF;
    report[2] = (x >> 8) & 0xFF;
    report[3] = y & 0xFF;
    report[4] = (y >> 8) & 0xFF;

    tud_hid_report(0, report, sizeof(report));
}

// ======================
// メイン
// ======================

int main(void)
{
    board_init();
    tusb_init();

    while (1)
    {
        tud_task();

        static uint32_t timer = 0;
        if (board_millis() - timer > 5000)
        {
            timer = board_millis();

            uint16_t x = convert_x(500);
            uint16_t y = convert_y(2000);

            send_touch(x, y, true);
            sleep_ms(50);
            send_touch(x, y, false);
        }
    }
}
