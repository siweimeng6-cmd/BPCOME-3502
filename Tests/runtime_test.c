/* Host fault-injection tests. runtime_impl.inc is extracted from production
 * bsp_eeprom.c by run_runtime_tests.ps1; no copy of the algorithm is maintained.
 */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef uint32_t TickType_t;
#include "runtime_config.h"
static int critical_depth;
#define taskENTER_CRITICAL() (++critical_depth)
#define taskEXIT_CRITICAL() (--critical_depth)

static uint8_t eeprom[65536];
static TickType_t fake_tick;
static int online, fail_read_address, write_result, corrupt_readback;
static unsigned write_limit, busy_polls, busy_remaining, write_calls;
static uint16_t last_write_address;
static unsigned fail_write_call, erased_pages;
extern uint8_t g_runtime_history_valid;

static TickType_t xTaskGetTickCount(void) { return fake_tick; }
static uint8_t ee_CheckOk(void)
{
    if (!online) return 0;
    if (busy_remaining) { --busy_remaining; return 0; }
    return 1;
}
static uint8_t ee_ReadBytes(uint8_t *out, uint16_t address, uint16_t size)
{
    if (!online || busy_remaining || address == fail_read_address) return 0;
    assert((unsigned)address + size <= sizeof(eeprom));
    memcpy(out, eeprom + address, size);
    if (corrupt_readback && write_calls && address == last_write_address)
        out[0] ^= 1;
    return 1;
}
static uint8_t ee_WriteBytes(uint8_t *in, uint16_t address, uint16_t size)
{
    unsigned count = size < write_limit ? size : write_limit;
    ++write_calls;
    last_write_address = address;
    assert(size == EE_PAGE_SIZE || address == RUNTIME_EE_SLOT_A_ADDR || address == RUNTIME_EE_SLOT_B_ADDR);
    assert(address / 128 == (address + size - 1) / 128);
    assert(critical_depth == 0); /* No EEPROM operation with scheduler IRQs masked. */
    if (fail_write_call && write_calls == fail_write_call) return 0;
    if (!online) return 0;
    if (size == EE_PAGE_SIZE)
    {
        assert(address == erased_pages * EE_PAGE_SIZE);
        assert(!g_runtime_history_valid);
        ++erased_pages;
    }
    memcpy(eeprom + address, in, count);
    busy_remaining = busy_polls;
    return (uint8_t)write_result;
}

#define printf(...) ((void)0)
#include "runtime_impl.inc"
#undef printf

static void reset_fixture(void)
{
    memset(eeprom, 0xFF, sizeof(eeprom));
    fake_tick = 0;
    online = 1;
    fail_read_address = -1;
    write_result = 1;
    corrupt_readback = 0;
    write_limit = 65536;
    busy_polls = busy_remaining = write_calls = 0;
    fail_write_call = erased_pages = 0;
    last_write_address = 0;
}
static void advance(uint32_t ticks)
{
    fake_tick += ticks;
    Runtime_Task_Update();
}
static void put_record(uint8_t slot, uint32_t seq, uint32_t minutes, uint32_t seconds)
{
    uint32_t record[RUNTIME_RECORD_WORDS];
    record[0] = RUNTIME_RECORD_MAGIC;
    record[1] = seq;
    record[2] = minutes;
    record[3] = seconds;
    record[4] = Runtime_RecordCRC(record);
    memcpy(eeprom + Runtime_SlotAddress(slot), record, sizeof(record));
}
static void boot_pair(uint32_t minutes, uint32_t seconds)
{
    reset_fixture();
    put_record(0, 10, minutes, seconds);
    put_record(1, 9, minutes, seconds);
    Runtime_Init();
    assert(g_runtime_history_valid);
}
static void reboot(void)
{
    fake_tick = 0;
    busy_remaining = 0;
    corrupt_readback = 0;
    fail_read_address = -1;
    Runtime_Init();
}

static void test_actual_ticks_and_fraction(void)
{
    unsigned i;
    boot_pair(0, 0);
    Runtime_Task_Update();
    assert(s_runtime_session_seconds == 0 && write_calls == 0);
    for (i = 0; i < 1000; ++i) advance(2100);
    assert(g_runtime_total_minutes == 35); /* Old implementation counted 33 min. */
    assert(s_runtime_session_seconds == 2100);
    boot_pair(0, 0);
    for (i = 0; i < 60000; ++i) advance(1);
    assert(g_runtime_total_minutes == 1 && s_runtime_fraction_ticks == 0);
    advance(999);
    assert(s_runtime_session_seconds == 60 && s_runtime_fraction_ticks == 999);
    advance(1);
    assert(s_runtime_session_seconds == 61 && s_runtime_fraction_ticks == 0);
}
static void test_wrap_and_long_gap(void)
{
    boot_pair(0, 0);
    fake_tick = UINT32_MAX - 499;
    Runtime_Init();
    advance(1500);
    assert(s_runtime_session_seconds == 1 && s_runtime_fraction_ticks == 500);
    advance(58500);
    assert(g_runtime_total_minutes == 1);
    boot_pair(0, 0);
    advance(999);
    advance(UINT32_MAX); /* Addition with the fraction must not overflow 32 bits. */
    assert(s_runtime_session_seconds == 4294968 && s_runtime_fraction_ticks == 294);
    boot_pair(0, 0);
    advance(63UL * 3600 * 1000);
    assert(g_runtime_total_minutes == 63 * 60);
    advance(10UL * 3600 * 1000); /* No calls during the entire 10-hour gap. */
    assert(g_runtime_total_minutes == 73 * 60);
    advance(27UL * 3600 * 1000);
    assert(g_runtime_total_minutes == 100 * 60);
}
static void test_100h_and_repeated_wrap(void)
{
    unsigned i;
    boot_pair(0, 0);
    for (i = 1; i <= 180000; ++i)
    {
        advance(2000);
        assert(g_runtime_total_minutes == i / 30);
    }
    assert(g_runtime_total_minutes == 6000 && write_calls == 200);
    reboot();
    assert(g_runtime_total_minutes == 6000);
    boot_pair(0, 0);
    for (i = 0; i < 2400; ++i) advance(3600000); /* 100 days; two tick wraps. */
    assert(g_runtime_total_minutes == 144000);
}
static void test_legacy_and_blank_migration(void)
{
    uint32_t legacy = 10000;
    reset_fixture();
    memcpy(eeprom + RUNTIME_EE_ADDR, &legacy, sizeof(legacy));
    Runtime_Init();
    assert(g_runtime_total_minutes == legacy);
    advance(0);
    advance(2000);
    assert(write_calls == 2 && s_runtime_valid_mask == 3);
    assert(memcmp(eeprom + RUNTIME_EE_ADDR, &legacy, sizeof(legacy)) == 0);
    reboot();
    assert(g_runtime_total_minutes == legacy && s_runtime_base_seconds == 600002);
    reset_fixture();
    Runtime_Init();
    advance(0); advance(0);
    assert(g_runtime_history_valid && write_calls == 2 && g_runtime_total_minutes == 0);
}
static void test_startup_read_protection(void)
{
    uint8_t before[sizeof(eeprom)];
    boot_pair(10000, 0);
    memcpy(before, eeprom, sizeof(before));
    fail_read_address = RUNTIME_EE_SLOT_B_ADDR;
    Runtime_Init();
    assert(!g_runtime_history_valid);
    advance(3600000);
    assert(!g_runtime_history_valid && write_calls == 0);
    assert(memcmp(before, eeprom, sizeof(before)) == 0);
    fail_read_address = -1;
    advance(5000);
    assert(g_runtime_history_valid && g_runtime_total_minutes == 10060);
    assert(s_runtime_session_seconds == 3605); /* Keep time spent waiting for history. */
    reboot();
    assert(g_runtime_total_minutes == 10060);
    reset_fixture();
    fail_read_address = RUNTIME_EE_ADDR;
    Runtime_Init(); advance(3600000);
    assert(!g_runtime_history_valid && write_calls == 0);
}
static void test_write_failure_retry(void)
{
    boot_pair(100, 0);
    write_result = 0; write_limit = 0;
    advance(1800000);
    assert(write_calls == 1 && s_runtime_saved_seconds == 0);
    assert(g_runtime_countdown_minutes == 0);
    advance(4999);
    assert(write_calls == 1);
    write_result = 1; write_limit = 65536;
    advance(1);
    assert(write_calls == 2 && s_runtime_saved_seconds == 1805);
    assert(g_runtime_countdown_minutes == 20); /* Total=130m05s; next aligned save=150m. */
    reboot();
    assert(g_runtime_total_minutes == 130 && s_runtime_base_seconds % 60 == 5);
}
static void test_write_confirmation(void)
{
    boot_pair(10, 0);
    corrupt_readback = 1;
    advance(1800000);
    assert(s_runtime_saved_seconds == 0 && s_runtime_active_slot == 0);
    corrupt_readback = 0;
    advance(5000);
    assert(s_runtime_saved_seconds == 1805);
    boot_pair(10, 0);
    busy_polls = 3;
    advance(1800000);
    assert(s_runtime_saved_seconds == 1800 && busy_remaining == 0);
    boot_pair(10, 0);
    busy_polls = RUNTIME_READY_POLLS + 1;
    advance(1800000);
    assert(s_runtime_saved_seconds == 0);
    busy_polls = busy_remaining = 0;
    advance(5000);
    assert(s_runtime_saved_seconds == 1805);
    boot_pair(10, 0);
    fail_read_address = RUNTIME_EE_SLOT_B_ADDR;
    advance(1800000);
    assert(s_runtime_saved_seconds == 0);
}
static void test_power_cut_every_byte(void)
{
    unsigned cut, result;
    uint8_t old_active[RUNTIME_RECORD_WORDS * sizeof(uint32_t)];
    for (result = 0; result <= 1; ++result)
        for (cut = 0; cut <= sizeof(old_active); ++cut)
        {
            boot_pair(1000, 7);
            memcpy(old_active, eeprom + RUNTIME_EE_SLOT_A_ADDR, sizeof(old_active));
            write_limit = cut; write_result = (int)result;
            advance(1800000);
            assert(memcmp(old_active, eeprom + RUNTIME_EE_SLOT_A_ADDR, sizeof(old_active)) == 0);
            reboot();
            assert(g_runtime_history_valid);
            /* A torn write may already equal the full record if trailing bytes
             * were unchanged. Either the old or the complete new value is safe. */
            assert(s_runtime_base_seconds == 60007 || s_runtime_base_seconds == 61807);
        }
}
static void test_crc_and_single_slot_repair(void)
{
    unsigned byte, bit;
    uint8_t before[sizeof(eeprom)];
    for (byte = 0; byte < RUNTIME_RECORD_WORDS * sizeof(uint32_t); ++byte)
        for (bit = 0; bit < 8; ++bit)
        {
            boot_pair(1000, 0);
            eeprom[RUNTIME_EE_SLOT_A_ADDR + byte] ^= (uint8_t)(1U << bit);
            reboot();
            assert(g_runtime_history_valid && s_runtime_active_slot == 1);
            advance(0);
            assert(s_runtime_valid_mask == 3 && last_write_address == RUNTIME_EE_SLOT_A_ADDR);
        }
    boot_pair(1000, 0);
    eeprom[RUNTIME_EE_SLOT_A_ADDR] ^= 1;
    eeprom[RUNTIME_EE_SLOT_B_ADDR] ^= 1;
    memcpy(before, eeprom, sizeof(before));
    reboot(); advance(1800000);
    assert(!g_runtime_history_valid && write_calls == 0);
    assert(memcmp(before, eeprom, sizeof(before)) == 0);
    reset_fixture();
    put_record(0, 1, 10, 60); /* CRC correct, invalid seconds field. */
    Runtime_Init(); advance(1800000);
    assert(!g_runtime_history_valid && write_calls == 0);
}
static void test_sequence_wrap_and_ambiguity(void)
{
    reset_fixture();
    put_record(0, UINT32_MAX, 1000, 0);
    put_record(1, UINT32_MAX - 1, 970, 0);
    Runtime_Init(); advance(1800000);
    assert(s_runtime_sequence == 0);
    reboot();
    assert(g_runtime_total_minutes == 1030 && s_runtime_active_slot == 1);
    advance(1800000); reboot();
    assert(g_runtime_total_minutes == 1060 && s_runtime_active_slot == 0);
    reset_fixture();
    put_record(0, 1, 10, 0); put_record(1, 1, 11, 0);
    Runtime_Init(); advance(1800000);
    assert(!g_runtime_history_valid && write_calls == 0);
}
static void test_torn_first_migration(void)
{
    uint32_t legacy = 12345;
    reset_fixture();
    memcpy(eeprom + RUNTIME_EE_ADDR, &legacy, sizeof(legacy));
    Runtime_Init();
    write_limit = 5; write_result = 0;
    advance(0); reboot();
    assert(!g_runtime_history_valid); /* Conservative lock; legacy remains recoverable. */
    assert(memcmp(eeprom + RUNTIME_EE_ADDR, &legacy, sizeof(legacy)) == 0);
}

static void test_countdown_alignment(void)
{
    unsigned i;
    uint32_t minutes, countdown;
    uint8_t valid;
    boot_pair(63 * 60, 7);
    assert(g_runtime_countdown_minutes == 30);
    advance(53000);
    assert(g_runtime_total_minutes == 63 * 60 + 1);
    assert(g_runtime_countdown_minutes == 29);
    for (i = 0; i < 10000; ++i)
    {
        advance(2100); /* Save requests regularly arrive after the exact deadline. */
        Runtime_GetSnapshot(&minutes, &countdown, &valid);
        assert(valid && critical_depth == 0);
        assert(countdown > 0 && countdown <= 30);
        assert((minutes + countdown) % 30 == 0);
        assert(s_runtime_save_deadline % 1800 == 0);
    }
    reboot();
    Runtime_GetSnapshot(&minutes, &countdown, &valid);
    assert(valid && (minutes + countdown) % 30 == 0);
    /* A failed save leaves the original deadline overdue; retry does not move
     * the subsequent boundary by the 5-second delay. */
    boot_pair(0, 0);
    write_result = 0; write_limit = 0;
    advance(1800000);
    assert(s_runtime_save_deadline == 1800 && g_runtime_countdown_minutes == 0);
    write_result = 1; write_limit = 65536;
    advance(5000);
    assert(s_runtime_save_deadline == 3600 && g_runtime_countdown_minutes == 30);
    advance(55000);
    assert(g_runtime_total_minutes == 31 && g_runtime_countdown_minutes == 29);
}

static void assert_erased_and_zero(void)
{
    unsigned address;
    uint32_t record[RUNTIME_RECORD_WORDS];
    assert(g_runtime_history_valid && g_runtime_total_minutes == 0);
    assert(g_runtime_countdown_minutes == 30 && s_runtime_fraction_ticks == 0);
    for (address = 0; address < sizeof(eeprom); ++address)
    {
        if ((address >= RUNTIME_EE_SLOT_A_ADDR && address < RUNTIME_EE_SLOT_A_ADDR + sizeof(record)) ||
            (address >= RUNTIME_EE_SLOT_B_ADDR && address < RUNTIME_EE_SLOT_B_ADDR + sizeof(record)))
            continue;
        assert(eeprom[address] == 0xFF);
    }
    for (address = 0; address < 2; ++address)
    {
        memcpy(record, eeprom + Runtime_SlotAddress((uint8_t)address), sizeof(record));
        assert(Runtime_RecordValid(record) && record[2] == 0 && record[3] == 0);
    }
}
static void test_clear_entire_eeprom(void)
{
    reset_fixture();
    memset(eeprom, 0xA5, sizeof(eeprom));
    put_record(0, 10, 12345, 17);
    put_record(1, 9, 12315, 17);
    Runtime_Init();
    advance(123456);
    assert(Runtime_ClearAll());
    assert(erased_pages == 512 && write_calls == 514);
    assert_erased_and_zero();
    advance(59999);
    assert(g_runtime_total_minutes == 0 && s_runtime_fraction_ticks == 999);
    advance(1);
    assert(g_runtime_total_minutes == 1);
    reboot();
    assert(g_runtime_total_minutes == 0); /* The erased old history must not return. */
    advance(60000);
    assert(g_runtime_total_minutes == 1);
}
static void test_clear_failures_and_retry(void)
{
    const unsigned failures[] = {1, 2, 3, 4, 257, 512, 513, 514};
    unsigned i, previous_writes;
    for (i = 0; i < sizeof(failures) / sizeof(failures[0]); ++i)
    {
        boot_pair(12345, 17);
        fail_write_call = failures[i];
        assert(!Runtime_ClearAll());
        assert(!g_runtime_history_valid);
        previous_writes = write_calls;
        advance(3600000);
        assert(write_calls == previous_writes && !g_runtime_history_valid);
        fail_write_call = 0;
        erased_pages = 0;
        assert(Runtime_ClearAll());
        assert_erased_and_zero();
        reboot();
        assert(g_runtime_total_minutes == 0 && g_runtime_history_valid);
    }
    boot_pair(12345, 17);
    corrupt_readback = 1;
    assert(!Runtime_ClearAll());
    assert(write_calls == 1 && !g_runtime_history_valid);
    advance(3600000);
    assert(write_calls == 1);
}

int main(void)
{
    test_actual_ticks_and_fraction();
    test_wrap_and_long_gap();
    test_100h_and_repeated_wrap();
    test_legacy_and_blank_migration();
    test_startup_read_protection();
    test_write_failure_retry();
    test_write_confirmation();
    test_power_cut_every_byte();
    test_crc_and_single_slot_repair();
    test_sequence_wrap_and_ambiguity();
    test_torn_first_migration();
    test_countdown_alignment();
    test_clear_entire_eeprom();
    test_clear_failures_and_retry();
    puts("PASS: 14 runtime suites (timing, persistence, 64KB erase/reset, erase failures/retry)");
    return 0;
}
