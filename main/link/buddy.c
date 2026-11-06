#include "link/buddy.h"

#if FEATURE_BUDDY

#include <string.h>

#include "esp_log.h"
#include "esp_mac.h"
#include "esp_now.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "input/input.h"
#include "net/net.h"
#include "nvs.h"

static const char *TAG = "buddy";

#define TICK_MS 5            // while a frame waits to go
// How often to look for BIT. Looking is listening for its HELLO, which it
// says every second, so a look keeps the radio on for up to PROBE_MS - and
// most of the day BIT is not there to hear, so away, the looking backs off to
// a couple of minutes. Handling the keychain looks at once (buddy_nudge), so
// it still notices BIT the moment someone picks it up at home.
#define PING_NEAR_MS 30000
#define PING_AWAY_MIN_MS 30000
#define PING_AWAY_MAX_MS 120000
#define NUDGE_GAP_MS 45000
#define PROBE_MS 1300
// BIT's radio dozes, and wakes for a moment after each HELLO (link/bitlink.h).
// Whatever YETI has to say goes then, and a ping that has not landed by the
// end of that moment waits for the next.
#define PING_GIVE_UP_MS 150
#define DWELL_MS 1100        // per channel while searching: BIT says HELLO every second
#define LOST_MS 15000        // listening and nothing heard this long: search again
#define CHANNELS 13
#define OUTSTANDING 8
// A send whose answer has not come in this long never will (its callback was
// lost - the queue was full): forget them all, or the count leaks until every
// send is refused and the other side hears nothing until a reboot.
#define ANSWER_LOST_MS 200
#define NVS_NS "buddy"

typedef enum { EV_RX = 1, EV_SENT, EV_OUT, EV_POKE } ev_kind_t;

typedef struct {
    uint8_t kind;
    bool ok;
    bool reliable;
    uint8_t mac[6];
    uint8_t len;
    uint8_t data[BL_FRAME_BYTES];
    bl_msg_t msg;
} ev_t;

typedef struct {
    bl_msg_t msg;
    bool delivered;
} done_t;

static QueueHandle_t s_ev, s_inbox, s_done;
static bool s_started;

// Set by the main loop, read by the task.
static volatile bool s_awake = true, s_listen, s_nudge, s_paused;
// Written by the task, read by the main loop.
static volatile uint32_t s_heard_at;
static volatile bool s_arrival;
static volatile uint8_t s_status = BUDDY_OFF;

// --- the task's own ---
static bl_tx_t s_tx;
static bl_rx_t s_rx;
static bl_presence_t s_presence;
static uint8_t s_peer[6];
static bool s_have_peer;
static uint8_t s_channel = 1;  // BIT's, once known
static bool s_searching;
static uint8_t s_hop;  // channels tried this search
static uint32_t s_hop_at;
static bool s_ps_none;  // the radio is held awake
static uint32_t s_next_ping, s_last_ping;
static uint32_t s_probe_until;  // looking for BIT until then; 0 not looking
static uint8_t s_probe_ch = 1;  // BIT not found yet: the channel the next look tries
static uint32_t s_diag_at;      // BIT's own counts were last logged then
// While listening, once a second: what came in and what failed to go, so a
// match that is lost can be read back from the log.
static uint32_t s_rx_frames, s_tx_failed, s_trace_at;
static uint32_t s_away_every = PING_AWAY_MIN_MS;
enum { OUT_ONCE = 0, OUT_RELIABLE, OUT_BROADCAST };
static uint8_t s_out_kind[OUTSTANDING];
static uint8_t s_out_head, s_out_count;
static uint32_t s_out_at;  // the last send, or answer to one

static const uint8_t BROADCAST[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

static inline uint32_t now_ms(void) { return (uint32_t)(esp_timer_get_time() / 1000); }

static void on_recv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
    if (len != BL_FRAME_BYTES || info == NULL) return;
    ev_t ev = {.kind = EV_RX, .len = (uint8_t)len};
    memcpy(ev.mac, info->src_addr, 6);
    memcpy(ev.data, data, BL_FRAME_BYTES);
    (void)xQueueSend(s_ev, &ev, 0);
}

static void on_sent(const uint8_t *mac, esp_now_send_status_t status) {
    (void)mac;
    const ev_t ev = {.kind = EV_SENT, .ok = status == ESP_NOW_SEND_SUCCESS};
    (void)xQueueSend(s_ev, &ev, 0);
}

static void load_peer(void) {
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) != ESP_OK) return;
    size_t len = sizeof(s_peer);
    uint8_t ch = 0;
    if (nvs_get_blob(h, "mac", s_peer, &len) == ESP_OK && len == 6 && nvs_get_u8(h, "ch", &ch) == ESP_OK &&
        ch >= 1 && ch <= CHANNELS) {
        s_have_peer = true;
        s_channel = ch;
    }
    nvs_close(h);
}

static void save_peer(void) {
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) return;
    esp_err_t err = nvs_set_blob(h, "mac", s_peer, 6);
    if (err == ESP_OK) err = nvs_set_u8(h, "ch", s_channel);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    if (err != ESP_OK) ESP_LOGW(TAG, "saving BIT failed: %s", esp_err_to_name(err));
}

static void set_channel(uint8_t ch) {
    const esp_err_t err = esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
    if (err != ESP_OK) ESP_LOGW(TAG, "channel %u: %s", ch, esp_err_to_name(err));
}

static void add_peer(void) {
    esp_now_peer_info_t p = {.channel = 0, .ifidx = WIFI_IF_STA, .encrypt = false};
    memcpy(p.peer_addr, s_peer, 6);
    const esp_err_t err = esp_now_add_peer(&p);
    if (err != ESP_OK && err != ESP_ERR_ESPNOW_EXIST) ESP_LOGW(TAG, "add peer: %s", esp_err_to_name(err));
}

static void update_status(uint32_t t) {
    if (s_searching) s_status = BUDDY_SEARCHING;
    else if (bl_presence_near(&s_presence, t)) s_status = BUDDY_NEAR;
    else s_status = s_have_peer ? BUDDY_AWAY : BUDDY_SEARCHING;
}

static void heard(uint32_t t) {
    // Answered: back to asking at the near pace.
    s_away_every = PING_AWAY_MIN_MS;
    if ((int32_t)(s_next_ping - (t + PING_NEAR_MS)) > 0) s_next_ping = t + PING_NEAR_MS;
    if (bl_presence_note(&s_presence, t)) {
        s_arrival = true;
        input_kick();
        ESP_LOGI(TAG, "BIT is near");
    }
    s_heard_at = t ? t : 1;
}

static bool send_frame(const uint8_t *mac, const bl_msg_t *m, uint8_t kind) {
    if (s_out_count >= OUTSTANDING) return false;
    bl_msg_t f = *m;
    f.role = BL_ROLE_YETI;
    if (f.type == BL_HELLO) memcpy(f.name, "YETI-7", 6);
    uint8_t buf[BL_FRAME_BYTES];
    bl_encode(&f, buf);
    if (esp_now_send(mac, buf, sizeof(buf)) != ESP_OK) return false;
    s_out_kind[(s_out_head + s_out_count) % OUTSTANDING] = kind;
    s_out_count++;
    s_out_at = now_ms();
    return true;
}

static void found(const uint8_t *mac, uint8_t ch) {
    const bool changed = !s_have_peer || memcmp(mac, s_peer, 6) != 0 || ch != s_channel;
    if (s_have_peer && memcmp(mac, s_peer, 6) != 0) esp_now_del_peer(s_peer);
    memcpy(s_peer, mac, 6);
    s_channel = ch;
    s_have_peer = true;
    if (!esp_now_is_peer_exist(s_peer)) add_peer();
    if (s_searching) {
        s_searching = false;
        ESP_LOGI(TAG, "found BIT " MACSTR " on channel %u", MAC2STR(mac), ch);
    }
    if (changed) save_peer();
}

static void take_frame(const ev_t *ev, uint32_t t) {
    bl_msg_t m;
    if (!bl_decode(ev->data, ev->len, &m) || m.role != BL_ROLE_BIT) return;
    s_rx_frames++;
    uint8_t ch = s_channel;
    wifi_second_chan_t second;
    (void)esp_wifi_get_channel(&ch, &second);
    if (m.type == BL_HELLO || !s_have_peer || memcmp(ev->mac, s_peer, 6) != 0) found(ev->mac, ch);
    heard(t);
    if (m.type == BL_HELLO) {
        // BIT is awake for a moment now: whatever is waiting goes at once,
        // and a look, or an open BIT screen, says hello back. An open screen
        // asks BIT to stay awake (d) - it wants answers, not a doze.
        if (!bl_tx_idle(&s_tx) && !s_tx.in_flight) s_tx.next_try = t;
        if (bl_tx_idle(&s_tx) && (s_probe_until != 0 || s_listen)) {
            const bl_msg_t ping = {.type = BL_PING, .d = s_listen};
            s_last_ping = t;
            (void)bl_tx_push(&s_tx, &ping, PING_GIVE_UP_MS, t);
        }
        if ((uint32_t)(t - s_diag_at) >= 15000 || s_diag_at == 0) {
            s_diag_at = t ? t : 1;
            ESP_LOGI(TAG, "BIT has heard %ld of ours (+%ld to all), radio %s", (long)m.a, (long)m.b,
                     m.d ? "awake" : "dozing");
        }
        return;
    }
    if (!bl_rx_fresh(&s_rx, m.seq)) return;
    if (xQueueSend(s_inbox, &m, 0) == pdTRUE) input_kick();
}

// Asleep, the radio only wakes to send. ESP-NOW's own default is to stay
// awake listening the whole time (a wake window of the maximum), which on a
// keychain means hearing BIT's HELLO every second at the price of most of the
// battery - so the window is shut, and opened only for a look (PROBE_MS) or
// while a BIT screen is up.
#define IDLE_WAKE_INTERVAL_MS 1000

static void radio_sleepy(void) {
    esp_err_t err = esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
    if (err == ESP_OK) err = esp_wifi_connectionless_module_set_wake_interval(IDLE_WAKE_INTERVAL_MS);
    if (err == ESP_OK) err = esp_now_set_wake_window(0);
    if (err != ESP_OK) ESP_LOGW(TAG, "power save: %s", esp_err_to_name(err));
}

static void radio_awake(bool on) {
    if (on == s_ps_none) return;
    s_ps_none = on;
    if (!on) {
        radio_sleepy();
        return;
    }
    // Listening: the window wide open again - power save off is not enough
    // on its own, a shut window still keeps the receiver off.
    esp_err_t err = esp_now_set_wake_window(65535);
    if (err == ESP_OK) err = esp_wifi_set_ps(WIFI_PS_NONE);
    if (err != ESP_OK) ESP_LOGW(TAG, "power save: %s", esp_err_to_name(err));
}

// Searching: one channel after another, BIT's last first, listening for its
// HELLO. A whole round without one, and it starts over.
static void search(uint32_t t) {
    if (!s_searching) {
        s_searching = true;
        s_hop = 0;
        s_hop_at = t;
        set_channel(s_channel);
        ESP_LOGI(TAG, "looking for BIT");
        return;
    }
    if ((int32_t)(t - s_hop_at) < DWELL_MS) return;
    s_hop_at = t;
    s_hop = (uint8_t)((s_hop + 1) % CHANNELS);
    set_channel((uint8_t)((s_channel - 1 + s_hop) % CHANNELS + 1));
}

static void buddy_task(void *arg) {
    (void)arg;
    s_next_ping = now_ms();
    uint32_t wait = TICK_MS;
    for (;;) {
        ev_t ev;
        if (xQueueReceive(s_ev, &ev, pdMS_TO_TICKS(wait)) == pdTRUE) {
            const uint32_t t = now_ms();
            switch (ev.kind) {
                case EV_RX:
                    take_frame(&ev, t);
                    break;
                case EV_SENT: {
                    if (s_out_count == 0) break;
                    const uint8_t kind = s_out_kind[s_out_head];
                    s_out_head = (uint8_t)((s_out_head + 1) % OUTSTANDING);
                    s_out_count--;
                    s_out_at = t;
                    if (ev.ok && kind != OUT_BROADCAST) heard(t);
                    if (!ev.ok && kind != OUT_BROADCAST) s_tx_failed++;
                    if (kind == OUT_RELIABLE) {
                        bl_msg_t m;
                        const bl_tx_outcome_t o = bl_tx_result(&s_tx, ev.ok, t, &m);
                        if (o != BL_TX_PENDING && m.type == BL_PING) {
                            ESP_LOGD(TAG, "ping %s after %u ms", o == BL_TX_DELIVERED ? "answered" : "unanswered",
                                     (unsigned)(t - s_last_ping));
                        }
                        if (o != BL_TX_PENDING && m.type != BL_PING) {
                            const done_t d = {m, o == BL_TX_DELIVERED};
                            if (xQueueSend(s_done, &d, 0) == pdTRUE) input_kick();
                        }
                    }
                    break;
                }
                case EV_OUT:
                    if (!s_have_peer) {
                        if (ev.reliable && ev.msg.type != BL_PING) {
                            const done_t d = {ev.msg, false};
                            (void)xQueueSend(s_done, &d, 0);
                            input_kick();
                        }
                        break;
                    }
                    if (ev.reliable) {
                        if (!bl_tx_push(&s_tx, &ev.msg, BL_GIVE_UP_MS, t)) ESP_LOGW(TAG, "send queue full");
                    } else {
                        (void)send_frame(s_peer, &ev.msg, OUT_ONCE);
                    }
                    break;
                default:
                    break;
            }
        }
        const uint32_t t = now_ms();
        bl_msg_t m;
        if (s_out_count > 0 && (uint32_t)(t - s_out_at) >= ANSWER_LOST_MS) {
            ESP_LOGW(TAG, "%u sends never answered; forgotten", s_out_count);
            s_out_count = 0;
        }
        if (s_paused) {  // the radio is net/update.h's for now
            wait = 1000;
            continue;
        }
        while (bl_tx_expired(&s_tx, t, &m)) {
            if (m.type == BL_PING) continue;
            const done_t d = {m, false};
            if (xQueueSend(s_done, &d, 0) == pdTRUE) input_kick();
        }

        // Listening: held awake, and searching while BIT is unknown or has
        // gone quiet. Not listening: asleep between sends, and back on BIT's
        // channel if a search was cut short.
        const bool listen = s_listen && s_awake;
        // A look ends once BIT has been heard and the hello back has gone, or
        // when PROBE_MS passes with no HELLO.
        if (s_probe_until != 0 &&
            ((int32_t)(t - s_probe_until) >= 0 ||
             (s_heard_at != 0 && (uint32_t)(t - s_heard_at) < PROBE_MS && bl_tx_idle(&s_tx)))) {
            s_probe_until = 0;
        }
        const bool lost = !s_have_peer || (uint32_t)(t - s_heard_at) >= LOST_MS || s_heard_at == 0;
        if (listen && (s_searching || lost)) {
            search(t);
        } else if (s_searching) {
            s_searching = false;
            set_channel(s_channel);
        }

        // Looks, while awake and not searching: hearing a HELLO says BIT is
        // near. Near, every PING_NEAR_MS; away, ever more rarely; handled, at
        // once. With a BIT screen open the radio listens anyway.
        if (s_nudge) {
            s_nudge = false;
            if ((uint32_t)(t - s_last_ping) >= NUDGE_GAP_MS) s_next_ping = t;
        }
        // Never found yet, each look tries the next channel: the whole band
        // in thirteen looks, so the keychain finds BIT by itself and the BIT
        // screens can stay hidden until it has.
        if (s_awake && !s_searching && (int32_t)(t - s_next_ping) >= 0) {
            if (!s_have_peer) {
                set_channel(s_probe_ch);
                s_probe_ch = (uint8_t)(s_probe_ch % CHANNELS + 1);
            }
            s_last_ping = t;
            if (bl_presence_near(&s_presence, t)) {
                s_next_ping = t + PING_NEAR_MS;
            } else {
                s_next_ping = t + s_away_every;
                s_away_every = s_away_every * 2 > PING_AWAY_MAX_MS ? PING_AWAY_MAX_MS : s_away_every * 2;
            }
            if (!listen) s_probe_until = t + PROBE_MS;
        }
        // After the look is scheduled, so the radio is open for the whole of it.
        radio_awake(listen || s_probe_until != 0);
        if (s_have_peer && !s_searching && bl_tx_due(&s_tx, t, &m) && !send_frame(s_peer, &m, OUT_RELIABLE)) {
            (void)bl_tx_result(&s_tx, false, t, NULL);
        }
        update_status(t);
        if (listen && (uint32_t)(t - s_trace_at) >= 1000) {
            uint8_t ch = 0;
            wifi_second_chan_t second;
            (void)esp_wifi_get_channel(&ch, &second);
            ESP_LOGI(TAG, "trace: %lu in, %lu unsent, BIT heard %lu ms ago, ch %u%s%s", (unsigned long)s_rx_frames,
                     (unsigned long)s_tx_failed, (unsigned long)(t - s_heard_at), ch, s_ps_none ? "" : " RADIO SHUT",
                     s_searching ? " SEARCHING" : "");
            s_rx_frames = s_tx_failed = 0;
            s_trace_at = t;
        }

        // How long to block. Short only while something is waiting to go:
        // waking every few milliseconds would keep the chip out of light
        // sleep, and that is most of the keychain's battery.
        if (!bl_tx_idle(&s_tx) || s_out_count > 0) {
            wait = TICK_MS;
        } else if (s_searching) {
            const int32_t left = (int32_t)(s_hop_at + DWELL_MS - t);
            wait = left > 0 ? (uint32_t)left : 1;
        } else if (s_probe_until != 0) {
            const int32_t left = (int32_t)(s_probe_until - t);
            wait = left > 0 ? (uint32_t)left : 1;
        } else if (listen) {
            wait = 250;  // a search may be due; the frames themselves wake us
        } else if (s_awake) {
            const int32_t left = (int32_t)(s_next_ping - t);
            wait = left > 0 ? (uint32_t)left : 1;
        } else {
            wait = 1000;
        }
    }
}

void buddy_init(void) {
    if (s_started) return;
    s_ev = xQueueCreate(32, sizeof(ev_t));
    s_inbox = xQueueCreate(8, sizeof(bl_msg_t));
    s_done = xQueueCreate(4, sizeof(done_t));
    if (s_ev == NULL || s_inbox == NULL || s_done == NULL) {
        ESP_LOGE(TAG, "no memory for the queues; no BIT");
        return;
    }
    esp_err_t err = net_radio_up() ? ESP_OK : ESP_FAIL;
    if (err == ESP_OK) err = esp_now_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "radio: %s; no BIT", esp_err_to_name(err));
        return;
    }
    esp_now_register_recv_cb(on_recv);
    esp_now_register_send_cb(on_sent);
    esp_now_peer_info_t bc = {.channel = 0, .ifidx = WIFI_IF_STA, .encrypt = false};
    memcpy(bc.peer_addr, BROADCAST, 6);
    (void)esp_now_add_peer(&bc);

    radio_sleepy();
    bl_tx_init(&s_tx, esp_random());
    bl_rx_init(&s_rx);
    bl_presence_init(&s_presence);
    load_peer();
    set_channel(s_channel);
    if (s_have_peer) {
        add_peer();
        ESP_LOGI(TAG, "BIT is " MACSTR " on channel %u", MAC2STR(s_peer), s_channel);
    }
    s_status = s_have_peer ? BUDDY_AWAY : BUDDY_SEARCHING;
    s_started = true;
    xTaskCreate(buddy_task, "buddy", 3072, NULL, 3, NULL);
}

// The task may be blocked for seconds; a change it must act on wakes it.
static void poke(void) {
    const ev_t ev = {.kind = EV_POKE};
    if (s_started) (void)xQueueSend(s_ev, &ev, 0);
}

void buddy_set_awake(bool awake) {
    if (awake == s_awake) return;
    s_awake = awake;
    poke();
}

void buddy_pause(bool paused) {
    if (paused == s_paused) return;
    s_paused = paused;
    if (!paused && s_started) set_channel(s_channel);  // the access point may have moved it
    poke();
}

void buddy_nudge(void) {
    s_nudge = true;
    poke();
}

void buddy_listen(bool on) {
    if (on == s_listen) return;
    s_listen = on;
    poke();
}

bool buddy_send(const bl_msg_t *m, bool reliable) {
    if (!s_started) return false;
    ev_t ev = {.kind = EV_OUT, .reliable = reliable, .msg = *m};
    return xQueueSend(s_ev, &ev, 0) == pdTRUE;
}

bool buddy_recv(bl_msg_t *m) { return s_started && xQueueReceive(s_inbox, m, 0) == pdTRUE; }

bool buddy_take_done(bl_msg_t *m, bool *delivered) {
    done_t d;
    if (!s_started || xQueueReceive(s_done, &d, 0) != pdTRUE) return false;
    *m = d.msg;
    *delivered = d.delivered;
    return true;
}

buddy_status_t buddy_status(void) { return (buddy_status_t)s_status; }
uint32_t buddy_heard_at(void) { return s_heard_at; }

bool buddy_take_arrival(void) {
    if (!s_arrival) return false;
    s_arrival = false;
    return true;
}

#endif
