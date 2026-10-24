#include <uacpi/kernel_api.h>
#include "nt351_kernel_abi.h"

#define NT351_INTERRUPT_TAG 0x49504155UL
#define NT351_INTERNAL_BUS 0UL
#define NT351_ISA_BUS 1UL
#define NT351_EISA_BUS 2UL
#define NT351_PCI_BUS 5UL
#define NT351_LEVEL_SENSITIVE 0UL

static NT351_U32 g_sci_phase;
static NT351_U32 g_sci_irq;
static NT351_U32 g_sci_vector;
static NT351_U32 g_sci_status;
static NT351_U32 g_sci_eisa_vector;
static NT351_U32 g_sci_pci_vector;

void __cdecl NtpwrSciQuery(NT351_U32 *phase, NT351_U32 *irq,
                          NT351_U32 *vector, NT351_U32 *status,
                          NT351_U32 *eisa_vector, NT351_U32 *pci_vector)
{
    *phase = g_sci_phase;
    *irq = g_sci_irq;
    *vector = g_sci_vector;
    *status = g_sci_status;
    *eisa_vector = g_sci_eisa_vector;
    *pci_vector = g_sci_pci_vector;
}


typedef NT351_U8 (__stdcall *NT351_ISR)(void *, void *);

typedef struct NT351_ACPI_INTERRUPT {
    void *nt_interrupt;
    uacpi_interrupt_handler handler;
    uacpi_handle context;
} NT351_ACPI_INTERRUPT;

__declspec(dllimport) NT351_U32 __stdcall HalGetInterruptVector(
    NT351_U32 bus_type, NT351_U32 bus_number,
    NT351_U32 bus_level, NT351_U32 bus_vector,
    NT351_U8 *irql, NT351_U32 *affinity);

__declspec(dllimport) NT351_LONG __stdcall IoConnectInterrupt(
    void **interrupt_object, NT351_ISR service_routine, void *service_context,
    void *spin_lock, NT351_U32 vector, NT351_U8 irql,
    NT351_U8 synchronize_irql, NT351_U32 mode, NT351_U8 share_vector,
    NT351_U32 affinity, NT351_U8 floating_save);

__declspec(dllimport) void __stdcall IoDisconnectInterrupt(void *interrupt_object);

static NT351_U8 __stdcall Nt351AcpiIsr(void *interrupt, void *context)
{
    NT351_ACPI_INTERRUPT *item = (NT351_ACPI_INTERRUPT *)context;
    (void)interrupt;
    return item->handler(item->context) != UACPI_INTERRUPT_NOT_HANDLED;
}

uacpi_status __cdecl uacpi_kernel_install_interrupt_handler(
    uacpi_u32 irq, uacpi_interrupt_handler handler,
    uacpi_handle context, uacpi_handle *out_irq_handle)
{
    NT351_ACPI_INTERRUPT *item;
    NT351_U8 irql = 0;
    NT351_U32 affinity = 0;
    NT351_U32 vector;
    NT351_LONG status;

    g_sci_phase = 0;
    g_sci_irq = irq;
    g_sci_vector = 0;
    g_sci_status = 0;
    g_sci_eisa_vector = 0;
    g_sci_pci_vector = 0;
    if (!out_irq_handle || !handler || irq == 0) return UACPI_STATUS_INVALID_ARGUMENT;
    *out_irq_handle = UACPI_NULL;

    vector = HalGetInterruptVector(NT351_INTERNAL_BUS, 0, irq, irq,
                                   &irql, &affinity);
    if (!vector || !affinity) {
        irql = 0;
        affinity = 0;
        vector = HalGetInterruptVector(NT351_ISA_BUS, 0, irq, irq,
                                       &irql, &affinity);
        if (vector && affinity)
            DbgPrint("NTPWR: SCI IRQ=%lu mapped via ISA\n", (unsigned long)irq);
    }
    g_sci_vector = vector;
    if (!vector || !affinity) {
        NT351_U8 probe_irql = 0;
        NT351_U32 probe_affinity = 0;
        g_sci_phase = 1;
        g_sci_eisa_vector = HalGetInterruptVector(NT351_EISA_BUS, 0, irq, irq,
                                                 &probe_irql, &probe_affinity);
        if (!probe_affinity) g_sci_eisa_vector = 0;
        probe_irql = 0;
        probe_affinity = 0;
        g_sci_pci_vector = HalGetInterruptVector(NT351_PCI_BUS, 0, irq, irq,
                                                &probe_irql, &probe_affinity);
        if (!probe_affinity) g_sci_pci_vector = 0;
        DbgPrint("NTPWR: SCI IRQ=%lu unresolved; EISA=0x%08lX PCI=0x%08lX\n",
                 (unsigned long)irq, (unsigned long)g_sci_eisa_vector,
                 (unsigned long)g_sci_pci_vector);
        return UACPI_STATUS_UNIMPLEMENTED;
    }

    item = ExAllocatePoolWithTag(NT351_NONPAGED_POOL, sizeof(*item), NT351_INTERRUPT_TAG);
    if (!item) return UACPI_STATUS_OUT_OF_MEMORY;
    item->nt_interrupt = UACPI_NULL;
    item->handler = handler;
    item->context = context;
    status = IoConnectInterrupt(&item->nt_interrupt, Nt351AcpiIsr, item,
                                UACPI_NULL, vector, irql, irql,
                                NT351_LEVEL_SENSITIVE, 1, affinity, 0);
    g_sci_status = (NT351_U32)status;
    if (status < 0) {
        g_sci_phase = 2;
        DbgPrint("NTPWR: SCI connect IRQ=%lu vector=%lu status=0x%08lX\n",
                 (unsigned long)irq, (unsigned long)vector, (unsigned long)status);
        ExFreePool(item);
        return UACPI_STATUS_UNIMPLEMENTED;
    }
    g_sci_phase = 3;
    *out_irq_handle = item;
    DbgPrint("NTPWR: SCI connected IRQ=%lu vector=%lu IRQL=%lu\n",
             (unsigned long)irq, (unsigned long)vector, (unsigned long)irql);
    return UACPI_STATUS_OK;
}

uacpi_status __cdecl uacpi_kernel_uninstall_interrupt_handler(
    uacpi_interrupt_handler handler, uacpi_handle irq_handle)
{
    NT351_ACPI_INTERRUPT *item = (NT351_ACPI_INTERRUPT *)irq_handle;
    if (!item || item->handler != handler) return UACPI_STATUS_INVALID_ARGUMENT;
    IoDisconnectInterrupt(item->nt_interrupt);
    ExFreePool(item);
    return UACPI_STATUS_OK;
}

uacpi_status __cdecl uacpi_kernel_handle_firmware_request(
    uacpi_firmware_request *request)
{
    (void)request;
    return UACPI_STATUS_DENIED;
}
