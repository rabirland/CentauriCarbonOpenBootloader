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
        static const int32_t FirmwareCodeOffset = 0xC000;
};

/*
new full-speed USB device number 11 using xhci_hcd
[134278.118540] usb 1-1.2: New USB device found, idVendor=1d50, idProduct=614e, bcdDevice= 1.00
[134278.118568] usb 1-1.2: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[134278.118582] usb 1-1.2: Product: stm32f401xc
[134278.118592] usb 1-1.2: Manufacturer: Klipper
[134278.118602] usb 1-1.2: SerialNumber: 39002C000B51333235353836

*/