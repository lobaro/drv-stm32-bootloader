#include <github.com/lobaro/c-utils/c_stdlibs.h>
#include <github.com/lobaro/c-utils/logging.h>
#include <github.com/lobaro/util-ringbuf/drv_ringbuf.h>
#include <github.com/lobaro/c-utils/lobaroAssert.h>
#include <github.com/lobaro/hal-stm32l151CB-A/hal.h>

// constants
#include "stm32_bootloader.h"
#include "internals.h"

ringBuffer_typedef(volatile char, rxRingBuf_t);
static rxRingBuf_t rxRingBuf;
static rxRingBuf_t* pRxRingBuf = &rxRingBuf;
static volatile char rxMem[2048];
static volatile uint32_t bytesInBuffer=0;

void drv_stm32boot_Timeout_IRQ_cb(){
    boot.isTimeout = true;
}

// should be called by uart irq handler
void drv_stm32boot_onByteRxed_IRQ_cb(char c){
    drv_rbuf_write(pRxRingBuf, c);
    bytesInBuffer++;
}

static bool api_OK(){
    if(boot.api.SetAlarm == NULL) return false;
    if(boot.api.GetTime == NULL) return false;
    if(boot.api.EnableAlarm == NULL) return false;
    if(boot.api.DisableAlarm == NULL) return false;
    if(boot.api.putA == NULL) return false;
    return true;
}

bool drv_stm32boot_run(drv_stm32boot_api_t api, Duration_t InactivityTimeoutSeks) {
    boot.api = api;
    boot.isTimeout = false;

    // can't start
    if(!api_OK() || InactivityTimeoutSeks == 0){
        return false;
    }

    Log("Starting serial stm32 bootloader\n");

    boot.selectedCmd = CmdNone;
    boot.hostInitDone = false;

    // init receive ringbuffer
    drv_rbuf_init(pRxRingBuf, 512, volatile char, rxMem);
    setBytesToReceive(1);

    Time_t timeNow = boot.api.GetTime();
    boot.api.SetAlarm(timeNow+InactivityTimeoutSeks);
    boot.api.EnableAlarm();

    while (!boot.isTimeout){

        // check for at least expected byte to be available for processing
        if(bytesInBuffer < boot.expectedRxBytes){
            continue;
        }

        // read expected bytes from ringbuf
        for(int i=0; i<boot.expectedRxBytes;i++){
            drv_rbuf_read(pRxRingBuf,&(boot.mem_rx[i]));
        }

        boot.api.DisableIRQs();
        bytesInBuffer-=boot.expectedRxBytes;
        boot.api.EnableIRQs();

        // wait for bootloader initialization
        // note: we do not make baudrate detection yet and expect using 115200, even parity, 8 databits
        if(boot.hostInitDone == false){
            if(boot.mem_rx[0] == BYTE_INITIALIZE){
                boot.hostInitDone = true;
                setBytesToReceive(1 + STM32_BOOT_CSUM_SIZE);
                send_ACK(); // signal host that we are ready to receive commands
            }else{
                setBytesToReceive(1); // continue waiting for init byte from host
            }
            continue;
        }

        // wait for next command to execute / select
        if(boot.selectedCmd == CmdNone && boot.expectedRxBytes == 2) {
            if(checkXorCsum((char*)boot.mem_rx, boot.expectedRxBytes) == false){
                Log("csum error while selecting next cmd\n");
                setBytesToReceive(1 + STM32_BOOT_CSUM_SIZE);
                continue;
            }

            if(selectCommand(boot.mem_rx[0]) == false){ // command not supported?
                setBytesToReceive(1 + STM32_BOOT_CSUM_SIZE); // continue waiting for supported cmd
                send_NACK();
                continue;
            }
        }

        // execute internal state machine for selected command
        if(boot.selectedCmdDoWorkLoop() == true){ // command done, else command restarts receive if needed internally
            // setup receive logic for next command
            boot.selectedCmd = CmdNone;
            setBytesToReceive(1 + STM32_BOOT_CSUM_SIZE);
        }

    }
    Log("Leaving serial stm32 bootloader\n");
}

