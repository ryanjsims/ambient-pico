#include "sio_client.h"

volatile int alarms_fired = 0;

int64_t alarm_callback(alarm_id_t id, void* user_data) {
    debug1("timer: refreshed watchdog\n");
    watchdog_update();
    if(alarms_fired < 2) {
        alarms_fired = alarms_fired + 1;
        // Reschedule the alarm for 7.33 seconds from now 3 times (gives the sio client 30 seconds to connect before reset)
        return 7333333ll;
    }
    return 0;
}