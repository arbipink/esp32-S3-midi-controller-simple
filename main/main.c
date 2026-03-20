#include "class/midi/midi_device.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hal/gpio_types.h"
#include "projdefs.h"
#include "tinyusb.h"
#include "tinyusb_default_config.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define MIDI_BUTTON_INPUT 18
#define PITCH_UP_BUTTON_INPUT 17
#define PITCH_DOWN_BUTTON_INPUT 16

#define MIDDLE_C 60

static const char *TAG = "Arbin midi";

enum interface_count { ITF_NUM_MIDI = 0, ITF_NUM_MIDI_STREAMING, ITF_COUNT };

enum usb_endpoints {
    EP_EMPTY = 0,
    EPNUM_MIDI,
};

#define TUSB_DESCRIPTOR_TOTAL_LEN (TUD_CONFIG_DESC_LEN + CFG_TUD_MIDI * TUD_MIDI_DESC_LEN)

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

void usb_init() {
    ESP_LOGI(TAG, "USB initialization");

    tinyusb_config_t tusb_cfg = TINYUSB_DEFAULT_CONFIG();

    tusb_cfg.descriptor.string = s_str_desc;
    tusb_cfg.descriptor.string_count = sizeof(s_str_desc) / sizeof(s_str_desc[0]);
    tusb_cfg.descriptor.full_speed_config = s_midi_cfg_desc;

    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));

    ESP_LOGI(TAG, "USB initialization DONE");
}

void button_init() {
    ESP_LOGI(TAG, "Button initialization");

    gpio_config_t button = {
        .pin_bit_mask = (1ULL << MIDI_BUTTON_INPUT) | (1ULL << PITCH_UP_BUTTON_INPUT) |
                        (1ULL << PITCH_DOWN_BUTTON_INPUT),
        .mode = GPIO_MODE_INPUT,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&button);

    ESP_LOGI(TAG, "Button initialization DONE");
}

void midi_task_write() {
    static uint8_t const cable_num = 0;
    static uint8_t const channel = 0;

    uint8_t current_note = MIDDLE_C;
    uint8_t pitch_at_start = 0;

    bool note_is_active = false;

    for (;;) {
        if (gpio_get_level(PITCH_UP_BUTTON_INPUT) && current_note < 128) {
            current_note++;
        }

        else if (gpio_get_level(PITCH_DOWN_BUTTON_INPUT) && current_note > 0) {
            current_note--;
        }

        if (gpio_get_level(MIDI_BUTTON_INPUT) && !note_is_active) {
            pitch_at_start = current_note;
            uint8_t note_on[3] = {NOTE_ON | channel, pitch_at_start, 127};
            tud_midi_stream_write(cable_num, note_on, 3);
            note_is_active = true;
        }

        else if (!gpio_get_level(MIDI_BUTTON_INPUT) && note_is_active) {
            uint8_t note_off[3] = {NOTE_OFF | channel, pitch_at_start, 0};
            tud_midi_stream_write(cable_num, note_off, 3);
            note_is_active = false;
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void app_main(void) {
    usb_init();
    button_init();
    midi_task_write();
}

// TO DO
// 1. Add potentio or a joystick to control midi cc
// 2. Make it polyphonic
// 3. Use interrupt instead of polling

// Testing button (turns out my error come from lousy cables ^_^)
// void app_main(void) {
//     gpio_config_t midi_button = {
//         .pin_bit_mask = (1ULL << MIDI_BUTTON_INPUT),
//         .mode = GPIO_MODE_INPUT,
//         .intr_type = GPIO_INTR_DISABLE,
//     };
//     gpio_config(&midi_button);
//     uint8_t midi_button_state = gpio_get_level(MIDI_BUTTON_INPUT);
//     while (1) {
//         midi_button_state = gpio_get_level(MIDI_BUTTON_INPUT);
//         if (midi_button_state == 1) {
//             printf("Hi from ESP32 \n");
//         } else {
//             printf("Goodbye \n");
//         }
//         vTaskDelay(pdMS_TO_TICKS(10));
//     }
// }