//
// Created by TheoRohde on 1/28/2020.

#include "stm32_bootloader.h"
#include "internals.h"
#include <github.com/lobaro/c-utils/logging.h>


// state
boot32_t boot;

 void send_ACK(){
    char ack = BYTE_ACK;
    boot.api.putA(&ack,1);

}

 void send_NACK(){
    char nack = BYTE_NACK;
    boot.api.putA(&nack,1);
}

 void setBytesToReceive(uint32_t expectedRxBytes){
    boot.expectedRxBytes = expectedRxBytes;
}

static void send(char* pData, size_t size){
    boot.api.putA(pData,size);
}


 bool checkXorCsum(char* pData, size_t size){
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

static bool GetId(){
    send_ACK(); // command ack
    Log("WriteMem: Start\n");
    boot.mem_tx[0] = 1;
    boot.mem_tx[1] = 0x04; // PID Byte 0
    boot.mem_tx[2] = 0x29;   // PID Byte 1 (our lora stm32 = 0x429)
    boot.mem_tx[3] = BYTE_ACK;
    send(boot.mem_tx, 4);
    Log("WriteMem: End\n");
    return true; // signal processing done
 }

 static bool WriteMem(){
  static uint32_t stage = 0;
  static uint32_t startAddress = 0;
  static uint32_t bytesToWrite = 0;

  if(stage==0){
      send_ACK(); // ack write memory command
      Log("WriteMem: Start\n");
      setBytesToReceive(4 + STM32_BOOT_CSUM_SIZE); // setup to receive memory write address
      bytesToWrite = 0;
      startAddress = 0;
      stage++;
      return false;
  }

  // Receive the start address (4 bytes) & checksum
  if(stage==1){
      if(checkXorCsum((char*)boot.mem_rx, boot.expectedRxBytes)){
          startAddress |= ((uint32_t)boot.mem_rx[0]) << 24;
          startAddress |= ((uint32_t)boot.mem_rx[1]) << 16;
          startAddress |= ((uint32_t)boot.mem_rx[2]) << 8;
          startAddress |= ((uint32_t)boot.mem_rx[3]);
          Log("WriteMem: start address = 0x%08x\n",startAddress);
          send_ACK(); // ack start address
          setBytesToReceive(1 ); // num of bytes to expect
          stage++;
          return false;
      }
  }

  // Receive the number (N) of bytes to be written (1 byte)...
  if(stage==2){
      bytesToWrite = boot.mem_rx[0];
      setBytesToReceive(bytesToWrite + 1 + STM32_BOOT_CSUM_SIZE ); // num of bytes to expect
      stage++;
      Log("WriteMem: %d Bytes to be written\n",bytesToWrite+1);
      return false;
  }

  //..., the data (N + 1 bytes)(2) and checksum
  if(stage==3){
      // we must include the bytesToWrite byte for csum calculation
      // xor order does not matter to add it for the check to the first byte
      // after checking the csum this manipulation must be taken back
      boot.mem_rx[0] ^= ((uint8_t)bytesToWrite);
      if(checkXorCsum((char*)boot.mem_rx, boot.expectedRxBytes)){
          boot.mem_rx[0]^= ((uint8_t)bytesToWrite);

          // *** do actual mem write here later ***
          for(int i=0; i< bytesToWrite+1;i++){
              Log("%02x ", boot.mem_rx[i]);
          }

          send_ACK();
          Log("\n");
      }else{
          send_NACK();
          Log("WriteMem: csum error\n");
      }
  }

  // error or finish
  Log("WriteMem: OK\n");
  stage = 0;
  return true;
 }

static bool ExtendedErase(){
    static uint32_t stage = 0;
    static uint32_t numPagesToErase = 0;
    static uint8_t partialCsum = 0;

    if(stage==0){
        send_ACK(); // ack extended erase memory command
        Log("ExtendedErase: Start\n");
        setBytesToReceive(2); // setup to receive number of pages to be erased

        numPagesToErase = 0;
        partialCsum = 0;
        stage++;
        return false;
    }

    // Receive Number of Pages to be erased N-1 (2 bytes) MSB first
    if(stage==1){
        numPagesToErase  = ((uint32_t)boot.mem_rx[0]) << 8;
        numPagesToErase |= ((uint32_t)boot.mem_rx[1]);
        numPagesToErase++;

            // save csum relevant data of this stage
            partialCsum = boot.mem_rx[0];
            partialCsum ^= boot.mem_rx[1];
            Log("ExtendedErase: Pages to be erased: %d\nExtendedErase: Page list rx bytes %d\n", numPagesToErase, (2 * numPagesToErase));

            setBytesToReceive((2 * numPagesToErase) + STM32_BOOT_CSUM_SIZE);
            stage++;
            return false;
    }

    // Receive the page numbers (16bit) to be erased
    if(stage==2){
        boot.mem_rx[0] ^= partialCsum; // inject csum from stage before
        if(checkXorCsum((char*)boot.mem_rx, boot.expectedRxBytes)){
            boot.mem_rx[0]^= partialCsum; // remove partial csum again

            uint16_t page2Erase;
            for(int i=0; i < numPagesToErase*2; i= i + 2){
                page2Erase  = ((uint32_t)boot.mem_rx[i]) << 8;
                page2Erase |= ((uint32_t)boot.mem_rx[i+1]);
                Log("ExtendedErase: Page #%d erased\n", page2Erase);
                // *** do actual mem erase here later ***
            }

            send_ACK();
        }else{
            send_NACK();
            Log("ExtendedErase: csum error\n");
        }
    }

    // error or finish
    Log("ExtendedErase: OK\n");
    stage = 0;
    return true;
}

static bool ReadMemory(){
    static uint32_t stage = 0;
    static uint32_t startAddress = 0;

    if(stage==0){
        send_ACK(); // ack read memory command
        Log("ReadMemory: Start\n");
        setBytesToReceive(4 + STM32_BOOT_CSUM_SIZE); // setup to receive number of pages to be erased

        startAddress = 0;
        stage++;
        return false;
    }

    // Receive the start address (4 bytes) & checksum
    if(stage==1){
        if(checkXorCsum((char*)boot.mem_rx, boot.expectedRxBytes)){
            startAddress |= ((uint32_t)boot.mem_rx[0]) << 24;
            startAddress |= ((uint32_t)boot.mem_rx[1]) << 16;
            startAddress |= ((uint32_t)boot.mem_rx[2]) << 8;
            startAddress |= ((uint32_t)boot.mem_rx[3]);
            Log("ReadMemory: start address = 0x%08x\n",startAddress);

            send_ACK(); // ack start address // todo check if valid, callbacks?
            setBytesToReceive(1 + STM32_BOOT_CSUM_SIZE); // num of bytes to expect
            stage++;
            return false;
        }
    }

    // Receive the page numbers (16bit) to be erased
    if(stage==2){
        if(checkXorCsum((char*)boot.mem_rx, boot.expectedRxBytes)){
            uint32_t bytesToSend = boot.mem_rx[0] + 1 ; // +1 not documentated by makes sense to read 256 bytes with 1 byte
            send_ACK();
            Log("ReadMemory: %d bytes\n", bytesToSend);
            char temp[256];
            for(int i=0; i<bytesToSend;i++){
                temp[i]=i;

            }
            boot.api.putA(temp, bytesToSend);

        }else{
            send_NACK();
            Log("ReadMemory: csum error\n");
        }
    }

    // error or finish
    Log("ReadMemory: OK\n");
    stage = 0;
    return true;
}



bool selectCommand(uint8_t cmd){
     switch(cmd){
         case (uint32_t) CmdGetId:
             Log("STM32 Boot: select CmdGetId\n");
             boot.selectedCmd = cmd;
             boot.selectedCmdDoWorkLoop = GetId;
             return true;

         case (uint32_t) CmdWriteMemory:
             Log("STM32 Boot: select CmdWriteMemory\n");
             boot.selectedCmd = cmd;
             boot.selectedCmdDoWorkLoop = WriteMem;
             return true;

         case (uint32_t) CmdExtendedErase:
             Log("STM32 Boot: select CmdExtendedErase\n");
             boot.selectedCmd = cmd;
             boot.selectedCmdDoWorkLoop = ExtendedErase;
             return true;

         case (uint32_t) CmdReadMemory:
             Log("STM32 Boot: select CmdReadMemory\n");
             boot.selectedCmd = cmd;
             boot.selectedCmdDoWorkLoop = ReadMemory;
             return true;

         default:
             Log("STM32 Boot: command %02x not supported!\n",cmd);
             return false;
     }

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
 }




