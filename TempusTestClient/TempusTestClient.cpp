// TempusTestClient.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <windows.h>

#include "tempus_api.h"

int main()
{
	bool connectionSuccess = USB_Connect();
    if (connectionSuccess)
    {
        std::cout << "Successfully connected over USB" << std::endl;
    }
    else
    {
        std::cout << "Failed to connect to USB" << std::endl;
        return 1;
    }
    
    std::cout << "Creating Thread..." << std::endl;
    DWORD dwThreadId = 0;
    CreateThread(
        NULL,                   // default security attributes
        0,                      // use default stack size  
        USB_StartThread,        // thread function name
        NULL,                   // argument to thread function 
        0,                      // use default creation flags 
        &dwThreadId);   // returns the thread identifier 
    std::cout << "Thread Created." << std::endl;
    Drive_TurnOn();
    std::cout << "Drive Turned On." << std::endl;

    unsigned short speed = 0;

    std::cout << "Starting DAQ" << std::endl;
    DAQ_Start();

    while (true)
    {
        std::cout << "Enter new speed: ";
        std::cin >> speed;
        Mnemonic_Write("WR", speed);
        if (speed == 0)
        {
            system("PAUSE");
            break;
        }
    }

    std::cout << "Stopping DAQ" << std::endl;
    DAQ_Stop();

    std::cout << "Turning Everything off..." << std::endl;
    Drive_TurnOff();
    std::cout << "Drive Turned off" << std::endl;
	USB_Disconnect();
    std::cout << "USB Disconnected" << std::endl;
}