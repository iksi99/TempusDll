#include "tempus_api.h"

#include <exception>

bool TEMPUS_API USB_Connect(void)
{
	bBoardConnected = true;
	bCommandProcessed = true;
	bDeviceHandleOpened = false;
	bCommandRequested = false;

	// initialize all variables used
	hCommandEvent = CreateEvent(
		NULL,   // default security attributes
		FALSE,  // auto-reset event object
		FALSE,  // initial state is nonsignaled
		NULL	// unnamed object
	);
	if (hCommandEvent == INVALID_HANDLE_VALUE)
	{
		bBoardConnected = false;
		return false;
	}
	hEvents[0] = hCommandEvent;

	hDeviceEvent = CreateEvent(
		NULL,   // default security attributes
		FALSE,  // auto-reset event object
		FALSE,  // initial state is nonsignaled
		NULL	// unnamed object
	);
	if (hDeviceEvent == INVALID_HANDLE_VALUE)
	{
		bBoardConnected = false;
		return false;
	}
	hEvents[1] = hDeviceEvent;

	iNumOfCommErrors = 0;

	// configure USB Communication
	try
	{
		ftStatus = FT_Open(0, &fthDevice);
		if (ftStatus != FT_OK)
			throw std::exception("USB Communication Error: Unable to open device");

		ftStatus = FT_Purge(fthDevice, FT_PURGE_RX | FT_PURGE_TX);
		if (ftStatus != FT_OK)
			throw std::exception("USB Communication Error: Unable to configure device");

		ftStatus = FT_SetUSBParameters(fthDevice, (ULONG)FTDI_DAQ_IN_TRANSFER_SIZE, (ULONG)0);
		if (ftStatus != FT_OK)
			throw std::exception("USB Communication Error: Unable to configure device");

		ftStatus = FT_SetTimeouts(fthDevice, (ULONG)FTDI_READ_TIMEOUT, (ULONG)FTDI_WRITE_TIMEOUT);
		if (ftStatus != FT_OK)
			throw std::exception("USB Communication Error: Unable to configure device");

		ftStatus = FT_SetLatencyTimer(fthDevice, (UCHAR)FTDI_LATENCY_TIMER_VALUE);
		if (ftStatus != FT_OK)
			throw std::exception("USB Communication Error: Unable to configure device");

		ftStatus = FT_SetEventNotification(fthDevice, (DWORD)FT_EVENT_RXCHAR, hDeviceEvent);
		if (ftStatus != FT_OK)
			throw std::exception("USB Communication Error: Unable to configure device");
	}
	catch (const std::exception& e)
	{
		if (fthDevice)
			FT_Close(fthDevice);

		return false;
	}

	return true;
}

void TEMPUS_API MakeThread()
{
	DWORD dwThreadId = 0;
	CreateThread(
		NULL,                   // default security attributes
		0,                      // use default stack size  
		USB_StartThread,        // thread function name
		NULL,                   // argument to thread function 
		0,                      // use default creation flags 
		&dwThreadId);   // returns the thread identifier 
}

DWORD TEMPUS_API USB_StartThread(LPVOID param)
{
	try
	{
		for (; ; )
		{

			DWORD dwRxBytesTest = 10;
			DWORD dwTxBytesTest = 10;
			DWORD dwDeviceEventTest = 10;

			dwEvent = WaitForMultipleObjects(
				2,			// number of objects in array
				hEvents,	// array of objects
				FALSE,		// wait for any
				1000		// timeout in [ msec ]
			);

			switch (dwEvent)
			{
			case WAIT_OBJECT_0 + 0:
				// hEvent[0] was signaled == Command Event

				memcpy(byTxBuffer, &PCMessage, sizeof(PCMessage));
				ftStatus = FT_Write(fthDevice, byTxBuffer, FTDI_MSG_OUT_REQUEST_SIZE, &dwTxBytesWritten);
				if (ftStatus != FT_OK)
					throw std::exception("USB Communication Error: Unable to write data to device");

				if (bTerminate == true)
				{
					// close all handles
					CloseHandle(hCommandEvent);
					CloseHandle(hDeviceEvent);
					if (fthDevice)
					{
						FT_Close(fthDevice);
						bBoardConnected = false;
						bDeviceHandleOpened = false;
						bCommandProcessed = true;
					}

					return 0;
				}

				bCommandRequested = true;

				break;

			case WAIT_OBJECT_0 + 1:
				// hEvent[1] was signaled == FTDI Device Event

				//ftStatus = FT_GetStatus(fthDevice, &dwRxBytesReceivedTemp, &dwTxBytesWritten, &dwDeviceEvent);
				ftStatus = FT_GetStatus(fthDevice, &dwRxBytesTest, &dwTxBytesTest, &dwDeviceEventTest);
				if (ftStatus != FT_OK)
					throw std::exception("USB Communication Error: bad status");
				if (dwDeviceEventTest != FT_EVENT_RXCHAR)
				{
					OutputDebugStringA("A\n");
					//throw std::exception("USB Communication Error: Unknown event occured");
					//break;
				}

				if (bCommandRequested == true)
				{
					ftStatus = FT_Read(fthDevice, byRxBuffer, dwRxBytesTest, &dwRxBytesReceived);
					if (ftStatus != FT_OK)
						throw std::exception("USB Communication Error: Unable to read data from device");

					memcpy(&DSPMessage, byRxBuffer, sizeof(DSPMessage));

					if (PCMessage.Command == CMD_STOP_USB_DAQ)
					{
						DSPMessage.Response = CMD_STOP_USB_DAQ;

						// TODO: Matlab Compatible
						//InterlockedIncrement(&lMainFormCommandProcessed);

						bCommandRequested = false;
					}
					else if (DSPMessage.Response != PCMessage.Command)
					{
						iNumOfCommErrors++;
					}
					else if (PCMessage.Command == CMD_MNEMONIC_READ || PCMessage.Command == CMD_MNEMONIC_WRITE)
					{
						for (iCounter = 0; iCounter < MNEMONIC_MAX_SIZE; iCounter++)
						{
							if (PCMessage.Mnemonic.Code[iCounter] != DSPMessage.Mnemonic.Code[iCounter])
							{
								iNumOfCommErrors++;

								break;
							}
						}

						if (iCounter == MNEMONIC_MAX_SIZE)
						{
							// TODO: Matlab Compatible
							//InterlockedIncrement(&lMainFormCommandProcessed);

							bCommandRequested = false;

							iNumOfCommErrors = 0;
						}
					}
					else
					{
						// TODO: Matlab Compatible
						//InterlockedIncrement(&lMainFormCommandProcessed);

						bCommandRequested = false;

						iNumOfCommErrors = 0;
					}

					if (iNumOfCommErrors > 3)
					{
						// TODO: Matlab Compatible
						//InterlockedIncrement(&lMainFormCommandProcessed);

						iNumOfCommErrors = 0;

						bCommandRequested = false;
					}
				}

				// TODO:Port DAQ to something matlab compatible
				
				if (dwRxBytesTest >= FTDI_DAQ_IN_REQUEST_SIZE && bDAQEnabled)
				{
					ftStatus = FT_Read(fthDevice, byRxBuffer, dwRxBytesTest, &dwRxBytesReceived);
					if (ftStatus != FT_OK)
						throw std::exception("USB Communication Error: Unable to read data from device");

					memcpy(&DSPPacket, byRxBuffer, sizeof(DSPPacket));

					for (int i = 0; i < DAQ_NUM_OF_SAMPLES_PER_CH; i++)
					{
						for (int j = 0; j < DAQ_NUM_OF_CHANNELS; j++)
						{
							char separator = (j == DAQ_NUM_OF_CHANNELS - 1) ? '\n' : ',';
							myfile << (short)DSPPacket.DAQData[i * DAQ_NUM_OF_CHANNELS + j] << separator;
						}
					}

					return 0;

					/*for (int i = 0; i < DAQ_NUM_OF_SAMPLES_PER_CH * DAQ_NUM_OF_CHANNELS; i++)
					{
						DAQBuffer[i] = 0;
					}*/

					OutputDebugStringA("B\n");

					/*for (int i = 0, j = 0; i < DAQ_SCOPE_NUM_OF_SAMPLES_PER_CH; i++, j += DAQ_SCOPE_CH_SIZE_DIV)
						for (int k = 0; k < DAQ_NUM_OF_CHANNELS; k++)
							for (int l = 0; l < DAQ_SCOPE_CH_SIZE_DIV; l++)
								*(DAQBuffer + k * DAQ_SCOPE_NUM_OF_SAMPLES_PER_CH + i) += (short)DSPPacket.DAQData[(j + l) * DAQ_NUM_OF_CHANNELS + k] / (double)DAQ_SCOPE_CH_SIZE_DIV;*/
				}

				break;

			case WAIT_TIMEOUT:
				// Timeout action

				ftStatus = FT_GetStatus(fthDevice, &dwRxBytesReceivedTemp, &dwTxBytesWritten, &dwDeviceEvent);
				if (ftStatus != FT_OK)
					throw std::exception("USB Communication Error: bad status");

				break;

			default:
				// Return value is invalid.
				break;
			}
		}
	}
	catch (const std::exception& e)
	{
		if (fthDevice)
			FT_Close(fthDevice);

		bCommandRequested = false;
	}

	return 0;
}

void TEMPUS_API USB_Disconnect(void)
{
	if (bBoardConnected == false || bCommandProcessed == false)
	{
		return;
	}

	PCMessage.Command = CMD_DRIVE_TURN_OFF;
	bTerminate = true;
	SetEvent(hCommandEvent);
}

void TEMPUS_API Drive_TurnOn(void)
{
	if (bBoardConnected == false)
	{
		return;
	}

	PCMessage.Command = CMD_DRIVE_TURN_ON;

	SetEvent(hCommandEvent);
	bCommandProcessed = false;
}

void TEMPUS_API Drive_TurnOff(void)
{
	if (bBoardConnected == false)
	{
		return;
	}

	PCMessage.Command = CMD_DRIVE_TURN_OFF;

	SetEvent(hCommandEvent);
	bCommandProcessed = false;
}

unsigned int TEMPUS_API Mnemonic_Read(const char* mnemonic)
{
	if (bBoardConnected == false)
	{
		return 0;
	}

	if (mnemonic == "")
	{
		return 0;
	}

	PCMessage.Command = CMD_MNEMONIC_READ;
	for (int i = 0; (i < MNEMONIC_MAX_SIZE) && (i < strlen(mnemonic)); i++) {
		PCMessage.Mnemonic.Code[i] = mnemonic[i]; // original was i+1 need to test for weird stuff
	}

	SetEvent(hCommandEvent);
	bCommandProcessed = false;

	return 0; // TODO proper reading, maybe with semaphore
}

void TEMPUS_API Mnemonic_Write(const char* mnemonic, unsigned short data)
{
	if (bBoardConnected == false)
		return;

	if (mnemonic == "")
		return;

	int length = strlen(mnemonic);
	PCMessage.Command = CMD_MNEMONIC_WRITE;
	for (int i = 0; (i < MNEMONIC_MAX_SIZE) && (i < strlen(mnemonic)); i++)
		PCMessage.Mnemonic.Code[i] = mnemonic[i]; // original was i+1 need to test for weird stuff

	PCMessage.Mnemonic.Data = data;

	SetEvent(hCommandEvent);
	bCommandProcessed = false;
}

void TEMPUS_API Params_Load(void)
{
	return void TEMPUS_API();
}

void TEMPUS_API Params_Save(void)
{
	return void TEMPUS_API();
}

void TEMPUS_API DAQ_Start(void)
{
	if (bBoardConnected == false)
		return;

	PCMessage.Command = CMD_START_USB_DAQ;

	myfile.open("DAQ.csv");

	SetEvent(hCommandEvent);
	bDAQEnabled = true;
	bCommandProcessed = false;
}

void TEMPUS_API DAQ_Stop(void)
{
	if (bBoardConnected == false)
		return;

	PCMessage.Command = CMD_STOP_USB_DAQ;

	myfile.close();

	SetEvent(hCommandEvent);
	bDAQEnabled = false;
	bCommandProcessed = false;
}

void TEMPUS_API Log_Start(void)
{
	return void TEMPUS_API();
}

void TEMPUS_API Log_Stop(void)
{
	return void TEMPUS_API();
}
