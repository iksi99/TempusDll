#pragma once

#include "ftd2xx.h"

#ifdef TEMPUS_EXPORTS
#define TEMPUS_API __declspec(dllexport)
#else
#define TEMPUS_API __declspec(dllimport)
#endif

// FTDI Constants
#define FTDI_READ_TIMEOUT				( 3000	)	// [ms]
#define FTDI_WRITE_TIMEOUT				( 3000	) 	// [ms]
#define FTDI_LATENCY_TIMER_VALUE		( 255	)	// [ms]

#define FTDI_EVENT_CHAR					( 0xFF	)
#define FTDI_EVENT_CHAR_ENABLE			( 0		)
#define FTDI_ERROR_CHAR					( 0		)
#define FTDI_ERROR_CHAR_ENABLE			( 0		)

#define FTDI_MSG_IN_SIZE_MULT			( 1		)
#define FTDI_MSG_OUT_SIZE_MULT			( 1		)

#define FTDI_MSG_IN_TRANSFER_SIZE		( 64 * FTDI_MSG_IN_SIZE_MULT	)	// [bytes]
#define FTDI_MSG_OUT_TRANSFER_SIZE		( 64 * FTDI_MSG_OUT_SIZE_MULT	)	// [bytes]

#define FTDI_MSG_IN_REQUEST_SIZE		( 62 * FTDI_MSG_IN_SIZE_MULT	)	// [bytes]
#define FTDI_MSG_OUT_REQUEST_SIZE		( 62 * FTDI_MSG_OUT_SIZE_MULT	)	// [bytes]

#define FTDI_MSG_RX_BUFFER_SIZE			( FTDI_MSG_IN_TRANSFER_SIZE * 2	)	// [bytes]
#define FTDI_MSG_TX_BUFFER_SIZE			( FTDI_MSG_OUT_TRANSFER_SIZE	)	// [bytes]

#define FTDI_DAQ_IN_SIZE_MULT			( 512	)
#define FTDI_DAQ_OUT_SIZE_MULT			( 1		)

#define FTDI_DAQ_IN_TRANSFER_SIZE		( 64 * FTDI_DAQ_IN_SIZE_MULT	)	// [bytes]
#define FTDI_DAQ_OUT_TRANSFER_SIZE		( 64 * FTDI_DAQ_OUT_SIZE_MULT	)	// [bytes]

#define FTDI_DAQ_IN_REQUEST_SIZE		( 62 * FTDI_DAQ_IN_SIZE_MULT	)	// [bytes]
#define FTDI_DAQ_OUT_REQUEST_SIZE		( 62 * FTDI_DAQ_OUT_SIZE_MULT	)	// [bytes]

#define FTDI_DAQ_RX_BUFFER_SIZE			( FTDI_DAQ_IN_TRANSFER_SIZE * 2	)	// [bytes]
#define FTDI_DAQ_TX_BUFFER_SIZE			( FTDI_DAQ_OUT_TRANSFER_SIZE	)	// [bytes]

// DAQ constants
#define DAQ_NUM_OF_CHANNELS				( 8		)	// # of 16b channels
#define DAQ_SAMPLE_RATE					( 10000	)	// [Hz]
#define DAQ_NUM_OF_SAMPLES_PER_CH		( FTDI_DAQ_IN_REQUEST_SIZE / DAQ_NUM_OF_CHANNELS / 2 - 1 ) // # of 16b samples per channel

#define DAQ_SCOPE_CH_SIZE_DIV			( 4 	)
#define DAQ_SCOPE_NUM_OF_SAMPLES_PER_CH	( DAQ_NUM_OF_SAMPLES_PER_CH / DAQ_SCOPE_CH_SIZE_DIV )
#define DAQ_SCOPE_SIZE_LIMIT			( DAQ_SCOPE_NUM_OF_SAMPLES_PER_CH * 5 * 2 )
#define DAQ_SCOPE_REFRESH_INTERVAL		( 200	)	// [ms]

// Enums
typedef enum
{
	STATE_WAIT,
	STATE_READY,
	STATE_DRIVE_TURNED_ON,
	STATE_DRIVE_ERROR
} DeviceState;

typedef enum
{
	CMD_WAKE_UP,
	CMD_QUIT,
	CMD_START,
	CMD_STOP,

	CMD_DRIVE_TURN_ON,
	CMD_DRIVE_TURN_OFF,
	CMD_DRIVE_RESET,
	CMD_DRIVE_CHECK,

	CMD_LOAD_PARAMS,
	CMD_SAVE_PARAMS,

	CMD_MNEMONIC_READ,
	CMD_MNEMONIC_WRITE,

	CMD_START_USB_DAQ,
	CMD_STOP_USB_DAQ
} Command;

typedef enum
{
	RX_POS_CMD = 0,
	RX_POS_MNEMONIC_CMD
} RxPosition;

typedef enum
{
	TX_POS_RESPONSE = 0,
	TX_POS_STATUS,
	TX_POS_MNEMONIC_DATA
} TxPosition;

// Message struct typedefs
#define MNEMONIC_MAX_SIZE 3

typedef struct
{
	unsigned short Command;
	struct
	{
		unsigned short Code[MNEMONIC_MAX_SIZE];
		unsigned short Data;
	} Mnemonic;
} TPCMessage;

typedef struct
{
	unsigned short Response;
	struct
	{
		unsigned short Code[MNEMONIC_MAX_SIZE];
		unsigned short Data;
	} Mnemonic;
	unsigned short State;
	unsigned short Status;
	unsigned short Spare[1];
} TDSPMessage;

typedef struct
{
	TDSPMessage	Message;
	unsigned short DAQData[DAQ_NUM_OF_SAMPLES_PER_CH * DAQ_NUM_OF_CHANNELS];
} TDSPPacket;

TPCMessage PCMessage;
TDSPMessage DSPMessage;

DWORD dwRxBytesReceived;
DWORD dwRxBytesReceivedTemp;
DWORD dwTxBytesWritten;
DWORD dwDeviceEvent;
DWORD dwNumberOfBytesToWrite;
DWORD dwNumberOfBytesWritten;

BOOL bBoardConnected;
BOOL bDeviceHandleOpened;
BOOL bCommandRequested;
BOOL bCommandProcessed;
BOOL bTerminate;

FT_HANDLE fthDevice;
FT_STATUS ftStatus;

HANDLE hCommandEvent;
HANDLE hDeviceEvent;

HANDLE hEvents[2];
DWORD  dwEvent;

char byRxBuffer[FTDI_DAQ_RX_BUFFER_SIZE];
char byTxBuffer[FTDI_DAQ_TX_BUFFER_SIZE];

unsigned int iNumOfCommErrors;
unsigned int iCounter;

// Functions for USB connection
extern "C" bool TEMPUS_API USB_Connect(void);
extern "C" DWORD TEMPUS_API USB_StartThread(LPVOID param);
extern "C" void TEMPUS_API USB_Disconnect(void);

// Functions for turning drive on and off
extern "C" void TEMPUS_API Drive_TurnOn(void);
extern "C" void TEMPUS_API Drive_TurnOff(void);

// Functions for reading and writing data to device
extern "C" unsigned int TEMPUS_API Mnemonic_Read(const char* mnemonic);
extern "C" void TEMPUS_API Mnemonic_Write(const char* mnemonic, unsigned short data);

// Functions for loading and saving parameters
extern "C" void TEMPUS_API Params_Load(void);
extern "C" void TEMPUS_API Params_Save(void);

// Functions for data acquisition and logging
extern "C" void TEMPUS_API DAQ_Start(void);
extern "C" void TEMPUS_API DAQ_Stop(void);
extern "C" void TEMPUS_API Log_Start(void);
extern "C" void TEMPUS_API Log_Stop(void);