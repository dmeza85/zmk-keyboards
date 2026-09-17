#define DT_DRV_COMPAT zmk_behavior_rgb_idle_timeout

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>
#include <drivers/behavior.h>

#include <zmk/event_manager.h>
#include <zmk/events/position_state_changed.h>
#include <zmk/events/sensor_event.h>
#include <zmk/behavior.h>
#include <zmk/behavior_queue.h>
#include <dt-bindings/zmk/rgb.h>
#include <dt-bindings/zmk/rgb_idle_timeout.h>

#if IS_ENABLED(CONFIG_ZMK_POINTING)
#include <zephyr/input/input.h>
#endif

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define IMPRINT_RGB_IDLE_MIN_MS (1 * 60 * 1000)
#define IMPRINT_RGB_IDLE_MAX_MS (30 * 60 * 1000)
#define IMPRINT_RGB_IDLE_STEP_MS (1 * 60 * 1000)
#define IMPRINT_RGB_IDLE_DEFAULT_MS (5 * 60 * 1000)

#define IS_CENTRAL (!IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL))

static uint32_t idle_timeout_ms = IMPRINT_RGB_IDLE_DEFAULT_MS;

#if IS_CENTRAL

static bool rgb_off_for_idle;
static bool idle_work_ready;
static struct k_work_delayable idle_off_work;

static void imprint_rgb_queue_cmd(uint32_t param1, uint32_t param2) {
    struct zmk_behavior_binding binding = {
        .behavior_dev = DEVICE_DT_NAME(DT_NODELABEL(rgb_ug)),
        .param1 = param1,
        .param2 = param2,
    };
    struct zmk_behavior_binding_event event = {
        .position = INT32_MAX,
        .timestamp = k_uptime_get(),
#if IS_ENABLED(CONFIG_ZMK_SPLIT)
        .source = ZMK_POSITION_STATE_CHANGE_SOURCE_LOCAL,
#endif
    };

    zmk_behavior_queue_add(&event, binding, true, 5);
    zmk_behavior_queue_add(&event, binding, false, 5);
}

static void imprint_rgb_idle_off_handler(struct k_work *work) {
    rgb_off_for_idle = true;
    imprint_rgb_queue_cmd(RGB_OFF_CMD, 0);
}

static void imprint_rgb_idle_ensure_ready(void) {
    if (idle_work_ready) {
        return;
    }

    idle_work_ready = true;
    k_work_init_delayable(&idle_off_work, imprint_rgb_idle_off_handler);
}

static void imprint_rgb_idle_rearm(void) {
    imprint_rgb_idle_ensure_ready();

    if (rgb_off_for_idle) {
        rgb_off_for_idle = false;
        imprint_rgb_queue_cmd(RGB_ON_CMD, 0);
    }

    k_work_reschedule(&idle_off_work, K_MSEC(idle_timeout_ms));
}

static int imprint_rgb_idle_activity_listener(const zmk_event_t *eh) {
    imprint_rgb_idle_rearm();
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(imprint_rgb_idle_timeout, imprint_rgb_idle_activity_listener);
ZMK_SUBSCRIPTION(imprint_rgb_idle_timeout, zmk_position_state_changed);
ZMK_SUBSCRIPTION(imprint_rgb_idle_timeout, zmk_sensor_event);

#if IS_ENABLED(CONFIG_ZMK_POINTING)
static void imprint_rgb_idle_input_cb(struct input_event *ev, void *user_data) {
    imprint_rgb_idle_rearm();
}

INPUT_CALLBACK_DEFINE(NULL, imprint_rgb_idle_input_cb, NULL);
#endif

#endif /* IS_CENTRAL */

#if IS_ENABLED(CONFIG_SETTINGS)

static int imprint_rgb_idle_settings_set(const char *name, size_t len, settings_read_cb read_cb,
                                         void *cb_arg) {
    const char *next;

    if (settings_name_steq(name, "ms", &next) && !next) {
        if (len != sizeof(idle_timeout_ms)) {
            return -EINVAL;
        }

        int rc = read_cb(cb_arg, &idle_timeout_ms, sizeof(idle_timeout_ms));
        if (rc < 0) {
            return rc;
        }

        if (idle_timeout_ms < IMPRINT_RGB_IDLE_MIN_MS) {
            idle_timeout_ms = IMPRINT_RGB_IDLE_MIN_MS;
        } else if (idle_timeout_ms > IMPRINT_RGB_IDLE_MAX_MS) {
            idle_timeout_ms = IMPRINT_RGB_IDLE_MAX_MS;
        }

        return 0;
    }

    return -ENOENT;
}

SETTINGS_STATIC_HANDLER_DEFINE(imprint_rgb_idle, "imprint/rgb_idle", NULL,
                               imprint_rgb_idle_settings_set, NULL, NULL);

static void imprint_rgb_idle_save_work_handler(struct k_work *work) {
    settings_save_one("imprint/rgb_idle/ms", &idle_timeout_ms, sizeof(idle_timeout_ms));
}

static struct k_work_delayable idle_save_work;
static bool idle_save_work_ready;

static void imprint_rgb_idle_save_debounced(void) {
    if (!idle_save_work_ready) {
        idle_save_work_ready = true;
        k_work_init_delayable(&idle_save_work, imprint_rgb_idle_save_work_handler);
    }

    k_work_reschedule(&idle_save_work, K_MSEC(CONFIG_ZMK_SETTINGS_SAVE_DEBOUNCE));
}

#endif /* IS_ENABLED(CONFIG_SETTINGS) */

static void imprint_rgb_idle_timeout_adjust(int8_t direction) {
    int32_t next = (int32_t)idle_timeout_ms +
                  (direction > 0 ? IMPRINT_RGB_IDLE_STEP_MS : -IMPRINT_RGB_IDLE_STEP_MS);

    if (next < IMPRINT_RGB_IDLE_MIN_MS) {
        next = IMPRINT_RGB_IDLE_MIN_MS;
    } else if (next > IMPRINT_RGB_IDLE_MAX_MS) {
        next = IMPRINT_RGB_IDLE_MAX_MS;
    }

    idle_timeout_ms = (uint32_t)next;
    LOG_INF("RGB idle timeout adjusted to %u ms", idle_timeout_ms);

#if IS_CENTRAL
    imprint_rgb_idle_rearm();
#endif

#if IS_ENABLED(CONFIG_SETTINGS)
    imprint_rgb_idle_save_debounced();
#endif
}

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
    switch (binding->param1) {
    case RGB_IDLE_TIMEOUT_INC_CMD:
        imprint_rgb_idle_timeout_adjust(1);
        return 0;
    case RGB_IDLE_TIMEOUT_DEC_CMD:
        imprint_rgb_idle_timeout_adjust(-1);
        return 0;
    }

    return -ENOTSUP;
}

static int on_keymap_binding_released(struct zmk_behavior_binding *binding,
                                      struct zmk_behavior_binding_event event) {
    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api behavior_rgb_idle_timeout_driver_api = {
    .binding_pressed = on_keymap_binding_pressed,
    .binding_released = on_keymap_binding_released,
    .locality = BEHAVIOR_LOCALITY_CENTRAL,
};

BEHAVIOR_DT_INST_DEFINE(0, NULL, NULL, NULL, NULL, POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,
                        &behavior_rgb_idle_timeout_driver_api);

#endif /* DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT) */
