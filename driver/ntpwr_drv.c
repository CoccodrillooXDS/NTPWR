#include <uacpi/uacpi.h>
#include <uacpi/types.h>
#include <uacpi/tables.h>
#include <uacpi/acpi.h>
#include <uacpi/registers.h>
#include <uacpi/sleep.h>
#include <uacpi/io.h>
#include "nt351_kernel_abi.h"

/* ABI-compatible declarations for the NT 3.51 driver interface. */
typedef unsigned short NT_WCHAR;
typedef void *NT_HANDLE;
typedef struct NT_UNICODE_STRING {
    unsigned short Length;
    unsigned short MaximumLength;
    NT_WCHAR *Buffer;
} NT_UNICODE_STRING;
typedef struct NT_OBJECT_ATTRIBUTES {
    NT351_U32 Length;
    NT_HANDLE RootDirectory;
    NT_UNICODE_STRING *ObjectName;
    NT351_U32 Attributes;
    void *SecurityDescriptor;
    void *SecurityQualityOfService;
} NT_OBJECT_ATTRIBUTES;
typedef struct NT_DRIVER_OBJECT_PREFIX {
    unsigned short Type;
    unsigned short Size;
    void *DeviceObject;
    NT351_U32 Flags;
    void *DriverStart;
    NT351_U32 DriverSize;
    void *DriverSection;
    NT351_U32 Count;
    NT_UNICODE_STRING DriverName;
    void *HardwareDatabase;
    void *FastIoDispatch;
    void *DriverInit;
    void *DriverStartIo;
    void (__stdcall *DriverUnload)(void *);
} NT_DRIVER_OBJECT_PREFIX;
NT351_ASSERT(sizeof(NT_OBJECT_ATTRIBUTES) == 24, ntpwr_object_attributes_24);
NT351_ASSERT(sizeof(NT_DRIVER_OBJECT_PREFIX) == 56, ntpwr_driver_object_prefix_56);

__declspec(dllimport) NT351_LONG __stdcall ZwOpenKey(
    NT_HANDLE *key, NT351_U32 access, NT_OBJECT_ATTRIBUTES *attributes);
__declspec(dllimport) NT351_LONG __stdcall ZwSetValueKey(
    NT_HANDLE key, NT_UNICODE_STRING *value_name, NT351_U32 index,
    NT351_U32 value_type, void *data, NT351_U32 data_size);
__declspec(dllimport) NT351_LONG __stdcall ZwClose(NT_HANDLE key);

#define NT_STATUS_SUCCESS 0L
#define NT_KEY_SET_VALUE 0x0002UL
#define NT_OBJ_CASE_INSENSITIVE 0x00000040UL
#define NT_REG_DWORD 4UL

static uacpi_bool g_namespace_loaded = UACPI_FALSE;
static int g_power_ready = 0;
static int g_s5_prepared = 0;
static int g_reduced = 0;
static uacpi_u8 g_slp_a, g_slp_b;
static uacpi_mapped_gas *g_reset_reg = UACPI_NULL;
static uacpi_u8 g_reset_value = 0;
extern void __cdecl NtpwrTraceAcpiStatus(const char *, unsigned long);
extern void __cdecl NtpwrTraceAcpiEvent(const char *);
extern uacpi_status ntpwr_uacpi_prepare_smi_cmd(void);

int NtpwrAcpiReady(void) { return g_power_ready; }
int NtpwrAcpiResetReady(void) { return g_reset_reg != UACPI_NULL; }

static NT_UNICODE_STRING UnicodeValue(const NT_WCHAR *text, unsigned short chars)
{
    NT_UNICODE_STRING value;
    value.Length = (unsigned short)(chars * 2);
    value.MaximumLength = (unsigned short)((chars + 1) * 2);
    value.Buffer = (NT_WCHAR *)text;
    return value;
}

/* Service parameters are written only during DriverEntry at PASSIVE_LEVEL. */
static void SetDword(NT_HANDLE key, const NT_WCHAR *name,
                     unsigned short chars, NT351_U32 value)
{
    NT_UNICODE_STRING field = UnicodeValue(name, chars);
    NT351_LONG status = ZwSetValueKey(key, &field, 0, NT_REG_DWORD, &value, sizeof(value));
    if (status < 0)
        DbgPrint("NTPWR: ZwSetValueKey status=0x%08lX\n", (unsigned long)status);
}

static const NT_WCHAR s_stage[] = {'P','r','o','b','e','S','t','a','g','e',0};
static const NT_WCHAR s_init[] = {'I','n','i','t','S','t','a','t','u','s',0};
static const NT_WCHAR s_load[] = {'L','o','a','d','S','t','a','t','u','s',0};
static const NT_WCHAR s_namespace[] = {'N','a','m','e','s','p','a','c','e','S','t','a','t','u','s',0};
static const NT_WCHAR s_eval[] = {'E','v','a','l','S','t','a','t','u','s',0};
static const NT_WCHAR s_count[] = {'S','5','C','o','u','n','t',0};
static const NT_WCHAR s_a[] = {'S','5','T','y','p','e','A',0};
static const NT_WCHAR s_b[] = {'S','5','T','y','p','e','B',0};

void __stdcall NtpwrProbeCleanup(void)
{
    g_power_ready = 0;
    g_s5_prepared = 0;
    g_reduced = 0;
    if (g_reset_reg) {
        uacpi_unmap_gas(g_reset_reg);
        g_reset_reg = UACPI_NULL;
    }
    if (g_namespace_loaded) {
        uacpi_state_reset();
        g_namespace_loaded = UACPI_FALSE;
    }
    DbgPrint("NTPWR: unloaded\n");
}

static void ReportGas(const char *name, const struct acpi_gas *gas,
                      uacpi_u32 legacy, uacpi_u8 legacy_length)
{
    uacpi_u64 address = gas->address;
    uacpi_u32 space = gas->address_space_id;
    uacpi_u32 width = gas->register_bit_width;
    uacpi_u32 offset = gas->register_bit_offset;
    uacpi_u32 access = gas->access_size;
    if (!address && legacy) {
        address = legacy;
        space = UACPI_ADDRESS_SPACE_SYSTEM_IO;
        width = (uacpi_u32)legacy_length * 8U;
        offset = 0;
        access = 0;
    }
    DbgPrint("NTPWR: %s space=%lu address=0x%08lX%08lX width=%lu offset=%lu access=%lu\n",
             name, (unsigned long)space,
             (unsigned long)(address >> 32), (unsigned long)address,
             (unsigned long)width, (unsigned long)offset, (unsigned long)access);
}

static void ReportFadt(const struct acpi_fadt *fadt)
{
    DbgPrint("NTPWR: FADT revision=%lu length=%lu flags=0x%08lX hardware-reduced=%lu\n",
             (unsigned long)fadt->hdr.revision, (unsigned long)fadt->hdr.length,
             (unsigned long)fadt->flags,
             (unsigned long)((fadt->flags & ACPI_HW_REDUCED_ACPI) != 0));
    ReportGas("PM1a_CNT", &fadt->x_pm1a_cnt_blk,
              fadt->pm1a_cnt_blk, fadt->pm1_cnt_len);
    ReportGas("PM1b_CNT", &fadt->x_pm1b_cnt_blk,
              fadt->pm1b_cnt_blk, fadt->pm1_cnt_len);
    if (fadt->flags & ACPI_HW_REDUCED_ACPI) {
        ReportGas("SLP_CNT", &fadt->sleep_control_reg, 0, 0);
        ReportGas("SLP_STS", &fadt->sleep_status_reg, 0, 0);
    }
}

static int ValidGas(const struct acpi_gas *g, uacpi_u32 width)
{
    if (!g->address || g->register_bit_width < width || g->register_bit_offset)
        return 0;
    if (g->address_space_id == UACPI_ADDRESS_SPACE_SYSTEM_IO)
        return g->address <= 0x10000ULL - (width / 8U);
    return g->address_space_id == UACPI_ADDRESS_SPACE_SYSTEM_MEMORY;
}

static void SetupReset(const struct acpi_fadt *f)
{
    const struct acpi_gas *reg = &f->reset_reg;
    uacpi_status status;

    if (!(f->flags & ACPI_RESET_REG_SUP)) {
        NtpwrTraceAcpiEvent("reset unavailable: RESET_REG_SUP not set");
        return;
    }
    if (!reg->address) {
        NtpwrTraceAcpiEvent("reset unavailable: RESET_REG missing");
        return;
    }
    if (reg->register_bit_width != 8 || reg->register_bit_offset ||
        reg->access_size > 1 ||
        (reg->address_space_id != UACPI_ADDRESS_SPACE_SYSTEM_IO &&
         reg->address_space_id != UACPI_ADDRESS_SPACE_SYSTEM_MEMORY)) {
        NtpwrTraceAcpiEvent("reset unavailable: unsupported RESET_REG");
        return;
    }

    status = uacpi_map_gas(reg, &g_reset_reg);
    if (status != UACPI_STATUS_OK) {
        g_reset_reg = UACPI_NULL;
        NtpwrTraceAcpiStatus("reset mapping failed", (unsigned long)status);
        return;
    }
    g_reset_value = f->reset_value;
    NtpwrTraceAcpiEvent("reset ready");
    DbgPrint("NTPWR: ACPI reset mapped space=%lu address=0x%08lX%08lX value=0x%02lX\n",
             (unsigned long)reg->address_space_id,
             (unsigned long)(reg->address >> 32), (unsigned long)reg->address,
             (unsigned long)g_reset_value);
}

int NtpwrAcpiReset(void)
{
    uacpi_status status;
    if (!g_reset_reg) return 0;
    status = uacpi_gas_write_mapped(g_reset_reg, g_reset_value);
    if (status != UACPI_STATUS_OK) {
        NtpwrTraceAcpiStatus("reset failed", (unsigned long)status);
        return 0;
    }
    return 1;
}

static int SetupS5(const struct acpi_fadt *f)
{
    uacpi_u64 value;
    uacpi_status ret;
    g_reduced = (f->flags & ACPI_HW_REDUCED_ACPI) != 0;
    if (g_reduced) {
        if (!ValidGas(&f->sleep_control_reg, 8) ||
            !ValidGas(&f->sleep_status_reg, 8)) return 0;
        ret = uacpi_read_register(UACPI_REGISTER_SLP_CNT, &value);
    } else {
        if (!ValidGas(&f->x_pm1a_cnt_blk, 16) ||
            (f->x_pm1b_cnt_blk.address && !ValidGas(&f->x_pm1b_cnt_blk, 16)))
            return 0;
        ret = uacpi_read_register(UACPI_REGISTER_PM1_CNT, &value);
    }
    if (ret != UACPI_STATUS_OK) {
        DbgPrint("NTPWR: ACPI register mapping failed status=%lu\n",
                 (unsigned long)ret);
        return 0;
    }
    if (!g_reduced && f->smi_cmd && (f->acpi_enable || f->acpi_disable)) {
        ret = ntpwr_uacpi_prepare_smi_cmd();
        if (ret != UACPI_STATUS_OK) {
            NtpwrTraceAcpiStatus("SMI_CMD mapping failed", (unsigned long)ret);
            return 0;
        }
    }
    return 1;
}

NT351_LONG __stdcall NtpwrProbeInitialize(NT_DRIVER_OBJECT_PREFIX *driver,
                                 NT_UNICODE_STRING *registry_path)
{
    NT_OBJECT_ATTRIBUTES attrs;
    NT_HANDLE key = (NT_HANDLE)0;
    NT351_LONG status;
    uacpi_status init_status, load_status = UACPI_STATUS_UNIMPLEMENTED;
    uacpi_status namespace_status = UACPI_STATUS_UNIMPLEMENTED;
    uacpi_status eval_status = UACPI_STATUS_UNIMPLEMENTED;
    uacpi_object *s5 = UACPI_NULL;
    uacpi_object_array parts;
    uacpi_u64 a = 0, b = 0;
    uacpi_u32 count = 0;
    uacpi_u32 stage = 0;

    if (!driver || !registry_path) return (NT351_LONG)0xC000000DL;
    (void)driver;
    attrs.Length = sizeof(attrs);
    attrs.RootDirectory = (NT_HANDLE)0;
    attrs.ObjectName = registry_path;
    attrs.Attributes = NT_OBJ_CASE_INSENSITIVE;
    attrs.SecurityDescriptor = 0;
    attrs.SecurityQualityOfService = 0;
    status = ZwOpenKey(&key, NT_KEY_SET_VALUE, &attrs);
    if (status < 0) {
        DbgPrint("NTPWR: registry open failed status=0x%08lX\n", (unsigned long)status);
        return status;
    }

    SetDword(key, s_stage, 10, 0);
    SetDword(key, s_init, 10, 0xFFFFFFFFUL);
    SetDword(key, s_load, 10, 0xFFFFFFFFUL);
    SetDword(key, s_namespace, 15, 0xFFFFFFFFUL);
    SetDword(key, s_eval, 10, 0xFFFFFFFFUL);
    SetDword(key, s_count, 7, 0);
    SetDword(key, s_a, 7, 0xFFFFFFFFUL);
    SetDword(key, s_b, 7, 0xFFFFFFFFUL);

    init_status = uacpi_initialize(UACPI_FLAG_NO_ACPI_MODE);
    SetDword(key, s_init, 10, (uacpi_u32)init_status);
    if (init_status != UACPI_STATUS_OK) {
        stage = 1;
        goto done;
    }
    load_status = uacpi_namespace_load();
    SetDword(key, s_load, 10, (uacpi_u32)load_status);
    if (load_status != UACPI_STATUS_OK) {
        stage = 2;
        goto done;
    }
    g_namespace_loaded = UACPI_TRUE;
    namespace_status = uacpi_namespace_initialize();
    SetDword(key, s_namespace, 15, (uacpi_u32)namespace_status);
    if (namespace_status != UACPI_STATUS_OK) {
        stage = 3;
        goto done;
    }
    eval_status = uacpi_eval_simple_package(UACPI_NULL, "\\_S5", &s5);
    if (eval_status != UACPI_STATUS_OK) {
        stage = 3;
        goto done;
    }
    eval_status = uacpi_object_get_package(s5, &parts);
    if (eval_status != UACPI_STATUS_OK || parts.count < 2) {
        if (eval_status == UACPI_STATUS_OK) eval_status = UACPI_STATUS_TYPE_MISMATCH;
        stage = 3;
        goto done;
    }
    if (parts.count > 0xFFFFFFFFUL) {
        eval_status = UACPI_STATUS_INVALID_ARGUMENT;
        stage = 3;
        goto done;
    }
    count = (uacpi_u32)parts.count;
    eval_status = uacpi_object_get_integer(parts.objects[0], &a);
    if (eval_status == UACPI_STATUS_OK)
        eval_status = uacpi_object_get_integer(parts.objects[1], &b);
    if (eval_status != UACPI_STATUS_OK || a > 7 || b > 7) {
        if (eval_status == UACPI_STATUS_OK) eval_status = UACPI_STATUS_INVALID_ARGUMENT;
        stage = 3;
        goto done;
    }
    SetDword(key, s_count, 7, count);
    SetDword(key, s_a, 7, (uacpi_u32)a);
    SetDword(key, s_b, 7, (uacpi_u32)b);
    stage = 4;
    {
        struct acpi_fadt *fadt = UACPI_NULL;
        if (uacpi_table_fadt(&fadt) == UACPI_STATUS_OK && fadt) {
            ReportFadt(fadt);
            SetupReset(fadt);
            g_slp_a = (uacpi_u8)a;
            g_slp_b = (uacpi_u8)b;
            g_power_ready = SetupS5(fadt);
        }
    }
    DbgPrint("NTPWR: S5 package=%lu slp_typa=%lu slp_typb=%lu\n",
             (unsigned long)count, (unsigned long)a, (unsigned long)b);
done:
    if (s5) uacpi_object_unref(s5);
    SetDword(key, s_eval, 10, (uacpi_u32)eval_status);
    SetDword(key, s_stage, 10, stage);
    ZwClose(key);
    if (stage != 4)
        DbgPrint("NTPWR: probe stopped stage=%lu init=%lu load=%lu namespace=%lu eval=%lu\n",
                 (unsigned long)stage, (unsigned long)init_status,
                 (unsigned long)load_status, (unsigned long)namespace_status,
                 (unsigned long)eval_status);
    return NT_STATUS_SUCCESS;
}

unsigned long NtpwrAcpiPrepare(void)
{
    uacpi_status status;
    if (!g_power_ready) return UACPI_STATUS_INIT_LEVEL_MISMATCH;
    g_s5_prepared = 0;

    status = uacpi_prepare_for_sleep_state(UACPI_SLEEP_STATE_S5);
    if (status == UACPI_STATUS_OK) g_s5_prepared = 1;
    NtpwrTraceAcpiStatus("S5 preparation", (unsigned long)status);
    return (unsigned long)status;
}

int NtpwrAcpiPowerOff(void)
{
    uacpi_u64 current, a, b, sci;
    uacpi_status ret;
    if (!g_power_ready) return 0;
    if (!g_s5_prepared) {
        NtpwrTraceAcpiStatus("S5 not prepared", 0);
        return 0;
    }
    g_s5_prepared = 0;
    if (g_reduced) {
        NtpwrTraceAcpiEvent("S5 requested");
        ret = uacpi_write_register(UACPI_REGISTER_SLP_CNT,
                                   ((uacpi_u64)g_slp_a << 2) | 0x20U);
    } else {
        ret = uacpi_read_register_field(UACPI_REGISTER_FIELD_SCI_EN, &sci);
        if (ret != UACPI_STATUS_OK) {
            NtpwrTraceAcpiStatus("mode read failed", (unsigned long)ret);
            return 0;
        }
        if (!sci) {
            ret = uacpi_enter_acpi_mode();
            if (ret != UACPI_STATUS_OK) {
                NtpwrTraceAcpiStatus("mode transition failed", (unsigned long)ret);
                return 0;
            }
            ret = uacpi_read_register_field(UACPI_REGISTER_FIELD_SCI_EN, &sci);
            if (ret != UACPI_STATUS_OK || !sci) {
                NtpwrTraceAcpiStatus("mode verification failed",
                                     (unsigned long)ret);
                return 0;
            }
            NtpwrTraceAcpiStatus("mode enabled", 0);
        }
        ret = uacpi_read_register(UACPI_REGISTER_PM1_CNT, &current);
        if (ret != UACPI_STATUS_OK) {
            NtpwrTraceAcpiStatus("PM1 read failed", (unsigned long)ret);
            return 0;
        }
        current &= ~0x3C00ULL;
        a = current | ((uacpi_u64)g_slp_a << 10);
        b = current | ((uacpi_u64)g_slp_b << 10);
        ret = uacpi_write_registers(UACPI_REGISTER_PM1_CNT, a, b);
        if (ret == UACPI_STATUS_OK) {
            NtpwrTraceAcpiEvent("S5 requested");
            ret = uacpi_write_registers(UACPI_REGISTER_PM1_CNT,
                                        a | 0x2000U, b | 0x2000U);
        }
    }
    if (ret != UACPI_STATUS_OK) {
        NtpwrTraceAcpiStatus("S5 write failed", (unsigned long)ret);
        return 0;
    }
    return 1;
}
