#ifndef _stm32bootloader_h
#define _stm32bootloader_h

#ifdef __cplusplus
extern "C" {
#endif

#include <github.com/lobaro/c-utils/datetime.h>

typedef struct {
    void (*putA)(const char *s, size_t len);

    void (*EnableAlarm)();
    void (*DisableAlarm)();
    void (*SetAlarm)(Time_t alarmTime);
    Time_t (*GetTime)();
} drv_stm32boot_api_t;

typedef struct {
    char dummy;
} drv_stm32boot_cfg_t;

//void drv_stm32boot_init(drv_stm32boot_api_t api, drv_stm32boot_cfg_t cfg);
bool drv_stm32boot_run(drv_stm32boot_api_t api, Duration_t inactiveTimeout);

// callback functions to be connected to external timeout / byte rx interrupts
void drv_stm32boot_onByteRxed_IRQ_cb(char c);
void drv_stm32boot_Timeout_IRQ_cb();

#ifdef __cplusplus
}
#endif

#endif