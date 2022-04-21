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

void TEMPUS_API USB_StartThread(void)
{
	try
	{
		for (; ; )
		{
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
				}

				bCommandRequested = true;

				break;

			case WAIT_OBJECT_0 + 1:
				// hEvent[1] was signaled == FTDI Device Event

				ftStatus = FT_GetStatus(fthDevice, &dwRxBytesReceivedTemp, &dwTxBytesWritten, &dwDeviceEvent);
				if (ftStatus != FT_OK)
					throw std::exception("USB Communication Error: bad status");
				if (dwDeviceEvent != FT_EVENT_RXCHAR)
					throw std::exception("USB Communication Error: Unknown event occured");

				ftStatus = FT_Read(fthDevice, byRxBuffer, dwRxBytesReceivedTemp, &dwRxBytesReceived);
				if (ftStatus != FT_OK)
					throw std::exception("USB Communication Error: Unable to read data from device");

				if (bCommandRequested == true)
				{
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
				/*
				if (dwRxBytesReceived >= FTDI_DAQ_IN_REQUEST_SIZE && InterlockedCompareExchange(&lMainFormDAQEnabled, 1, 1))
				{
					memcpy(&DSPPacket, byRxBuffer, sizeof(DSPPacket));

					DAQScopeDataBuffer.Zero();
					double* pdbDAQScopeDataBufferWrite = DAQScopeDataBuffer.Write();
					for (i = 0, j = 0; i < DAQ_SCOPE_NUM_OF_SAMPLES_PER_CH; i++, j += DAQ_SCOPE_CH_SIZE_DIV)
						for (k = 0; k < DAQ_NUM_OF_CHANNELS; k++)
							for (l = 0; l < DAQ_SCOPE_CH_SIZE_DIV; l++)
								*(pdbDAQScopeDataBufferWrite + k * DAQ_SCOPE_NUM_OF_SAMPLES_PER_CH + i) += (short)DSPPacket.DAQData[(j + l) * DAQ_NUM_OF_CHANNELS + k];
					DAQScopeDataBuffer /= (double)DAQ_SCOPE_CH_SIZE_DIV;

					Synchronize(&DisplayData);

					LoggerClient.write(byRxBuffer + sizeof(TDSPMessage), sizeof(DSPPacket.DAQData), 0);
				}*/

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
	if (bBoardConnected == false || bCommandProcessed == false)
	{
		return;
	}

	PCMessage.Command = CMD_DRIVE_TURN_ON;

	SetEvent(hCommandEvent);
	bCommandProcessed = false;
}

void TEMPUS_API Drive_TurnOff(void)
{
	if (bBoardConnected == false || bCommandProcessed == false)
	{
		return;
	}

	PCMessage.Command = CMD_DRIVE_TURN_OFF;

	SetEvent(hCommandEvent);
	bCommandProcessed = false;
}

unsigned int TEMPUS_API Mnemonic_Read(const char* mnemonic)
{
	if (bBoardConnected == false || bCommandProcessed == false)
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
	if (bBoardConnected == false || bCommandProcessed == false)
		return;

	if (mnemonic == "")
		return;

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
	return void TEMPUS_API();
}

void TEMPUS_API DAQ_Stop(void)
{
	return void TEMPUS_API();
}

void TEMPUS_API Log_Start(void)
{
	return void TEMPUS_API();
}

void TEMPUS_API Log_Stop(void)
{
	return void TEMPUS_API();
}
