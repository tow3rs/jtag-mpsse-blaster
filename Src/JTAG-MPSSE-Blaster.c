#include "JTAG-MPSSE-Blaster.h"

Device validDevices[] =
{
	{ .Type = FT_DEVICE_232H, .VID = 0x0403, .PID = 0x6014 } // Single RS232-HS - (FT232H)
};

#ifdef _WIN32
static HANDLE hFtd2xxLibrary = 0;
#else
static void* hFtd2xxLibrary = 0;
#define GetProcAddress dlsym
#endif

FT_OPEN FT_Open;
FT_READ FT_Read;
FT_WRITE FT_Write;
FT_CLOSE FT_Close;
FT_SETCHARS FT_SetChars;
FT_SETBITMODE FT_SetBitMode;
FT_SETTIMEOUTS FT_SetTimeouts;
FT_RESETDEVICE FT_ResetDevice;
FT_SETFLOWCONTROL FT_SetFlowControl;
FT_GETQUEUESTATUS FT_GetQueueStatus;
FT_SETLATENCYTIMER FT_SetLatencyTimer;
FT_SETUSBPARAMETERS FT_SetUSBParameters;
FT_GETDEVICEINFODETAIL FT_GetDeviceInfoDetail;
FT_CREATEDEVICEINFOLIST FT_CreateDeviceInfoList;

FT_STATUS InitFTD2xx()
{
	if (hFtd2xxLibrary != NULL)
	{
		return FT_OK;
	}

#ifdef _WIN32
	hFtd2xxLibrary = LoadLibraryW(L"ftd2xx.dll");
#else
	hFtd2xxLibrary = dlopen("libftd2xx.so", RTLD_LAZY);
#endif

	if (hFtd2xxLibrary)
	{
		FT_Open = (FT_OPEN)GetProcAddress(hFtd2xxLibrary, "FT_Open");
		FT_Read = (FT_READ)GetProcAddress(hFtd2xxLibrary, "FT_Read");
		FT_Write = (FT_WRITE)GetProcAddress(hFtd2xxLibrary, "FT_Write");
		FT_Close = (FT_CLOSE)GetProcAddress(hFtd2xxLibrary, "FT_Close");
		FT_SetChars = (FT_SETCHARS)GetProcAddress(hFtd2xxLibrary, "FT_SetChars");
		FT_SetBitMode = (FT_SETBITMODE)GetProcAddress(hFtd2xxLibrary, "FT_SetBitMode");
		FT_SetTimeouts = (FT_SETTIMEOUTS)GetProcAddress(hFtd2xxLibrary, "FT_SetTimeouts");
		FT_ResetDevice = (FT_RESETDEVICE)GetProcAddress(hFtd2xxLibrary, "FT_ResetDevice");
		FT_SetFlowControl = (FT_SETFLOWCONTROL)GetProcAddress(hFtd2xxLibrary, "FT_SetFlowControl");
		FT_GetQueueStatus = (FT_GETQUEUESTATUS)GetProcAddress(hFtd2xxLibrary, "FT_GetQueueStatus");	
		FT_SetLatencyTimer = (FT_SETLATENCYTIMER)GetProcAddress(hFtd2xxLibrary, "FT_SetLatencyTimer");
		FT_SetUSBParameters = (FT_SETUSBPARAMETERS)GetProcAddress(hFtd2xxLibrary, "FT_SetUSBParameters");
		FT_GetDeviceInfoDetail = (FT_GETDEVICEINFODETAIL)GetProcAddress(hFtd2xxLibrary, "FT_GetDeviceInfoDetail");
		FT_CreateDeviceInfoList = (FT_CREATEDEVICEINFOLIST)GetProcAddress(hFtd2xxLibrary, "FT_CreateDeviceInfoList");

		return !(FT_Open &&
			FT_Read &&
			FT_Write &&
			FT_Close &&
			FT_SetChars &&
			FT_SetBitMode &&
			FT_SetTimeouts &&
			FT_ResetDevice &&
			FT_SetFlowControl &&
			FT_GetQueueStatus &&
			FT_SetLatencyTimer &&
			FT_SetUSBParameters &&
			FT_GetDeviceInfoDetail &&
			FT_CreateDeviceInfoList);
	}

	return FT_OTHER_ERROR;
}

FT_STATUS FTSendReceive(FT_HANDLE ftdih, unsigned char* outBuffer, unsigned int outSize, unsigned char* inBuffer, unsigned int inSize)
{
	DWORD tempCount;
	FT_STATUS status = FT_OK;
	DWORD totalProgress;
	unsigned char* bufferPointer;
	unsigned int partialCount;

	if (outSize && outBuffer)
	{
		totalProgress = 0;
		bufferPointer = outBuffer;
		partialCount = outSize;
		do
		{
			status = FT_Write(ftdih, bufferPointer, partialCount, &tempCount);
			bufferPointer += tempCount;
			partialCount -= tempCount;
			totalProgress += tempCount;
		} while (totalProgress < outSize && (status == FT_OK));
	}

	if (inSize && inBuffer && status == FT_OK)
	{
		do
		{
			status = FT_GetQueueStatus(ftdih, &tempCount);
		} while (tempCount < inSize && (status == FT_OK));

		totalProgress = 0;
		bufferPointer = inBuffer;
		partialCount = inSize;
		do
		{
			status = FT_Read(ftdih, bufferPointer, partialCount, &tempCount);
			bufferPointer += tempCount;
			partialCount -= tempCount;
			totalProgress += tempCount;
		} while (totalProgress < inSize && (status == FT_OK));
	}

	return status;
}

JTAGClientData* InitDevice(unsigned int devNum, JTAGServerOperations* serverOperations, void* serverInstance)
{
	FT_STATUS status;
	unsigned char buffer[1024];
	DWORD dwNumBytesToSend;
	DWORD dwNumBytesToRead;
	DWORD dwNumBytesSent;
	DWORD dwNumBytesRead;
	int clockDivisor;
	DWORD numDevices;
	DWORD dwFlags;

	status = InitFTD2xx();

	if (status != FT_OK)
	{
		return NULL;
	}

	status = FT_CreateDeviceInfoList(&numDevices);
	if (status != FT_OK || numDevices < devNum + 1)
	{
		return NULL;
	}

	status = FT_GetDeviceInfoDetail(devNum, &dwFlags, NULL, NULL, NULL, NULL, NULL, NULL);

	if ((status != FT_OK) || (dwFlags & FT_FLAGS_OPENED))
	{
		return NULL;
	}

	JTAGClientData* hwData = malloc(sizeof(JTAGClientData));

	if (!hwData)
	{
		return NULL;
	}

	memset(hwData, 0, sizeof(JTAGClientData));

	hwData->serverOperations = serverOperations;
	hwData->serverInstance = serverInstance;
	hwData->lastTMS = -1;
	hwData->status = Iddle;
	hwData->isHighSpeed = dwFlags & FT_FLAGS_HISPEED;

	status |= FT_Open(devNum, &hwData->deviceHandle);
	status |= FT_ResetDevice(hwData->deviceHandle);
	status |= FT_SetUSBParameters(hwData->deviceHandle, 65536, 65535);
	status |= FT_SetChars(hwData->deviceHandle, 0, 0, 0, 0);
	status |= FT_SetTimeouts(hwData->deviceHandle, 200, 200);
	status |= FT_SetLatencyTimer(hwData->deviceHandle, 10);
	status |= FT_SetFlowControl(hwData->deviceHandle, FT_FLOW_RTS_CTS, 0x00, 0x00);  //Turn on flow control to synchronize IN requests https://www.ftdicommunity.com/index.php?topic=657.0
	status |= FT_SetBitMode(hwData->deviceHandle, 0, FT_BITMODE_RESET);
	status |= FT_SetBitMode(hwData->deviceHandle, 0, FT_BITMODE_MPSSE);

	dwNumBytesToSend = 0;
	buffer[dwNumBytesToSend++] = 0xAA;
	buffer[dwNumBytesToSend++] = FT_OPCODE_SEND_IMMEDIATE;
	status |= FT_Write(hwData->deviceHandle, buffer, dwNumBytesToSend, &dwNumBytesSent);
	do
	{
		status |= FT_GetQueueStatus(hwData->deviceHandle, &dwNumBytesToRead);
	} while ((dwNumBytesToRead == 0) && (status == FT_OK));

	status |= FT_Read(hwData->deviceHandle, buffer, dwNumBytesToRead, &dwNumBytesRead);

	for (DWORD dwCount = 0; dwCount < dwNumBytesRead - 1; dwCount++)		
	{
		if ((buffer[dwCount] == 0xFA) && (buffer[dwCount + 1] == 0xAA))
		{
			break;
		}
	}

	// Set initial states of the MPSSE interface - low byte, both pin directions and output values
	// Pin	  Signal	Direction
	// ADBUS0 TCK		output 1
	// ADBUS1 TDI		output 1
	// ADBUS2 TDO		input  0
	// ADBUS3 TMS		output 1
	// ADBUS4 GPIOL0	input 0
	// ADBUS5 GPIOL1	input 0
	// ADBUS6 GPIOL2	input 0
	// ADBUS7 GPIOL3	input 0

	dwNumBytesToSend = 0;
	buffer[dwNumBytesToSend++] = FT_OPCODE_SET_DATA_BITS_LOW_BYTE;
	buffer[dwNumBytesToSend++] = 0x08;		// Initial state
	buffer[dwNumBytesToSend++] = 0x0B;		// Direction
	
	// Set initial states of the MPSSE interface - high byte, both pin directions and output values
	// Pin	  Signal	 Direction
	// ACBUS0 GPIOH0	 input 0
	// ACBUS1 GPIOH1	 input 0
	// ACBUS2 GPIOH2	 input 0
	// ACBUS3 GPIOH3	 input 0
	// ACBUS4 GPIOH4	 input 0
	// ACBUS5 GPIOH5	 input 0
	// ACBUS6 GPIOH6	 input 0
	// ACBUS7 GPIOH7	 input 0

	buffer[dwNumBytesToSend++] = FT_OPCODE_SET_DATA_BITS_HIGH_BYTE;
	buffer[dwNumBytesToSend++] = 0;			// Initial state
	buffer[dwNumBytesToSend++] = 0;			// Direction
	status |= FTSendReceive(hwData->deviceHandle, buffer, dwNumBytesToSend, NULL, 0);

	dwNumBytesToSend = 0;
	buffer[dwNumBytesToSend++] = FT_OPCODE_DISABLE_LOOPBACK;

	if (hwData->isHighSpeed)
	{
		clockDivisor = 1;
		hwData->jtagClock = 15000000;
		buffer[dwNumBytesToSend++] = FT_OPCODE_DISABLE_DIVIDE_BY_5; // Use 60MHz master clock (disable divide by 5)
	}
	else
	{
		clockDivisor = 0;
		hwData->jtagClock = 6000000;
	}
	buffer[dwNumBytesToSend++] = FT_OPCODE_DISABLE_ADAPTIVE_CLOCKING;
	buffer[dwNumBytesToSend++] = FT_OPCODE_DISABLE_THREE_PHASE_CLOCKING;
	buffer[dwNumBytesToSend++] = FT_OPCODE_SET_CLOCK_DIVIDER; 
	buffer[dwNumBytesToSend++] = clockDivisor & 0xFF;			//Set 0xValueL of clock divisor
	buffer[dwNumBytesToSend++] = (clockDivisor >> 8) & 0xFF;	//Set 0xValueH of clock divisor

	status |= FTSendReceive(hwData->deviceHandle, buffer, dwNumBytesToSend, NULL, 0);

	if (status != FT_OK)
	{
		free(hwData);
		return NULL;
	}

	return hwData;
}

BOOL ScanPorts(unsigned int index, char* portName, int size)
{
	DWORD numDevices = 0;
	DWORD deviceCount = 0;
	FT_STATUS ftStatus;
	DWORD dwID;
	DWORD dwType;
	char description[64];

	ftStatus = FT_CreateDeviceInfoList(&numDevices);
	if (ftStatus != FT_OK || numDevices < index + 1)
	{
		return FALSE;
	}

	for (DWORD deviceIndex = 0; deviceIndex < numDevices; deviceIndex++)
	{
		ftStatus = FT_GetDeviceInfoDetail(deviceIndex, NULL, &dwType, &dwID, NULL, NULL, description, NULL);
		if (ftStatus != FT_OK)
		{
			continue;
		}

		for (DWORD j = 0; j < sizeof(validDevices) / sizeof(Device); j++)
		{
			if (dwType == validDevices[j].Type && dwID >> 16 == validDevices[j].VID && (dwID & 0xFFFF) == validDevices[j].PID)
			{
				if (deviceCount == index)
				{
#ifdef _WIN32
					sprintf_s(portName, size, "%02d %s", index, description);
#else
					sprintf(portName, "%02d %s", index, description);
#endif
					return TRUE;
				}
				else
				{
					deviceCount++;
				}
			}
		}
	}
	return FALSE;
}

DWORD OpenHardware(JTAGClientData** mpsseBlaster, char* deviceName, JTAGServerOperations* serverOperations, void* serverInstance)
{
	int devnum = -1;

#ifdef _WIN32
	if (mpsseBlaster && deviceName && sscanf_s(deviceName, "%d", &devnum))
#else
	if (mpsseBlaster && deviceName && sscanf(deviceName, "%d", &devnum))
#endif
	{
		*mpsseBlaster = InitDevice(devnum, serverOperations, serverInstance);
		if (*mpsseBlaster)
		{
			return 1;
		}
	}
	return 0;
}

DWORD CloseHardware(JTAGClientData* mpsseBlaster)
{
	if (mpsseBlaster)
	{
		if (mpsseBlaster->deviceHandle)
		{
			FT_Close(mpsseBlaster->deviceHandle);
		}
		free(mpsseBlaster);
	}
	return 0;
}

BOOL Flush(JTAGClientData* mpsseBlaster, BOOL store, int position)
{
	if (mpsseBlaster->bytePosition)
	{
		int inBitCount = (*mpsseBlaster->numberOfBytes + 1) * 8;

		mpsseBlaster->ftIOBuffer[mpsseBlaster->bytePosition] = FT_OPCODE_SEND_IMMEDIATE;
		FTSendReceive(mpsseBlaster->deviceHandle, mpsseBlaster->ftIOBuffer, *mpsseBlaster->numberOfBytes + 5, mpsseBlaster->ftIOBuffer, *mpsseBlaster->numberOfBytes + 1);

		for (int inBitPosition = 0; inBitPosition < inBitCount; inBitPosition++)
		{
			unsigned char tdoSingleBit = 1 & (mpsseBlaster->ftIOBuffer[inBitPosition / 8] >> inBitPosition % 8);
			mpsseBlaster->tdoBits[mpsseBlaster->tdoPosition / 8] |= tdoSingleBit << mpsseBlaster->tdoPosition % 8;
			mpsseBlaster->tdoPosition++;
		}
	}

	if (mpsseBlaster->bitPosition)
	{
		unsigned char buffer[4] =
		{
			FT_OPCODE_CLOCK_DATA_BITS,
			mpsseBlaster->bitPosition - 1,
			mpsseBlaster->currentByte,
			FT_OPCODE_SEND_IMMEDIATE
		};

		FTSendReceive(mpsseBlaster->deviceHandle, buffer, 4, buffer, 1);

		unsigned char tdoPartialByte = buffer[0] >> (8 - mpsseBlaster->bitPosition);

		for (unsigned char bitCount = 0; bitCount < mpsseBlaster->bitPosition; bitCount++)
		{
			unsigned char tdoSingleBit = 1 & (tdoPartialByte >> bitCount);
			mpsseBlaster->tdoBits[mpsseBlaster->tdoPosition / 8] |= tdoSingleBit << (mpsseBlaster->tdoPosition % 8);
			mpsseBlaster->tdoPosition++;
		}
	}

	if (mpsseBlaster->tdoPosition)
	{
		if (mpsseBlaster->serverOperations->StoreTDO)
		{
			mpsseBlaster->serverOperations->StoreTDO(mpsseBlaster->serverInstance, mpsseBlaster->tdoBits, mpsseBlaster->tdoPosition);
		}
		memset(mpsseBlaster->tdoBits, 0, BUFFER_SIZE);
		mpsseBlaster->bitPosition = 0;
		mpsseBlaster->tdoPosition = 0;
		mpsseBlaster->bytePosition = 0;

		if (mpsseBlaster->status == MultiByte)
		{
			mpsseBlaster->status = FirstByte;
		}
		if (mpsseBlaster->serverOperations->IndicateFlush)
		{
			mpsseBlaster->serverOperations->IndicateFlush(mpsseBlaster->serverInstance);
		}
	}

	return TRUE;
}

DWORD ClockBits(JTAGClientData* mpsseBlaster, int tms, int tdi, int bitCount, int position)
{
	for (int currentBit = 0; currentBit < bitCount; currentBit++)
	{
		if (mpsseBlaster->lastTMS == tms)
		{
			if (mpsseBlaster->bitPosition == 0)
			{
				mpsseBlaster->currentByte = 0;
			}

			mpsseBlaster->currentByte |= tdi << mpsseBlaster->bitPosition;
			mpsseBlaster->bitPosition++;

			if (mpsseBlaster->bitPosition == 8)
			{
				if (mpsseBlaster->status == FirstByte)
				{
					mpsseBlaster->bytePosition = 0;
					mpsseBlaster->ftIOBuffer[mpsseBlaster->bytePosition] = FT_OPCODE_CLOCK_DATA_BYTES;
					mpsseBlaster->bytePosition++;
					mpsseBlaster->numberOfBytes = (unsigned short*)&mpsseBlaster->ftIOBuffer[mpsseBlaster->bytePosition];
					mpsseBlaster->bytePosition += 2;
					*mpsseBlaster->numberOfBytes = 0;
					mpsseBlaster->status = MultiByte;
				}
				else
				{
					(*mpsseBlaster->numberOfBytes)++;
				}
				mpsseBlaster->ftIOBuffer[mpsseBlaster->bytePosition] = mpsseBlaster->currentByte;
				mpsseBlaster->bitPosition = 0;
				mpsseBlaster->currentByte = 0;
				mpsseBlaster->bytePosition++;

				if (mpsseBlaster->bytePosition > 65534)
				{
					Flush(mpsseBlaster, TRUE, position);
				}
			}
		}
		else
		{
			Flush(mpsseBlaster, TRUE, position);

			unsigned char ioBuffer[4] =
			{
				FT_OPCODE_CLOCK_TMS,
				0,
				((tdi ? 0x80 : 0) | (tms ? 0x3 : 0)),
				FT_OPCODE_SEND_IMMEDIATE
			};

			FTSendReceive(mpsseBlaster->deviceHandle, ioBuffer, 4, ioBuffer, 1);

			mpsseBlaster->tdoBits[mpsseBlaster->tdoPosition / 8] |= ((ioBuffer[0] >> 7) & 1) << (mpsseBlaster->tdoPosition % 8);
			mpsseBlaster->tdoPosition++;
			mpsseBlaster->lastTMS = tms;
			mpsseBlaster->status = FirstByte;
		}
	}

	return 0;
}

DWORD ClockBytes(JTAGClientData* mpsseBlaster, int tms, unsigned char* tdi, int bitCount, int position)
{
	for (int bitPosition = 0; bitPosition < bitCount; bitPosition++)
	{
		ClockBits(
			mpsseBlaster,
			tms,
			(tdi[bitPosition / 8] >> (bitPosition % 8)) & 0x1,
			1,
			position + bitPosition);
	}

	return 0;
}

DWORD GetParam(JTAGClientData* mpsseBlaster, char* paramName, unsigned long*  paramValue)
{
	if (!strcmp(paramName, "JtagClock"))
	{
		if (paramValue)
		{
			*paramValue = mpsseBlaster->jtagClock;
		}
		return 0;
	}

	return 0x2D;
}

static JtagClientOperations hwDevice =
{
	.Size = sizeof(JtagClientOperations),
	.Name = "JTAG-MPSSE-Blaster",
	.Features = 0x800,
	.Flags = 0,
	.Reserved1 = NULL,
	.ScanPorts = ScanPorts,
	.Reserved2 = NULL,
	.OpenHardware = OpenHardware,
	.CloseHardware = CloseHardware,
	.SetParam = NULL,
	.GetParam = GetParam,
	.DirectControl = NULL,
	.ClockRaw = ClockBits,
	.ClockMultiple = ClockBytes,
	.Delay = NULL,
	.Flush = Flush,
	.EnableRecirculate = NULL,
	.DisableRecirculate = NULL,
	.Recirculate = NULL,
	.SetParamBlock = NULL,
	.GetParamBlock = NULL,
	.InitDriver = NULL,
	.EnableActiveSerial = NULL,
	.RemoveDriver = NULL,
};

#ifndef _TEST_TOOL
#ifdef  _WIN32
__declspec(dllexport)
#else
__attribute__((visibility("default"))) 
#endif
#endif

void* get_supported_hardware(int index)
{
	if (index != 0)
	{
		return NULL;
	}

	DWORD dwID;
	DWORD dwType;
	DWORD numDevices = 0;

	FT_STATUS ftStatus = InitFTD2xx();

	if (ftStatus != FT_OK)
	{
		return NULL;
	}

	ftStatus = FT_CreateDeviceInfoList(&numDevices);
	if (ftStatus != FT_OK || !numDevices)
	{
		return NULL;
	}

	for (DWORD deviceIndex = 0; deviceIndex < numDevices; deviceIndex++)
	{
		ftStatus = FT_GetDeviceInfoDetail(deviceIndex, NULL, &dwType, &dwID, NULL, NULL, NULL, NULL);
		if (ftStatus != FT_OK)
		{
			continue;
		}

		for (DWORD devCount = 0; devCount < sizeof(validDevices) / sizeof(Device); devCount++)
		{
			if (dwType == validDevices[devCount].Type && dwID >> 16 == validDevices[devCount].VID && (dwID & 0xFFFF) == validDevices[devCount].PID)
			{
				return &hwDevice;
			}
		}
	}

	return NULL;
}
