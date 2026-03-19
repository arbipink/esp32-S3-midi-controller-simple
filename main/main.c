#include <stdint.h>
#include <stdlib.h>

#include "class/midi/midi_device.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "projdefs.h"
#include "tinyusb.h"
#include "tinyusb_default_config.h"

static const char *TAG = "Arbin midi";

enum interface_count { ITF_NUM_MIDI = 0, ITF_NUM_MIDI_STREAMING, ITF_COUNT };

enum usb_endpoints {
    EP_EMPTY = 0,
    EPNUM_MIDI,
};

#define TUSB_DESCRIPTOR_TOTAL_LEN                                              \
    (TUD_CONFIG_DESC_LEN + CFG_TUD_MIDI * TUD_MIDI_DESC_LEN)

// Basic MIDI Messages
#define NOTE_OFF 0x80
#define NOTE_ON 0x90

static const char *s_str_desc[5] = {
    (char[]){0x09, 0x04}, // 0: is supported language is English (0x0409)
    "Arbin Company",      // 1: Manufacturer
    "Arbin synth",        // 2: Product
    "123456",             // 3: Serials, should use chip ID
    "MIDI device hehe",   // 4: MIDI
};

static const uint8_t s_midi_cfg_desc[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_COUNT, 0, TUSB_DESCRIPTOR_TOTAL_LEN, 0, 100),
    TUD_MIDI_DESCRIPTOR(ITF_NUM_MIDI, 4, EPNUM_MIDI, (0x80 | EPNUM_MIDI), 64),
};

static void midi_task_read_example(void *arg) {
    // The MIDI interface always creates input and output port/jack descriptors
    // regardless of these being used or not. Therefore incoming traffic should
    // be read (possibly just discarded) to avoid the sender blocking in IO
    uint8_t packet[4];
    bool read = false;
    for (;;) {
        vTaskDelay(1);
        while (tud_midi_available()) {
            read = tud_midi_packet_read(packet);
            if (read) {
                ESP_LOGI(TAG,
                         "Read - Time (ms since boot): %lld, Data: %02hhX "
                         "%02hhX %02hhX %02hhX",
                         esp_timer_get_time(), packet[0], packet[1], packet[2],
                         packet[3]);
            }
        }
    }
}

// static void periodic_midi_write_example_cb(void *arg) {
//     uint8_t const note_sequence[] = {
//         69, 64, 69, 65, 69, 67, 69, 65, 69, 71, 69, 72, 69, 71, 69, 72,
//     };
//
//     static uint8_t const cable_num = 0;
//     static uint8_t const channel = 0;
//     static uint32_t note_pos = 0;
//
//     int previous = note_pos - 1;
//
//     if (previous < 0) {
//         previous = sizeof(note_sequence) - 1;
//     }
//
//     ESP_LOGI(TAG, "Writing MIDI data %d", note_sequence[note_pos]);
//
//     if (tud_midi_mounted()) {
//         uint8_t note_on[3] = {NOTE_ON | channel, note_sequence[note_pos],
//         127}; tud_midi_stream_write(cable_num, note_on, 3);
//
//         uint8_t note_off[3] = {NOTE_OFF | channel, note_sequence[previous],
//         0}; tud_midi_stream_write(cable_num, note_off, 3);
//     }
//
//     note_pos++;
//
//     if (note_pos >= sizeof(note_sequence)) {
//         note_pos = 0;
//     }
// }

void app_main(void) {
    ESP_LOGI(TAG, "USB initialization");

    tinyusb_config_t tusb_cfg = TINYUSB_DEFAULT_CONFIG();

    tusb_cfg.descriptor.string = s_str_desc;
    tusb_cfg.descriptor.string_count =
        sizeof(s_str_desc) / sizeof(s_str_desc[0]);
    tusb_cfg.descriptor.full_speed_config = s_midi_cfg_desc;

    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));

    ESP_LOGI(TAG, "USB initialization DONE");

    static uint8_t const cable_num = 0;
    static uint8_t const channel = 0;

    uint8_t note_on[3] = {NOTE_ON | channel, 72, 127};

    uint8_t note_off[3] = {NOTE_OFF | channel, 72, 0};

    // Read received MIDI packets
    ESP_LOGI(TAG, "MIDI read task init");
    xTaskCreate(midi_task_read_example, "midi_task_read_example", 4 * 1024,
                NULL, 5, NULL);

    while (1) {
        if (tud_midi_mounted()) {
            tud_midi_stream_write(cable_num, note_on, 3);
            vTaskDelay(pdMS_TO_TICKS(1000));
            tud_midi_stream_write(cable_num, note_off, 3);
            vTaskDelay(pdMS_TO_TICKS(1000));
        } else {
            vTaskDelay(pdMS_TO_TICKS(5000));
        }
    }
}
