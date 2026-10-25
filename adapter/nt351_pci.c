#include <uacpi/kernel_api.h>
#include "nt351_kernel_abi.h"

extern void __cdecl NtpwrTracePciWrite(unsigned long, unsigned long,
                                      unsigned long, unsigned long,
                                      unsigned long, unsigned long);

#define NT351_PCI_TAG 0x49504355UL
#define NT351_PCI_CONFIG_BYTES 256U

typedef struct NT351_PCI_HANDLE {
    NT351_U32 bus;
    NT351_U32 slot;
} NT351_PCI_HANDLE;

static uacpi_status nt351_pci_read(
    uacpi_handle handle, uacpi_size offset, uacpi_u32 width, void *value)
{
    NT351_PCI_HANDLE *pci = (NT351_PCI_HANDLE *)handle;
    NT351_U32 done;
    if (!pci || !value || offset > NT351_PCI_CONFIG_BYTES ||
        width > NT351_PCI_CONFIG_BYTES - offset || (offset & (width - 1U)))
        return UACPI_STATUS_INVALID_ARGUMENT;
    done = HalGetBusDataByOffset(NT351_PCI_CONFIGURATION, pci->bus,
                                 pci->slot, value, (NT351_U32)offset, width);
    return done == width ? UACPI_STATUS_OK : UACPI_STATUS_NOT_FOUND;
}

uacpi_status __cdecl uacpi_kernel_pci_device_open(
    uacpi_pci_address addr, uacpi_handle *out_handle)
{
    NT351_PCI_HANDLE *pci;
    uacpi_u16 vendor = 0xFFFFU;
    if (!out_handle) return UACPI_STATUS_INVALID_ARGUMENT;
    *out_handle = UACPI_NULL;
    if (addr.segment != 0 || addr.device >= 32 || addr.function >= 8)
        return UACPI_STATUS_NOT_FOUND;
    pci = (NT351_PCI_HANDLE *)ExAllocatePoolWithTag(
        NT351_NONPAGED_POOL, (NT351_U32)sizeof(*pci), NT351_PCI_TAG);
    if (!pci) return UACPI_STATUS_OUT_OF_MEMORY;
    pci->bus = addr.bus;
    pci->slot = (NT351_U32)addr.device | ((NT351_U32)addr.function << 5);
    if (nt351_pci_read(pci, 0, 2, &vendor) != UACPI_STATUS_OK || vendor == 0xFFFFU) {
        ExFreePool(pci);
        return UACPI_STATUS_NOT_FOUND;
    }
    *out_handle = pci;
    return UACPI_STATUS_OK;
}

void __cdecl uacpi_kernel_pci_device_close(uacpi_handle handle)
{
    if (handle) ExFreePool(handle);
}

uacpi_status __cdecl uacpi_kernel_pci_read8(
    uacpi_handle handle, uacpi_size offset, uacpi_u8 *value)
{
    return nt351_pci_read(handle, offset, 1, value);
}

uacpi_status __cdecl uacpi_kernel_pci_read16(
    uacpi_handle handle, uacpi_size offset, uacpi_u16 *value)
{
    return nt351_pci_read(handle, offset, 2, value);
}

uacpi_status __cdecl uacpi_kernel_pci_read32(
    uacpi_handle handle, uacpi_size offset, uacpi_u32 *value)
{
    return nt351_pci_read(handle, offset, 4, value);
}

static uacpi_status nt351_pci_write(
    uacpi_handle handle, uacpi_size offset, uacpi_u32 width,
    void *value)
{
    NT351_PCI_HANDLE *pci = (NT351_PCI_HANDLE *)handle;
    NT351_U32 done;
    if (!pci || !value || offset > NT351_PCI_CONFIG_BYTES ||
        width > NT351_PCI_CONFIG_BYTES - offset || (offset & (width - 1U)))
        return UACPI_STATUS_INVALID_ARGUMENT;
    done = HalSetBusDataByOffset(NT351_PCI_CONFIGURATION, pci->bus,
                                 pci->slot, value, (NT351_U32)offset, width);
    NtpwrTracePciWrite(pci->bus, pci->slot, (NT351_U32)offset, width,
                       width == 1 ? *(uacpi_u8 *)value :
                       width == 2 ? *(uacpi_u16 *)value : *(uacpi_u32 *)value,
                       done);
    return done == width ? UACPI_STATUS_OK : UACPI_STATUS_NOT_FOUND;
}

uacpi_status __cdecl uacpi_kernel_pci_write8(
    uacpi_handle handle, uacpi_size offset, uacpi_u8 value)
{
    return nt351_pci_write(handle, offset, 1, &value);
}

uacpi_status __cdecl uacpi_kernel_pci_write16(
    uacpi_handle handle, uacpi_size offset, uacpi_u16 value)
{
    return nt351_pci_write(handle, offset, 2, &value);
}

uacpi_status __cdecl uacpi_kernel_pci_write32(
    uacpi_handle handle, uacpi_size offset, uacpi_u32 value)
{
    return nt351_pci_write(handle, offset, 4, &value);
}
