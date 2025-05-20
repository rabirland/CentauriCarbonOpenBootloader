#pragma once

#include <Arduino.h>

class Hardware {
    public:
        /// @brief The size of the flash memory of STM32F402RTC6
        static const int32_t FlashSize = 256 * 1024 * 1024;

        /// @brief The position of the flash memory within an STM32 MCU.
        static const int32_t STM32BaseAddress = 0x08000000;

        /// @brief The offset from `STM32BaseAddress` where the stock elegoo firmware (upgrade-hotend and upgrade-sg) must be written to.
        static const int32_t FirmwareBinaryFileOffset = 0x8000;

        /// @brief  The offset from `STM32BaseAddress` where the reset handler address is stored within the stock elegoo firmware
        ///         assuming it's flashed to `STM32BaseAddress + FirmwareBinaryFileOffset`
        static const int32_t FirmwareResetHandlerOffset = 0xC000;
};