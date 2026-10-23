#include <uacpi/kernel_api.h>
#include "nt351_kernel_abi.h"

#define NT351_POOL_TAG_SPIN 0x4E535541UL
#define NT351_EFLAGS_IF 0x00000200UL

typedef struct NT351_UACPI_SPINLOCK {
    volatile uacpi_u32 acquired;
} NT351_UACPI_SPINLOCK;

/* x86 ring-0 EFLAGS.IF management; no change to the Windows IRQL. */
uacpi_interrupt_state __cdecl uacpi_kernel_disable_interrupts(void)
{
    unsigned long flags;
    __asm__ __volatile__("pushfl\n\tpopl %0\n\tcli" : "=r"(flags) : : "memory", "cc");
    return (uacpi_interrupt_state)flags;
}

void __cdecl uacpi_kernel_restore_interrupts(uacpi_interrupt_state state)
{
    /* Restore only the original IF bit; do not overwrite other CPU flags. */
    if (state & NT351_EFLAGS_IF)
        __asm__ __volatile__("sti" : : : "memory");
    else
        __asm__ __volatile__("" : : : "memory");
}

uacpi_handle __cdecl uacpi_kernel_create_spinlock(void)
{
    NT351_UACPI_SPINLOCK *lock = (NT351_UACPI_SPINLOCK *)ExAllocatePoolWithTag(
        NT351_NONPAGED_POOL, (NT351_U32)sizeof(NT351_UACPI_SPINLOCK),
        NT351_POOL_TAG_SPIN
    );
    if (!lock)
        return UACPI_NULL;
    lock->acquired = 0;
    return (uacpi_handle)lock;
}

void __cdecl uacpi_kernel_free_spinlock(uacpi_handle handle)
{
    if (handle)
        ExFreePool(handle);
}

uacpi_cpu_flags __cdecl uacpi_kernel_lock_spinlock(uacpi_handle handle)
{
    NT351_UACPI_SPINLOCK *lock = (NT351_UACPI_SPINLOCK *)handle;
    uacpi_interrupt_state flags = uacpi_kernel_disable_interrupts();
    /* xchg is atomic on i486; acquire/release ordering guards critical data. */
    while (__atomic_exchange_n(&lock->acquired, 1U, __ATOMIC_ACQUIRE)) {
        while (__atomic_load_n(&lock->acquired, __ATOMIC_RELAXED))
            __asm__ __volatile__("nop" : : : "memory");
    }
    return (uacpi_cpu_flags)flags;
}

void __cdecl uacpi_kernel_unlock_spinlock(uacpi_handle handle, uacpi_cpu_flags flags)
{
    NT351_UACPI_SPINLOCK *lock = (NT351_UACPI_SPINLOCK *)handle;
    __atomic_store_n(&lock->acquired, 0U, __ATOMIC_RELEASE);
    uacpi_kernel_restore_interrupts((uacpi_interrupt_state)flags);
}
