#include <uacpi/kernel_api.h>

extern void __cdecl NtpwrTraceAcpiEvent(const char *);

uacpi_status __cdecl uacpi_kernel_install_interrupt_handler(
    uacpi_u32 irq, uacpi_interrupt_handler handler,
    uacpi_handle context, uacpi_handle *out_irq_handle)
{
    (void)irq;
    (void)handler;
    (void)context;
    if (out_irq_handle) *out_irq_handle = UACPI_NULL;
    return UACPI_STATUS_UNIMPLEMENTED;
}

uacpi_status __cdecl uacpi_kernel_uninstall_interrupt_handler(
    uacpi_interrupt_handler handler, uacpi_handle irq_handle)
{
    (void)handler;
    (void)irq_handle;
    return UACPI_STATUS_UNIMPLEMENTED;
}

uacpi_status __cdecl uacpi_kernel_handle_firmware_request(
    uacpi_firmware_request *request)
{
    (void)request;
    NtpwrTraceAcpiEvent("firmware request denied");
    return UACPI_STATUS_DENIED;
}
