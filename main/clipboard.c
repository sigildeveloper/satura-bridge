#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <inttypes.h>

#include "clipboard.h"
#include "http_utils.h"
#include "storage.h"

#define CLIP_NVS_KEY "clipboard"
#define PAGE_BUF     8192

typedef struct {
    char     text[CLIP_TEXT_MAX];
    bool     is_link;
    uint32_t id;
} clip_entry_t;

typedef struct {
    uint32_t     next_id;
    uint8_t      ring_count;
    clip_entry_t ring[CLIP_RING_MAX];
    uint8_t      fav_count;
    clip_entry_t favorites[CLIP_FAV_MAX];
} clip_store_t;

static clip_store_t g_store;
static bool g_loaded = false;

static void clip_save(void) {
    storage_set_blob(CLIP_NVS_KEY, &g_store, sizeof(g_store));
}

void clipboard_init(void) {
    if (g_loaded) return;
    memset(&g_store, 0, sizeof(g_store));
    g_store.next_id = 1;
    /* Ignore a size mismatch from an older/different build — starts
     * fresh rather than reading garbage into fixed-size text buffers. */
    storage_get_blob(CLIP_NVS_KEY, &g_store, sizeof(g_store));
    if (g_store.ring_count > CLIP_RING_MAX) g_store.ring_count = 0;
    if (g_store.fav_count > CLIP_FAV_MAX) g_store.fav_count = 0;
    if (g_store.next_id == 0) g_store.next_id = 1;
    g_loaded = true;
}

static bool looks_like_link(const char *text) {
    while (*text == ' ' || *text == '\t') text++;
    return strncasecmp(text, "http://", 7) == 0 ||
           strncasecmp(text, "https://", 8) == 0;
}

static void clip_add(const char *text) {
    if (!text[0]) return;

    if (g_store.ring_count == CLIP_RING_MAX) {
        /* Drop the oldest (index 0), shift the rest down. CLIP_RING_MAX
         * is small (15) and this only runs on a user-triggered add, so
         * an O(n) memmove here is not worth a circular index. */
        memmove(&g_store.ring[0], &g_store.ring[1],
                sizeof(clip_entry_t) * (CLIP_RING_MAX - 1));
        g_store.ring_count--;
    }

    clip_entry_t *e = &g_store.ring[g_store.ring_count++];
    strncpy(e->text, text, CLIP_TEXT_MAX - 1);
    e->text[CLIP_TEXT_MAX - 1] = '\0';
    e->is_link = looks_like_link(e->text);
    e->id = g_store.next_id++;

    clip_save();
}

/* Finds an entry by id in either array. Returns NULL if not found;
 * sets *out_is_fav to which array it came from. */
static clip_entry_t *clip_find(uint32_t id, bool *out_is_fav, int *out_idx) {
    for (int i = 0; i < g_store.ring_count; i++) {
        if (g_store.ring[i].id == id) {
            if (out_is_fav) *out_is_fav = false;
            if (out_idx) *out_idx = i;
            return &g_store.ring[i];
        }
    }
    for (int i = 0; i < g_store.fav_count; i++) {
        if (g_store.favorites[i].id == id) {
            if (out_is_fav) *out_is_fav = true;
            if (out_idx) *out_idx = i;
            return &g_store.favorites[i];
        }
    }
    return NULL;
}

static void clip_pin(uint32_t id) {
    if (g_store.fav_count >= CLIP_FAV_MAX) return; /* favorites full — silently no-op */
    bool is_fav; int idx;
    clip_entry_t *e = clip_find(id, &is_fav, &idx);
    if (!e || is_fav) return;

    g_store.favorites[g_store.fav_count++] = *e;
    memmove(&g_store.ring[idx], &g_store.ring[idx + 1],
            sizeof(clip_entry_t) * (g_store.ring_count - idx - 1));
    g_store.ring_count--;
    clip_save();
}

static void clip_unpin(uint32_t id) {
    bool is_fav; int idx;
    clip_entry_t *e = clip_find(id, &is_fav, &idx);
    if (!e || !is_fav) return;

    /* Goes back to the ring as the newest entry — if the ring is full,
     * this reuses the normal eviction path so the oldest ring entry
     * (not the just-unpinned one) is what gets dropped. */
    clip_entry_t unpinned = *e;
    memmove(&g_store.favorites[idx], &g_store.favorites[idx + 1],
            sizeof(clip_entry_t) * (g_store.fav_count - idx - 1));
    g_store.fav_count--;
    clip_add(unpinned.text); /* re-saves */
}

static void clip_delete(uint32_t id) {
    bool is_fav; int idx;
    clip_entry_t *e = clip_find(id, &is_fav, &idx);
    if (!e) return;

    if (is_fav) {
        memmove(&g_store.favorites[idx], &g_store.favorites[idx + 1],
                sizeof(clip_entry_t) * (g_store.fav_count - idx - 1));
        g_store.fav_count--;
    } else {
        memmove(&g_store.ring[idx], &g_store.ring[idx + 1],
                sizeof(clip_entry_t) * (g_store.ring_count - idx - 1));
        g_store.ring_count--;
    }
    clip_save();
}

/* ============================================================
 * HTTP handlers
 * ============================================================ */

static size_t render_entry(char *page, size_t len, size_t cap,
                            const clip_entry_t *e, bool is_fav) {
    char esc[CLIP_TEXT_MAX * 2];
    html_escape(e->text, esc, sizeof(esc));

    len += snprintf(page + len, cap - len,
        "<div style='background:%s;padding:8px;margin:6px 0;text-align:left;'>",
        is_fav ? "#fff3cd" : "#ecf0f1");

    if (e->is_link) {
        len += snprintf(page + len, cap - len,
            "<a href='%s' style='word-wrap:break-word;'>%s</a>", e->text, esc);
    } else {
        len += snprintf(page + len, cap - len,
            "<textarea rows='3' style='width:95%%;'>%s</textarea>", esc);
    }

    len += snprintf(page + len, cap - len,
        "<br><a href='/clip/action?op=%s&id=%" PRIu32 "'>[%s]</a>"
        " &nbsp; <a href='/clip/action?op=delete&id=%" PRIu32 "' style='color:#e74c3c;'>[delete]</a>"
        "</div>",
        is_fav ? "unpin" : "pin", e->id, is_fav ? "unpin" : "pin", e->id);

    return len;
}

esp_err_t handler_clip_get(httpd_req_t *req) {
    clipboard_init();
    set_no_cache(req, "text/html");

    char *page = malloc(PAGE_BUF);
    if (!page) return ESP_ERR_NO_MEM;
    size_t len = 0;

    len += snprintf(page + len, PAGE_BUF - len,
        "<html><head><title>Clipboard</title>"
        "<meta charset='utf-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<meta name='format-detection' content='telephone=no'>"
        "</head>"
        "<body style='font-family:sans-serif;padding:20px;text-align:center;'>"
        "<h2>Clipboard</h2>"
        "<p style='font-size:90%%;color:#555;'>Paste from a computer, read or "
        "tap on the phone. Text is kept as-is; anything starting with http(s)"
        "&#58;// becomes a clickable link.</p>"
        "<form action='/clip/add' method='post'>"
        "<textarea name='text' rows='4' style='width:95%%;' "
        "placeholder='Paste text or a link...'></textarea>"
        "<p><input type='submit' value='Add' style='font-size:110%%;'></p>"
        "</form><hr>");

    if (g_store.fav_count > 0) {
        len += snprintf(page + len, PAGE_BUF - len, "<h3>Favorites</h3>");
        for (int i = 0; i < g_store.fav_count && len < PAGE_BUF - 700; i++) {
            len = render_entry(page, len, PAGE_BUF, &g_store.favorites[i], true);
        }
        len += snprintf(page + len, PAGE_BUF - len, "<hr>");
    }

    len += snprintf(page + len, PAGE_BUF - len, "<h3>Recent (%d/%d)</h3>",
                     g_store.ring_count, CLIP_RING_MAX);
    if (g_store.ring_count == 0) {
        len += snprintf(page + len, PAGE_BUF - len, "<p><i>Nothing yet</i></p>");
    }
    /* Newest first. */
    for (int i = g_store.ring_count - 1; i >= 0 && len < PAGE_BUF - 700; i--) {
        len = render_entry(page, len, PAGE_BUF, &g_store.ring[i], false);
    }

    len += snprintf(page + len, PAGE_BUF - len,
        "<hr><a href='/'>Back</a></body></html>");

    esp_err_t r = httpd_resp_sendstr(req, page);
    free(page);
    return r;
}

esp_err_t handler_clip_add_post(httpd_req_t *req) {
    clipboard_init();

    /* Free-text paste, urlencoded by the form submit — allow generous
     * room for %XX-expansion of the CLIP_TEXT_MAX decoded result.
     * httpd_query_key_value() only extracts the raw substring between
     * '&'/'=' — it does NOT url-decode (same reason
     * handler_networks_connect_get calls url_decode() separately). */
    char *buf = malloc(900);
    if (!buf) return ESP_ERR_NO_MEM;
    int rec = httpd_req_recv(req, buf, 899);
    if (rec <= 0) { free(buf); return ESP_FAIL; }
    buf[rec] = '\0';

    char raw[900] = {0};
    if (httpd_query_key_value(buf, "text", raw, sizeof(raw)) == ESP_OK) {
        char text[CLIP_TEXT_MAX] = {0};
        url_decode(raw, text, sizeof(text));
        clip_add(text);
    }
    free(buf);

    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "/clip");
    return httpd_resp_send(req, NULL, 0);
}

esp_err_t handler_clip_action_get(httpd_req_t *req) {
    clipboard_init();

    char query[64] = {0};
    char op[16] = {0};
    char id_s[16] = {0};
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        httpd_query_key_value(query, "op", op, sizeof(op));
        httpd_query_key_value(query, "id", id_s, sizeof(id_s));
    }
    uint32_t id = (uint32_t)strtoul(id_s, NULL, 10);

    if (strcmp(op, "pin") == 0) clip_pin(id);
    else if (strcmp(op, "unpin") == 0) clip_unpin(id);
    else if (strcmp(op, "delete") == 0) clip_delete(id);

    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "/clip");
    return httpd_resp_send(req, NULL, 0);
}
