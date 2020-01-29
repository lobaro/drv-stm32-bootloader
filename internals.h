//
// Created by TheoRohde on 1/24/2020.
//

#ifndef APP_MESSPRINT_V2_BOOT_INTERNALS_H
#define APP_MESSPRINT_V2_BOOT_INTERNALS_H

#ifdef __cplusplus
extern "C" {
#endif


#include <stdint.h>
#include <github.com/lobaro/util-ringbuf/drv_ringbuf.h>


static const uint8_t BYTE_INITIALIZE = 0x7F;
static const uint8_t BYTE_ACK = 0x79;
static const uint8_t BYTE_NACK = 0x1F;

static const uint8_t CmdNone              = 0xff; // aka waiting for next command
static const uint8_t CmdGet              = 0x00;
static const uint8_t CmdGetVersionStatus = 0x01;
static const uint8_t CmdGetId            = 0x02;
static const uint8_t CmdReadMemory       = 0x11;
static const uint8_t CmdGo               = 0x21;
static const uint8_t CmdWriteMemory      = 0x31;
static const uint8_t CmdErase            = 0x43;
static const uint8_t CmdExtendedErase    = 0x44;
static const uint8_t CmdWriteProtect     = 0x63;
static const uint8_t CmdWriteUnProtect   = 0x73;
static const uint8_t CmdReadoutProtect   = 0x82;
static const uint8_t CmdReadoutUnProtect = 0x92;

#define STM32_BOOT_RX_MEM_SIZE 1280
#define STM32_BOOT_TX_MEM_SIZE 1280
#define STM32_BOOT_RINGBUF_SIZE 1280
#define STM32_BOOT_CSUM_SIZE 1


typedef struct{
    drv_stm32boot_api_t api;

    bool hostInitDone;
    bool isTimeout;

    size_t expectedRxBytes;
    uint8_t selectedCmd;
    bool (*selectedCmdDoWorkLoop)(); // returns bytes expected to be received for next processing (0=cmd is done)

    char mem_rx[STM32_BOOT_RX_MEM_SIZE]; // working buf rx
    char mem_tx[STM32_BOOT_TX_MEM_SIZE]; // working buf tx
}boot32_t;

extern boot32_t boot;
bool checkXorCsum(char* pData, size_t size);


void send_ACK();
void send_NACK();
void setBytesToReceive(uint32_t expectedRxBytes);
bool selectCommand(uint8_t cmd);


#ifdef __cplusplus
}
#endif

#endif //APP_MESSPRINT_V2_BOOT_INTERNALS_H
