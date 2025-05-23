/*
 Very basic YModem uploader. Based on https://github.com/miuser00/YModem
 */

using System.IO.Ports;
using YModemTester;

if (args.Length > 0 && args[0] == "-h")
{
    Console.WriteLine("Usage:");
    Console.WriteLine("YModemTester.exe [COM Port Name] [File path To Upload]");
    Console.WriteLine("If the COM port is unavailable, the app will wait until it becomes available.");
    return;
}

if (args.Length < 2)
{
    Console.WriteLine("Please provide the COM port name and file path to upload.");
    Console.WriteLine("Use 'YModemTester.exe -h' for reference");
    return;
}

if (File.Exists(args[1]) == false)
{
    Console.WriteLine($"File '{args[1]}' does not exist.");
    return;
}

var targetPort = args[0];

Console.WriteLine($"Waiting for COM port '{targetPort}' to be available");
var consolePos = Console.GetCursorPosition();
while (true)
{
    
    var ports = SerialPort.GetPortNames();

    Console.SetCursorPosition(consolePos.Left, consolePos.Top);
    Console.WriteLine("Available Ports: " + string.Join(", ", ports));

    if (ports.Contains(targetPort))
    {
        break;
    }
}


var serialPort = new SerialPort(targetPort);
var transmitter = new YModemTransmitter(serialPort, true);
serialPort.ReadTimeout = 90000;
serialPort.DtrEnable = true;
serialPort.ReadBufferSize = 2048;
serialPort.WriteBufferSize = 2048;
serialPort.Open();

transmitter.SendFile(args[1]);