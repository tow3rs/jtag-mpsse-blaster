#ifdef _WIN32
#include <Windows.h>
#else
#include <dlfcn.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
typedef unsigned int	DWORD;
typedef unsigned int	ULONG;
typedef unsigned short	USHORT;
typedef unsigned short	SHORT;
typedef unsigned char	UCHAR;
typedef unsigned short	WORD;
typedef unsigned char	BYTE;
typedef unsigned int	BOOL;
typedef unsigned char	CHAR;
typedef char*			PCHAR;
typedef void*			PVOID;
typedef void*			HANDLE;
typedef unsigned int	LONG;
typedef int				INT;
typedef unsigned int	UINT;
typedef void*			HANDLE;
#define TRUE			1
#define FALSE			0
#endif

#define BUFFER_SIZE 65536

// Opcodes
#define FT_OPCODE_CLOCK_TMS 0x6B
#define FT_OPCODE_SEND_IMMEDIATE 0x87
#define FT_OPCODE_CLOCK_DATA_BITS 0x3B
#define FT_OPCODE_DISABLE_LOOPBACK 0x85
#define FT_OPCODE_CLOCK_DATA_BYTES 0x39
#define FT_OPCODE_SET_CLOCK_DIVIDER 0x86
#define FT_OPCODE_DISABLE_DIVIDE_BY_5 0x8A
#define FT_OPCODE_SET_DATA_BITS_LOW_BYTE 0x80
#define FT_OPCODE_SET_DATA_BITS_HIGH_BYTE 0x82
#define FT_OPCODE_DISABLE_ADAPTIVE_CLOCKING 0x97
#define FT_OPCODE_DISABLE_THREE_PHASE_CLOCKING 0x8D

typedef void* FT_HANDLE;
typedef unsigned int FT_STATUS;
typedef unsigned int FT_DEVICE;

typedef struct
{
	ULONG Type;
	USHORT VID;
	USHORT PID;
}Device;

typedef enum
{
	Iddle,
	FirstByte,
	MultiByte,
}StateMachine;

typedef struct
{
	ULONG	Size;
	void	(*StoreTDO)(void*, unsigned char*, unsigned long);
	void	(*StoreUnknownTDO)(void*, unsigned long);
	void	(*StoreNeededTDO)(void*, unsigned char*, unsigned long);
	int		(*TDONeeded)(void*, int);
	void	(*IndicateFlush)(void*);
}JTAGServerOperations;

typedef struct
{
	JTAGServerOperations* serverOperations;
	void* serverInstance;
	BOOL isHighSpeed;
	DWORD jtagClock;

	unsigned char ftIOBuffer[BUFFER_SIZE];
	unsigned char tdoBits[BUFFER_SIZE];
	unsigned char tdoBitCountPerByte[BUFFER_SIZE];

	unsigned short* numberOfBytes;
	unsigned int tdoPosition;
	int lastTMS;
	unsigned char bitPosition;
	unsigned char currentByte;
	StateMachine status;
	unsigned int bytePosition;
	unsigned int bytesToReceive;
	FT_HANDLE deviceHandle;
}JTAGClientData;

typedef struct
{
	ULONG Size;
	UCHAR Name[32];
	DWORD Features;
	unsigned long long Flags;
	void* Reserved1;
	BOOL(*ScanPorts)(unsigned int, char* portName, int);
	void* Reserved2;
	DWORD(*OpenHardware)(JTAGClientData** mpsseBlaster, char* deviceName, JTAGServerOperations* serverOperations, void* serverInstance);
	DWORD(*CloseHardware)(JTAGClientData* mpsseBlaster);
	DWORD(*SetParam)(JTAGClientData* mpsseBlaster, char* param_name, unsigned long val);
	DWORD(*GetParam)(JTAGClientData* mpsseBlaster, char* param_name, unsigned long* retval);
	DWORD(*DirectControl)(JTAGClientData* mpsseBlaster, unsigned long, unsigned long*);
	DWORD(*ClockRaw)(JTAGClientData* mpsseBlaster, int, int, int, int);
	DWORD(*ClockMultiple)(JTAGClientData* mpsseBlaster, int, unsigned char*, int, int);
	DWORD(*Delay)(JTAGClientData* mpsseBlaster, unsigned long);
	BOOL(*Flush)(JTAGClientData* mpsseBlaster, BOOL store, int size);
	DWORD(*EnableRecirculate)(JTAGClientData* mpsseBlaster, unsigned long);
	DWORD(*DisableRecirculate)(JTAGClientData* mpsseBlaster);
	DWORD(*Recirculate)(JTAGClientData* mpsseBlaster, unsigned long, int, int);
	DWORD(*SetParamBlock)(JTAGClientData* mpsseBlaster, char* name, unsigned char* buffer, unsigned long size);
	DWORD(*GetParamBlock)(JTAGClientData* mpsseBlaster, char* name, unsigned char* buffer, unsigned long* size);
	DWORD(*InitDriver)(JTAGClientData** mpsseBlaster, char* deviceName, JTAGServerOperations* serverOperations, void* serverInstance);
	DWORD(*EnableActiveSerial)(BOOL enable);
	DWORD(*RemoveDriver)(char*);
}JtagClientOperations;

// Device status
enum 
{
	FT_OK,
	FT_INVALID_HANDLE,
	FT_DEVICE_NOT_FOUND,
	FT_DEVICE_NOT_OPENED,
	FT_IO_ERROR,
	FT_INSUFFICIENT_RESOURCES,
	FT_INVALID_PARAMETER,
	FT_INVALID_BAUD_RATE,
	FT_DEVICE_NOT_OPENED_FOR_ERASE,
	FT_DEVICE_NOT_OPENED_FOR_WRITE,
	FT_FAILED_TO_WRITE_DEVICE,
	FT_EEPROM_READ_FAILED,
	FT_EEPROM_WRITE_FAILED,
	FT_EEPROM_ERASE_FAILED,
	FT_EEPROM_NOT_PRESENT,
	FT_EEPROM_NOT_PROGRAMMED,
	FT_INVALID_ARGS,
	FT_NOT_SUPPORTED,
	FT_OTHER_ERROR,
	FT_DEVICE_LIST_NOT_READY,
};

// Device information flags
enum 
{
	FT_FLAGS_OPENED = 1,
	FT_FLAGS_HISPEED = 2
};

// Device types
enum
{
	FT_DEVICE_BM,
	FT_DEVICE_AM,
	FT_DEVICE_100AX,
	FT_DEVICE_UNKNOWN,
	FT_DEVICE_2232C,
	FT_DEVICE_232R,
	FT_DEVICE_2232H,
	FT_DEVICE_4232H,
	FT_DEVICE_232H,
	FT_DEVICE_X_SERIES,
	FT_DEVICE_4222H_0,
	FT_DEVICE_4222H_1_2,
	FT_DEVICE_4222H_3,
	FT_DEVICE_4222_PROG,
	FT_DEVICE_900,
	FT_DEVICE_930,
	FT_DEVICE_UMFTPD3A,
};

// Bit Modes
#define FT_BITMODE_RESET 0x00
#define FT_BITMODE_MPSSE 0x02

// Flow Control
#define FT_FLOW_RTS_CTS		0x0100

// FT
typedef FT_STATUS(*FT_CLOSE)(FT_HANDLE ftHandle);
typedef FT_STATUS(*FT_RESETDEVICE)(FT_HANDLE ftHandle);
typedef FT_STATUS(*FT_CREATEDEVICEINFOLIST)(DWORD* lpdwNumDevs);
typedef FT_STATUS(*FT_OPEN)(DWORD deviceNumber, FT_HANDLE* pHandle);
typedef FT_STATUS(*FT_SETLATENCYTIMER)(FT_HANDLE ftHandle, UCHAR ucLatency);
typedef FT_STATUS(*FT_GETQUEUESTATUS)(FT_HANDLE ftHandle, DWORD* dwRxBytes);
typedef FT_STATUS(*FT_SETBITMODE)(FT_HANDLE ftHandle, UCHAR ucMask, UCHAR ucEnable);
typedef FT_STATUS(*FT_SETTIMEOUTS)(FT_HANDLE ftHandle, ULONG ReadTimeout, ULONG WriteTimeout);
typedef FT_STATUS(*FT_READ)(FT_HANDLE ftHandle, void* lpBuffer, DWORD nBufferSize, DWORD* lpBytesReturned);
typedef FT_STATUS(*FT_WRITE)(FT_HANDLE ftHandle, void* lpBuffer, DWORD nBufferSize, DWORD* lpBytesWritten);
typedef FT_STATUS(*FT_SETFLOWCONTROL)(FT_HANDLE ftHandle, USHORT FlowControl, UCHAR XonChar, UCHAR XoffChar);
typedef FT_STATUS(*FT_SETUSBPARAMETERS)(FT_HANDLE ftHandle, ULONG ulInTransferSize, ULONG ulOutTransferSize);
typedef FT_STATUS(*FT_SETCHARS)(FT_HANDLE ftHandle, UCHAR EventChar, UCHAR EventCharEnabled, UCHAR ErrorChar, UCHAR ErrorCharEnabled);
typedef FT_STATUS(*FT_GETDEVICEINFODETAIL)(DWORD dwIndex, DWORD* lpdwFlags, DWORD* lpdwType, DWORD* lpdwID, DWORD* lpdwLocId, PCHAR pcSerialNumber, PCHAR pcDescription, FT_HANDLE* ftHandle);
