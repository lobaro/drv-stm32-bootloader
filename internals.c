//
// Created by TheoRohde on 1/28/2020.

#include "stm32_bootloader.h"
#include "internals.h"
#include "module_logging.h"

// inspiration https://github.com/Mirn/STM32Fx_AN3155_bootloader/blob/master/boot_stm32F100x/bootloader.c
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

// *****
// GET ID
// *****
static bool GetId(){
    send_ACK(); // command ack
    LOG("GetID: Start\n");

    boot.mem_tx[0] = 1;

    boot.mem_tx[1] = (uint8_t) ((boot.chipID >> 8) & 0xff) ;   // PID Byte 0
    boot.mem_tx[2] = (uint8_t) (boot.chipID & 0xff); // PID Byte 1 (our lora stm32 = 0x429)
    boot.mem_tx[3] = BYTE_ACK;
    send(boot.mem_tx, 4);

    LOG("GetID: End\n\n");
    return true; // signal processing done
 }

// *****
// WRITE
// *****
 static bool WriteMem(){
  static uint32_t stage = 0;
  static uint32_t startAddress = 0;
  static uint32_t bytesToWrite = 0;

  if(stage==0){
      send_ACK(); // ack write memory command

      LOG("WriteMem: Start\n");

      setBytesToReceive(4 + STM32_BOOT_CSUM_SIZE); // setup to receive memory write address
      bytesToWrite = 0;
      startAddress = 0;
      stage++;
      return false;
  }

  // Receive the start address (4 bytes) & checksum
  if(stage==1){
      if(checkXorCsum((char*)boot.mem_rx, boot.expectedRxBytes)){
          startAddress |= ((uint32_t)boot.mem_rx[0]) << (uint32_t)24;
          startAddress |= ((uint32_t)boot.mem_rx[1]) << (uint32_t)16;
          startAddress |= ((uint32_t)boot.mem_rx[2]) << (uint32_t)8;
          startAddress |= ((uint32_t)boot.mem_rx[3]);

          //LOG("WriteMem: start address = 0x%08x\n",startAddress);

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

      //LOG("WriteMem: %d Bytes to be written:\n",bytesToWrite+1);

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

          bytesToWrite++; // bytesToWrite send by Host is -1 (to be able to write up to 256 byte with just 1 byte size)

          LOG("WriteMem: Write %d bytes to address 0x%08x - ", bytesToWrite, startAddress);
          if(boot.api.writeFlash(startAddress,boot.mem_rx,bytesToWrite)){
              LOG("OK\n");
              send_ACK();
          }else{
              LOG("ERROR\n");
              send_NACK();
          }

      }else{
          send_NACK();
          LOG("WriteMem: csum error\n");
      }
  }

  // error or finish
  LOG("WriteMem: END\n\n");
  stage = 0;
  return true;
 }

 // *****
 // ERASE
 // *****
static bool ExtendedErase(){
    static uint32_t stage = 0;
    static uint32_t numPagesToErase = 0;
    static uint8_t partialCsum = 0;
    static uint8_t partialCsum2 = 0;
    if(stage==0){
        send_ACK(); // ack extended erase memory command

        LOG("ExtendedErase: Start\n");
        setBytesToReceive(2); // setup to receive number of pages to be erased

        numPagesToErase = 0;
        partialCsum = 0;
        partialCsum2 = 0;
        stage++;
        return false;
    }

    // Receive Number of Pages to be erased N-1 (2 bytes) MSB first
    if(stage==1){
        numPagesToErase  = ((uint32_t)boot.mem_rx[0]) << (uint32_t)8;
        numPagesToErase |= ((uint32_t)boot.mem_rx[1]);
        numPagesToErase++;

            // save csum relevant data of this stage
            partialCsum = boot.mem_rx[0];
            partialCsum ^= boot.mem_rx[1];

            LOG("ExtendedErase: Pages to be erased: %d\nExtendedErase: Page list rx bytes %d\n", numPagesToErase, (2 * numPagesToErase)+STM32_BOOT_CSUM_SIZE);

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
            for(int i=0; i < numPagesToErase; i++){
                page2Erase  = ((uint32_t)boot.mem_rx[i*2]) << 8;
                page2Erase |= ((uint32_t)boot.mem_rx[i*2+1]);
                LOG("ExtendedErase: Erasing page #%d - ", page2Erase);

                // actual flash callback
                if(!boot.api.deleteFlashPage(page2Erase)){
                    stage = 0;
                    send_NACK();
                    LOG("ERROR\n");
                    LOG("ExtendedErase: END\n\n");
                    return true;
                }else{
                    LOG("OK\n");
                }
            }
            LOG("ExtendedErase: END\n\n");
            stage = 0;
            send_ACK();
            return true;
        }else{
            LOG("ExtendedErase: csum error\n");
            LOG("ExtendedErase: END\n\n");
            send_NACK();
            stage = 0;
            return true;
        }
    }

    // error or finish
     stage = 0;
     return true;
}

// *****
// READ
// *****
static bool ReadMemory(){
    static uint32_t stage = 0;
    static uint32_t startAddress = 0;

    if(stage==0){
        send_ACK(); // ack read memory command

        //LOG("ReadMemory: Start\n");
        setBytesToReceive(4 + STM32_BOOT_CSUM_SIZE); // setup to receive number of pages to be erased

        startAddress = 0;
        stage++;
        return false;
    }

    // Receive the start address (4 bytes) & checksum
    if(stage==1){
        if(checkXorCsum((char*)boot.mem_rx, boot.expectedRxBytes)){
            startAddress |= ((uint32_t)boot.mem_rx[0]) << (uint32_t)24;
            startAddress |= ((uint32_t)boot.mem_rx[1]) << (uint32_t)16;
            startAddress |= ((uint32_t)boot.mem_rx[2]) << (uint32_t)8;
            startAddress |= ((uint32_t)boot.mem_rx[3]);

            //LOG("ReadMemory: start address = 0x%08x\n",startAddress);

            send_ACK(); // ack start address // todo check if valid, callbacks?
            setBytesToReceive(1 + STM32_BOOT_CSUM_SIZE); // num of bytes to expect
            stage++;
            return false;
        }
    }

    // Receive the bytes to be read by host and send by the bootloader (up to 256 bytes)
    if(stage==2){
        if(checkXorCsum((char*)boot.mem_rx, boot.expectedRxBytes)){
            uint32_t bytesToSend = boot.mem_rx[0] + 1 ; // +1 not documented by makes sense to read 256 bytes with 1 byte

            LOG("ReadMemory: Read %d bytes from address 0x%08x - ", bytesToSend, startAddress);
            send_ACK();
            if(boot.api.readFlash(startAddress, (uint8_t*)boot.mem_tx, bytesToSend)){
                boot.api.putA(boot.mem_tx, bytesToSend);
                LOG("OK\n");
            }else{
                send_NACK();
                LOG("ERROR\n");
            }


        }else{
            send_NACK();
            LOG("ReadMemory: csum error\n");
        }
    }

    // error or finish
    LOG("ReadMemory: END\n\n");

    stage = 0;
    return true;
}


bool selectCommand(uint8_t cmd){
     switch(cmd){
         case (uint32_t) CmdGetId:

             LOG("STM32 Boot: select CmdGetId\n");

             boot.selectedCmd = cmd;
             boot.selectedCmdDoWorkLoop = GetId;
             return true;

         case (uint32_t) CmdWriteMemory:

             LOG("STM32 Boot: select CmdWriteMemory\n");

             boot.selectedCmd = cmd;
             boot.selectedCmdDoWorkLoop = WriteMem;
             return true;

         case (uint32_t) CmdExtendedErase:

             LOG("STM32 Boot: select CmdExtendedErase\n");

             boot.selectedCmd = cmd;
             boot.selectedCmdDoWorkLoop = ExtendedErase;
             return true;

         case (uint32_t) CmdReadMemory:

             LOG("STM32 Boot: select CmdReadMemory\n");

             boot.selectedCmd = cmd;
             boot.selectedCmdDoWorkLoop = ReadMemory;
             return true;

         default:
             LOG("STM32 Boot: command %02x not supported!\n",cmd);
             return false;
     }
 }




