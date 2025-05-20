#include <Arduino.h>
#include "ymodem.h"

uint8_t inputFile[128];

void setup() {
  SerialUSB.begin();
  int32_t fileSize = 0;
  YModem::ReceiveFile(&inputFile[0], &fileSize);

  // First 32bit is the stack pointer address
  // Second 4 bytes are the reset handler (which is also the entry point)
  volatile uint32_t* stackAddressPointer = (uint32_t*)(Hardware::STM32BaseAddress + Hardware::FirmwareResetHandlerOffset);
  volatile uint32_t* entryPointPointer = (uint32_t*)(Hardware::STM32BaseAddress + Hardware::FirmwareResetHandlerOffset + 0x4);

  volatile uint32_t stackAddress = *stackAddressPointer;
  volatile uint32_t entryPoint = *entryPointPointer;

  void (*FirmwareEntryPoint)(void) = (void (*)(void))entryPoint;

  __set_MSP(stackAddress);
	SysTick->CTRL = 0;
	SysTick->LOAD = 0;
	SysTick->VAL = 0;

  while (1)
  {
    FirmwareEntryPoint();
  }
}

void loop() {
}