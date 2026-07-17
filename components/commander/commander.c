/*
 * commander.c — G-code remote control task
 *
 * USB Serial JTAG (built-in) + WiFi TCP (port 8888).
 */

#include "commander.h"
#include "gcode.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/usb_serial_jtag.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

static const char *TAG = "cmd";

static motor_t      **g_motors        = NULL;
static int           g_mcount         = 0;
static int           tcp_listen       = -1;
static int           tcp_client       = -1;
static SemaphoreHandle_t reply_mutex  = NULL;
static volatile int  transport        = 0;  /* 0=none, 1=USB, 2=TCP */
static char          wifi_status[40]  = "WiFi: off";

/* ---- forward declarations ---- */

static void wifi_set_status(const char *s);

/* ---- reply ---- */

void commander_reply(const char *fmt, ...)
{
    va_list ap; va_start(ap, fmt);
    char buf[256];
    int len = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (len < 0) return;

    if (xSemaphoreTake(reply_mutex, pdMS_TO_TICKS(100)) != pdTRUE) return;

    if (transport == 1)
        usb_serial_jtag_write_bytes(buf, len, pdMS_TO_TICKS(50));
    else if (transport == 2 && tcp_client >= 0)
        send(tcp_client, buf, len, 0);

    xSemaphoreGive(reply_mutex);
}

/* ---- WiFi status getter (for UI) ---- */

const char *commander_get_wifi_status(void) { return wifi_status; }

static void wifi_set_status(const char *s)
{
    strncpy(wifi_status, s, sizeof(wifi_status) - 1);
    wifi_status[sizeof(wifi_status) - 1] = 0;
}

/* ---- USB Serial JTAG receive task ---- */

static void usb_recv_task(void *arg)
{
    (void)arg;
    char buf[256]; int pos = 0;
    while (1) {
        uint8_t ch;
        int r = usb_serial_jtag_read_bytes(&ch, 1, pdMS_TO_TICKS(100));
        if (r <= 0) continue;
        if (ch == '\r' || ch == '\n') {
            if (pos > 0) {
                buf[pos] = 0;
                ESP_LOGI(TAG, "USB < %s", buf);

                if (strncmp(buf, "$SSID=", 6) == 0) {
                    nvs_handle_t h;
                    if (nvs_open("wifi", NVS_READWRITE, &h) == ESP_OK) {
                        nvs_set_str(h, "ssid", buf + 6);
                        nvs_commit(h); nvs_close(h);
                    }
                    commander_reply("ok ssid saved\r\n");
                } else if (strncmp(buf, "$PASS=", 6) == 0) {
                    nvs_handle_t h;
                    if (nvs_open("wifi", NVS_READWRITE, &h) == ESP_OK) {
                        nvs_set_str(h, "pass", buf + 6);
                        nvs_commit(h); nvs_close(h);
                    }
                    commander_reply("ok pass saved\r\n");
                } else if (strncmp(buf, "$WIFI=", 6) == 0) {
                    nvs_handle_t h; char ssid[64]="", pass[64]="";
                    if (nvs_open("wifi", NVS_READONLY, &h) == ESP_OK) {
                        size_t l = sizeof(ssid);
                        nvs_get_str(h, "ssid", ssid, &l);
                        l = sizeof(pass);
                        nvs_get_str(h, "pass", pass, &l);
                        nvs_close(h);
                    }
                    if (ssid[0]) {
                        wifi_config_t wc = {0};
                        strncpy((char *)wc.sta.ssid, ssid, 32);
                        strncpy((char *)wc.sta.password, pass, 64);
                        esp_wifi_set_config(WIFI_IF_STA, &wc);
                        wifi_set_status("WiFi: connecting...");
                        esp_wifi_connect();
                        commander_reply("ok connecting to %s\r\n", ssid);
                    } else {
                        commander_reply("error: no SSID set\r\n");
                    }
                } else {
                    transport = 1;
                    gcode_parse(buf, g_motors, g_mcount);
                }
                pos = 0;
            }
        } else if (pos < (int)sizeof(buf) - 1) {
            buf[pos++] = (char)ch;
        }
    }
}

/* ---- WiFi event + TCP server ---- */

static void wifi_event_handler(void *arg, esp_event_base_t base,
                                int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        wifi_set_status("WiFi: connecting...");
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *ev = (ip_event_got_ip_t *)data;
        snprintf(wifi_status, sizeof(wifi_status),
                 "WiFi: " IPSTR, IP2STR(&ev->ip_info.ip));
        ESP_LOGI(TAG, "WiFi got IP: %s", wifi_status);
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED)
        ESP_LOGI(TAG, "WiFi disconnected — reconnect...");
}

static void wifi_setup(void)
{
    nvs_flash_init();
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                         wifi_event_handler, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                         wifi_event_handler, NULL, NULL);

    nvs_handle_t h;
    if (nvs_open("wifi", NVS_READONLY, &h) == ESP_OK) {
        char ssid[64] = "";
        size_t l = sizeof(ssid);
        if (nvs_get_str(h, "ssid", ssid, &l) == ESP_OK && ssid[0]) {
            wifi_config_t wc = {0};
            strncpy((char *)wc.sta.ssid, ssid, 32);
            l = sizeof(wc.sta.password);
            nvs_get_str(h, "pass", (char *)wc.sta.password, &l);
            esp_wifi_set_mode(WIFI_MODE_STA);
            esp_wifi_set_config(WIFI_IF_STA, &wc);
            wifi_set_status("WiFi: connecting...");
            esp_wifi_start();
        }
        nvs_close(h);
    }
}

static void tcp_setup(void)
{
    tcp_listen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (tcp_listen < 0) return;

    int opt = 1;
    setsockopt(tcp_listen, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(8888);

    bind(tcp_listen, (struct sockaddr *)&addr, sizeof(addr));
    listen(tcp_listen, 1);
    ESP_LOGI(TAG, "TCP server on :8888");
}

static void tcp_accept_task(void *arg)
{
    (void)arg;
    while (1) {
        if (tcp_listen < 0) { vTaskDelay(pdMS_TO_TICKS(500)); continue; }
        struct sockaddr_in6 a; socklen_t al = sizeof(a);
        int c = accept(tcp_listen, (struct sockaddr *)&a, &al);
        if (c >= 0) {
            if (tcp_client >= 0) close(tcp_client);
            tcp_client = c;
            ESP_LOGI(TAG, "TCP client connected");
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

static void tcp_recv_task(void *arg)
{
    (void)arg;
    char buf[256];
    while (1) {
        if (tcp_client < 0) { vTaskDelay(pdMS_TO_TICKS(200)); continue; }
        int r = recv(tcp_client, buf, sizeof(buf) - 1, 0);
        if (r <= 0) { close(tcp_client); tcp_client = -1; continue; }
        buf[r] = 0;
        while (r > 0 && (buf[r-1] == '\r' || buf[r-1] == '\n')) buf[--r] = 0;

        ESP_LOGI(TAG, "TCP < %s", buf);

        if (strncmp(buf, "$SSID=", 6) == 0) {
            nvs_handle_t h;
            if (nvs_open("wifi", NVS_READWRITE, &h) == ESP_OK) {
                nvs_set_str(h, "ssid", buf + 6);
                nvs_commit(h); nvs_close(h);
            }
            commander_reply("ok ssid saved\r\n");
        } else if (strncmp(buf, "$PASS=", 6) == 0) {
            nvs_handle_t h;
            if (nvs_open("wifi", NVS_READWRITE, &h) == ESP_OK) {
                nvs_set_str(h, "pass", buf + 6);
                nvs_commit(h); nvs_close(h);
            }
            commander_reply("ok pass saved\r\n");
        } else if (strncmp(buf, "$WIFI=", 6) == 0) {
            nvs_handle_t h; char ssid[64]="", pass[64]="";
            if (nvs_open("wifi", NVS_READONLY, &h)==ESP_OK) {
                size_t l=sizeof(ssid); nvs_get_str(h,"ssid",ssid,&l);
                l=sizeof(pass); nvs_get_str(h,"pass",pass,&l); nvs_close(h);
            }
            if (ssid[0]) {
                wifi_config_t wc={0};
                strncpy((char*)wc.sta.ssid,ssid,32);
                strncpy((char*)wc.sta.password,pass,64);
                esp_wifi_set_config(WIFI_IF_STA,&wc);
                wifi_set_status("WiFi: connecting...");
                esp_wifi_connect();
                commander_reply("ok connecting to %s\r\n",ssid);
            } else { commander_reply("error: no SSID set\r\n"); }
        } else {
            transport = 2;
            gcode_parse(buf, g_motors, g_mcount);
        }
    }
}

/* ---- Entry ---- */

esp_err_t commander_init(const commander_cfg_t *cfg)
{
    g_motors = cfg->motors;
    g_mcount = cfg->motor_count;
    reply_mutex = xSemaphoreCreateMutex();

    wifi_setup();
    tcp_setup();

    /* Install USB Serial JTAG driver before launching tasks */
    usb_serial_jtag_driver_config_t usb_cfg = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(usb_serial_jtag_driver_install(&usb_cfg));

    xTaskCreatePinnedToCore(tcp_accept_task, "tcp_accept", 3*1024, NULL, 3, NULL, tskNO_AFFINITY);
    xTaskCreatePinnedToCore(tcp_recv_task,  "tcp_recv",  4*1024, NULL, 3, NULL, tskNO_AFFINITY);
    xTaskCreatePinnedToCore(usb_recv_task,  "usb_recv",  4*1024, NULL, 2, NULL, tskNO_AFFINITY);

    ESP_LOGI(TAG, "Commander ready — USB Serial JTAG + WiFi TCP:8888");
    return ESP_OK;
}
