#include <uacpi/kernel_api.h>
#include "nt351_kernel_abi.h"

#define NT351_PORT_SPACE_SIZE 65536U
#define NT351_POOL_TAG_IO 0x4F495541UL

typedef struct NT351_IO_RANGE {
    uacpi_u32 base;
    uacpi_u32 length;
} NT351_IO_RANGE;

static uacpi_status nt351_port_address(
    uacpi_handle handle, uacpi_size offset, uacpi_u32 width,
    NT351_U32 *port)
{
    NT351_IO_RANGE *range = (NT351_IO_RANGE *)handle;
    if (!range || !port || offset > range->length ||
        width > range->length - offset)
        return UACPI_STATUS_INVALID_ARGUMENT;
    *port = range->base + (NT351_U32)offset;
    return UACPI_STATUS_OK;
}

uacpi_status __cdecl uacpi_kernel_io_map(
    uacpi_io_addr base, uacpi_size len, uacpi_handle *out_handle)
{
    NT351_IO_RANGE *range;
    if (!out_handle)
        return UACPI_STATUS_INVALID_ARGUMENT;
    *out_handle = UACPI_NULL;
    if (!len || base >= NT351_PORT_SPACE_SIZE ||
        len > (uacpi_size)(NT351_PORT_SPACE_SIZE - base))
        return UACPI_STATUS_INVALID_ARGUMENT;
    range = ExAllocatePoolWithTag(NT351_NONPAGED_POOL,
                                  (NT351_U32)sizeof(*range), NT351_POOL_TAG_IO);
    if (!range)
        return UACPI_STATUS_OUT_OF_MEMORY;
    range->base = (uacpi_u32)base;
    range->length = (uacpi_u32)len;
    *out_handle = range;
    return UACPI_STATUS_OK;
}

void __cdecl uacpi_kernel_io_unmap(uacpi_handle handle)
{
    if (handle)
        ExFreePool(handle);
}

uacpi_status __cdecl uacpi_kernel_io_read8(
    uacpi_handle handle, uacpi_size offset, uacpi_u8 *value)
{
    NT351_U32 port;
    uacpi_status status;
    if (!value)
        return UACPI_STATUS_INVALID_ARGUMENT;
    status = nt351_port_address(handle, offset, 1, &port);
    if (status != UACPI_STATUS_OK)
        return status;
    *value = READ_PORT_UCHAR((NT351_U8 *)(uacpi_uintptr)port);
    return UACPI_STATUS_OK;
}

uacpi_status __cdecl uacpi_kernel_io_read16(
    uacpi_handle handle, uacpi_size offset, uacpi_u16 *value)
{
    NT351_U32 port;
    uacpi_status status;
    if (!value)
        return UACPI_STATUS_INVALID_ARGUMENT;
    status = nt351_port_address(handle, offset, 2, &port);
    if (status != UACPI_STATUS_OK)
        return status;
    *value = READ_PORT_USHORT((unsigned short *)(uacpi_uintptr)port);
    return UACPI_STATUS_OK;
}

uacpi_status __cdecl uacpi_kernel_io_read32(
    uacpi_handle handle, uacpi_size offset, uacpi_u32 *value)
{
    NT351_U32 port;
    uacpi_status status;
    if (!value)
        return UACPI_STATUS_INVALID_ARGUMENT;
    status = nt351_port_address(handle, offset, 4, &port);
    if (status != UACPI_STATUS_OK)
        return status;
    *value = READ_PORT_ULONG((NT351_U32 *)(uacpi_uintptr)port);
    return UACPI_STATUS_OK;
}

uacpi_status __cdecl uacpi_kernel_io_write8(
    uacpi_handle handle, uacpi_size offset, uacpi_u8 value)
{
    NT351_U32 port;
    uacpi_status status = nt351_port_address(handle, offset, 1, &port);
    if (status != UACPI_STATUS_OK)
        return status;
    WRITE_PORT_UCHAR((NT351_U8 *)(uacpi_uintptr)port, value);
    return UACPI_STATUS_OK;
}

uacpi_status __cdecl uacpi_kernel_io_write16(
    uacpi_handle handle, uacpi_size offset, uacpi_u16 value)
{
    NT351_U32 port;
    uacpi_status status = nt351_port_address(handle, offset, 2, &port);
    if (status != UACPI_STATUS_OK)
        return status;
    WRITE_PORT_USHORT((unsigned short *)(uacpi_uintptr)port, value);
    return UACPI_STATUS_OK;
}

uacpi_status __cdecl uacpi_kernel_io_write32(
    uacpi_handle handle, uacpi_size offset, uacpi_u32 value)
{
    NT351_U32 port;
    uacpi_status status = nt351_port_address(handle, offset, 4, &port);
    if (status != UACPI_STATUS_OK)
        return status;
    WRITE_PORT_ULONG((NT351_U32 *)(uacpi_uintptr)port, value);
    return UACPI_STATUS_OK;
}
