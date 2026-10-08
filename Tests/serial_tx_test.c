#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

typedef int BaseType_t;
typedef uint32_t TickType_t;
typedef HANDLE SemaphoreHandle_t;
typedef struct {
    char wire[1024];
    unsigned count, pending;
    int fail_txe, fail_tc;
} USART_TypeDef;
static USART_TypeDef debug_uart, cpu_uart;
#define DEBUG_USARTx (&debug_uart)
#define HEALTH_USARTx (&cpu_uart)
#define USART_FLAG_TXE 0x80
#define USART_FLAG_TC 0x40
#define RESET 0
#define pdFALSE 0
#define pdTRUE 1
#define portMAX_DELAY UINT32_MAX
#define taskSCHEDULER_NOT_STARTED 1
#define taskSCHEDULER_RUNNING 2
#define taskSCHEDULER_SUSPENDED 0

static int in_isr, scheduler = taskSCHEDULER_RUNNING;
static uint32_t primask, basepri;
static volatile LONG takes, gives;
static TickType_t last_wait;
static DWORD main_thread;
static HANDLE start_writer, writer_attempt;
static int concurrent_test, writer_kind, pause_sent;
static const char ack[] = "\r\nClearRuntime:OK\r\n";
static BaseType_t xPortIsInsideInterrupt(void) { return in_isr; }
static BaseType_t xTaskGetSchedulerState(void) { return scheduler; }
static uint32_t __get_PRIMASK(void) { return primask; }
static uint32_t __get_BASEPRI(void) { return basepri; }
static BaseType_t xSemaphoreTake(SemaphoreHandle_t sem, TickType_t wait)
{
    assert(!in_isr);
    if (scheduler != taskSCHEDULER_RUNNING || primask || basepri)
        assert(wait == 0);
    InterlockedIncrement(&takes);
    last_wait = wait;
    if (concurrent_test && GetCurrentThreadId() != main_thread)
        assert(SetEvent(writer_attempt));
    /* A test deadlock must fail within 5s instead of hanging the runner. */
    return WaitForSingleObject(sem, wait ? 5000 : 0) == WAIT_OBJECT_0;
}
static void xSemaphoreGive(SemaphoreHandle_t sem)
{
    InterlockedIncrement(&gives);
    assert(ReleaseSemaphore(sem, 1, NULL));
}
static int USART_GetFlagStatus(USART_TypeDef *uart, uint16_t flag)
{
    if (flag == USART_FLAG_TXE)
    {
        if (uart->fail_txe) return RESET;
        if (uart->pending) { uart->pending = 0; return RESET; }
    }
    else
    {
        assert(flag == USART_FLAG_TC);
        if (uart->fail_tc) return RESET;
        uart->pending = 0;
    }
    return 1;
}
static void USART_SendData(USART_TypeDef *uart, uint8_t ch)
{
    assert(!in_isr && !uart->pending);
    if (concurrent_test && uart == DEBUG_USARTx && GetCurrentThreadId() != main_thread)
        assert(uart->count >= strlen(ack)); /* The entire ACK precedes the GPIO sender. */
    assert(uart->count + 1 < sizeof(uart->wire));
    uart->wire[uart->count++] = (char)ch;
    uart->wire[uart->count] = 0;
    uart->pending = 1;
    if (concurrent_test && uart == DEBUG_USARTx &&
        GetCurrentThreadId() == main_thread && !pause_sent && uart->count == 5)
    {
        pause_sent = 1;
        assert(SetEvent(start_writer));
        assert(WaitForSingleObject(writer_attempt, 5000) == WAIT_OBJECT_0);
        /* Other thread has sent CPU output and is now trying the DEBUG mutex. */
        assert(strcmp(cpu_uart.wire, "CPU") == 0);
        assert(uart->count == 5);
    }
}
#define fputc DebugTest_PutChar
#include "serial_tx_impl.inc"
#undef fputc

static void reset_wire(void)
{
    memset(&debug_uart, 0, sizeof(debug_uart));
    memset(&cpu_uart, 0, sizeof(cpu_uart));
}
static DWORD WINAPI competing_writer(LPVOID unused)
{
    uint8_t bytes[] = {'G', 'P'};
    (void)unused;
    assert(WaitForSingleObject(start_writer, 5000) == WAIT_OBJECT_0);
    assert(Usart_SendString(HEALTH_USARTx, "CPU"));
    switch (writer_kind)
    {
    case 0: assert(DebugTest_PutChar('G', NULL) == 'G'); break;
    case 1: Usart_SendByte(DEBUG_USARTx, 'G'); break;
    case 2: Usart_SendHalfWord(DEBUG_USARTx, 0x4750); break;
    case 3: Usart_SendArray(DEBUG_USARTx, bytes, 2); break;
    default: assert(Usart_SendString(DEBUG_USARTx, "GPIO")); break;
    }
    return 0;
}
static void test_concurrent_senders(void)
{
    for (writer_kind = 0; writer_kind < 5; ++writer_kind)
    {
        HANDLE thread;
        char expected[64];
        reset_wire();
        pause_sent = 0;
        assert(ResetEvent(start_writer) && ResetEvent(writer_attempt));
        concurrent_test = 1;
        thread = CreateThread(NULL, 0, competing_writer, NULL, 0, NULL);
        assert(thread);
        assert(Usart_SendString(DEBUG_USARTx, (char *)ack));
        assert(WaitForSingleObject(thread, 5000) == WAIT_OBJECT_0);
        assert(CloseHandle(thread));
        concurrent_test = 0;
        strcpy(expected, ack);
        strcat(expected, writer_kind < 2 ? "G" : writer_kind < 4 ? "GP" : "GPIO");
        assert(strcmp(debug_uart.wire, expected) == 0);
    }
}
static void test_timeout_and_recovery(void)
{
    LONG before;
    reset_wire();
    before = gives;
    debug_uart.fail_txe = 1;
    assert(!Usart_SendString(DEBUG_USARTx, "ACK"));
    assert(debug_uart.count == 0 && gives == before + 1);
    debug_uart.fail_txe = 0;
    debug_uart.fail_tc = 1;
    assert(!Usart_SendString(DEBUG_USARTx, "ACK"));
    assert(strcmp(debug_uart.wire, "ACK") == 0 && gives == before + 2);
    reset_wire();
    assert(Usart_SendString(DEBUG_USARTx, "RECOVERED"));
    assert(strcmp(debug_uart.wire, "RECOVERED") == 0);
    cpu_uart.fail_txe = 1;
    assert(!Usart_SendString(HEALTH_USARTx, "CPU"));
    cpu_uart.fail_txe = 0;
    assert(Usart_SendString(HEALTH_USARTx, "CPU"));
}
static void test_context_guards(void)
{
    LONG before;
    SemaphoreHandle_t saved;
    reset_wire();
    before = takes;
    in_isr = 1;
    assert(!Usart_SendString(DEBUG_USARTx, "IRQ"));
    assert(!Usart_SendString(HEALTH_USARTx, "IRQ"));
    assert(DebugTest_PutChar('I', NULL) == EOF);
    assert(takes == before && debug_uart.count == 0 && cpu_uart.count == 0);
    in_isr = 0;
    scheduler = taskSCHEDULER_NOT_STARTED;
    assert(Usart_SendString(DEBUG_USARTx, "BOOT") && takes == before);
    scheduler = taskSCHEDULER_RUNNING;
    saved = s_debug_tx_mutex;
    s_debug_tx_mutex = NULL;
    assert(!Usart_SendString(DEBUG_USARTx, "NO MUTEX"));
    s_debug_tx_mutex = saved;
    assert(xSemaphoreTake(saved, 0));
    basepri = 0x50;
    assert(DebugTest_PutChar('X', NULL) == EOF && last_wait == 0);
    basepri = 0;
    primask = 1;
    assert(!Usart_SendString(DEBUG_USARTx, "X") && last_wait == 0);
    primask = 0;
    scheduler = taskSCHEDULER_SUSPENDED;
    assert(!Usart_SendString(DEBUG_USARTx, "X") && last_wait == 0);
    scheduler = taskSCHEDULER_RUNNING;
    xSemaphoreGive(saved);
    basepri = 0x50; /* Uncontended startup critical-section logging still works. */
    assert(DebugTest_PutChar('!', NULL) == '!' && last_wait == 0);
    basepri = 0;
    assert(strcmp(debug_uart.wire, "BOOT!") == 0);
}
static void test_buffer_boundaries_and_txe_order(void)
{
    unsigned i;
    uint8_t bytes[300];
    reset_wire();
    assert(Usart_SendString(DEBUG_USARTx, "") && debug_uart.count == 0);
    for (i = 0; i < sizeof(bytes); ++i) bytes[i] = (uint8_t)i;
    Usart_SendArray(DEBUG_USARTx, bytes, sizeof(bytes));
    assert(debug_uart.count == sizeof(bytes));
    assert(memcmp(debug_uart.wire, bytes, sizeof(bytes)) == 0);
    reset_wire();
    assert(DebugTest_PutChar('A', NULL) == 'A');
    assert(debug_uart.pending);
    Usart_SendByte(DEBUG_USARTx, 'B');
    assert(Usart_SendString(DEBUG_USARTx, "C"));
    assert(strcmp(debug_uart.wire, "ABC") == 0);
}
int main(void)
{
    main_thread = GetCurrentThreadId();
    s_debug_tx_mutex = CreateSemaphore(NULL, 1, 1, NULL);
    start_writer = CreateEvent(NULL, TRUE, FALSE, NULL);
    writer_attempt = CreateEvent(NULL, TRUE, FALSE, NULL);
    assert(s_debug_tx_mutex && start_writer && writer_attempt);
    test_concurrent_senders();
    test_timeout_and_recovery();
    test_context_guards();
    test_buffer_boundaries_and_txe_order();
    assert(CloseHandle(s_debug_tx_mutex));
    assert(CloseHandle(start_writer));
    assert(CloseHandle(writer_attempt));
    puts("PASS: production UART TX, 5 concurrent DEBUG senders, CPU independence, TXE/TC timeout recovery, ISR/critical guards, 300-byte array");
    return 0;
}
