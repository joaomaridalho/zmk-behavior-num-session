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
#include <zmk/hid.h>
#include <zmk/keys.h>
#include <zmk/keymap.h>

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

#ifndef ZMK_KEY_PARAM_DECODE
struct zmk_key_param {
    zmk_mod_flags_t modifiers;
    uint8_t page;
    uint16_t id;
};

#define ZMK_KEY_PARAM_DECODE(param)                                                                \
    (struct zmk_key_param) {                                                                       \
        .modifiers = SELECT_MODS(param), .page = ZMK_HID_USAGE_PAGE(param),                        \
        .id = ZMK_HID_USAGE_ID(param),                                                             \
    }
#endif

struct key_list {
    size_t size;
    struct zmk_key_param keys[];
};

struct behavior_num_session_config {
    const struct key_list *continue_keys;
    bool ignore_alphas;
    bool ignore_numbers;
};

struct active_num_session {
    bool is_active;
    bool is_modified;
    uint8_t layer;
    const struct behavior_num_session_config *config;
};

static struct active_num_session active_num_sessions[CONFIG_ZMK_BEHAVIOR_NUM_SESSION_MAX_ACTIVE];

static bool num_session_is_alpha(uint16_t usage_page, zmk_key_t usage_id) {
    if (usage_page != HID_USAGE_KEY) {
        return false;
    }

    return usage_id >= HID_USAGE_KEY_KEYBOARD_A && usage_id <= HID_USAGE_KEY_KEYBOARD_Z;
}

static bool num_session_is_numeric(uint16_t usage_page, zmk_key_t usage_id) {
    if (usage_page != HID_USAGE_KEY) {
        return false;
    }

    return ((usage_id >= HID_USAGE_KEY_KEYBOARD_1_AND_EXCLAMATION &&
             usage_id <= HID_USAGE_KEY_KEYBOARD_0_AND_RIGHT_PARENTHESIS) ||
            (usage_id >= HID_USAGE_KEY_KEYPAD_1_AND_END &&
             usage_id <= HID_USAGE_KEY_KEYPAD_0_AND_INSERT) ||
            usage_id == HID_USAGE_KEY_KEYPAD_00 || usage_id == HID_USAGE_KEY_KEYPAD_000);
}

static bool key_list_contains(const struct key_list *list, uint16_t usage_page, zmk_key_t usage_id,
                              zmk_mod_flags_t modifiers) {
    if (list == NULL) {
        return false;
    }

    for (int i = 0; i < list->size; i++) {
        const struct zmk_key_param *key = &list->keys[i];

        if (key->page == usage_page && key->id == usage_id &&
            (key->modifiers & modifiers) == key->modifiers) {
            return true;
        }
    }

    return false;
}

static bool num_session_should_continue(const struct behavior_num_session_config *config,
                                        const struct zmk_keycode_state_changed *ev) {
    if (config == NULL) {
        return false;
    }

    if (config->ignore_alphas && num_session_is_alpha(ev->usage_page, ev->keycode)) {
        return true;
    }

    if (config->ignore_numbers && num_session_is_numeric(ev->usage_page, ev->keycode)) {
        return true;
    }

    zmk_mod_flags_t modifiers = ev->implicit_modifiers | zmk_hid_get_explicit_mods();

    return key_list_contains(config->continue_keys, ev->usage_page, ev->keycode, modifiers);
}

static struct active_num_session *find_active_num_session(uint8_t layer) {
    for (int i = 0; i < CONFIG_ZMK_BEHAVIOR_NUM_SESSION_MAX_ACTIVE; i++) {
        if (active_num_sessions[i].is_active && active_num_sessions[i].layer == layer) {
            return &active_num_sessions[i];
        }
    }

    return NULL;
}

static struct active_num_session *
reserve_num_session(uint8_t layer, const struct behavior_num_session_config *config) {
    struct active_num_session *num_session = find_active_num_session(layer);
    if (num_session != NULL) {
        num_session->config = config;
        return num_session;
    }

    for (int i = 0; i < CONFIG_ZMK_BEHAVIOR_NUM_SESSION_MAX_ACTIVE; i++) {
        if (!active_num_sessions[i].is_active) {
            active_num_sessions[i].layer = layer;
            active_num_sessions[i].config = config;
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

    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    const struct behavior_num_session_config *cfg = dev->config;

    struct active_num_session *num_session = reserve_num_session(binding->param1, cfg);
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

    for (int i = 0; i < CONFIG_ZMK_BEHAVIOR_NUM_SESSION_MAX_ACTIVE; i++) {
        struct active_num_session *num_session = &active_num_sessions[i];
        if (!num_session->is_active) {
            continue;
        }

        if (is_mod(ev->usage_page, ev->keycode)) {
            num_session->is_modified = true;
            continue;
        }

        if (!num_session->is_modified && num_session_should_continue(num_session->config, ev)) {
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

#define KEY_LIST_ITEM(i, n, prop) ZMK_KEY_PARAM_DECODE(DT_INST_PROP_BY_IDX(n, prop, i))

#define PROP_KEY_LIST(n, prop)                                                                     \
    COND_CODE_1(DT_NODE_HAS_PROP(DT_DRV_INST(n), prop),                                            \
                ({                                                                                 \
                    .size = DT_INST_PROP_LEN(n, prop),                                             \
                    .keys = {LISTIFY(DT_INST_PROP_LEN(n, prop), KEY_LIST_ITEM, (, ), n, prop)},    \
                }),                                                                                \
                ({.size = 0}))

#define NUM_SESSION_INST(n)                                                                        \
    static const struct key_list num_session_continue_list_##n = PROP_KEY_LIST(n, continue_list);  \
    static const struct behavior_num_session_config behavior_num_session_config_##n = {            \
        .continue_keys = &num_session_continue_list_##n,                                           \
        .ignore_alphas = DT_INST_PROP(n, ignore_alphas),                                           \
        .ignore_numbers = DT_INST_PROP(n, ignore_numbers),                                         \
    };                                                                                             \
    BEHAVIOR_DT_INST_DEFINE(n, num_session_init, NULL, NULL, &behavior_num_session_config_##n,     \
                            POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                      \
                            &num_session_driver_api);

DT_INST_FOREACH_STATUS_OKAY(NUM_SESSION_INST)

#endif /* DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT) */
