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

// ======================
// USB Descriptors
// ======================

uint8_t const * tud_hid_descriptor_report_cb(void)
{
  return desc_hid_report;
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
