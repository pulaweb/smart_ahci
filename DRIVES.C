
/*
    Watcom C - Simple hard drives detection for 32-bit protected mode using AHCI under a DOS extender
    Written by Piotr Ulaszewski (pulaweb) on the 27th of December 2019, modified for SMART on the 27th of July 2021
    This is just a demonstartion on how to obtain hard drive info and SMART info on any AHCI system
*/

#include <stdlib.h>
#include <stdio.h>
#include <conio.h>
#include <string.h>
#include <i86.h>
#include <math.h>
#include "drives.h"
#include "ahci.h"


// frequency of the high resolution timer
unsigned __int64 calculated_frequency = 0;

// ATA status and error registers
BYTE status_register = 0;
BYTE device_register = 0;
BYTE chigh_register = 0;
BYTE clow_register = 0;
BYTE sector_register = 0;
BYTE count_register = 0;
BYTE error_register = 0;

BYTE lbahigh07_register = 0;
BYTE lbahigh815_register = 0;
BYTE lbamid07_register = 0;
BYTE lbamid815_register = 0;
BYTE lbalow07_register = 0;
BYTE lbalow815_register = 0;
BYTE count07_register = 0;
BYTE count815_register = 0;

// detected drives array
DISKDRIVE diskdrive[128];

// data for SMART attributes
SMARTDATA smartdata;

// temporary string buffer
char tmpstring[1024];


int main (void) {

    int i, l;
    unsigned __int64 calculated_frequency_start = 0;
    unsigned __int64 calculated_frequency_stop = 0;
    int total_ahci_drives = 0;

    // calculate frequency in cycles for 1ns of pentium timer
    QueryPerformanceCounter(&calculated_frequency_start);
    delay(1000);
    QueryPerformanceCounter(&calculated_frequency_stop);
    calculated_frequency = (calculated_frequency_stop - calculated_frequency_start);

    // detect AHCI
    if (ahci_detect_ahci() == FALSE) {
        printf("AHCI controller not detected error!\n");
        exit(0);
    }
    
    total_ahci_drives = ahci_detect_drives (diskdrive);
    printf("\n");

    // walk through all detected drives and display info
    for (i = 0; i < total_ahci_drives; i++) {      
        printf("Drive nr %d size in sectors : %d, ", i, diskdrive[i].total_sectors);
        printf("drive nr %d size in GB : %d", i, diskdrive[i].total_gb);
        printf("\n");
                    
        // print drives info on the screen
        printf("Drive model : ");
        printf(diskdrive[i].drive_model);
        printf("\n");
                    
        printf("Drive serial number : ");
        printf(diskdrive[i].drive_serial);
        printf("\n");
                    
        printf("Drive firmware revision : ");
        printf(diskdrive[i].drive_firmware);
        printf("\n\n");                    
    }

    // prepare and open ahci transfer system for drive 0
    ahci_start_disk_system(&diskdrive[0]);
        
    // *******************************
    // SMART data for drive 0 example 
    // *******************************

    // display SMART data for drive 0
    memset(&smartdata, 0, sizeof(SMARTDATA));
    if (ahci_get_SMART_data (&smartdata, &diskdrive[0]) == FALSE) {
                printf("Cound not get SMART data from %s.\n", diskdrive[0].drive_model);
                ahci_close_ahci();
                exit(1);
    }                    
    
    // reset smart status to perfect drive
    smartdata.smartstatus = 0;

    // display SMART infos                    
    printf("SMART data for : %s\n\n", diskdrive[0].drive_model);

    // print attributes
    printf("Attribute name, Critical, ID, Value, Worst, Warning, XData, WData, Status\n");

    // start the SMART attributes loop
    for (i = 0; i < 256; i++) {
        // skip if smart entry is empty
        if (smartdata.smartentry[i].attrib == 0) {
            i++;
            continue;
        }

        // analyze SMART status
        // alter SMART status - set to warning for critical attributes if they show even a value of 1
        if ((smartdata.smartentry[i].attrib == 1 && smartdata.smartentry[i].rawlword) || // raw read error rate
            (smartdata.smartentry[i].attrib == 5 && smartdata.smartentry[i].rawlword) || // reallocated sector count
            (smartdata.smartentry[i].attrib == 7 && smartdata.smartentry[i].rawlword) || // seek error rate
            (smartdata.smartentry[i].attrib == 10 && smartdata.smartentry[i].rawlword) || // Spinup retry count
            (smartdata.smartentry[i].attrib == 13 && smartdata.smartentry[i].rawlword) || // soft read error rate
            (smartdata.smartentry[i].attrib == 184 && smartdata.smartentry[i].rawlword) || // end to end error
            (smartdata.smartentry[i].attrib == 187 && smartdata.smartentry[i].rawlword) || // uncorrectable errors
            (smartdata.smartentry[i].attrib == 195 && smartdata.smartentry[i].rawlword) || // hardware ecc recovered
            (smartdata.smartentry[i].attrib == 196 && smartdata.smartentry[i].rawlword) || // reallocated events count
            (smartdata.smartentry[i].attrib == 197 && smartdata.smartentry[i].rawlword) || // pending sector count
            (smartdata.smartentry[i].attrib == 198 && smartdata.smartentry[i].rawlword) || // offline scan unc
            (smartdata.smartentry[i].attrib == 199 && smartdata.smartentry[i].rawlword) || // udma crc error rate
            (smartdata.smartentry[i].attrib == 200 && smartdata.smartentry[i].rawlword) || // write error rate
            (smartdata.smartentry[i].attrib == 201 && smartdata.smartentry[i].rawlword)) { // soft read error rate
                if (!smartdata.smartstatus) smartdata.smartstatus = 1; // set SMART status to warning if status still OK, skip if drive already bad
        } 

        // alter SMART status to bad if error
        if (smartdata.smartentry[i].value < smartdata.smartentry[i].threshold) smartdata.smartstatus = 2;

        // start displaying attributes
        printf("%s, ", smartdata.smartentry[i].name);

        if (smartdata.smartentry[i].threshold) printf ("Y, ");
        else printf("N, ");

        itoa(smartdata.smartentry[i].attrib, tmpstring, 10);
        printf("%s, ",tmpstring);

        itoa(smartdata.smartentry[i].value, tmpstring, 10);
        printf("%s, ", tmpstring);

        itoa(smartdata.smartentry[i].worst, tmpstring, 10);
        printf("%s, ", tmpstring);

        itoa(smartdata.smartentry[i].threshold, tmpstring, 10);
        printf("%s, ", tmpstring);

        // raw data 6 words
        itoa(smartdata.smartentry[i].rawxword, tmpstring, 16);
        // XData
        l = strlen(tmpstring);
        while (l < 4) {
            printf("0");
            l++;
        }
        printf("%s ", tmpstring);

        itoa(smartdata.smartentry[i].rawhword, tmpstring, 16);
        l = strlen(tmpstring);
        while (l < 4) {
            strcat (tmpstring, "0");
            l++;
        }
        printf("%s, ", tmpstring);

        // WData
        itoa(smartdata.smartentry[i].rawlword, tmpstring, 10);
        strupr (tmpstring);
        printf("%s, ", tmpstring);

        // print line status
        if (smartdata.smartentry[i].value >= smartdata.smartentry[i].threshold) printf("OK");
        else printf("BAD");

        printf("\n");
    }
                    
    // display drive status
    printf("\nDrive status : ");
    switch (smartdata.smartstatus) {
        case 0:
            printf("perfect");
            break;
        case 1:
            printf("warning");
            break;
        case 2:
            printf("damaged");
            break;
        default:
            printf("unknown");
            break;
    }
    printf("\n");

    // close ahci transfer system for drive 0
    ahci_reset_disk_system(&diskdrive[0]);
    
    ahci_close_ahci();
    return 0;
}


/*************************************/
/* temporary for testing rdtsc timer */
/*************************************/

void QueryPerformanceFrequency (unsigned __int64 *freq)
{
    *freq = calculated_frequency;
}

void QueryPerformanceCounter (unsigned __int64 *count)
{
    _asm rdtsc;   // pentium or above
    _asm mov ebx, count;
    _asm mov [ebx], eax;
    _asm mov [ebx + 4], edx;
}


/******************/
/* DPMI functions */
/******************/

BOOL DPMI_SimulateRMI (BYTE IntNum, DPMIREGS *regs) {
    BYTE noerror = 0;

    _asm {
        mov edi, [regs]
        sub ecx,ecx
        sub ebx,ebx
        mov bl, [IntNum]
        mov eax, 0x300
        int 0x31
        setnc [noerror]
    }   
    return noerror;
}

BOOL DPMI_DOSmalloc (DWORD size, WORD *segment, WORD *selector) {
    BYTE noerror = 0;
    
    _asm {
        mov eax, 0x100
        mov ebx, [size]
        add ebx, 0x0f
        shr ebx, 4
        int 0x31
        setnc [noerror]
        mov ebx, [segment]
        mov [ebx], ax
        mov ebx, [selector]
        mov [ebx], dx
    }
    return noerror;
}

void DPMI_DOSfree (WORD *selector) {
    
    _asm {
        mov eax, [selector]
        mov dx, [eax]
        mov eax, 0x101
        int 0x31
    }
}

BOOL DPMI_MapMemory (unsigned long *physaddress, unsigned long *linaddress, unsigned long size) {
    BYTE noerror = 0;

    _asm {
        mov eax,[physaddress]          ; pointer to physical address
        mov ebx,[eax]                  ; physical address
        mov cx,bx
        shr ebx,16                     ; BX:CX = physical address
        mov esi,[size]                 ; size in bytes
        mov di,si
        shr esi,16                     ; SI:DI = size in bytes
        mov eax,0x800                  ; physical address mapping
        int 0x31
        setnc [noerror]
        shl ebx,16
        mov bx,cx
        mov eax,[linaddress]           ; pointer to linear address
        mov [eax],ebx                  ; linaddress = BX:CX
    }
    return noerror;
}

BOOL DPMI_UnmapMemory (unsigned long *linearaddress) {
    BYTE noerror = 0;

    _asm {
        mov eax,[linearaddress]        ; pointer to linear address
        mov ebx,[eax]                  ; linear address
        mov cx,bx
        shr ebx,16                     ; BX:CX = linear address
        mov eax,0x801                  ; free physical address mapping
        int 0x31
        setnc [noerror]
    }
    return noerror;
}


/******************/
/* text functions */
/******************/

char *text_ConvertToString (char stringdata[256], int count)
{
    int i = 0;
    static char string[512];

    // characters are stored backwards
    for (i = 0; i < count; i += 2) {
        string [i] = (char) (stringdata[i + 1]);
        string [i + 1] = (char) (stringdata[i]);
    }

    // end the string
    string[i] = '\0';

    return string;
}

char *text_CutSpacesAfter (char *str)
{
    int i = 0;
    static char cut [512];

    // cut spaces after text
    strcpy (cut, str);
    for (i = strlen(cut) - 1; i > 0 && cut[i] == ' '; i--)
        cut[i] = '\0';

    return cut;
}

char *text_CutSpacesBefore (char *str)
{
    int i = 0;
    int j = 0;
    static char cut [512];

    // cut spaces before text
    for (i = 0; i < strlen(str); i++)
        if (str[i] != ' ') {
            cut[j] = str[i];
            j++;
        }
    cut[j] = '\0';

    return cut;
}

void cursor_up (void) {
    _asm {
        mov ah,3                      ; ah = 3 to get cursor position
        mov bh,0                      ; bh = 0 for video page 0
        int 0x10
        dec dh
        mov ah,2                      ; ah = 2 to set cursor position
        mov bh,0                      ; bh = 0 for video page 0
        int 0x10
    }
    return; 
}


