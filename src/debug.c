
#include <stdio.h>
#include <stdarg.h>
#include "tusb.h"
#include "debug.h"


#define LOG_BUFFER_SIZE 1024  // increase if needed


static uint8_t log_buf[LOG_BUFFER_SIZE];
static volatile size_t head = 0;
static volatile size_t tail = 0;
static volatile size_t dropped = 0;

static size_t rb_free(void)
{
    if (head >= tail)
        return LOG_BUFFER_SIZE - (head - tail) - 1;
    else
        return (tail - head) - 1;
}

static size_t rb_used(void)
{
    if (head >= tail)
        return head - tail;
    else
        return LOG_BUFFER_SIZE - (tail - head);
}

static void rb_write(const uint8_t *data, size_t len)
{
    for (size_t i = 0; i < len; i++)
    {
        size_t next = (head + 1) % LOG_BUFFER_SIZE;

        if (next == tail)
        {
            // buffer full → drop byte
            dropped++;
            break;
        }

        log_buf[head] = data[i];
        head = next;
    }
}

void usb_debug_printf(const char *fmt, ...)
{
    char tmp[128];

    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(tmp, sizeof(tmp), fmt, args);
    va_end(args);

    if (len <= 0) return;

    if (len >= sizeof(tmp))
        len = sizeof(tmp) - 1;

    rb_write((uint8_t*)tmp, len);
}

void usb_log_flush(void)
{
    if (!tud_cdc_connected())
        return;

    while (rb_used() > 0)
    {
        uint32_t avail = tud_cdc_write_available();
        if (avail == 0)
            break;

        size_t chunk = rb_used();
        if (chunk > avail)
            chunk = avail;

        // handle wrap-around
        size_t first = chunk;
        if (tail + chunk > LOG_BUFFER_SIZE)
            first = LOG_BUFFER_SIZE - tail;

        tud_cdc_write(&log_buf[tail], first);
        tail = (tail + first) % LOG_BUFFER_SIZE;

        if (first < chunk)
        {
            size_t second = chunk - first;
            tud_cdc_write(&log_buf[tail], second);
            tail = (tail + second) % LOG_BUFFER_SIZE;
        }
    }

    tud_cdc_write_flush();
}

void usb_log_report_drops(void)
{
    if (dropped > 0)
    {
        char msg[64];
        int len = snprintf(msg, sizeof(msg),
                           "[LOG DROPPED %u BYTES]\n", (unsigned)dropped);

        dropped = 0;
        rb_write((uint8_t*)msg, len);
    }
}