#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef int BaseType_t;
typedef struct { uint16_t SR, DR; } USART_TypeDef;
static USART_TypeDef debug_uart, cpu_uart;
#define DEBUG_USARTx (&debug_uart)
#define HEALTH_USARTx (&cpu_uart)
#define USART_FLAG_RXNE 0x20
#define USART_FLAG_ORE 0x08
#define USART_FLAG_NE 0x04
#define USART_FLAG_FE 0x02
#define USART_FLAG_PE 0x01
#define pdFALSE 0
#define portYIELD_FROM_ISR(woken) ((void)(woken))
static int critical_depth, in_isr;
#define taskENTER_CRITICAL() (++critical_depth)
#define taskEXIT_CRITICAL() (--critical_depth)
static void *xSelfResetSemaphore;
static unsigned resets, clears, replies[2];
static uint8_t clear_result, send_result;
static char last_reply[2][64];
static void (*during_clear)(void);
static void (*during_reply)(unsigned port);
static uint16_t USART_ReceiveData(USART_TypeDef *uart)
{ uart->SR = 0; return uart->DR; }
static void xSemaphoreGiveFromISR(void *sem, BaseType_t *woken)
{ assert(sem && in_isr); ++resets; *woken = 1; }
static uint8_t Runtime_ClearAll(void)
{
    assert(!in_isr && critical_depth == 0);
    ++clears;
    if (during_clear) during_clear();
    return clear_result;
}
static uint8_t Usart_SendString(USART_TypeDef *uart, char *text)
{
    unsigned port = uart == DEBUG_USARTx ? 0 : 1;
    assert(!in_isr && critical_depth == 0);
    assert(strlen(text) < sizeof(last_reply[0]));
    strcpy(last_reply[port], text);
    ++replies[port];
    if (during_reply) during_reply(port);
    return send_result;
}
#include "serial_config.h"
#include "serial_impl.inc"

static void byte(uint8_t port, uint8_t ch, uint16_t errors)
{
    USART_TypeDef *uart = port == SERIAL_COMMAND_DEBUG ? DEBUG_USARTx : HEALTH_USARTx;
    uart->SR = USART_FLAG_RXNE | errors;
    uart->DR = ch;
    in_isr = 1;
    Serial_CommandIRQ(uart, port);
    in_isr = 0;
}
static void feed(uint8_t port, const char *text)
{
    while (*text) byte(port, (uint8_t)*text++, 0);
}
static void reset_fixture(void)
{
    memset(s_command_lines, 0, sizeof(s_command_lines));
    s_command_result[0] = s_command_result[1] = COMMAND_IDLE;
    s_clear_request_mask = 0;
    resets = clears = replies[0] = replies[1] = 0;
    memset(last_reply, 0, sizeof(last_reply));
    clear_result = send_result = 1;
    during_clear = NULL;
    during_reply = NULL;
    xSelfResetSemaphore = &resets;
    assert(!critical_depth && !in_isr);
}
static void test_both_ports_and_reset(void)
{
    unsigned port;
    for (port = 0; port < 2; ++port)
    {
        reset_fixture();
        feed((uint8_t)port, "ClearRuntime\r\n");
        assert(clears == 0 && s_clear_request_mask == (1U << port));
        Serial_ProcessRuntimeCommands();
        assert(clears == 1);
        Serial_SendCommandReply((uint8_t)port);
        assert(replies[port] == 1 && replies[1 - port] == 0);
        assert(strcmp(last_reply[port], "\r\nClearRuntime:OK\r\n") == 0);
        Serial_ProcessRuntimeCommands();
        Serial_SendCommandReply((uint8_t)port);
        assert(clears == 1 && replies[port] == 1);
    }
    feed(SERIAL_COMMAND_DEBUG, "Reset\r\n");
    assert(resets == 1);
    feed(SERIAL_COMMAND_CPU, "Reset\n");
    assert(resets == 1);
    xSelfResetSemaphore = NULL;
    feed(SERIAL_COMMAND_DEBUG, "Reset\n");
    assert(resets == 1);
}
static void test_partial_and_simultaneous(void)
{
    reset_fixture();
    feed(0, "Clear"); feed(1, "ClearRun");
    Serial_ProcessRuntimeCommands(); assert(clears == 0);
    feed(0, "Runtime\n"); feed(1, "time\r");
    feed(0, "ClearRuntime\nClearRuntime\n");
    Serial_ProcessRuntimeCommands(); assert(clears == 1);
    feed(0, "ClearRuntime\n"); /* Result outstanding: do not repeat erase. */
    Serial_ProcessRuntimeCommands(); assert(clears == 1);
    Serial_SendCommandReply(0); Serial_SendCommandReply(1);
    assert(replies[0] == 1 && replies[1] == 1);
    feed(0, "ClearRuntime\n");
    Serial_ProcessRuntimeCommands(); assert(clears == 2);
}
static void join_from_cpu(void) { feed(1, "ClearRuntime\n"); }
static void test_failure_and_request_during_erase(void)
{
    reset_fixture();
    clear_result = 0;
    during_clear = join_from_cpu;
    feed(0, "ClearRuntime\n");
    Serial_ProcessRuntimeCommands();
    Serial_SendCommandReply(0); Serial_SendCommandReply(1);
    assert(clears == 1 && replies[0] == 1 && replies[1] == 1);
    assert(strcmp(last_reply[0], "\r\nClearRuntime:ERROR EEPROM\r\n") == 0);
    assert(strcmp(last_reply[1], last_reply[0]) == 0);
    clear_result = send_result = 1; during_clear = NULL;
    feed(1, "ClearRuntime\n"); Serial_ProcessRuntimeCommands(); Serial_SendCommandReply(1);
    assert(clears == 2 && strcmp(last_reply[1], "\r\nClearRuntime:OK\r\n") == 0);
}
static void test_invalid_lines_and_rx_errors(void)
{
    unsigned port;
    for (port = 0; port < 2; ++port)
    {
        reset_fixture();
        feed((uint8_t)port, "\r\nclearruntime\n ClearRuntime\nClearRuntimeX\n");
        feed((uint8_t)port, "01234567890123456789ClearRuntime\n");
        feed((uint8_t)port, "ClearRuntime"); byte((uint8_t)port, 0, 0); feed((uint8_t)port, "\n");
        feed((uint8_t)port, "Clear"); byte((uint8_t)port, 'R', USART_FLAG_ORE);
        feed((uint8_t)port, "untime\n");
        Serial_ProcessRuntimeCommands(); assert(clears == 0);
        feed((uint8_t)port, "ClearRuntime\n");
        Serial_ProcessRuntimeCommands(); assert(clears == 1);
    }
}
static void repeat_during_reply(unsigned port)
{
    assert(s_command_result[port] != COMMAND_IDLE);
    feed((uint8_t)port, "ClearRuntime\r\nClearRuntime\n");
}
static void start_line_during_reply(unsigned port)
{
    assert(s_command_result[port] != COMMAND_IDLE);
    feed((uint8_t)port, "ClearRun");
}
static void test_retries_while_sending(void)
{
    unsigned port, ok;
    for (port = 0; port < 2; ++port)
    {
        for (ok = 0; ok < 2; ++ok)
        {
            reset_fixture();
            clear_result = (uint8_t)ok;
            feed((uint8_t)port, "ClearRuntime\n");
            Serial_ProcessRuntimeCommands();
            during_reply = repeat_during_reply;
            Serial_SendCommandReply((uint8_t)port);
            Serial_ProcessRuntimeCommands();
            assert(clears == 1 && s_command_result[port] == COMMAND_IDLE);
            assert(s_clear_request_mask == 0);
            during_reply = NULL;
            feed((uint8_t)port, "ClearRuntime\n");
            Serial_ProcessRuntimeCommands();
            assert(clears == 2); /* A fresh command AFTER the reply is allowed. */
        }

        reset_fixture();
        feed((uint8_t)port, "ClearRuntime\n");
        Serial_ProcessRuntimeCommands();
        during_reply = start_line_during_reply;
        Serial_SendCommandReply((uint8_t)port);
        assert(s_command_result[port] == COMMAND_IDLE);
        feed((uint8_t)port, "time\n"); /* Retry spans the end of the ACK. */
        Serial_ProcessRuntimeCommands();
        assert(clears == 1 && s_clear_request_mask == 0);
        during_reply = NULL;
        feed((uint8_t)port, "ClearRuntime\n");
        Serial_ProcessRuntimeCommands();
        assert(clears == 2);
    }
}
static void test_reply_failure_only_retries_transmission(void)
{
    unsigned port, ok;
    for (port = 0; port < 2; ++port)
    {
        for (ok = 0; ok < 2; ++ok)
        {
            reset_fixture();
            clear_result = (uint8_t)ok;
            feed((uint8_t)port, "ClearRuntime\n");
            Serial_ProcessRuntimeCommands();
            send_result = 0;
            during_reply = repeat_during_reply;
            Serial_SendCommandReply((uint8_t)port);
            assert(s_command_result[port] == (ok ? COMMAND_OK : COMMAND_ERROR));
            feed((uint8_t)port, "ClearRuntime\n");
            Serial_ProcessRuntimeCommands();
            assert(clears == 1 && replies[port] == 1);
            send_result = 1;
            Serial_SendCommandReply((uint8_t)port);
            Serial_ProcessRuntimeCommands();
            assert(clears == 1 && replies[port] == 2);
            assert(s_command_result[port] == COMMAND_IDLE);
            during_reply = NULL;
            Serial_SendCommandReply((uint8_t)port);
            assert(replies[port] == 2);
        }
    }
}
int main(void)
{
    test_both_ports_and_reset();
    test_partial_and_simultaneous();
    test_failure_and_request_during_erase();
    test_invalid_lines_and_rx_errors();
    test_retries_while_sending();
    test_reply_failure_only_retries_transmission();
    puts("PASS: dual UART commands, Reset compatibility, CR/LF, coalescing through ACK and split retries, TX retry without re-erase, failures, malformed/error rejection; no erase/TX in ISR");
    return 0;
}
