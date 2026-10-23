#include <uacpi/kernel_api.h>
#include "nt351_kernel_abi.h"

#define NT351_PAGE_SIZE 4096U

static uacpi_bool nt351_rsdp_valid(const uacpi_u8 *p, uacpi_size available)
{
    static const char signature[8] = {'R', 'S', 'D', ' ', 'P', 'T', 'R', ' '};
    uacpi_u32 length;
    uacpi_u32 i;
    uacpi_u8 sum = 0;
    if (available < 20) return UACPI_FALSE;
    for (i = 0; i < 8; i++) if (p[i] != (uacpi_u8)signature[i]) return UACPI_FALSE;
    for (i = 0; i < 20; i++) sum = (uacpi_u8)(sum + p[i]);
    if (sum) return UACPI_FALSE;
    if (p[15] < 2) return UACPI_TRUE;
    if (available < 36) return UACPI_FALSE;
    length = (uacpi_u32)p[20] | ((uacpi_u32)p[21] << 8)
           | ((uacpi_u32)p[22] << 16) | ((uacpi_u32)p[23] << 24);
    if (length < 36 || length > available) return UACPI_FALSE;
    sum = 0;
    for (i = 0; i < length; i++) sum = (uacpi_u8)(sum + p[i]);
    return sum == 0 ? UACPI_TRUE : UACPI_FALSE;
}

/* No AML execution, interrupts, PCI or firmware writes are enabled here. */
static uacpi_status nt351_scan_region(
    uacpi_u32 address, uacpi_u32 length, uacpi_phys_addr *result)
{
    uacpi_u32 offset;
    for (offset = 0; offset < length; offset += NT351_PAGE_SIZE) {
        uacpi_u32 chunk = length - offset;
        uacpi_u32 checked = chunk;
        const uacpi_u8 *p;
        uacpi_u32 i;
        if (checked > NT351_PAGE_SIZE) checked = NT351_PAGE_SIZE;
        if (chunk > NT351_PAGE_SIZE + 64) chunk = NT351_PAGE_SIZE + 64;
        p = (const uacpi_u8 *)uacpi_kernel_map((uacpi_phys_addr)(address + offset), chunk);
        if (p == UACPI_MAP_FAILED) return UACPI_STATUS_MAPPING_FAILED;
        for (i = 0; i + 20 <= checked; i += 16) {
            if (nt351_rsdp_valid(p + i, chunk - i)) {
                *result = (uacpi_phys_addr)(address + offset + i);
                uacpi_kernel_unmap((void *)p, chunk);
                return UACPI_STATUS_OK;
            }
        }
        uacpi_kernel_unmap((void *)p, chunk);
    }
    return UACPI_STATUS_NOT_FOUND;
}

uacpi_status __cdecl uacpi_kernel_get_rsdp(uacpi_phys_addr *out_rsdp_address)
{
    const uacpi_u8 *ebda_ptr;
    uacpi_u32 segment;
    uacpi_status ret;
    if (out_rsdp_address == UACPI_NULL) return UACPI_STATUS_INVALID_ARGUMENT;
    *out_rsdp_address = 0;
    ebda_ptr = (const uacpi_u8 *)uacpi_kernel_map(0x40E, 2);
    if (ebda_ptr != UACPI_MAP_FAILED) {
        segment = (uacpi_u32)ebda_ptr[0] | ((uacpi_u32)ebda_ptr[1] << 8);
        uacpi_kernel_unmap((void *)ebda_ptr, 2);
        if (segment >= 0x8000 && segment < 0xA000) {
            ret = nt351_scan_region(segment << 4, 1024, out_rsdp_address);
            if (ret == UACPI_STATUS_OK) return ret;
        }
    }
    return nt351_scan_region(0xE0000, 0x20000, out_rsdp_address);
}

void *__cdecl uacpi_kernel_alloc(uacpi_size size)
{
    if (size == 0 || size > 0xFFFFFFFFUL) return UACPI_NULL;
    return ExAllocatePoolWithTag(NT351_NONPAGED_POOL, (NT351_U32)size, NT351_POOL_TAG);
}

void __cdecl uacpi_kernel_free(void *mem)
{
    if (mem) ExFreePool(mem);
}

void *__cdecl uacpi_kernel_map(uacpi_phys_addr address, uacpi_size length)
{
    NT351_PHYSICAL_ADDRESS phys;
    void *mapped;
    if (!length || length > 0xFFFFFFFFUL) return UACPI_MAP_FAILED;
    phys.LowPart = (NT351_U32)address;
    phys.HighPart = (NT351_LONG)(address >> 32);
    mapped = MmMapIoSpace(phys, (NT351_U32)length, 0);
    return mapped ? mapped : UACPI_MAP_FAILED;
}

void __cdecl uacpi_kernel_unmap(void *addr, uacpi_size length)
{
    if (addr && addr != UACPI_MAP_FAILED && length && length <= 0xFFFFFFFFUL)
        MmUnmapIoSpace(addr, (NT351_U32)length);
}

void __cdecl uacpi_kernel_log(uacpi_log_level level, const uacpi_char *message)
{
    (void)level;
    if (message) DbgPrint("uACPI: %s", message);
}
