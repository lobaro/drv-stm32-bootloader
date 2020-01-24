//
// Created by TheoRohde on 1/24/2020.
//

#ifndef APP_MESSPRINT_V2_BOOT_CONSTANTS_H
#define APP_MESSPRINT_V2_BOOT_CONSTANTS_H

static const uint8_t BYTE_INITIALIZE = 0x7F;
static const uint8_t BYTE_ACK = 0x79;
static const uint8_t BYTE_NACK = 0x1F;


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


#endif //APP_MESSPRINT_V2_BOOT_CONSTANTS_H
