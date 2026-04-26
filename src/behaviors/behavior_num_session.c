/*
 * Copyright (c) 2026 Joao Maridalho
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_behavior_num_session

#include <errno.h>

#include <drivers/behavior.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <dt-bindings/zmk/hid_usage.h>
#include <dt-bindings/zmk/hid_usage_pages.h>
#include <zmk/behavior.h>
#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/keymap.h>

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

#define ZMK_BHV_NUM_SESSION_MAX_ACTIVE 10

struct active_num_session {
    bool is_active;
    bool is_modified;
    uint8_t layer;
};

static struct active_num_session active_num_sessions[ZMK_BHV_NUM_SESSION_MAX_ACTIVE];

static bool num_session_is_modifier(const struct zmk_keycode_state_changed *ev) {
    return ev->usage_page == HID_USAGE_KEY &&
           ev->keycode >= HID_USAGE_KEY_KEYBOARD_LEFTCONTROL &&
           ev->keycode <= HID_USAGE_KEY_KEYBOARD_RIGHT_GUI;
}

static bool num_session_is_numeric(const struct zmk_keycode_state_changed *ev) {
    if (ev->usage_page != HID_USAGE_KEY) {
        return false;
    }

    return ((ev->keycode >= HID_USAGE_KEY_KEYBOARD_1_AND_EXCLAMATION &&
             ev->keycode <= HID_USAGE_KEY_KEYBOARD_0_AND_RIGHT_PARENTHESIS) ||
            (ev->keycode >= HID_USAGE_KEY_KEYPAD_1_AND_END &&
             ev->keycode <= HID_USAGE_KEY_KEYPAD_0_AND_INSERT) ||
            ev->keycode == HID_USAGE_KEY_KEYPAD_00 || ev->keycode == HID_USAGE_KEY_KEYPAD_000);
}

static struct active_num_session *find_active_num_session(uint8_t layer) {
    for (int i = 0; i < ZMK_BHV_NUM_SESSION_MAX_ACTIVE; i++) {
        if (active_num_sessions[i].is_active && active_num_sessions[i].layer == layer) {
            return &active_num_sessions[i];
        }
    }

    return NULL;
}

static struct active_num_session *reserve_num_session(uint8_t layer) {
    struct active_num_session *num_session = find_active_num_session(layer);
    if (num_session != NULL) {
        return num_session;
    }

    for (int i = 0; i < ZMK_BHV_NUM_SESSION_MAX_ACTIVE; i++) {
        if (!active_num_sessions[i].is_active) {
            active_num_sessions[i].layer = layer;
            return &active_num_sessions[i];
        }
    }

    return NULL;
}

static void activate_num_session(struct active_num_session *num_session) {
    zmk_keymap_layer_activate(num_session->layer, false);
    num_session->is_active = true;
    num_session->is_modified = false;
}

static void deactivate_num_session(struct active_num_session *num_session) {
    zmk_keymap_layer_deactivate(num_session->layer, false);
    num_session->is_active = false;
    num_session->is_modified = false;
}

static int on_num_session_binding_pressed(struct zmk_behavior_binding *binding,
                                          struct zmk_behavior_binding_event event) {
    ARG_UNUSED(event);

    struct active_num_session *num_session = reserve_num_session(binding->param1);
    if (num_session == NULL) {
        LOG_WRN("No free num-session slot for layer %d", binding->param1);
        return -ENOMEM;
    }

    activate_num_session(num_session);
    return ZMK_BEHAVIOR_OPAQUE;
}

static int on_num_session_binding_released(struct zmk_behavior_binding *binding,
                                           struct zmk_behavior_binding_event event) {
    ARG_UNUSED(binding);
    ARG_UNUSED(event);
    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api num_session_driver_api = {
    .binding_pressed = on_num_session_binding_pressed,
    .binding_released = on_num_session_binding_released,
    .locality = BEHAVIOR_LOCALITY_CENTRAL,
};

static int num_session_keycode_state_changed_listener(const zmk_event_t *eh) {
    struct zmk_keycode_state_changed *ev = as_zmk_keycode_state_changed(eh);
    if (ev == NULL || !ev->state) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    for (int i = 0; i < ZMK_BHV_NUM_SESSION_MAX_ACTIVE; i++) {
        struct active_num_session *num_session = &active_num_sessions[i];
        if (!num_session->is_active) {
            continue;
        }

        if (num_session_is_modifier(ev)) {
            num_session->is_modified = true;
            continue;
        }

        if (num_session_is_numeric(ev) && !num_session->is_modified) {
            continue;
        }

        deactivate_num_session(num_session);
    }

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(behavior_num_session, num_session_keycode_state_changed_listener);
ZMK_SUBSCRIPTION(behavior_num_session, zmk_keycode_state_changed);

static int num_session_init(const struct device *dev) {
    ARG_UNUSED(dev);
    return 0;
}

#define NUM_SESSION_INST(n)                                                                          \
    BEHAVIOR_DT_INST_DEFINE(n, num_session_init, NULL, NULL, NULL, POST_KERNEL,                    \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &num_session_driver_api);

DT_INST_FOREACH_STATUS_OKAY(NUM_SESSION_INST)

#endif /* DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT) */
