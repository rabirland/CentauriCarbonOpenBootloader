#pragma once
#include <Arduino.h>
#include "Utils.h"
#include "Hardware.h"

#define PACKET_SEQNO_INDEX      (1)
#define PACKET_SEQNO_COMP_INDEX (2)

#define PACKET_HEADER           (3)
#define PACKET_TRAILER          (2)
#define PACKET_OVERHEAD         (PACKET_HEADER + PACKET_TRAILER)
#define PACKET_SIZE             (128)
#define PACKET_1K_SIZE          (1024)

#define FILE_NAME_LENGTH        (256)
#define FILE_SIZE_LENGTH        (16)

#define SOH                     (0x01)  /* start of 128-byte data packet */
#define STX                     (0x02)  /* start of 1024-byte data packet */
#define EOT                     (0x04)  /* end of transmission */
#define ACK                     (0x06)  /* acknowledge */
#define NAK                     (0x15)  /* negative acknowledge */
#define CA                      (0x18)  /* two of these in succession aborts transfer */
#define CRC16                   (0x43)  /* 'C' == 0x43, request 16-bit CRC */

#define ABORT1                  (0x41)  /* 'A' == 0x41, abort by user */
#define ABORT2                  (0x61)  /* 'a' == 0x61, abort by user */

#define NAK_TIMEOUT             (0x100000)
#define MAX_ERRORS              (5)

enum struct ReceiveByteResult : uint8_t {
    /// @brief Successfully received a byte.
    Ok,

    /// @brief Could not receive a byte because it has timed out.
    TimedOut,
};

enum struct ReceivePacketResult : uint8_t {
    // We have successfully received a packet
    Ok,

    /// @brief We have reached the end of file.
    FileDone,

    /// @brief Could not receive the initial byte.
    InitialByteFail,

    /// @brief The host aborted the transmission
    Aborted,

    /// @brief The host sent an unexpected value
    Unknown,

    /// @brief Receiving the full packet has timed out
    Incomplete,

    /// @brief The packet is corrupted
    Malformed,
};

enum struct ReceiveFileResult : uint8_t {
    /// @brief The file received successfully
    Ok,

    /// @brief Receiving the file has failed
    Failed,
};

enum struct FileNamePacketResult : uint8_t {
    /// @brief The filename packet is valid and processed
    Ok,

    /// @brief The name of the file is empty
    EmptyName,

    /// @brief The file is too large, does not fit in the MCU's flash memory
    TooLarge,
};

enum struct DataPacketResult : uint8_t {
    /// @brief The data packet is valid and processed
    Ok,
};

class YModem {
    private:
        static uint8_t FileName[128];

        /// @brief  Tries to read a byte from the serial USB. If not available, waits 30 seconds before
        ///         returning a timeout error.
        /// @param out The pointer to write the received byte.
        /// @return The status of the receiving.
        static ReceiveByteResult ReceiveByte(uint8_t* out) {
            uint32_t start = millis();
            while (millis() < start + 5000) {
                if (SerialUSB.available())
                {
                    *out = (uint8_t)SerialUSB.read();
                    return ReceiveByteResult::Ok;
                }
            }

            return ReceiveByteResult::TimedOut;
        }

        /// @brief Sends a single byte to the host.
        /// @param data The byte to send.
        static void SendByte(uint8_t data)
        {
            SerialUSB.write(data);
        }

        /// @brief Receives a whole YModen packet.
        /// @param outputBuffer The buffer to write the packet to.
        /// @param packetLength The pointer to store the length of the packet.
        /// @return The receive status.
        static ReceivePacketResult ReceivePacket(uint8_t* outputBuffer, int32_t* packetLength)
        {
            uint16_t packetSize = 0;
            uint8_t receivedByte = 0;

            // Get and analyze Packet Header
            auto result = ReceiveByte(&receivedByte);

            if (result != ReceiveByteResult::Ok)
            {
                return ReceivePacketResult::InitialByteFail;
            }

            switch (receivedByte)
            {
                case SOH:
                    packetSize = PACKET_SIZE;
                    break;

                case STX:
                    packetSize = PACKET_1K_SIZE;
                    break;

                // Reached the end of transmission
                case EOT:
                    return ReceivePacketResult::FileDone;

                case CA:
                    if ((ReceiveByte(&receivedByte) == ReceiveByteResult::Ok) && (receivedByte == CA))
                    {
                        *packetLength = 0;
                        return ReceivePacketResult::Aborted;
                    }
                    else
                    {
                        // Was expecting second CA
                        return ReceivePacketResult::Unknown;
                    }
                case ABORT1:
                case ABORT2:
                    return ReceivePacketResult::Aborted;

                default:
                    return ReceivePacketResult::Unknown;
            }

            // Receive Packet Data
            outputBuffer[0] = receivedByte;
            uint8_t tst[128];
            Utils::Int2Str(&tst[0], packetSize);
            for (uint16_t i = 1; i < (packetSize + PACKET_OVERHEAD); i++)
            {
                if (ReceiveByte(&outputBuffer[i]) != ReceiveByteResult::Ok)
                {
                    return ReceivePacketResult::Incomplete;
                }
            }

            if (outputBuffer[PACKET_SEQNO_INDEX] != ((outputBuffer[PACKET_SEQNO_COMP_INDEX] ^ 0xff) & 0xff))
            {
                return ReceivePacketResult::Malformed;
            }

            *packetLength = packetSize;
            return ReceivePacketResult::Ok;
        }

        /// @brief  Process the packet content assuming it's a file name packet (first packet of an YModem transmission)
        ///         Does not send responses!
        /// @param packetBuffer The buffer containing the received packet
        /// @return The result of the process
        static FileNamePacketResult HandleFilenamePacket(uint8_t* packetBuffer)
        {
            if (packetBuffer[PACKET_HEADER] == 0)
            {
                return FileNamePacketResult::EmptyName;
            }

            /* Copy file name */
            int fileNameLength;
            for (int i = 0; i < FILE_NAME_LENGTH; i++)
            {
                uint8_t character = packetBuffer[PACKET_HEADER + i];

                if (character == '\0')
                {
                    break;
                }

                FileName[i] = character;
                fileNameLength = i;
            }
            fileNameLength++;
            FileName[fileNameLength] = '\0'; // Close file name

            uint8_t fileSizeText[16];
            uint8_t fileSizeTextLength = 0;
            for (int i = 0; i < FILE_SIZE_LENGTH; i++)
            {
                uint8_t character = packetBuffer[fileNameLength + 1 + i];

                if (character == ' ')
                {
                    break;
                }

                fileSizeText[i] = character;
                fileSizeTextLength = i;
            }
            fileSizeText[fileSizeTextLength + 1] = '\0';

            int32_t fileSize = 0;
            Utils::Str2Int(fileSizeText, &fileSize);

            /* Test the size of the image to be sent */
            /* Image size is greater than Flash size */
            if (fileSize > (Hardware::FlashSize))
            {
                return FileNamePacketResult::TooLarge;
            }
            
            /* erase user application area */
            // TODO
            // flash_err = FLASH_If_Erase(APPLICATION_ADDRESS);
            // if (flash_err != 0)
            // {
            //     /* End session */
            //     SendByte(CA);
            //     SendByte(CA);
            //     return -3;
            // }

            return FileNamePacketResult::Ok;
        }

        /// @brief WIP, Handles a data packet
        /// @return The result of the process
        static DataPacketResult HandleDataPacket()
        {
            // memcpy(buf_ptr, packet_data + PACKET_HEADER, packet_length);
            // ramsource = (uint32_t)buf;

            return DataPacketResult::Ok;
            
            /* Write received data in Flash */
            // TODO
            // if (FLASH_If_Write(&flashdestination, (uint32_t*) ramsource, (uint16_t) packet_length/4)  == 0)
            // {
            //     Send_Byte(ACK);
            // }
            // else /* An error occurred while writing to Flash memory */
            // {
            //     /* End session */
            //     Send_Byte(CA);
            //     Send_Byte(CA);
            //     return -2;
            // }
        }

    public:
        /// @brief Initializes the required hardwares for the YModem protocol
        static void Init() {
            SerialUSB.begin();
        }

        /// @brief Attempts to receive a firmware from a host.
        /// @param outputBuffer The pointer to a memory where the received file should be stored.
        /// @param fileSize Pointer to where the received file's size should be written
        static ReceiveFileResult ReceiveFile(uint8_t* outputBuffer, int32_t* fileSize)
        {
            uint8_t packetBuffer[PACKET_1K_SIZE + PACKET_OVERHEAD];
            int32_t packetLength;
            // The amount of packets we successfully received
            int32_t packetsReceived = 0;
            volatile uint32_t /*flashdestination,*/ ramsource, flash_err;

            while (true)
            {
                auto packetResult = ReceivePacket(&packetBuffer[0], &packetLength);

                if (packetResult == ReceivePacketResult::InitialByteFail)
                {
                    SendByte(CA);
                    SendByte(CA);
                    return ReceiveFileResult::Failed;
                }

                // If the packet was corrupted
                if (packetResult == ReceivePacketResult::Unknown
                    || packetResult == ReceivePacketResult::Malformed
                    || packetResult == ReceivePacketResult::Incomplete)
                {
                    SendByte(ACK);
                    return ReceiveFileResult::Failed;
                }

                // If we successfully reached the end of file
                if (packetResult == ReceivePacketResult::FileDone)
                {
                    SendByte(ACK);
                    return ReceiveFileResult::Ok;
                }

                // If the host aborted the packet
                if (packetResult == ReceivePacketResult::Aborted)
                {
                    SendByte(CA);
                    SendByte(CA);
                    return ReceiveFileResult::Failed;
                }

                // Check if the received packet's index matches what we are expecting
                if ((packetBuffer[PACKET_SEQNO_INDEX] & 0xff) != (packetsReceived & 0xff))
                {
                    SendByte(NAK);
                    return ReceiveFileResult::Failed;
                }

                // Status should be OK here, but add a clause just in case
                if (packetResult != ReceivePacketResult::Ok)
                {
                    SendByte(CA);
                    SendByte(CA);
                    return ReceiveFileResult::Failed;
                }

                // First packet, should contain the file name
                if (packetsReceived == 0)
                {
                    auto fileNameResult = HandleFilenamePacket(packetBuffer);

                    if (fileNameResult == FileNamePacketResult::EmptyName)
                    {
                        SendByte(ACK);
                        return ReceiveFileResult::Failed;
                    }
                    else if (fileNameResult == FileNamePacketResult::TooLarge)
                    {
                        SendByte(CA);
                        SendByte(CA);
                    }
                    else if (fileNameResult == FileNamePacketResult::Ok)
                    {
                        packetsReceived++;
                        SendByte(ACK);
                        SendByte(CRC16);
                    }
                    else
                    {
                        // Auxiliary branch if any case is not handled
                        SendByte(CA);
                        SendByte(CA);
                    }
                }
                else // Regular Data Packets
                {
                    auto dataPacketResult = HandleDataPacket();

                    if (dataPacketResult == DataPacketResult::Ok)
                    {
                        SendByte(ACK);
                        packetsReceived++;
                    }
                    else
                    {
                        SendByte(CA);
                        SendByte(CA);
                        return ReceiveFileResult::Failed;
                    }
                }

                //SendByte(CRC16);
            }

            SendByte(CA);
            SendByte(CA);
            return ReceiveFileResult::Failed;
        }
};

uint8_t YModem::FileName[128];