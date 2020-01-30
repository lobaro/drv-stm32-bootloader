#ifndef _stm32bootloader_h
#define _stm32bootloader_h

#ifdef __cplusplus
extern "C" {
#endif

#include <github.com/lobaro/c-utils/datetime.h>

#define STM32_BOOT_CHIP_ID_LOBARO_LORA (0x429)

typedef struct {
    // general bootloader api
    void (*putA)(const char *s, size_t len);
    void (*EnableIRQs)();
    void (*DisableIRQs)();

    // flash api
    bool (*deleteFlashPage)(uint16_t pageNumber);
    bool (*writeFlash)(uint32_t addr, uint8_t* pData, size_t size);
    bool (*readFlash)(uint32_t addr, uint8_t* pData, size_t size);

    // only needed if InactivityTimeoutSeks != 0 (e.g. timeout used)
    void (*EnableAlarm)();
    void (*DisableAlarm)();
    void (*SetAlarm)(Time_t alarmTime);
    Time_t (*GetTime)();

    // optional (make sure not to use the same if the bootloader operates on!)
    void (* log)(const char* format, ...);
} drv_stm32boot_api_t;

bool drv_stm32boot_run(drv_stm32boot_api_t api, Duration_t InactivityTimeoutSeks, uint16_t chipID);

// callback functions to be connected to external timeout / byte rx interrupts
void drv_stm32boot_onByteRxed_IRQ_cb(char c);
void drv_stm32boot_Timeout_IRQ_cb(); // only needed if InactivityTimeoutSeks != 0 (timeout used)

#ifdef __cplusplus
}
#endif

#endif