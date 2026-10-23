/* Implements the NT 3.51 DDK fastcall InterlockedExchange ABI for i486. */
long __fastcall InterlockedExchange(volatile long *target, long value)
{
    return __atomic_exchange_n(target, value, __ATOMIC_SEQ_CST);
}
