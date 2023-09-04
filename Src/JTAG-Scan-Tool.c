#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "JTAG-MPSSE-Blaster.h"

void* get_supported_hardware(int index);

unsigned char tdoData[8];
unsigned int tdoBitCount;

void StoreTDO(void* serverInstance, unsigned char* buffer, unsigned long size)
{
    for (unsigned long i = 0; i < size; i++)
    {
        unsigned char actual = 1 & (buffer[i / 8] >> (i % 8));

        tdoData[tdoBitCount / 8] |= actual << (tdoBitCount % 8);
        tdoBitCount++;
    }
}

int main(void)
{
    JtagClientOperations const* clientOperations = (JtagClientOperations*)get_supported_hardware(0);

    if (clientOperations)
    {
        JTAGServerOperations serverOperations = { 0 };
        serverOperations.StoreTDO = &StoreTDO;
        char portName[64];

        for (;;)
        {                       
            int portsFound = FALSE;
            printf("Device:\t\t[%s]\n\n", clientOperations->Name);

            for (int portNumber = 0; clientOperations->ScanPorts(portNumber, portName, 64); portNumber++)
            {
                portsFound = TRUE;

                JTAGClientData* clientData;
                memset(tdoData, 0, sizeof(tdoData));
                tdoBitCount = 0;

                clientOperations->InitDriver(&clientData, portName, &serverOperations, NULL);

                clientOperations->ClockRaw(clientData, 1, 1, 12, 0);
                clientOperations->ClockRaw(clientData, 0, 1, 1, 0);
                clientOperations->ClockRaw(clientData, 1, 1, 1, 0);
                clientOperations->ClockRaw(clientData, 0, 1, 10, 0);
                clientOperations->ClockRaw(clientData, 0, 1, 1, 0);
                clientOperations->ClockRaw(clientData, 0, 0, 6, 0);
                clientOperations->ClockRaw(clientData, 0, 1, 2, 0);
                clientOperations->ClockRaw(clientData, 0, 0, 2, 0);
                clientOperations->ClockRaw(clientData, 0, 1, 2, 0);
                clientOperations->ClockRaw(clientData, 0, 0, 1, 0);
                clientOperations->ClockRaw(clientData, 0, 1, 7, 0);
                clientOperations->ClockRaw(clientData, 0, 0, 2, 0);

                clientOperations->Flush(clientData, TRUE, 0);
                clientOperations->CloseHardware(clientData);

                printf("  Port:\t\t\t[%s]\n", portName);
                printf("  Raw TDO bits:\t\t");

                for (unsigned int tdoBytePosition = 0; tdoBytePosition < tdoBitCount; tdoBytePosition++)
                {
                    if (tdoBytePosition && !(tdoBytePosition % 8))
                    {
                        printf(" ");
                    }
                    printf("%d", (tdoData[tdoBytePosition / 8 ] >> (tdoBytePosition % 8)) & 1);
                }

                printf("\n  Raw TDO bytes:\t");

                for (unsigned int tdoBytePosition = 0; tdoBytePosition <= tdoBitCount / 8; tdoBytePosition++)
                {
                    printf("%02X ", tdoData[tdoBytePosition]);
                }

                printf("\n  JTAG IDCode:\t\t%08X\n\n", *(int*)(tdoData + 2));
            }

            if (!portsFound)
            {
                printf("  No ports available on the device\n");
            }

            getchar();
        }
    }
    else
    {
        printf("No devices\n");
        getchar();
    }

    return 0;
}
