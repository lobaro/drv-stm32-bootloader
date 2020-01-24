#include <github.com/lobaro/c-utils/c_stdlibs.h>
#include "stm32_bootloader.h"
#include <github.com/lobaro/c-utils/logging.h>
#include <github.com/lobaro/util-ringbuf/drv_ringbuf.h>
#include <github.com/lobaro/c-utils/lobaroAssert.h>
#include <github.com/lobaro/hal-stm32l151CB-A/hal.h>

// constants
#include "constants.h"

typedef struct{
    drv_stm32boot_cfg_t cfg;
    drv_stm32boot_api_t api;

    bool hostInitDone;
    bool isTimeout;
    bool waitingForNextCommand;
    size_t expectedRxBytes;
    char mem[256]; // working buf
}boot32_t;

// state
static boot32_t boot;

// uart rx ringbuffer
ringBuffer_typedef(volatile char, rxRingBuf_t);
static rxRingBuf_t rxRingBuf;
static rxRingBuf_t* pRxRingBuf = &rxRingBuf;
static volatile char rxMem[512]; // most likely 256 would be enough for large mem writes and reads

static bool checkXorCsum(char* pData, size_t size){
    if(pData==NULL || size < 2){
        return false;
    }

    if(size == 2){
        if(pData[0] == (char)~(pData[1])){ // cast needed
            return true;
        }else{
            return false;
        }
    }

    char csum = pData[0];
    for(size_t i=1; i<size;i++){
        csum = (char)(csum ^ pData[i]);
    }
    if(csum == 0) {
        return true;
    }
    return false;
}

// should be called by uart irq handler
void drv_stm32boot_onByteRxed_IRQ_cb(char c){
   // Log("%c",c);
    drv_rbuf_write(pRxRingBuf, c);
}

void drv_stm32boot_Timeout_IRQ_cb(){
    boot.isTimeout = true;
}

static bool api_OK(){
    if(boot.api.SetAlarm == NULL) return false;
    if(boot.api.GetTime == NULL) return false;
    if(boot.api.EnableAlarm == NULL) return false;
    if(boot.api.DisableAlarm == NULL) return false;
    if(boot.api.putA == NULL) return false;

    return true;
}

static void send_ACK(){
    char ack = BYTE_ACK;
    boot.api.putA(&ack,1);

}

static void send_NACK(){
    char nack = BYTE_NACK;
    boot.api.putA(&nack,1);

}

static void send(char* pData, size_t size){
    boot.api.putA(pData,size);

}

bool drv_stm32boot_run(drv_stm32boot_api_t api, Duration_t InactivityTimeoutSeks) { //}, drv_stm32boot_cfg_t cfg){
    boot.api = api;
    // boot.cfg = cfg;

    boot.isTimeout = false;

    // can't start
    if(!api_OK() || InactivityTimeoutSeks == 0){
        return false;
    }

    Log("Starting serial stm32 bootloader\n");

    // init receive ringbuffer
    drv_rbuf_init(pRxRingBuf, 512, volatile char, rxMem);

    boot.expectedRxBytes = 1;
    boot.hostInitDone = false;
    boot.waitingForNextCommand = true;

    Time_t timeNow = boot.api.GetTime();
    boot.api.SetAlarm(timeNow+InactivityTimeoutSeks);
    boot.api.EnableAlarm();

    uint32_t elementsInBuf = 0;
    while (!boot.isTimeout){
        hal_delay_ms(2);
        // debug log
        drv_rbuf_elements(pRxRingBuf, &elementsInBuf);
       //  Log("Elements in rx buf: %d, waiting for %d\n", elementsInBuf, boot.expectedRxBytes);

        if(elementsInBuf >= boot.expectedRxBytes){

            // copy bytes to check on next from ring buffer
            for(int i=0; i<boot.expectedRxBytes;i++){
                drv_rbuf_read(pRxRingBuf,&(boot.mem[i]));
            }

            // wait for init sequence from host done
            // note: we do not make baudrate detection yet and expect using 115200, even parity, 8 databits
            if(boot.hostInitDone == false){
                if(boot.mem[0] == BYTE_INITIALIZE){
                    boot.hostInitDone = true;
                    boot.expectedRxBytes = 2;
                    send_ACK(); // signal host that we are ready to receive commands
                }
                continue;
            }

            // wait for command or process ongoing
            if(checkXorCsum(boot.mem, boot.expectedRxBytes)==false){
                Log("stop processing %d bytes, Csum error!\n", boot.expectedRxBytes);
                continue; // checksum error
            }
            Log("continue processing %d bytes, Csum OK!\n", boot.expectedRxBytes);

            if(boot.waitingForNextCommand && boot.expectedRxBytes == 2){
                char cmd = boot.mem[0];

                //[1] is the csum but we checked on it already before
                switch (cmd){
                    case (uint32_t )CmdGetId:
                        send_ACK();
                        Log("got command CmdGetId\n");

                        boot.mem[0] = 1;
                        boot.mem[1] = 0x04; // PID
                        boot.mem[2] = 44;
                        boot.mem[3] = BYTE_ACK;
                        send(boot.mem,4);
                        break;
                    default:
                        send_NACK();
                        boot.expectedRxBytes == 1;
                        continue;
                }

            }

        }
    }

    Log("Leaving serial stm32 bootloader\n");
}

