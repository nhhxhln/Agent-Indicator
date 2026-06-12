/* Wi-Fi 链路:STA 接入 PC 的 AP → WebSocket server(/ws)→ mDNS 广播 _agentind._tcp。
 * host 侧 agentind 经 zeroconf 发现并连接。 */
#include <string.h>

#include "comm.h"

#include "esp_check.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "mdns.h"
#include "nvs_flash.h"
#include "proto/proto.h"
#include "sdkconfig.h"

static const char *TAG = "comm_wifi";

static httpd_handle_t s_server;
static int s_ws_fd = -1;
static proto_parser_t s_parser;

/* ---- 活动链路管理(comm.h 公共部分放在本文件,避免单独小文件) ---- */
static comm_link_t s_active = COMM_LINK_NONE;

comm_link_t comm_active_link(void) { return s_active; }

void comm_on_frame(comm_link_t link, uint8_t type, const uint8_t *payload, uint16_t len)
{
    s_active = link;
    extern void app_state_apply_frame(uint8_t, const uint8_t *, uint16_t);
    app_state_apply_frame(type, payload, len);
}

void comm_send(uint8_t type, const uint8_t *payload, uint16_t len)
{
    extern void comm_twai_send(uint8_t, const uint8_t *, uint16_t);
    extern void comm_usb_send(uint8_t, const uint8_t *, uint16_t);
    switch (s_active) {
    case COMM_LINK_WIFI: {
        if (s_ws_fd < 0 || !s_server) return;
        uint8_t buf[PROTO_MAX_PAYLOAD + PROTO_OVERHEAD];
        size_t n = proto_build(buf, type, payload, len);
        httpd_ws_frame_t f = {
            .type = HTTPD_WS_TYPE_BINARY, .payload = buf, .len = n,
        };
        httpd_ws_send_frame_async(s_server, s_ws_fd, &f);
        break;
    }
    case COMM_LINK_CAN: comm_twai_send(type, payload, len); break;
    case COMM_LINK_USB: comm_usb_send(type, payload, len); break;
    default: break;
    }
}

/* ---- WebSocket handler ---- */
static void on_frame_cb(uint8_t type, uint8_t flags, const uint8_t *payload,
                        uint16_t len, void *ctx)
{
    (void)flags; (void)ctx;
    comm_on_frame(COMM_LINK_WIFI, type, payload, len);
}

static esp_err_t ws_handler(httpd_req_t *req)
{
    if (req->method == HTTP_GET) { /* 握手完成 */
        s_ws_fd = httpd_req_to_sockfd(req);
        proto_parser_reset(&s_parser);
        ESP_LOGI(TAG, "ws client connected fd=%d", s_ws_fd);
        return ESP_OK;
    }
    httpd_ws_frame_t f = { 0 };
    ESP_RETURN_ON_ERROR(httpd_ws_recv_frame(req, &f, 0), TAG, "ws len");
    if (f.len == 0 || f.len > PROTO_MAX_PAYLOAD + PROTO_OVERHEAD) return ESP_OK;
    uint8_t buf[PROTO_MAX_PAYLOAD + PROTO_OVERHEAD];
    f.payload = buf;
    ESP_RETURN_ON_ERROR(httpd_ws_recv_frame(req, &f, f.len), TAG, "ws recv");
    proto_parser_feed(&s_parser, buf, f.len, on_frame_cb, NULL);
    return ESP_OK;
}

static const httpd_uri_t ws_uri = {
    .uri = "/ws", .method = HTTP_GET, .handler = ws_handler, .is_websocket = true,
};

/* ---- UI Wi-Fi 页接口 ---- */
#include "ui/screens/screens.h"

void comm_wifi_scan_async(void)
{
    wifi_scan_config_t sc = { 0 };
    esp_wifi_scan_start(&sc, false); /* 完成事件中取结果 */
}

void comm_wifi_set_credentials(const char *ssid, const char *pass)
{
    wifi_config_t wc = { 0 };
    strlcpy((char *)wc.sta.ssid, ssid, sizeof(wc.sta.ssid));
    strlcpy((char *)wc.sta.password, pass, sizeof(wc.sta.password));
    esp_wifi_disconnect();
    esp_wifi_set_config(WIFI_IF_STA, &wc); /* 默认存 NVS,重启仍生效 */
    esp_wifi_connect();
    ui_wifi_set_status("connecting...");
}

/* ---- Wi-Fi STA ---- */
static void wifi_evt(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        s_ws_fd = -1;
        ui_wifi_set_status("disconnected");
        esp_wifi_connect(); /* 简单重连;量产加退避 */
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_SCAN_DONE) {
        uint16_t n = 10;
        wifi_ap_record_t recs[10];
        if (esp_wifi_scan_get_ap_records(&n, recs) == ESP_OK) {
            ui_wifi_clear_networks();
            for (int i = 0; i < n; i++)
                ui_wifi_add_network((const char *)recs[i].ssid, recs[i].rssi,
                                    recs[i].authmode != WIFI_AUTH_OPEN);
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *evt = (ip_event_got_ip_t *)data;
        char st[48];
        snprintf(st, sizeof(st), "connected " IPSTR, IP2STR(&evt->ip_info.ip));
        ui_wifi_set_status(st);
        ESP_LOGI(TAG, "got ip, ws server ready");
    }
}

esp_err_t comm_wifi_start(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_evt, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_evt, NULL);

    wifi_config_t wc = { 0 };
    strlcpy((char *)wc.sta.ssid, CONFIG_AGENTIND_WIFI_SSID, sizeof(wc.sta.ssid));
    strlcpy((char *)wc.sta.password, CONFIG_AGENTIND_WIFI_PASSWORD, sizeof(wc.sta.password));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wc));
    ESP_ERROR_CHECK(esp_wifi_start());

    httpd_config_t hc = HTTPD_DEFAULT_CONFIG();
    ESP_ERROR_CHECK(httpd_start(&s_server, &hc));
    ESP_ERROR_CHECK(httpd_register_uri_handler(s_server, &ws_uri));

    ESP_ERROR_CHECK(mdns_init());
    mdns_hostname_set("agentind");
    mdns_service_add("agent-indicator", "_agentind", "_tcp", 80, NULL, 0);
    return ESP_OK;
}
