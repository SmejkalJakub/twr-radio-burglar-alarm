#include <application.h>

#define PIR_PUB_MIN_INTEVAL  (5 * 60 * 1000)
#define BATTERY_UPDATE_INTERVAL (60 * 60 * 1000)
#define TEMPERATURE_PUB_NO_CHANGE_INTEVAL (1 * 60 * 1000)

// LED instance
twr_led_t led;

// PIR instance
twr_module_pir_t pir;

// Thermometer instance
twr_tmp112_t tmp112;
event_param_t temperature_event_param = { .next_pub = 0 };

// Alarm On/Off
bool isAlarmOn = false;

twr_tick_t pir_next_pub = 0;

static const twr_radio_sub_t subs[] = {
    {"alarm/-/set/state", TWR_RADIO_SUB_PT_INT, twr_change_alarm_state, (void *) true}
};

void twr_change_alarm_state(uint64_t *id, const char *topic, void *value, void *param)
{
    int val = *(int *)value;
    twr_log_debug("spoustim alarm %d", val);
    isAlarmOn = val;
    if(!val)
    {
        pir_next_pub = 0;
    }
}

void battery_event_handler(twr_module_battery_event_t event, void *event_param)
{
    (void) event_param;

    float voltage;

    if (event == TWR_MODULE_BATTERY_EVENT_UPDATE)
    {
        if (twr_module_battery_get_voltage(&voltage))
        {
            twr_radio_pub_battery(&voltage);
        }
    }
}


void tmp112_event_handler(twr_tmp112_t *self, twr_tmp112_event_t event, void *event_param)
{
    float value;
    event_param_t *param = (event_param_t *)event_param;

    if (event == TWR_TMP112_EVENT_UPDATE)
    {
        if (twr_tmp112_get_temperature_celsius(self, &value))
        {
            twr_radio_pub_temperature(param->channel, &value);

            param->value = value;
            param->next_pub = twr_scheduler_get_spin_tick() + TEMPERATURE_PUB_NO_CHANGE_INTEVAL;
        }
    }
}


void pir_event_handler(twr_module_pir_t *self, twr_module_pir_event_t event, void *event_param)
{
    (void) self;
    (void) event_param;

    twr_log_debug("jsem tu ");
    if (event == TWR_MODULE_PIR_EVENT_MOTION && isAlarmOn)
    {
        twr_log_debug("pohyb");
        if (pir_next_pub < twr_scheduler_get_spin_tick())
        {
            twr_log_debug("posilam pohyb ");
            twr_radio_pub_event_count(TWR_RADIO_PUB_EVENT_PIR_MOTION, 0);

            pir_next_pub = twr_scheduler_get_spin_tick() + PIR_PUB_MIN_INTEVAL;
        }
    }
}

void application_init(void)
{
    // Initialize logging
    twr_log_init(TWR_LOG_LEVEL_DUMP, TWR_LOG_TIMESTAMP_ABS);

    // Initialize battery
    twr_module_battery_init();
    twr_module_battery_set_event_handler(battery_event_handler, NULL);
    twr_module_battery_set_update_interval(BATTERY_UPDATE_INTERVAL);

    // Initialize LED
    twr_led_init(&led, TWR_GPIO_LED, false, false);
    twr_led_set_mode(&led, TWR_LED_MODE_OFF);
    twr_led_pulse(&led, 1000);

    // Initialize thermometer sensor on core module
    temperature_event_param.channel = TWR_RADIO_PUB_CHANNEL_R1_I2C0_ADDRESS_ALTERNATE;
    twr_tmp112_init(&tmp112, TWR_I2C_I2C0, 0x49);
    twr_tmp112_set_event_handler(&tmp112, tmp112_event_handler, &temperature_event_param);
    twr_tmp112_set_update_interval(&tmp112, TEMPERATURE_PUB_NO_CHANGE_INTEVAL);

    // Initialize pir
    twr_module_pir_init(&pir);
    twr_module_pir_set_event_handler(&pir, pir_event_handler, NULL);

    twr_radio_init(TWR_RADIO_MODE_NODE_SLEEPING);
    twr_radio_set_subs((twr_radio_sub_t *) subs, sizeof(subs)/sizeof(twr_radio_sub_t));
    twr_radio_pairing_request("burglar-alarm", VERSION);
    twr_radio_set_rx_timeout_for_sleeping_node(100);
}
