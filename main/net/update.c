#include "net/update.h"

#if FEATURE_UPDATE

#include <stdio.h>
#include <string.h>

#include "esp_app_desc.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_image_format.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_ota_ops.h"
#include "esp_random.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "input/input.h"
#include "link/buddy.h"
#include "net/net.h"

static const char *TAG = "update";

#define SSID "YETI-7"
#define CHANNEL 6
#define CHUNK 4096
#define RESTART_MS 1500
// The first bytes of an image, through to the end of its app description:
// enough to tell whose firmware it is before writing any of it.
#define HEAD_BYTES (sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t))

// Written by the server's task and the WiFi events, read by the screen.
static volatile uint8_t s_phase = APP_UPD_OFF;
static volatile uint8_t s_pct;
static const char *volatile s_why;
static char s_pass[12];
static httpd_handle_t s_server;
static esp_netif_t *s_ap;
static bool s_handlers;

static const char PAGE[] =
    "<!doctype html><html><head><meta charset=utf-8>"
    "<meta name=viewport content='width=device-width,initial-scale=1'>"
    "<title>YETI-7 update</title><style>"
    "body{font:16px system-ui,sans-serif;background:#111;color:#eee;max-width:420px;margin:40px auto;padding:0 16px}"
    "h1{font-size:22px}button,input{font-size:16px;margin:8px 0}"
    "progress{width:100%;height:20px}#msg{margin-top:12px;min-height:1.5em}"
    "</style></head><body><h1>YETI-7 firmware</h1>"
    "<p>Pick the keychain's <b>.bin</b> (oled_keychain.bin) and send it.</p>"
    "<input type=file id=f accept='.bin'><br><button id=go>Send</button>"
    "<progress id=p max=100 value=0></progress><div id=msg></div><script>"
    "go.onclick=function(){var f=document.getElementById('f').files[0];"
    "if(!f){msg.textContent='Choose a file first.';return}"
    "var x=new XMLHttpRequest();x.open('POST','/update');"
    "x.upload.onprogress=function(e){if(e.lengthComputable)p.value=e.loaded*100/e.total};"
    "x.onload=function(){msg.textContent=x.responseText};"
    "x.onerror=function(){msg.textContent='The connection dropped. Try again.'};"
    "msg.textContent='Sending...';go.disabled=true;x.send(f)}"
    "</script></body></html>";

static esp_err_t page(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, PAGE, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t fail(httpd_req_t *req, esp_ota_handle_t ota, const char *why, const char *say) {
    if (ota) esp_ota_abort(ota);
    s_why = why;
    // Stopped from the keychain, the screen has gone: it stays off.
    if (s_phase != APP_UPD_OFF) s_phase = APP_UPD_FAILED;
    input_kick();
    ESP_LOGW(TAG, "%s", say);
    httpd_resp_set_status(req, "400 Bad Request");
    return httpd_resp_send(req, say, HTTPD_RESP_USE_STRLEN);
}

static void restart(void *arg) {
    (void)arg;
    esp_restart();
}

static esp_err_t upload(httpd_req_t *req) {
    const esp_partition_t *slot = esp_ota_get_next_update_partition(NULL);
    if (slot == NULL) return fail(req, 0, "NO SLOT", "This keychain has no second app slot. Flash it once by USB.");
    if (req->content_len < HEAD_BYTES || req->content_len > slot->size) {
        return fail(req, 0, "WRONG SIZE", "That file is not the size of a firmware.");
    }
    static uint8_t buf[CHUNK];
    s_phase = APP_UPD_RECEIVING;
    s_pct = 0;
    input_kick();

    // The head first, whole, to check whose firmware it is.
    size_t have = 0;
    while (have < HEAD_BYTES) {
        const int n = httpd_req_recv(req, (char *)buf + have, CHUNK - have);
        if (n <= 0) return fail(req, 0, "CUT OFF", "The upload stopped.");
        have += (size_t)n;
    }
    const esp_image_header_t *ih = (const esp_image_header_t *)buf;
    const esp_app_desc_t *d =
        (const esp_app_desc_t *)(buf + sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t));
    const esp_app_desc_t *mine = esp_app_get_description();
    if (ih->magic != ESP_IMAGE_HEADER_MAGIC || d->magic_word != ESP_APP_DESC_MAGIC_WORD) {
        return fail(req, 0, "NOT FIRMWARE", "That is not an ESP32 firmware image.");
    }
    if (strncmp(d->project_name, mine->project_name, sizeof(d->project_name)) != 0) {
        static char say[128];
        snprintf(say, sizeof(say), "That is %.32s firmware, not the keychain's (%.32s).", d->project_name,
                 mine->project_name);
        return fail(req, 0, "WRONG FIRMWARE", say);
    }
    ESP_LOGI(TAG, "receiving %s %s, %d bytes", d->project_name, d->version, (int)req->content_len);

    esp_ota_handle_t ota = 0;
    if (esp_ota_begin(slot, OTA_SIZE_UNKNOWN, &ota) != ESP_OK) return fail(req, 0, "FLASH ERROR", "Could not start writing.");
    size_t done = 0;
    for (;;) {
        if (have > 0) {
            if (esp_ota_write(ota, buf, have) != ESP_OK) return fail(req, ota, "FLASH ERROR", "Writing failed.");
            done += have;
            have = 0;
            const uint8_t pct = (uint8_t)(done * 100 / req->content_len);
            if (pct != s_pct) {
                s_pct = pct;
                input_kick();
            }
        }
        if (done >= req->content_len) break;
        if (s_phase != APP_UPD_RECEIVING) return fail(req, ota, "STOPPED", "Stopped on the keychain.");
        const int n = httpd_req_recv(req, (char *)buf, CHUNK);
        if (n == HTTPD_SOCK_ERR_TIMEOUT) continue;
        if (n <= 0) return fail(req, ota, "CUT OFF", "The upload stopped.");
        have = (size_t)n;
    }
    // esp_ota_end checks the image whole - its segments and its hash - and
    // only then may it be chosen for the next boot.
    if (esp_ota_end(ota) != ESP_OK) return fail(req, 0, "BAD IMAGE", "The image did not verify.");
    if (esp_ota_set_boot_partition(slot) != ESP_OK) return fail(req, 0, "FLASH ERROR", "Could not select it.");
    s_phase = APP_UPD_DONE;
    s_pct = 100;
    input_kick();
    ESP_LOGI(TAG, "written to %s; restarting", slot->label);
    httpd_resp_send(req, "Done. YETI-7 is restarting into it.", HTTPD_RESP_USE_STRLEN);
    static esp_timer_handle_t t;
    const esp_timer_create_args_t args = {.callback = restart, .name = "upd_restart"};
    if (t == NULL && esp_timer_create(&args, &t) != ESP_OK) esp_restart();
    esp_timer_start_once(t, RESTART_MS * 1000);
    return ESP_OK;
}

static void on_wifi(void *arg, esp_event_base_t base, int32_t id, void *data) {
    (void)arg;
    (void)base;
    (void)data;
    if (id == WIFI_EVENT_AP_STACONNECTED && s_phase == APP_UPD_WAITING) s_phase = APP_UPD_JOINED;
    if (id == WIFI_EVENT_AP_STADISCONNECTED && s_phase == APP_UPD_JOINED) s_phase = APP_UPD_WAITING;
    input_kick();
}

bool update_begin(void) {
    if (s_server != NULL) return true;  // up already
    if (!net_radio_up()) return false;
    buddy_pause(true);
    if (s_ap == NULL) s_ap = esp_netif_create_default_wifi_ap();
    if (!s_handlers) {
        esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_AP_STACONNECTED, on_wifi, NULL);
        esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_AP_STADISCONNECTED, on_wifi, NULL);
        s_handlers = true;
    }
    snprintf(s_pass, sizeof(s_pass), "%08lu", (unsigned long)(esp_random() % 100000000UL));
    wifi_config_t ap = {.ap = {.channel = CHANNEL, .max_connection = 1, .authmode = WIFI_AUTH_WPA2_PSK}};
    memcpy(ap.ap.ssid, SSID, strlen(SSID));
    ap.ap.ssid_len = (uint8_t)strlen(SSID);
    memcpy(ap.ap.password, s_pass, strlen(s_pass));
    esp_err_t err = esp_wifi_set_ps(WIFI_PS_NONE);
    if (err == ESP_OK) err = esp_wifi_set_mode(WIFI_MODE_APSTA);
    if (err == ESP_OK) err = esp_wifi_set_config(WIFI_IF_AP, &ap);
    if (err == ESP_OK) net_tx_power();  // the access point needs it as much
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.stack_size = 6144;
    cfg.recv_wait_timeout = 10;
    if (err == ESP_OK) err = httpd_start(&s_server, &cfg);
    if (err == ESP_OK) {
        const httpd_uri_t get = {.uri = "/", .method = HTTP_GET, .handler = page};
        const httpd_uri_t post = {.uri = "/update", .method = HTTP_POST, .handler = upload};
        httpd_register_uri_handler(s_server, &get);
        httpd_register_uri_handler(s_server, &post);
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "could not start: %s", esp_err_to_name(err));
        update_end();
        return false;
    }
    s_why = NULL;
    s_pct = 0;
    s_phase = APP_UPD_WAITING;
    ESP_LOGI(TAG, "WiFi %s up; page at 192.168.4.1", SSID);
    return true;
}

void update_end(void) {
    if (s_phase == APP_UPD_DONE) return;  // restarting regardless
    s_phase = APP_UPD_OFF;                // an upload in progress sees this and stops
    if (s_server) {
        httpd_stop(s_server);
        s_server = NULL;
    }
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
    net_tx_power();
    buddy_pause(false);
    ESP_LOGI(TAG, "WiFi off");
}

void update_status(app_update_t *u) {
    memset(u, 0, sizeof(*u));
    u->phase = s_phase;
    u->pct = s_pct;
    u->why = s_why;
    strcpy(u->ssid, SSID);
    strcpy(u->pass, s_pass);
    strcpy(u->addr, "192.168.4.1");
}

#endif
