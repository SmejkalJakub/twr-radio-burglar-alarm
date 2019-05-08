#include <application.h>

#define PIR_PUB_MIN_INTEVAL  (5 * 60 * 1000)
#define BATTERY_UPDATE_INTERVAL (60 * 60 * 1000)
#define TEMPERATURE_PUB_NO_CHANGE_INTEVAL (1 * 60 * 1000)

// LED instance
bc_led_t led;

// PIR instance
bc_module_pir_t pir;

// Thermometer instance
bc_tmp112_t tmp112;
event_param_t temperature_event_param = { .next_pub = 0 };

// Alarm On/Off
bool isAlarmOn = false;

bc_tick_t pir_next_pub = 0;

void bc_change_alarm_state(uint64_t *id, const char *topic, void *value, void *param);


static const bc_radio_sub_t subs[] = {
    {"alarm/-/set/state", BC_RADIO_SUB_PT_INT, bc_change_alarm_state, (void *) true}
};

void bc_change_alarm_state(uint64_t *id, const char *topic, void *value, void *param)
{
    int val = *(int *)value;
    bc_log_debug("spoustim alarm %d", val);
    isAlarmOn = val;
    if(!val)
    {
        pir_next_pub = 0;
    }
}

void battery_event_handler(bc_module_battery_event_t event, void *event_param)
{
    (void) event_param;

    float voltage;

    if (event == BC_MODULE_BATTERY_EVENT_UPDATE)
    {
        if (bc_module_battery_get_voltage(&voltage))
        {
            bc_radio_pub_battery(&voltage);
        }
    }
}


void tmp112_event_handler(bc_tmp112_t *self, bc_tmp112_event_t event, void *event_param)
{
    float value;
    event_param_t *param = (event_param_t *)event_param;

    if (event == BC_TMP112_EVENT_UPDATE)
    {
        if (bc_tmp112_get_temperature_celsius(self, &value))
        {
            bc_radio_pub_temperature(param->channel, &value);

            param->value = value;
            param->next_pub = bc_scheduler_get_spin_tick() + TEMPERATURE_PUB_NO_CHANGE_INTEVAL;
        }
    }
}


void pir_event_handler(bc_module_pir_t *self, bc_module_pir_event_t event, void *event_param)
{
    (void) self;
    (void) event_param;

    bc_log_debug("jsem tu ");
    if (event == BC_MODULE_PIR_EVENT_MOTION && isAlarmOn)
    {
        bc_log_debug("pohyb");
        if (pir_next_pub < bc_scheduler_get_spin_tick())
        {
            bc_log_debug("posilam pohyb ");
            bc_radio_pub_event_count(BC_RADIO_PUB_EVENT_PIR_MOTION, 0);

            pir_next_pub = bc_scheduler_get_spin_tick() + PIR_PUB_MIN_INTEVAL;
        }
    }
}

void application_init(void)
{
    // Initialize logging
    bc_log_init(BC_LOG_LEVEL_DUMP, BC_LOG_TIMESTAMP_ABS);

    // Initialize battery
    bc_module_battery_init();
    bc_module_battery_set_event_handler(battery_event_handler, NULL);
    bc_module_battery_set_update_interval(BATTERY_UPDATE_INTERVAL);

    // Initialize LED
    bc_led_init(&led, BC_GPIO_LED, false, false);
    bc_led_set_mode(&led, BC_LED_MODE_OFF);
    bc_led_pulse(&led, 1000);

    // Initialize thermometer sensor on core module
    temperature_event_param.channel = BC_RADIO_PUB_CHANNEL_R1_I2C0_ADDRESS_ALTERNATE;
    bc_tmp112_init(&tmp112, BC_I2C_I2C0, 0x49);
    bc_tmp112_set_event_handler(&tmp112, tmp112_event_handler, &temperature_event_param);
    bc_tmp112_set_update_interval(&tmp112, TEMPERATURE_PUB_NO_CHANGE_INTEVAL);

    // Initialize pir
    bc_module_pir_init(&pir);
    bc_module_pir_set_event_handler(&pir, pir_event_handler, NULL);

    bc_radio_init(BC_RADIO_MODE_NODE_SLEEPING);
    bc_radio_set_subs((bc_radio_sub_t *) subs, sizeof(subs)/sizeof(bc_radio_sub_t));
    bc_radio_pairing_request("burglar-alarm", VERSION);
    bc_radio_set_rx_timeout_for_sleeping_node(100);
}