#include <uacpi/kernel_api.h>
#include "nt351_kernel_abi.h"

#define NT351_WORK_TAG 0x4B575541UL
#define NT351_DELAYED_WORK_QUEUE 1UL

typedef struct NT351_UACPI_WORK {
    NT351_WORK_QUEUE_ITEM nt_work;
    uacpi_work_handler callback;
    uacpi_handle context;
} NT351_UACPI_WORK;

static volatile NT351_U32 nt351_work_pending = 0;

static void __stdcall nt351_execute_work(void *context)
{
    NT351_UACPI_WORK *item = (NT351_UACPI_WORK *)context;
    uacpi_work_handler callback = item->callback;
    uacpi_handle argument = item->context;
    item->callback = UACPI_NULL;
    item->context = UACPI_NULL;
    /* Callback may requeue or free 'item'. Do not touch it afterwards. */
    callback(argument);
    (void)__atomic_sub_fetch(&nt351_work_pending, 1U, __ATOMIC_ACQ_REL);
}

uacpi_handle __cdecl uacpi_kernel_create_work_item(void)
{
    NT351_UACPI_WORK *item = (NT351_UACPI_WORK *)ExAllocatePoolWithTag(
        NT351_NONPAGED_POOL, (NT351_U32)sizeof(*item), NT351_WORK_TAG);
    if (!item) return UACPI_NULL;
    item->nt_work.List.Flink = UACPI_NULL;
    item->nt_work.List.Blink = UACPI_NULL;
    item->nt_work.WorkerRoutine = nt351_execute_work;
    item->nt_work.Parameter = item;
    item->callback = UACPI_NULL;
    item->context = UACPI_NULL;
    return item;
}

void __cdecl uacpi_kernel_free_work_item(uacpi_handle handle)
{
    if (handle) ExFreePool(handle);
}

void __cdecl uacpi_kernel_schedule_work(
    uacpi_work_type type, uacpi_handle handle,
    uacpi_work_handler callback, uacpi_handle context)
{
    NT351_UACPI_WORK *item = (NT351_UACPI_WORK *)handle;
    (void)type;
    if (!item || !callback) {
        DbgPrint("uACPI: invalid work item\n");
        return;
    }
    item->callback = callback;
    item->context = context;
    item->nt_work.List.Flink = UACPI_NULL;
    (void)__atomic_add_fetch(&nt351_work_pending, 1U, __ATOMIC_ACQ_REL);
    ExQueueWorkItem(&item->nt_work, NT351_DELAYED_WORK_QUEUE);
}

uacpi_status __cdecl uacpi_kernel_wait_for_work_completion(void)
{
    NT351_LARGE_INTEGER interval;
    NT351_U32 attempt;
    interval.QuadPart = -10000LL; /* 1 ms */
    /* Interrupt registration is intentionally disabled in this audit. */
    for (attempt = 0; attempt < 30000; ++attempt) {
        if (__atomic_load_n(&nt351_work_pending, __ATOMIC_ACQUIRE) == 0)
            return UACPI_STATUS_OK;
        (void)KeDelayExecutionThread(0, 0, &interval);
    }
    return UACPI_STATUS_TIMEOUT;
}
