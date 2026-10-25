#ifndef NTPWR_H
#define NTPWR_H

#define NTPWR_IOCTL_QUERY 0x00222000UL
#define NTPWR_IOCTL_ARM   0x00222004UL
#define NTPWR_IOCTL_ARM_EXPERIMENTAL 0x00222008UL
#define NTPWR_IOCTL_DISARM 0x0022200CUL
#define NTPWR_IOCTL_VERIFY_FILE 0x00222010UL
#define NTPWR_IOCTL_PREPARE_S5 0x00222014UL
#define NTPWR_IOCTL_SERIAL_DEBUG 0x00222018UL
#define NTPWR_SIGNATURE 0x52575051UL
#define NTPWR_REPORT_VERSION 2UL

#define NTPWR_OK 0UL
#define NTPWR_BAD_KERNEL_BASE 1UL
#define NTPWR_BAD_PE 2UL
#define NTPWR_BAD_IMAGE 3UL
#define NTPWR_NO_TEXT 4UL
#define NTPWR_NO_BRANCH 5UL
#define NTPWR_AMBIGUOUS_BRANCH 6UL
#define NTPWR_BAD_IAT 7UL
#define NTPWR_BAD_HAL_TARGET 8UL
#define NTPWR_NOT_VERIFIED 9UL
#define NTPWR_ALREADY_MODIFIED 10UL
#define NTPWR_ALREADY_ARMED 11UL
#define NTPWR_WRITE_FAILED 12UL
#define NTPWR_IAT_CHANGED 13UL
#define NTPWR_NO_ACPI_SHUTDOWN 14UL
#define NTPWR_WINLOGON_UNSUPPORTED 15UL

#define NTPWR_FLAG_KNOWN 1UL
#define NTPWR_FLAG_IMPORT 2UL
#define NTPWR_FLAG_COMPATIBLE 4UL
#define NTPWR_FLAG_ARMED 8UL
#define NTPWR_FLAG_FILE_IMPORT 32UL
#define NTPWR_FLAG_HAL_EXPORT 64UL

#pragma pack(4)
typedef struct _NTPWR_FILE_IMPORT {
    unsigned long Size;
    unsigned long KernelTimestamp;
    unsigned long KernelChecksum;
    unsigned long KernelImageSize;
    unsigned long HalIatAddress;
} NTPWR_FILE_IMPORT;

typedef struct _NTPWR_REPORT {
    unsigned long Signature;
    unsigned long Version;
    unsigned long Size;
    unsigned long Flags;
    unsigned long Result;
    unsigned long KernelBase;
    unsigned long KernelTimestamp;
    unsigned long KernelChecksum;
    unsigned long KernelImageSize;
    unsigned long KernelPreferredBase;
    unsigned long KernelTextRva;
    unsigned long KernelTextSize;
    unsigned long BranchMatches;
    unsigned long BranchAddress;
    unsigned long CallAddress;
    unsigned long IatAddress;
    unsigned long HalTarget;
    unsigned long AcpiReady;
    unsigned long HalImportFromTable;
    unsigned long HalCallCandidates;
    unsigned long FirstHalCall;
    unsigned char BranchContext[48];
    unsigned char IatContext[16];
    unsigned char HalCallContext[48];
} NTPWR_REPORT;
#pragma pack()

#endif
