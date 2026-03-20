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

void app_main(void) {
    ESP_LOGI(TAG, "USB initialization");

    tinyusb_config_t tusb_cfg = TINYUSB_DEFAULT_CONFIG();

    tusb_cfg.descriptor.string = s_str_desc;
    tusb_cfg.descriptor.string_count =
        sizeof(s_str_desc) / sizeof(s_str_desc[0]);
    tusb_cfg.descriptor.full_speed_config = s_midi_cfg_desc;

    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));

    ESP_LOGI(TAG, "USB initialization DONE");

    ESP_LOGI(TAG, "Button initialization");

    gpio_config_t midi_button = {
        .pin_bit_mask = (1ULL << MIDI_BUTTON_INPUT),
        .mode = GPIO_MODE_INPUT,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&midi_button);

    uint8_t midi_button_state = gpio_get_level(MIDI_BUTTON_INPUT);

    gpio_config_t pitch_up_button = {
        .pin_bit_mask = (1ULL << PITCH_UP_BUTTON_INPUT),
        .mode = GPIO_MODE_INPUT,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&pitch_up_button);

    uint8_t pitch_up_button_state = gpio_get_level(MIDI_BUTTON_INPUT);

    gpio_config_t pitch_down_button = {
        .pin_bit_mask = (1ULL << PITCH_DOWN_BUTTON_INPUT),
        .mode = GPIO_MODE_INPUT,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&pitch_down_button);

    uint8_t pitch_down_button_state = gpio_get_level(MIDI_BUTTON_INPUT);

    ESP_LOGI(TAG, "Button initialization DONE");

    static uint8_t const cable_num = 0;
    static uint8_t const channel = 0;

    uint8_t current_note = MIDDLE_C;

    uint8_t note_on[3] = {NOTE_ON | channel, current_note, 127};

    uint8_t note_off[3] = {NOTE_OFF | channel, current_note, 0};

    // Read received MIDI packets
    ESP_LOGI(TAG, "MIDI read task init");
    xTaskCreate(midi_task_read_example, "midi_task_read_example", 4 * 1024,
                NULL, 5, NULL);

    while (1) {
        midi_button_state = gpio_get_level(MIDI_BUTTON_INPUT);
        pitch_up_button_state = gpio_get_level(PITCH_UP_BUTTON_INPUT);
        pitch_down_button_state = gpio_get_level(PITCH_DOWN_BUTTON_INPUT);

        if (tud_midi_mounted()) {
            if (midi_button_state) {
                tud_midi_stream_write(cable_num, note_on, 3);
            } else if (pitch_up_button_state) {
                current_note++;
            } else if (pitch_down_button_state) {
                current_note--;
            } else {
                tud_midi_stream_write(cable_num, note_off, 3);
            }
        }

        if (current_note > 127)
            current_note = 127;
        if (current_note < 1)
            current_note = 1;

        note_on[1] = current_note;
        note_off[1] = current_note;

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

// TO DO
// 1. Create different function for specific task instead of bloatin app_main()
// 2. Handle hanging note problem
// 3. Add potentio or a joystick to control midi cc
// 4. Use interrupt instead of polling

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