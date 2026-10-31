#ifndef RADIO_TASK_H
#define RADIO_TASK_H
#include <stdint.h>
typedef struct {
    const char *error;
    int rssi,peak;
    uint32_t timeouts;
    uint8_t ready,state;
} RadioReading;
extern RadioReading g_radio;
void Radio_Begin(uint32_t now);
void Radio_Task(uint32_t now);
#endif
