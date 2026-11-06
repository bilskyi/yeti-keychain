// Input plumbing: sources produce input_event_t, a queue carries them, the
// dispatcher hands each one to every subscriber (the face, the power policy).
//
// Adding an input = writing one input_source_t and listing it in input.c's
// SOURCES table under its FEATURE_* flag. Nothing downstream changes.
//
// With every input feature off (v0.1) the table is empty: input_poll() does
// nothing and no events ever arrive, but everything is wired.

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "input/input_event.h"

typedef struct {
    const char *name;
    esp_err_t (*init)(void);
    // Called once per frame from the main loop. A source can post from here
    // (polled) or from its own ISR/task with input_post_from_isr().
    void (*poll)(uint32_t now_ms);
    // Arm this source as a deep-sleep wake-up (GPIO0-5 only). May be NULL.
    void (*arm_wakeup)(void);
    // True while the source needs poll() at the fast frame rate (a button
    // held, the device moving). May be NULL.
    bool (*busy)(uint32_t now_ms);
} input_source_t;

typedef void (*input_handler_t)(input_event_t ev, uint32_t now_ms, void *ctx);

void input_init(void);
bool input_subscribe(input_handler_t fn, void *ctx);

// Thread-safe. False when the queue is full (event dropped).
bool input_post(input_event_t ev);
bool input_post_from_isr(input_event_t ev);  // IRAM-safe, from GPIO ISRs

void input_poll(uint32_t now_ms);      // run every source's poll()
void input_dispatch(uint32_t now_ms);  // drain the queue into subscribers
bool input_busy(uint32_t now_ms);      // any source wants fast polling

// The main loop sleeps here between frames instead of vTaskDelay, so a GPIO
// interrupt (button press, LIS3DH INT1) can end the wait early: a press is
// seen at once, not at the next 200 ms idle frame. Automatic light sleep
// still happens inside the wait; the pin's level wake brings the chip back.
void input_wait(uint32_t timeout_ms);
void input_kick_from_isr(void);  // IRAM-safe: end the current input_wait()
void input_kick(void);           // the same from a task (the mic's)

// Installs the shared GPIO ISR service and enables GPIO light-sleep wake-up.
// Sources call it from init(); safe to call more than once.
esp_err_t input_gpio_isr_init(void);

// How many sources are compiled in; 0 means nothing can wake the device.
int input_source_count(void);
void input_arm_wakeup(void);

// Latest LIS3DH reading in mg, sampled every frame, in the screen frame
// (input/orient.h: x right, y up, z out). False when there is no
// accelerometer or no sample yet. Games read tilt from it.
bool input_accel_mg(int mg[3]);
// The same sample in the board's own axes, for calibrating.
bool input_accel_raw_mg(int mg[3]);
// Board -> screen axes (input/orient.h); applied from the next sample.
void input_accel_set_orient(const int8_t map[3]);

// Microphone (input/mic.c). Listening keeps the chip out of light sleep, so
// main.c turns it on only while awake. The deaf check (our buzzer is
// playing) is called from the mic's task.
void input_mic_listen(bool on);
void input_mic_set_deaf_check(bool (*check)(void));
int input_mic_syllables(void);  // in the last INPUT_TALK, 1-9
int input_mic_loudness(void);   // now, 0-100 (0 while not listening); -1 = no mic

// The sources (defined in their own files, only under their flags).
extern const input_source_t input_source_buttons;
extern const input_source_t input_source_accel;
extern const input_source_t input_source_touch;
extern const input_source_t input_source_mic;
