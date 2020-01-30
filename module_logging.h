#pragma once

#ifndef configLOG_STM32_BOOTLOADER
#define configLOG_STM32_BOOTLOADER 0
#endif

#if configLOG_STM32_BOOTLOADER == 1
#define LOG(...) boot.api.log(__VA_ARGS__)
#else
#define LOG(...) {}
#endif
