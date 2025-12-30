#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/led.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <zmk/battery.h>
#include <zmk/ble.h>
#include <zmk/endpoints.h>
#include <zmk/hid_indicators.h>
#include <zmk/pm.h>
#include <zmk/usb.h>

#include <zmk/events/battery_state_changed.h>
#include <zmk/events/ble_active_profile_changed.h>
#include <zmk/events/hid_indicators_changed.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/events/usb_conn_state_changed.h>

/* HID Lock indicator bit (only Caps Lock is used) */
#define CAPSLOCK_BIT BIT(1)

/* LED color bits: bit0=Red, bit1=Green, bit2=Blue */
#define COLOR_OFF     0b000
#define COLOR_RED     0b001
#define COLOR_GREEN   0b010
#define COLOR_YELLOW  0b011  /* Red + Green */
#define COLOR_BLUE    0b100
#define COLOR_MAGENTA 0b101  /* Red + Blue */
#define COLOR_CYAN    0b110  /* Green + Blue */
#define COLOR_WHITE   0b111

/* BLE profile colors: BT1=Yellow, BT2=Cyan, BT3=Magenta */
#define PROFILE_COLORS {COLOR_YELLOW, COLOR_CYAN, COLOR_MAGENTA}

/* Special keycodes to trigger LED updates (defined in common.keymap) */
#define KEYCODE_SHOW_LED     0xAB
#define KEYCODE_SHOW_BATTERY 0xAC
#define KEYCODE_SOFT_OFF_LED 0xAD

/* Lock layer number for LED feedback */
#define LOCK_LAYER 4

/* Feedback indicator flash count */
#define FEEDBACK_FLASH_COUNT   8    /* 2 blinks * 4 phases */
#define SOFT_OFF_FLASH_COUNT   12   /* 3 blinks * 4 phases */
#define USB_FLASH_COUNT        4    /* 1 blink * 4 phases */

/* LED timing constants */
#define LED_TICK_MS           20
#define FLASH_COUNT_CONNECTED 12   /* 3 blinks * 4 phases */
#define FLASH_COUNT_SEARCHING 60   /* 15 blinks * 4 phases */

/* Battery thresholds */
#define BATTERY_LOW_THRESHOLD    10
#define BATTERY_INIT_VALUE       111  /* High value to avoid false low-battery at startup */
#define BATTERY_HIGH_THRESHOLD   80
#define BATTERY_MED_THRESHOLD    50
#define BATTERY_LOW_MED_THRESHOLD 20

/* Battery indicator flash count */
#define BATTERY_FLASH_COUNT      12   /* 3 blinks * 4 phases */

/* BLE profile count */
#define MAX_BLE_PROFILES 3

/* Connection states */
enum connection_state {
    CONN_IDLE = 0,       /* No connection activity */
    CONN_SEARCHING = 1,  /* Searching/advertising for connection */
    CONN_CONNECTED = 2   /* Connected to device */
};

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define LED_GPIO_NODE_ID DT_COMPAT_GET_ANY_STATUS_OKAY(gpio_leds)

BUILD_ASSERT(DT_NODE_EXISTS(DT_ALIAS(indicator_r)),
             "An alias for a red LED is not found for RGBLED_WIDGET");
BUILD_ASSERT(DT_NODE_EXISTS(DT_ALIAS(indicator_g)),
             "An alias for a green LED is not found for RGBLED_WIDGET");
BUILD_ASSERT(DT_NODE_EXISTS(DT_ALIAS(indicator_b)),
             "An alias for a blue LED is not found for RGBLED_WIDGET");

// GPIO-based LED device and indices of red/green/blue LEDs inside its DT node
static const struct device *led_dev = DEVICE_DT_GET(LED_GPIO_NODE_ID);
static const uint8_t led_idx[] = {DT_NODE_CHILD_IDX(DT_ALIAS(indicator_r)),
                                  DT_NODE_CHILD_IDX(DT_ALIAS(indicator_g)),
                                  DT_NODE_CHILD_IDX(DT_ALIAS(indicator_b))};

struct indicator_state_t {
    uint8_t keylock;
    uint8_t connection;
    uint8_t active_device;
    uint8_t battery;
    uint8_t flash_times;
    uint8_t battery_show;       /* Flag to show battery indicator */
    uint8_t battery_flash_times;
    uint8_t feedback_show;      /* Flag to show feedback indicator */
    uint8_t feedback_flash_times;
    uint8_t feedback_color;     /* Color for feedback indicator */
    uint8_t soft_off_pending;   /* Flag to execute soft_off after LED */
} indicator_state;

static void set_indicator_color(uint8_t bits) {
    static uint8_t last_bits = 0;
    if (bits != last_bits) {
        for (uint8_t pos = 0; pos < 3; pos++) {
            if (bits & (1<<pos)) {
                led_on(led_dev, led_idx[pos]);
            } else {
                led_off(led_dev, led_idx[pos]);
            }
        }
        last_bits = bits;
    }
}

static void get_lock_indicators(void) {
    uint8_t state = zmk_hid_indicators_get_current_profile();
    LOG_DBG("LOCK LEDS: %d", state);
    indicator_state.keylock = state;
}

static void hid_indicators_status_update_cb(const zmk_event_t *eh) {
    get_lock_indicators();
}

ZMK_LISTENER(widget_hid_indicators_status, hid_indicators_status_update_cb);
ZMK_SUBSCRIPTION(widget_hid_indicators_status, zmk_hid_indicators_changed);

static void ble_active_profile_update(void) {
    uint8_t profile_index = zmk_ble_active_profile_index();
    if (profile_index >= MAX_BLE_PROFILES) {
        return;
    }
    indicator_state.active_device = profile_index;
    if (zmk_ble_active_profile_is_connected()) {
        indicator_state.connection = CONN_CONNECTED;
        indicator_state.flash_times = FLASH_COUNT_CONNECTED;
    } else {
        indicator_state.connection = CONN_SEARCHING;
        indicator_state.flash_times = FLASH_COUNT_SEARCHING;
    }
    LOG_DBG("Device_BT%d, Connection State: %d", indicator_state.active_device + 1, indicator_state.connection);
}

static void ble_active_profile_update_cb(const zmk_event_t *eh) {
    ble_active_profile_update();
}

ZMK_LISTENER(ble_active_profile_listener, ble_active_profile_update_cb);
ZMK_SUBSCRIPTION(ble_active_profile_listener, zmk_ble_active_profile_changed);

static void battery_indicator_show(void) {
    indicator_state.battery_show = 1;
    indicator_state.battery_flash_times = BATTERY_FLASH_COUNT;
    LOG_DBG("Battery level: %d%%", indicator_state.battery);
}

static void feedback_indicator_show(uint8_t color, uint8_t flash_count) {
    indicator_state.feedback_show = 1;
    indicator_state.feedback_color = color;
    indicator_state.feedback_flash_times = flash_count;
    LOG_DBG("Feedback indicator: color=%d, count=%d", color, flash_count);
}

/* Layer state change listener for Lock layer LED feedback */
static int layer_state_changed_cb(const zmk_event_t *eh) {
    struct zmk_layer_state_changed *layer_ev = as_zmk_layer_state_changed(eh);
    if (layer_ev == NULL) {
        return 0;
    }

    if (layer_ev->layer == LOCK_LAYER) {
        if (layer_ev->state) {
            /* Lock layer activated: Yellow LED */
            feedback_indicator_show(COLOR_YELLOW, FEEDBACK_FLASH_COUNT);
        } else {
            /* Lock layer deactivated: Green LED */
            feedback_indicator_show(COLOR_GREEN, FEEDBACK_FLASH_COUNT);
        }
    }
    return 0;
}

ZMK_LISTENER(layer_indicator, layer_state_changed_cb);
ZMK_SUBSCRIPTION(layer_indicator, zmk_layer_state_changed);

#if IS_ENABLED(CONFIG_ZMK_USB)
/* Auto-switch endpoint based on USB connection state */
static int usb_conn_state_changed_cb(const zmk_event_t *eh) {
    struct zmk_usb_conn_state_changed *usb_ev = as_zmk_usb_conn_state_changed(eh);
    if (usb_ev == NULL) {
        return 0;
    }

    switch (usb_ev->conn_state) {
        case ZMK_USB_CONN_HID:
            /* USB HID ready: switch to USB output with white LED */
            zmk_endpoints_select_transport(ZMK_TRANSPORT_USB);
            feedback_indicator_show(COLOR_WHITE, USB_FLASH_COUNT);
            break;
        case ZMK_USB_CONN_NONE:
            /* USB disconnected: switch back to BLE */
            if (indicator_state.feedback_show && indicator_state.feedback_color == COLOR_WHITE) {
                /* Cancel pending USB indicator so it doesn't fire after disconnect */
                indicator_state.feedback_show = 0;
                indicator_state.feedback_flash_times = 0;
                indicator_state.feedback_color = COLOR_OFF;
            }
            zmk_endpoints_select_transport(ZMK_TRANSPORT_BLE);
            /* Refresh BLE indicator so BT color blink resumes after USB disconnect */
            ble_active_profile_update();
            break;
        default:
            break;
    }
    return 0;
}

ZMK_LISTENER(usb_auto_switch, usb_conn_state_changed_cb);
ZMK_SUBSCRIPTION(usb_auto_switch, zmk_usb_conn_state_changed);
#endif // IS_ENABLED(CONFIG_ZMK_USB)

static uint8_t get_battery_color(uint8_t level) {
    if (level >= BATTERY_HIGH_THRESHOLD) {
        return COLOR_GREEN;      /* 80%+ : Green */
    } else if (level >= BATTERY_MED_THRESHOLD) {
        return COLOR_CYAN;       /* 50-80%: Cyan */
    } else if (level >= BATTERY_LOW_MED_THRESHOLD) {
        return COLOR_YELLOW;     /* 20-50%: Yellow */
    } else {
        return COLOR_RED;        /* <20% : Red */
    }
}

static int zmk_handle_keycode_user(struct zmk_keycode_state_changed *event) {
    zmk_key_t key = event->keycode;
    LOG_DBG("key 0x%X", key);
    if (key == KEYCODE_SHOW_LED) {
        ble_active_profile_update();
    } else if (key == KEYCODE_SHOW_BATTERY) {
        battery_indicator_show();
    } else if (key == KEYCODE_SOFT_OFF_LED) {
        /* Red LED feedback then execute soft_off */
        feedback_indicator_show(COLOR_RED, SOFT_OFF_FLASH_COUNT);
        indicator_state.soft_off_pending = 1;
    }
    return ZMK_EV_EVENT_HANDLED;
}

static int keycode_user_listener(const zmk_event_t *eh) {
    struct zmk_keycode_state_changed *kc_state;

    kc_state = as_zmk_keycode_state_changed(eh);

    if (kc_state != NULL) {
        return zmk_handle_keycode_user(kc_state);
    }

    return 0;
}

ZMK_LISTENER(keycode_user, keycode_user_listener);
ZMK_SUBSCRIPTION(keycode_user, zmk_keycode_state_changed);

#if IS_ENABLED(CONFIG_ZMK_BATTERY_REPORTING)
static int led_battery_listener_cb(const zmk_event_t *eh) {
    uint8_t battery_level = as_zmk_battery_state_changed(eh)->state_of_charge;
    indicator_state.battery = battery_level;
    return 0;
}

ZMK_LISTENER(led_battery_listener, led_battery_listener_cb);
ZMK_SUBSCRIPTION(led_battery_listener, zmk_battery_state_changed);
#endif // IS_ENABLED(CONFIG_ZMK_BATTERY_REPORTING)

void led_process_thread(void) {
    static const uint8_t profile_color_bits[MAX_BLE_PROFILES] = PROFILE_COLORS;
    static uint16_t led_timer_steps = 0;

    while (true) {
        k_sleep(K_MSEC(LED_TICK_MS));
        led_timer_steps++;

        if (indicator_state.connection != CONN_IDLE) {
            /* BLE connection state indicator */
            if (indicator_state.active_device >= MAX_BLE_PROFILES) {
                continue;
            }

            if ((led_timer_steps & 0xf) == 0xf) {
                indicator_state.flash_times--;
                uint8_t color_bits = profile_color_bits[indicator_state.active_device];

                switch ((led_timer_steps >> 4) & 0x3) {
                    case 0:
                        set_indicator_color(COLOR_OFF);
                        break;
                    case 1:
                        set_indicator_color(color_bits);
                        break;
                    case 2:
                        if (indicator_state.connection != CONN_CONNECTED) {
                            set_indicator_color(COLOR_OFF);
                        }
                        break;
                    case 3:
                        if (indicator_state.connection != CONN_CONNECTED) {
                            bt_addr_le_t *addr = zmk_ble_active_profile_addr();
                            /* Red if profile cleared, Blue if has saved address */
                            if (bt_addr_le_eq(addr, BT_ADDR_LE_ANY)) {
                                set_indicator_color(COLOR_RED);
                            } else {
                                set_indicator_color(COLOR_BLUE);
                            }
                        }
                        break;
                }

                if (indicator_state.flash_times == 0) {
                    indicator_state.connection = CONN_IDLE;
                }
            }
        } else if (indicator_state.battery_show) {
            /* Battery level indicator */
            if ((led_timer_steps & 0xf) == 0xf) {
                indicator_state.battery_flash_times--;
                uint8_t color = get_battery_color(indicator_state.battery);

                if ((led_timer_steps >> 4) & 0x1) {
                    set_indicator_color(color);
                } else {
                    set_indicator_color(COLOR_OFF);
                }

                if (indicator_state.battery_flash_times == 0) {
                    indicator_state.battery_show = 0;
                    set_indicator_color(COLOR_OFF);
                }
            }
        } else if (indicator_state.feedback_show) {
            /* Feedback indicator (soft_off, lock layer) */
            if ((led_timer_steps & 0xf) == 0xf) {
                indicator_state.feedback_flash_times--;

                if ((led_timer_steps >> 4) & 0x1) {
                    set_indicator_color(indicator_state.feedback_color);
                } else {
                    set_indicator_color(COLOR_OFF);
                }

                if (indicator_state.feedback_flash_times == 0) {
                    indicator_state.feedback_show = 0;
                    set_indicator_color(COLOR_OFF);

                    /* Execute soft_off if pending */
                    if (indicator_state.soft_off_pending) {
                        indicator_state.soft_off_pending = 0;
                        zmk_pm_soft_off();
                    }
                }
            }
        } else if (indicator_state.battery < BATTERY_LOW_THRESHOLD) {
            /* Low battery warning: blink red */
            if ((led_timer_steps & 0x1f) == 0xf) {
                set_indicator_color(COLOR_RED);
            } else if ((led_timer_steps & 0x1f) == 0x1f) {
                set_indicator_color(COLOR_OFF);
            }
        } else {
            /* Caps Lock indicator */
            if (indicator_state.keylock & CAPSLOCK_BIT) {
                set_indicator_color(COLOR_MAGENTA);
            } else {
                set_indicator_color(COLOR_OFF);
            }
        }
    }
}

/* LED process thread: runs every 20ms, starts 100ms after boot */
K_THREAD_DEFINE(led_process_tid, 1024, led_process_thread, NULL, NULL, NULL,
                K_LOWEST_APPLICATION_THREAD_PRIO, 0, 100);

void klink_indicator_init_thread(void) {
    /* Initialize as searching state to show LED pattern on boot */
    indicator_state.connection = CONN_SEARCHING;
    /* Set battery to high value to avoid false low-battery warning at startup */
    indicator_state.battery = BATTERY_INIT_VALUE;
}

/* Initialization thread: runs 200ms after boot */
K_THREAD_DEFINE(klink_indicator_init_tid, 1024, klink_indicator_init_thread, NULL, NULL, NULL,
                K_LOWEST_APPLICATION_THREAD_PRIO, 0, 200);
