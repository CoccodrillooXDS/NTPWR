#include <uacpi/kernel_api.h>
#include "nt351_kernel_abi.h"

/* Diagnostic builds never write to MMIO. */
uacpi_u8 __cdecl uacpi_kernel_mmio_read8(void *ptr) { return *(volatile uacpi_u8 *)ptr; }
uacpi_u16 __cdecl uacpi_kernel_mmio_read16(void *ptr) { return *(volatile uacpi_u16 *)ptr; }
uacpi_u32 __cdecl uacpi_kernel_mmio_read32(void *ptr) { return *(volatile uacpi_u32 *)ptr; }
uacpi_u64 __cdecl uacpi_kernel_mmio_read64(void *ptr)
{
    volatile uacpi_u32 *words = (volatile uacpi_u32 *)ptr;
    uacpi_u32 lo = words[0], hi = words[1];
    return ((uacpi_u64)hi << 32) | lo;
}
void __cdecl uacpi_kernel_mmio_write8(void *ptr, uacpi_u8 val) { (void)ptr; (void)val; }
void __cdecl uacpi_kernel_mmio_write16(void *ptr, uacpi_u16 val) { (void)ptr; (void)val; }
void __cdecl uacpi_kernel_mmio_write32(void *ptr, uacpi_u32 val) { (void)ptr; (void)val; }
void __cdecl uacpi_kernel_mmio_write64(void *ptr, uacpi_u64 val) { (void)ptr; (void)val; }
