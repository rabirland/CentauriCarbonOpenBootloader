using System.IO.Ports;
using System.Text;

namespace YModemTester;

public class YModemTransmitter
{
    const byte SOH = 1;
    const byte STX = 2;
    const byte EOT = 4;
    const byte ACK = 6;
    const byte NAK = 0x15;
    const byte C = 0x43;
    const byte CAN = 0x18;

    public const int DataSize = 1024;
    public const int CrcSize = 2;

    SerialPort serialPort;
    public DateTime startDateTime = new DateTime(0);

    public YModemTransmitter(SerialPort sp, bool timeout)
    {
        serialPort = sp;
        startDateTime = new DateTime(0);
        if (timeout)
        {
            serialPort.ReadTimeout = 3000;
        }
        else
        {
            serialPort.ReadTimeout = 1000000;
        }
    }

    public bool SendFile(string path)
    {
        var fileStream = new FileStream(path, FileMode.Open, FileAccess.Read);
        var packetCount = (int)(fileStream.Length - 1) / DataSize + 1;

        var invertedPacketNumber = 255;
        var data = new byte[DataSize];
        var CRC = new byte[CrcSize];

        var crc16Ccitt = new Crc16Ccitt(InitialCrcValue.Zeros);
        var packetIndex = 0;
        Thread.Sleep(1);

        try
        {
            serialPort.DiscardInBuffer();
            if (startDateTime.Ticks == 0)
            {
                startDateTime = DateTime.Now;
            }

            Console.Write($"Sending Initial packet 0 / {packetCount}...");
            SendInitialPacket(STX, packetIndex, invertedPacketNumber, packetCount, data, DataSize, path, fileStream, CRC, CrcSize);
            Console.Write($"Sent...");

            var read = (byte)serialPort.ReadByte();
            if (read != ACK)
            {
                Console.WriteLine($"NOT ACK: 0x{read:X}");
                return false;
            }

            if (serialPort.ReadByte() != C)
            {
                Console.WriteLine($"NOT C: 0x{read:X}");
                return false;
            }

            Console.WriteLine("ACK");
            packetIndex++;

            while (fileStream.Position < fileStream.Length)
            {
                Console.Write($"Sending Packet {packetIndex} / {packetCount}...");
                var filePositionBefore = fileStream.Position;
                var readBytes = fileStream.Read(data, 0, DataSize);

                if (readBytes == 0)
                {
                    Console.WriteLine("Could not read from file");
                    break;
                }

                // Fill the remaining bytes with 0x1A
                for (int i = readBytes; i < DataSize; i++)
                {
                    data[i] = 0x1A;
                }

                // Roll packet Index
                if (packetIndex > 255)
                {
                    packetIndex -= 256;
                }

                var fileName = Path.GetFileName(path);
                invertedPacketNumber = 255 - packetIndex;
                CRC = crc16Ccitt.ComputeChecksumBytes(data);

                SendPacket(STX, packetIndex, invertedPacketNumber, data, DataSize, CRC, CrcSize);
                Console.Write($"Sent...");

                int signal = serialPort.ReadByte();
                if (signal == ACK)
                {
                    Console.WriteLine("ACK");
                    packetIndex++;
                }
                else if (signal == NAK)
                {
                    Console.WriteLine("NAK, Resending");
                    fileStream.Position = filePositionBefore;
                    packetIndex--;
                }
                else if (signal == CAN)
                {
                    Console.WriteLine("CAN, Client Rejected");
                    return false;
                }
                else
                {
                    Console.WriteLine($"Unexpected: 0x{signal:X}");
                    return false;
                }
            }

            Console.Write("Sending EOT...");
            serialPort.Write(new byte[] { EOT }, 0, 1);
            Console.Write("Sent...");

            int act1 = serialPort.ReadByte();
            if (act1 == ACK)
            {
                Console.Write("ACK1...");
            }
            else if (act1 == NAK)
            {
                Console.Write("NAK, Sending EOT again...");
                serialPort.Write(new byte[] { EOT }, 0, 1);
                Console.Write("Sent...");
            }
            else
            {
                Console.WriteLine($"Unexpected: 0x{act1:X}");
                return false;
            }
            

            int act2 = serialPort.ReadByte();
            if (act2 == ACK)
            {
                Console.Write("ACK2...");
            }
            else
            {
                Console.WriteLine($"Unexpected: 0x{act2:X}");
                return false;
            }
            Console.WriteLine();

            Console.Write($"Waiting for CRC request 0x{C:X}...");
            var crcRequest = serialPort.ReadByte();
            if (crcRequest != C)
            {
                Console.WriteLine($"Unexpected: 0x{crcRequest:X}");
                return false;
            }
            Console.WriteLine($"Done");

            packetIndex = 0;
            invertedPacketNumber = 255;
            data = new byte[128];
            data[0] = 0x00;
            data[1] = data[3] = data[5] = 0x30;
            data[2] = data[4] = 0x20;
            CRC = new byte[CrcSize];
            CRC = crc16Ccitt.ComputeChecksumBytes(data);

            Console.Write($"Sending Closing Packet...");
            SendClosingPacket(SOH, packetIndex, invertedPacketNumber, data, 128, CRC, CrcSize);
            Console.Write("Sent...");

            var ack3 = serialPort.ReadByte();
            if (ack3 != ACK)
            {
                Console.WriteLine($"Unexpected: 0x{ack3:X}");
                return false;
            }
            Console.WriteLine("ACK");
            TimeSpan span = DateTime.Now - startDateTime;

            Console.WriteLine("File successfully sent");
        }
        catch (Exception e)
        {
            Console.WriteLine($"Exception: {e.Message}");
            return false;
        }
        finally
        {
            fileStream.Close();
        }


        return true;
    }

    private void SendInitialPacket(
        byte STX,
        int packetNumber,
        int invertedPacketNumber,
        int packetCount,
        byte[] data,
        int dataSize,
        string path,
        FileStream fileStream,
        byte[] CRC,
        int crcSize)
    {
        var fileName = Path.GetFileName(path).Replace(" ", "_");
        var fileNameSize = fileStream.Length.ToString();
        var lastWriteTime = File.GetLastWriteTime(path);
        var epoch = new DateTimeOffset(1970, 1, 1, 0, 0, 0, TimeSpan.Zero);
        var unixTime = (lastWriteTime.ToUniversalTime().Ticks - epoch.Ticks) / TimeSpan.TicksPerSecond;
        var fileModTime = Convert.ToString(unixTime, 8);
        var packageCount = Convert.ToString(packetCount, 8);
        var fileNameBytes = Encoding.GetEncoding("ascii").GetBytes(fileName);

        int i;
        for (i = 0; i < fileNameBytes.Length && (fileNameBytes[i] != 0); i++)
        {
            data[i] = (byte)fileNameBytes[i];
        }
        data[i] = 0;
        int j;
        for (j = 0; j < fileNameSize.Length && (fileNameSize.ToCharArray()[j] != 0); j++)
        {
            data[i + j + 1] = (byte)fileNameSize.ToCharArray()[j];
        }
        data[i + j + 1] = (byte)(' ');
        int m;
        for (m = 0; m < fileModTime.Length && (fileModTime.ToCharArray()[m] != 0); m++)
        {
            data[i + j + m + 2] = (byte)fileModTime.ToCharArray()[m];
        }
        data[i + j + m + 2] = (byte)(' ');
        int n;
        for (n = 0; n < packageCount.Length && (packageCount.ToCharArray()[n] != 0); n++)
        {
            data[i + j + n + m + 3] = (byte)packageCount.ToCharArray()[n];
        }
        data[i + j + m + n + 3] = (byte)(' ');
        for (int k = (i + j + m + n + 4); k < dataSize; k++)
        {
            data[k] = 0;
        }

        Crc16Ccitt crc16Ccitt = new Crc16Ccitt(InitialCrcValue.Zeros);
        CRC = crc16Ccitt.ComputeChecksumBytes(data);
        SendPacket(STX, packetNumber, invertedPacketNumber, data, dataSize, CRC, crcSize);
    }

    private void SendClosingPacket(byte SOH, int packetNumber, int invertedPacketNumber, byte[] data, int dataSize, byte[] CRC, int crcSize)
    {
        Crc16Ccitt crc16Ccitt = new Crc16Ccitt(InitialCrcValue.Zeros);
        CRC = crc16Ccitt.ComputeChecksumBytes(data);
        SendPacket(SOH, packetNumber, invertedPacketNumber, data, dataSize, CRC, crcSize);
    }

    private void SendPacket(byte STX, int packetNumber, int invertedPacketNumber, byte[] data, int dataSize, byte[] CRC, int crcSize)
    {
        int packetSize = 1 + 1 + 1 + dataSize + crcSize;
        byte[] packet = new byte[packetSize];

        packet[0] = STX;
        packet[1] = (byte)packetNumber;
        packet[2] = (byte)invertedPacketNumber;
        Array.Copy(data, 0, packet, 3, dataSize);
        Array.Copy(CRC, 0, packet, 3 + dataSize, crcSize);

        serialPort.Write(packet, 0, packet.Length);
    }
}
