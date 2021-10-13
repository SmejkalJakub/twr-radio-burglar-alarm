#ifndef _APPLICATION_H
#define _APPLICATION_H

#ifndef VERSION
#define VERSION "vdev"
#endif

#include <bcl.h>
#include <twr.h>

void twr_change_alarm_state(uint64_t *id, const char *topic, void *value, void *param);


typedef struct
{
    uint8_t channel;
    float value;
    twr_tick_t next_pub;

} event_param_t;

#endif // _APPLICATION_H
