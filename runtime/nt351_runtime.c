/* Freestanding i486 runtime primitives for a link-only integration test. */
typedef __SIZE_TYPE__ nt_size_t;
typedef unsigned long long nt_u64;

void *memcpy(void *dst, const void *src, nt_size_t len)
{
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    nt_size_t i;
    for (i = 0; i < len; ++i)
        d[i] = s[i];
    return dst;
}

void *memmove(void *dst, const void *src, nt_size_t len)
{
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    nt_size_t i;
    if (d == s || len == 0)
        return dst;
    if ((unsigned long long)(unsigned long)d < (unsigned long long)(unsigned long)s) {
        for (i = 0; i < len; ++i)
            d[i] = s[i];
    } else {
        for (i = len; i != 0; --i)
            d[i - 1] = s[i - 1];
    }
    return dst;
}

void *memset(void *dst, int value, nt_size_t len)
{
    unsigned char *d = (unsigned char *)dst;
    nt_size_t i;
    for (i = 0; i < len; ++i)
        d[i] = (unsigned char)value;
    return dst;
}

int memcmp(const void *lhs, const void *rhs, nt_size_t len)
{
    const unsigned char *a = (const unsigned char *)lhs;
    const unsigned char *b = (const unsigned char *)rhs;
    nt_size_t i;
    for (i = 0; i < len; ++i) {
        if (a[i] != b[i])
            return (int)a[i] - (int)b[i];
    }
    return 0;
}

static void nt_divzero(void)
{
    volatile unsigned int zero = 0;
    volatile unsigned int one = 1;
    one /= zero;
}

void nt_divmod64(nt_u64 dividend, nt_u64 divisor, nt_u64 *quotient, nt_u64 *remainder)
{
    nt_u64 q = 0;
    nt_u64 r = 0;
    unsigned int i;
    if (divisor == 0) {
        nt_divzero();
        *quotient = 0;
        *remainder = 0;
        return;
    }
    for (i = 0; i < 64; ++i) {
        unsigned int carry = (unsigned int)(r >> 63);
        r = (r << 1) | ((dividend >> (63 - i)) & 1ULL);
        if (carry || r >= divisor) {
            r -= divisor;
            q = (q << 1) | 1ULL;
        } else {
            q <<= 1;
        }
    }
    *quotient = q;
    *remainder = r;
}

/* MSVC x86 compiler helpers consume their 16-byte arguments with ret 16. */
nt_u64 __attribute__((stdcall)) nt_aulldiv(nt_u64 a, nt_u64 b) __asm__("__aulldiv");
nt_u64 __attribute__((stdcall)) nt_aulldiv(nt_u64 a, nt_u64 b)
{
    nt_u64 q, r;
    nt_divmod64(a, b, &q, &r);
    return q;
}

nt_u64 __attribute__((stdcall)) nt_aullrem(nt_u64 a, nt_u64 b) __asm__("__aullrem");
nt_u64 __attribute__((stdcall)) nt_aullrem(nt_u64 a, nt_u64 b)
{
    nt_u64 q, r;
    nt_divmod64(a, b, &q, &r);
    return r;
}
