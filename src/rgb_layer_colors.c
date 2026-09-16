#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>
#include <zephyr/logging/log.h>
#include <zephyr/devicetree.h>
#include <zephyr/device.h>

#include <zmk/event_manager.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/keymap.h>
#include <zmk/behavior.h>
#include <zmk/behavior_queue.h>
#include <dt-bindings/zmk/rgb.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define RGB_EFFECT_SOLID 0
#define RGB_EFFECT_SWIRL 3

struct layer_color {
    uint8_t layer;
    uint32_t hsb_value;
    uint8_t effect;
};

#define LAYER_COLOR(_layer, _h, _s, _b)                                                          \
    { .layer = _layer, .hsb_value = RGB_COLOR_HSB_VAL(_h, _s, _b), .effect = RGB_EFFECT_SOLID }

#define LAYER_COLOR_EFFECT(_layer, _h, _s, _b, _effect)                                          \
    { .layer = _layer, .hsb_value = RGB_COLOR_HSB_VAL(_h, _s, _b), .effect = _effect }

static const struct layer_color layer_colors[] = {
    LAYER_COLOR(11, 0, 0, 65),
    LAYER_COLOR(7, 30, 85, 65),
    LAYER_COLOR(6, 310, 80, 65),
    LAYER_COLOR(5, 270, 75, 65),
    LAYER_COLOR(4, 50, 90, 65),
    LAYER_COLOR(3, 165, 80, 65),
    LAYER_COLOR_EFFECT(2, 0, 90, 65, RGB_EFFECT_SWIRL),
    LAYER_COLOR(1, 120, 75, 65),
    LAYER_COLOR(0, 220, 75, 65),
};

static void imprint_rgb_queue_action(struct zmk_behavior_binding_event *event, uint32_t param1,
                                     uint32_t param2) {
    struct zmk_behavior_binding binding = {
        .behavior_dev = DEVICE_DT_NAME(DT_NODELABEL(rgb_ug)),
        .param1 = param1,
        .param2 = param2,
    };

    zmk_behavior_queue_add(event, binding, true, 5);
    zmk_behavior_queue_add(event, binding, false, 5);
}

static void imprint_rgb_apply_active_layer_color(void) {
    for (int i = ZMK_KEYMAP_LAYERS_LEN - 1; i >= 0; i--) {
        if (!zmk_keymap_layer_active(i)) {
            continue;
        }

        for (int j = 0; j < ARRAY_SIZE(layer_colors); j++) {
            if (layer_colors[j].layer != i) {
                continue;
            }

            struct zmk_behavior_binding_event event = {
                .position = INT32_MAX,
                .timestamp = k_uptime_get(),
#if IS_ENABLED(CONFIG_ZMK_SPLIT)
                .source = ZMK_POSITION_STATE_CHANGE_SOURCE_LOCAL,
#endif
            };

            imprint_rgb_queue_action(&event, RGB_COLOR_HSB_CMD, layer_colors[j].hsb_value);
            imprint_rgb_queue_action(&event, RGB_EFS_CMD, layer_colors[j].effect);
            return;
        }
    }
}

static int imprint_rgb_layer_color_listener(const zmk_event_t *eh) {
    imprint_rgb_apply_active_layer_color();
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(imprint_rgb_layer_colors, imprint_rgb_layer_color_listener);
ZMK_SUBSCRIPTION(imprint_rgb_layer_colors, zmk_layer_state_changed);
