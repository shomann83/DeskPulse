#include "ui.h"
#include "splash.h"
#include <lvgl.h>
#include "logo.h"
#include "icons.h"
#include "codex_icon.h"
#include "hal/board_caps.h"

// Custom fonts (scaled for 314 PPI, ~1.9x from original 165 PPI)
LV_FONT_DECLARE(font_tiempos_56);
LV_FONT_DECLARE(font_tiempos_34);
LV_FONT_DECLARE(font_styrene_48);
LV_FONT_DECLARE(font_styrene_28);
LV_FONT_DECLARE(font_styrene_24);
LV_FONT_DECLARE(font_styrene_20);
LV_FONT_DECLARE(font_styrene_16);
LV_FONT_DECLARE(font_styrene_14);
LV_FONT_DECLARE(font_styrene_12);
LV_FONT_DECLARE(font_mono_32);

// Layout values computed from the active board's geometry. Populated once
// in ui_init() and treated as const for the rest of the program. Adding a
// new display size means extending compute_layout() with another
// breakpoint — never editing the screen-builder functions below.
struct Layout {
    int16_t scr_w, scr_h;
    int16_t margin;
    int16_t title_y;
    int16_t content_y;
    int16_t content_w;

    // Usage screen — no title row, the two provider panels fill the screen.
    // The *_y values are offsets inside a panel's content area.
    int16_t usage_top_y;
    int16_t usage_panel_h;
    int16_t usage_panel_gap;
    int16_t usage_icon_size;
    int16_t usage_label_y;   // caption: "5h", "Week", "Credits"
    int16_t usage_value_y;   // big percentage / balance
    int16_t usage_bar_y;
    int16_t usage_bar_h;
    int16_t usage_reset_y;   // reset countdown / message estimate
    const lv_font_t* usage_name_font;
    const lv_font_t* usage_status_font;
    const lv_font_t* usage_pct_font;
    const lv_font_t* usage_label_font;
    const lv_font_t* usage_reset_font;

    // Bluetooth screen
    int16_t bt_info_panel_h;
    int16_t bt_reset_zone_h;
    const lv_font_t* bt_title_font;
    const lv_font_t* bt_status_font;
    const lv_font_t* bt_device_font;
    const lv_font_t* bt_credit_1_font;
    const lv_font_t* bt_credit_2_font;
};
static Layout L = {};

// Pick layout values from the active board's pixel dimensions. The two
// existing boards happen to land on the two breakpoints below; new ports
// inherit the closer one — visually OK, may need a polish pass for
// pixel-perfect alignment but never blocks the port from booting.
static void compute_layout(const BoardCaps& c) {
    L.scr_w = c.width;
    L.scr_h = c.height;
    L.margin = 20;
    L.title_y = 30;

    if (c.height >= 460) {
        // Large layout — tuned for 480x480 (AMOLED-2.16).
        L.content_y = 100;                // Bluetooth screen
        L.usage_top_y = 20;
        L.usage_panel_h = 212;            // (480 - 2*20 - 16) / 2
        L.usage_panel_gap = 16;
        L.usage_icon_size = 40;
        L.usage_label_y = 54;
        L.usage_value_y = 78;
        L.usage_bar_y = 128;
        L.usage_bar_h = 14;
        L.usage_reset_y = 152;
        L.usage_name_font = &font_styrene_28;
        L.usage_status_font = &font_styrene_20;
        L.usage_pct_font = &font_styrene_48;
        L.usage_label_font = &font_styrene_24;
        L.usage_reset_font = &font_styrene_24;
        L.bt_info_panel_h = 160;
        L.bt_reset_zone_h = 110;
        L.bt_title_font    = &font_tiempos_56;
        L.bt_status_font   = &font_styrene_48;
        L.bt_device_font   = &font_styrene_28;
        L.bt_credit_1_font = &font_styrene_24;
        L.bt_credit_2_font = &font_styrene_20;
    } else {
        // Compact layout — tuned for 368x448 (AMOLED-1.8).
        L.content_y = 85;                 // Bluetooth screen
        L.usage_top_y = 20;
        L.usage_panel_h = 198;            // (448 - 2*20 - 12) / 2
        L.usage_panel_gap = 12;
        L.usage_icon_size = 30;
        L.usage_label_y = 44;
        L.usage_value_y = 66;
        L.usage_bar_y = 102;
        L.usage_bar_h = 12;
        L.usage_reset_y = 122;
        L.usage_name_font = &font_styrene_24;
        L.usage_status_font = &font_styrene_16;
        L.usage_pct_font = &font_styrene_28;
        L.usage_label_font = &font_styrene_20;
        L.usage_reset_font = &font_styrene_20;
        L.bt_info_panel_h = 140;
        L.bt_reset_zone_h = 90;
        L.bt_title_font    = &font_tiempos_34;
        L.bt_status_font   = &font_styrene_28;
        L.bt_device_font   = &font_styrene_20;
        L.bt_credit_1_font = &font_styrene_16;
        L.bt_credit_2_font = &font_styrene_14;
    }

    L.content_w = L.scr_w - 2 * L.margin;
}

// Anthropic brand palette — design tokens live in theme.h
#include "theme.h"
#define COL_BG        THEME_BG
#define COL_PANEL     THEME_PANEL
#define COL_TEXT      THEME_TEXT
#define COL_DIM       THEME_DIM
#define COL_ACCENT    THEME_ACCENT
#define COL_GREEN     THEME_GREEN
#define COL_AMBER     THEME_AMBER
#define COL_RED       THEME_RED
#define COL_BAR_BG    THEME_BAR_BG

// ---- Usage screen widgets ----
static lv_obj_t* usage_dual_container;

// One metric slot: a caption, a big value, an optional bar and a sub-line.
// Which rate-limit window (or the credit balance) a slot shows is decided at
// update time, not at construction — providers gain and lose windows.
struct MetricWidgets {
    lv_obj_t* label;
    lv_obj_t* value;
    lv_obj_t* bar;
    lv_obj_t* sub;
    int bar_y;  // the sub-line moves up to here while the bar is hidden
    int sub_y;
    int sub_x;
};

struct ProviderUsageWidgets {
    lv_obj_t* panel;
    lv_obj_t* mark;
    lv_obj_t* name;
    lv_obj_t* status;
    MetricWidgets left;
    MetricWidgets right;
};
static ProviderUsageWidgets dual_widgets[USAGE_PROVIDER_COUNT];

// ---- Bluetooth screen widgets ----
static lv_obj_t* ble_container;
static lv_obj_t* lbl_ble_status;
static lv_obj_t* lbl_ble_device;
static lv_obj_t* lbl_ble_mac;

// ---- Header icon (Bluetooth screen only) ----
static lv_obj_t* header_icon_img;

// ---- Shared ----
static lv_image_dsc_t logo_dsc;
static lv_image_dsc_t codex_icon_dsc;
static lv_image_dsc_t bluetooth_icon_dsc;
static screen_t current_screen = SCREEN_USAGE;

static lv_color_t pct_color(float pct) {
    if (pct >= 80.0f) return COL_RED;
    if (pct >= 50.0f) return COL_AMBER;
    return COL_GREEN;
}

static void format_reset_short(int mins, char* buf, size_t len) {
    if (mins < 0) {
        snprintf(buf, len, "--");
    } else if (mins < 60) {
        snprintf(buf, len, "%dm", mins);
    } else if (mins < 1440) {
        snprintf(buf, len, "%dh %dm", mins / 60, mins % 60);
    } else {
        snprintf(buf, len, "%dd %dh", mins / 1440, (mins % 1440) / 60);
    }
}

static void set_header_icon(const lv_image_dsc_t* dsc) {
    if (!header_icon_img) return;
    if (!dsc) {
        lv_obj_add_flag(header_icon_img, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    const int slot = LOGO_WIDTH;
    lv_image_set_src(header_icon_img, dsc);
    lv_obj_set_pos(header_icon_img,
                   L.margin + (slot - (int)dsc->header.w) / 2,
                   L.title_y - 10 + (slot - (int)dsc->header.h) / 2);
    lv_obj_clear_flag(header_icon_img, LV_OBJ_FLAG_HIDDEN);
}

static const lv_image_dsc_t* header_icon_for_screen(screen_t screen) {
    switch (screen) {
    case SCREEN_BLUETOOTH:    return &bluetooth_icon_dsc;
    default:                  return nullptr;
    }
}

static const lv_image_dsc_t* provider_icon_for_usage(UsageProvider provider) {
    return provider == USAGE_PROVIDER_CODEX ? &codex_icon_dsc : &logo_dsc;
}

static int provider_icon_source_w(UsageProvider provider) {
    return provider == USAGE_PROVIDER_CODEX ? ICON_CODEX_W : LOGO_WIDTH;
}

// Forward decls — callbacks defined near ui_show_screen below
static void global_click_cb(lv_event_t* e);
static void ble_reset_click_cb(lv_event_t* e);

static lv_obj_t* make_panel(lv_obj_t* parent, int x, int y, int w, int h) {
    lv_obj_t* panel = lv_obj_create(parent);
    lv_obj_set_pos(panel, x, y);
    lv_obj_set_size(panel, w, h);
    lv_obj_set_style_bg_color(panel, COL_PANEL, 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(panel, 8, 0);
    lv_obj_set_style_border_width(panel, 0, 0);
    lv_obj_set_style_pad_left(panel, 16, 0);
    lv_obj_set_style_pad_right(panel, 16, 0);
    lv_obj_set_style_pad_top(panel, 12, 0);
    lv_obj_set_style_pad_bottom(panel, 12, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(panel, LV_OBJ_FLAG_EVENT_BUBBLE);
    return panel;
}

static lv_obj_t* make_bar(lv_obj_t* parent, int x, int y, int w, int h) {
    lv_obj_t* bar = lv_bar_create(parent);
    lv_obj_set_pos(bar, x, y);
    lv_obj_set_size(bar, w, h);
    lv_bar_set_range(bar, 0, 100);
    lv_bar_set_value(bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(bar, COL_BAR_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(bar, 6, LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar, COL_GREEN, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(bar, 6, LV_PART_INDICATOR);
    return bar;
}

static void init_icon_dsc(lv_image_dsc_t* dsc, int w, int h, const uint16_t* data) {
    dsc->header.w = w;
    dsc->header.h = h;
    dsc->header.cf = LV_COLOR_FORMAT_RGB565;
    dsc->header.stride = w * 2;
    dsc->data = (const uint8_t*)data;
    dsc->data_size = w * h * 2;
}

static void init_icon_dsc_rgb565a8(lv_image_dsc_t* dsc, int w, int h, const uint8_t* data) {
    dsc->header.w = w;
    dsc->header.h = h;
    dsc->header.cf = LV_COLOR_FORMAT_RGB565A8;
    dsc->header.stride = w * 2;
    dsc->data = data;
    dsc->data_size = w * h * 3;
}

static void init_image_descriptors(void) {
    init_icon_dsc_rgb565a8(&logo_dsc, LOGO_WIDTH, LOGO_HEIGHT, logo_data);
    init_icon_dsc_rgb565a8(&codex_icon_dsc, ICON_CODEX_W, ICON_CODEX_H, icon_codex_data);
    init_icon_dsc(&bluetooth_icon_dsc, ICON_BLUETOOTH_W, ICON_BLUETOOTH_H, icon_bluetooth_data);
}

// ======== Usage Screen ========

static void style_metric_label(lv_obj_t* lbl, const char* text) {
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_font(lbl, L.usage_label_font, 0);
    lv_obj_set_style_text_color(lbl, COL_DIM, 0);
}

static void make_provider_mark(lv_obj_t* panel, ProviderUsageWidgets* widgets,
                               UsageProvider provider) {
    lv_obj_t* mark = lv_obj_create(panel);
    lv_obj_set_size(mark, L.usage_icon_size, L.usage_icon_size);
    lv_obj_set_pos(mark, 0, 0);
    lv_obj_set_style_bg_opa(mark, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(mark, 0, 0);
    lv_obj_set_style_pad_all(mark, 0, 0);
    lv_obj_clear_flag(mark, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* img = lv_image_create(mark);
    lv_image_set_src(img, provider_icon_for_usage(provider));
    lv_image_set_scale(img, (uint32_t)L.usage_icon_size * 256 /
                            provider_icon_source_w(provider));
    lv_obj_center(img);
    widgets->mark = mark;
}

// One column of the two-provider overview panel.
static void make_dual_metric_column(lv_obj_t* panel, MetricWidgets* out,
                                    int x, int col_w) {
    out->label = lv_label_create(panel);
    style_metric_label(out->label, "--");
    lv_obj_set_pos(out->label, x, L.usage_label_y);

    out->value = lv_label_create(panel);
    lv_label_set_text(out->value, "--%");
    lv_obj_set_style_text_font(out->value, L.usage_pct_font, 0);
    lv_obj_set_style_text_color(out->value, COL_TEXT, 0);
    lv_obj_set_pos(out->value, x, L.usage_value_y);

    out->bar = make_bar(panel, x, L.usage_bar_y, col_w, L.usage_bar_h);

    out->sub = lv_label_create(panel);
    lv_label_set_text(out->sub, "--");
    lv_obj_set_style_text_font(out->sub, L.usage_reset_font, 0);
    lv_obj_set_style_text_color(out->sub, COL_DIM, 0);
    lv_obj_set_pos(out->sub, x, L.usage_reset_y);

    out->bar_y = L.usage_bar_y;
    out->sub_y = L.usage_reset_y;
    out->sub_x = x;
}

static void make_provider_usage_panel(lv_obj_t* parent, ProviderUsageWidgets* widgets,
                                      UsageProvider provider, int y, int h,
                                      const char* name) {
    widgets->panel = make_panel(parent, L.margin, y, L.content_w, h);
    const int inner_w = L.content_w - 32;

    make_provider_mark(widgets->panel, widgets, provider);

    widgets->name = lv_label_create(widgets->panel);
    lv_label_set_text(widgets->name, name);
    lv_obj_set_style_text_font(widgets->name, L.usage_name_font, 0);
    lv_obj_set_style_text_color(widgets->name, COL_TEXT, 0);
    lv_obj_set_pos(widgets->name, L.usage_icon_size + 12,
                   (L.usage_icon_size - lv_font_get_line_height(L.usage_name_font)) / 2);

    widgets->status = lv_label_create(widgets->panel);
    lv_label_set_text(widgets->status, "waiting");
    lv_obj_set_style_text_font(widgets->status, L.usage_status_font, 0);
    lv_obj_set_style_text_color(widgets->status, COL_DIM, 0);
    lv_obj_align(widgets->status, LV_ALIGN_TOP_RIGHT, 0,
                 (L.usage_icon_size - lv_font_get_line_height(L.usage_status_font)) / 2);

    const int gap = 14;
    const int col_w = (inner_w - gap) / 2;
    const int col2 = col_w + gap;

    make_dual_metric_column(widgets->panel, &widgets->left, 0, col_w);
    make_dual_metric_column(widgets->panel, &widgets->right, col2, col_w);
}

static lv_obj_t* make_usage_root(lv_obj_t* scr) {
    lv_obj_t* root = lv_obj_create(scr);
    lv_obj_set_size(root, L.scr_w, L.scr_h);
    lv_obj_set_pos(root, 0, 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(root, 0, 0);
    lv_obj_set_style_pad_all(root, 0, 0);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(root, global_click_cb, LV_EVENT_CLICKED, NULL);
    return root;
}

// Single screen: Claude on top, ChatGPT below, filling the whole panel. No
// title, battery, animation line or per-provider detail screens — the device
// runs on USB power and stays on this view.
static void init_usage_screen(lv_obj_t* scr) {
    usage_dual_container = make_usage_root(scr);
    make_provider_usage_panel(usage_dual_container, &dual_widgets[USAGE_PROVIDER_CLAUDE],
                              USAGE_PROVIDER_CLAUDE, L.usage_top_y,
                              L.usage_panel_h, "Claude");
    make_provider_usage_panel(usage_dual_container, &dual_widgets[USAGE_PROVIDER_CODEX],
                              USAGE_PROVIDER_CODEX,
                              L.usage_top_y + L.usage_panel_h + L.usage_panel_gap,
                              L.usage_panel_h, "ChatGPT");
}

// ======== Bluetooth Screen ========

static void init_bluetooth_screen(lv_obj_t* scr) {
    ble_container = lv_obj_create(scr);
    lv_obj_set_size(ble_container, L.scr_w, L.scr_h);
    lv_obj_set_pos(ble_container, 0, 0);
    lv_obj_set_style_bg_opa(ble_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ble_container, 0, 0);
    lv_obj_set_style_pad_all(ble_container, 0, 0);
    lv_obj_clear_flag(ble_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(ble_container, global_click_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t* lbl_ble_title = lv_label_create(ble_container);
    lv_label_set_text(lbl_ble_title, "Bluetooth");
    lv_obj_set_style_text_font(lbl_ble_title, L.bt_title_font, 0);
    lv_obj_set_style_text_color(lbl_ble_title, COL_TEXT, 0);
    lv_obj_align(lbl_ble_title, LV_ALIGN_TOP_MID, 16, L.title_y);

    lv_obj_t* p_info = make_panel(ble_container, L.margin, L.content_y,
                                  L.content_w, L.bt_info_panel_h);

    lbl_ble_status = lv_label_create(p_info);
    lv_label_set_text(lbl_ble_status, "Initializing...");
    lv_obj_set_style_text_font(lbl_ble_status, L.bt_status_font, 0);
    lv_obj_set_style_text_color(lbl_ble_status, COL_DIM, 0);
    lv_obj_set_pos(lbl_ble_status, 0, 2);

    lbl_ble_device = lv_label_create(p_info);
    lv_label_set_text(lbl_ble_device, "Device: ---");
    lv_obj_set_style_text_font(lbl_ble_device, L.bt_device_font, 0);
    lv_obj_set_style_text_color(lbl_ble_device, COL_DIM, 0);
    lv_obj_set_pos(lbl_ble_device, 0, 64);

    lbl_ble_mac = lv_label_create(p_info);
    lv_label_set_text(lbl_ble_mac, "Address: ---");
    lv_obj_set_style_text_font(lbl_ble_mac, L.bt_device_font, 0);
    lv_obj_set_style_text_color(lbl_ble_mac, COL_DIM, 0);
    lv_obj_set_pos(lbl_ble_mac, 0, 100);

    int reset_y = L.content_y + L.bt_info_panel_h + 16;
    lv_obj_t* reset_zone = lv_obj_create(ble_container);
    lv_obj_set_pos(reset_zone, L.margin, reset_y);
    lv_obj_set_size(reset_zone, L.content_w, L.bt_reset_zone_h);
    lv_obj_set_style_bg_color(reset_zone, COL_PANEL, 0);
    lv_obj_set_style_bg_opa(reset_zone, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(reset_zone, 8, 0);
    lv_obj_set_style_border_width(reset_zone, 0, 0);
    lv_obj_set_style_pad_column(reset_zone, 14, 0);
    lv_obj_set_flex_flow(reset_zone, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(reset_zone, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(reset_zone, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(reset_zone, ble_reset_click_cb, LV_EVENT_CLICKED, NULL);

    static lv_image_dsc_t icon_trash_dsc;
    init_icon_dsc(&icon_trash_dsc, ICON_TRASH2_W, ICON_TRASH2_H, icon_trash2_data);
    lv_obj_t* trash_img = lv_image_create(reset_zone);
    lv_image_set_src(trash_img, &icon_trash_dsc);

    lv_obj_t* reset_lbl = lv_label_create(reset_zone);
    lv_label_set_text(reset_lbl, "Reset Bluetooth");
    lv_obj_set_style_text_font(reset_lbl, L.bt_device_font, 0);
    lv_obj_set_style_text_color(reset_lbl, COL_DIM, 0);

    lv_obj_t* lbl_credit = lv_label_create(ble_container);
    lv_label_set_text(lbl_credit, "Built by @hermannbjorgvin");
    lv_obj_set_style_text_font(lbl_credit, L.bt_credit_1_font, 0);
    lv_obj_set_style_text_color(lbl_credit, COL_DIM, 0);
    lv_obj_align(lbl_credit, LV_ALIGN_BOTTOM_MID, 0, -46);

    lv_obj_t* lbl_credit2 = lv_label_create(ble_container);
    lv_label_set_text(lbl_credit2, "Clawd animation by @amaanbuilds");
    lv_obj_set_style_text_font(lbl_credit2, L.bt_credit_2_font, 0);
    lv_obj_set_style_text_color(lbl_credit2, COL_DIM, 0);
    lv_obj_align(lbl_credit2, LV_ALIGN_BOTTOM_MID, 0, -20);

    lv_obj_add_flag(ble_container, LV_OBJ_FLAG_HIDDEN);
}

// ======== Public API ========

void ui_init(void) {
    compute_layout(board_caps());

    lv_obj_t* scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, COL_BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    init_image_descriptors();

    init_usage_screen(scr);
    init_bluetooth_screen(scr);
    splash_init(scr);

    if (splash_get_root()) {
        lv_obj_add_event_cb(splash_get_root(), global_click_cb, LV_EVENT_CLICKED, NULL);
    }

    header_icon_img = lv_image_create(scr);
    lv_image_set_src(header_icon_img, &logo_dsc);
    lv_obj_set_pos(header_icon_img, L.margin, L.title_y - 10);
}

static void set_usage_bar(lv_obj_t* bar, int pct, lv_color_t color) {
    lv_bar_set_value(bar, pct, LV_ANIM_ON);
    lv_obj_set_style_bg_color(bar, color, LV_PART_INDICATOR);
}

// Name a window by its length, so a provider changing its limits relabels the
// device instead of lying: 5h stays "5h", a 7-day window reads "Week".
static void format_window_name(int window_s, char* buf, size_t len) {
    if (window_s <= 0)             snprintf(buf, len, "--");
    else if (window_s == 7 * 86400) snprintf(buf, len, "Week");
    else if (window_s >= 86400)     snprintf(buf, len, "%dd", window_s / 86400);
    else if (window_s >= 3600)      snprintf(buf, len, "%dh", window_s / 3600);
    else                            snprintf(buf, len, "%dm", window_s / 60);
}

static void show_metric_bar(MetricWidgets* m, bool visible) {
    if (visible) lv_obj_clear_flag(m->bar, LV_OBJ_FLAG_HIDDEN);
    else         lv_obj_add_flag(m->bar, LV_OBJ_FLAG_HIDDEN);
    // Without a bar the sub-line would sit below a hole, so close the gap.
    lv_obj_set_pos(m->sub, m->sub_x, visible ? m->sub_y : m->bar_y);
}

static void clear_metric(MetricWidgets* m) {
    lv_label_set_text(m->label, "--");
    lv_label_set_text(m->value, "--%");
    lv_label_set_text(m->sub, "--");
    show_metric_bar(m, true);
    set_usage_bar(m->bar, 0, COL_DIM);
}

static void set_metric_window(MetricWidgets* m, int window_s, float pct,
                              int reset_mins) {
    char name[12];
    char reset[24];
    format_window_name(window_s, name, sizeof(name));
    format_reset_short(reset_mins, reset, sizeof(reset));

    const int rounded = (int)(pct + 0.5f);
    lv_label_set_text(m->label, name);
    lv_label_set_text_fmt(m->value, "%d%%", rounded);
    show_metric_bar(m, true);
    set_usage_bar(m->bar, rounded, pct_color(pct));
    lv_label_set_text(m->sub, reset);
}

// Prepaid credits have no ceiling, so this slot shows a balance and no bar.
static void set_metric_credits(MetricWidgets* m, const ProviderUsageData* usage) {
    lv_label_set_text(m->label, "Credits");
    // The daemon already rounds to two decimals; cut here too so an older
    // daemon can't push a long balance into the neighbouring column.
    char balance[sizeof(usage->credits_balance)];
    snprintf(balance, sizeof(balance), "%s",
             usage->credits_balance[0] ? usage->credits_balance : "0");
    char* dot = strchr(balance, '.');
    if (dot && strlen(dot) > 3) dot[3] = '\0';
    lv_label_set_text(m->value, balance);
    show_metric_bar(m, false);
    if (usage->credits_has) lv_label_set_text_fmt(m->sub, "~%d msg", usage->credits_msgs);
    else                    lv_label_set_text(m->sub, "none");
}

// Fill a provider's two slots. The weekly window always has one; the other slot
// holds the short window while the provider has one and falls back to credits
// when it doesn't — ChatGPT lost its 5h window in July 2026.
static void update_metric_slots(MetricWidgets* spare, MetricWidgets* weekly,
                                const ProviderUsageData* usage) {
    if (usage->session_window_s > 0) {
        set_metric_window(spare, usage->session_window_s, usage->session_pct,
                          usage->session_reset_mins);
    } else if (usage->credits_valid) {
        set_metric_credits(spare, usage);
    } else {
        clear_metric(spare);
    }
    set_metric_window(weekly, usage->weekly_window_s, usage->weekly_pct,
                      usage->weekly_reset_mins);
}

static void update_provider_usage_widgets(ProviderUsageWidgets* dual,
                                          const ProviderUsageData* usage) {
    if (!usage->valid) {
        lv_label_set_text(dual->status, "waiting");
        lv_obj_set_style_text_color(dual->status, COL_DIM, 0);
        clear_metric(&dual->left);
        clear_metric(&dual->right);
        return;
    }

    // The spare slot keeps the left column so both providers line up.
    update_metric_slots(&dual->left, &dual->right, usage);
    lv_label_set_text(dual->status, usage->ok ? usage->status : "error");
    lv_obj_set_style_text_color(dual->status, usage->ok ? COL_GREEN : COL_RED, 0);
}

void ui_update(const UsageData* data) {
    if (!data->valid) return;

    for (int i = 0; i < USAGE_PROVIDER_COUNT; i++) {
        update_provider_usage_widgets(&dual_widgets[i], &data->providers[i]);
    }
}

// Usage is the only regular screen. Splash and Bluetooth are only reachable
// via serial commands; a tap there returns to Usage, a tap on Usage does nothing.
static void global_click_cb(lv_event_t* e) {
    (void)e;
    if (current_screen != SCREEN_USAGE) ui_show_screen(SCREEN_USAGE);
}

static void ble_reset_click_cb(lv_event_t* e) {
    (void)e;
    ble_clear_bonds();
}

static void hide_screen_roots(void) {
    lv_obj_add_flag(usage_dual_container, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ble_container, LV_OBJ_FLAG_HIDDEN);
    splash_hide();
}

static void show_screen_root(screen_t screen) {
    switch (screen) {
    case SCREEN_SPLASH:       splash_show(); break;
    case SCREEN_USAGE:        lv_obj_clear_flag(usage_dual_container, LV_OBJ_FLAG_HIDDEN); break;
    case SCREEN_BLUETOOTH:    lv_obj_clear_flag(ble_container, LV_OBJ_FLAG_HIDDEN); break;
    default: break;
    }
}

void ui_show_screen(screen_t screen) {
    hide_screen_roots();
    show_screen_root(screen);
    set_header_icon(header_icon_for_screen(screen));
    current_screen = screen;
}

screen_t ui_get_current_screen(void) {
    return current_screen;
}

void ui_update_ble_status(ble_state_t state, const char* name, const char* mac) {
    switch (state) {
    case BLE_STATE_CONNECTED:
        lv_label_set_text(lbl_ble_status, "Connected");
        lv_obj_set_style_text_color(lbl_ble_status, COL_GREEN, 0);
        break;
    case BLE_STATE_ADVERTISING:
        lv_label_set_text(lbl_ble_status, "Advertising...");
        lv_obj_set_style_text_color(lbl_ble_status, COL_AMBER, 0);
        break;
    case BLE_STATE_DISCONNECTED:
        lv_label_set_text(lbl_ble_status, "Disconnected");
        lv_obj_set_style_text_color(lbl_ble_status, COL_RED, 0);
        break;
    default:
        lv_label_set_text(lbl_ble_status, "Initializing...");
        lv_obj_set_style_text_color(lbl_ble_status, COL_DIM, 0);
        break;
    }

    if (name) {
        static char nbuf[48];
        snprintf(nbuf, sizeof(nbuf), "Device: %s", name);
        lv_label_set_text(lbl_ble_device, nbuf);
    }
    if (mac) {
        static char mbuf[48];
        snprintf(mbuf, sizeof(mbuf), "Address: %s", mac);
        lv_label_set_text(lbl_ble_mac, mbuf);
    }
}
