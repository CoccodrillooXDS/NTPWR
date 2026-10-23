#include <uacpi/kernel_api.h>
#include "nt351_kernel_abi.h"

#define NT351_SEMAPHORE_LIMIT 0x7FFFFFFFL
#define NT351_STATUS_SUCCESS 0
#define NT351_STATUS_TIMEOUT 0x00000102L
#define NT351_POOL_TAG_SEM 0x4D535541UL
#define NT351_NS_PER_SEC 1000000000ULL

/* The NT dispatcher semaphore is 20 bytes on the original 32-bit DDK ABI. */
uacpi_handle __cdecl uacpi_kernel_create_semaphore(uacpi_u32 initial_units)
{
    NT351_KSEMAPHORE *sem;
    if (initial_units > (uacpi_u32)NT351_SEMAPHORE_LIMIT)
        return UACPI_NULL;
    sem = ExAllocatePoolWithTag(NT351_NONPAGED_POOL,
                                (NT351_U32)sizeof(*sem), NT351_POOL_TAG_SEM);
    if (!sem) return UACPI_NULL;
    KeInitializeSemaphore(sem, (NT351_LONG)initial_units, NT351_SEMAPHORE_LIMIT);
    return sem;
}

void __cdecl uacpi_kernel_free_semaphore(uacpi_handle handle)
{
    if (handle) ExFreePool(handle);
}

uacpi_status __cdecl uacpi_kernel_wait_for_semaphore(
    uacpi_handle handle, uacpi_u16 timeout_ms)
{
    NT351_LARGE_INTEGER timeout;
    NT351_LARGE_INTEGER *deadline = &timeout;
    NT351_LONG status;
    if (!handle) return UACPI_STATUS_INVALID_ARGUMENT;
    if (timeout_ms == 0xFFFFU) {
        deadline = (NT351_LARGE_INTEGER *)0;
    } else {
        timeout.QuadPart = -(long long)((uacpi_u32)timeout_ms * 10000U);
    }
    status = KeWaitForSingleObject(handle, 0 /* Executive */,
                                    0 /* KernelMode */, 0, deadline);
    if (status == NT351_STATUS_SUCCESS) return UACPI_STATUS_OK;
    if (status == NT351_STATUS_TIMEOUT) return UACPI_STATUS_TIMEOUT;
    return UACPI_STATUS_INTERNAL_ERROR;
}

void __cdecl uacpi_kernel_signal_semaphore(uacpi_handle handle)
{
    if (handle) (void)KeReleaseSemaphore(handle, 0, 1, 0);
}

uacpi_thread_id __cdecl uacpi_kernel_get_thread_id(void)
{
    return (uacpi_thread_id)KeGetCurrentThread();
}

/* KeQueryPerformanceCounter is a HAL export on NT 3.51 x86. */
uacpi_u64 __cdecl uacpi_kernel_get_nanoseconds_since_boot(void)
{
    NT351_LARGE_INTEGER freq;
    NT351_LARGE_INTEGER ticks = KeQueryPerformanceCounter(&freq);
    uacpi_u64 f, t, secs, rem;
    if (freq.QuadPart <= 0 || ticks.QuadPart < 0) return 0;
    f = (uacpi_u64)freq.QuadPart;
    t = (uacpi_u64)ticks.QuadPart;
    secs = t / f;
    rem = t % f;
    /* Bound the multiplication before converting subsecond ticks. */
    if (secs > (~(uacpi_u64)0) / NT351_NS_PER_SEC) return ~(uacpi_u64)0;
    if (f > (~(uacpi_u64)0) / NT351_NS_PER_SEC) {
        /* Preserve monotonicity for unusual counters above ~18 GHz. */
        return secs * NT351_NS_PER_SEC + rem / (f / NT351_NS_PER_SEC + 1U);
    }
    return secs * NT351_NS_PER_SEC + (rem * NT351_NS_PER_SEC) / f;
}

void __cdecl uacpi_kernel_stall(uacpi_u8 usec)
{
    KeStallExecutionProcessor((NT351_U32)usec);
}

void __cdecl uacpi_kernel_sleep(uacpi_u64 msec)
{
    while (msec) {
        /* Bound the relative delay to 60 seconds per wait. */
        uacpi_u64 batch = msec > 60000U ? 60000U : msec;
        NT351_LARGE_INTEGER duration;
        duration.QuadPart = -(long long)(batch * 10000U);
        (void)KeDelayExecutionThread(0 /* KernelMode */, 0, &duration);
        msec -= batch;
    }
}
