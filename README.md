# ymodem_helper

`ymodem_helper` is a platform-independent C11 receiver for XMODEM and YMODEM transfers.

IF YOU HAVE ANY QUESTIONS, REFER TO ANY AVAILABLE AI AND HE WILL DO THE WORK FOR YOU.

## Features

- Receives XMODEM-CRC, XMODEM-1K, and YMODEM transfers, subject to the limitations below.
- Supports 128-byte and 1024-byte data frames.
- Validates frames with CRC16.
- Generates protocol replies through an output queue.
- Reports file data and transfer status through a callback.
- Requires only the C standard library.

## Protocol Limitations

This module is not fully compliant with the [X/YMODEM Protocol Reference](https://www.classic-computing.de/hellwie/pdf/XMODEM_YMODEM%20Protocol%20Reference.pdf). It has these known differences:

- YMODEM uses a single-EOT flow. After the first `EOT`, the receiver queues `NAK`, `ACK`, and `C` without waiting for a second `EOT`. A standard double-EOT sender may not work.
- The receiver supports CRC16 only. It does not support the original XMODEM 8-bit checksum or CRC-to-checksum fallback.
- Handshake retry timing uses task poll counts instead of elapsed time. Active transfers have no byte timeout or retry limit, and `YMODEM_TIME_OUT` is never reported.
- Only the first frame in a sequence of invalid frames produces `NAK`. A valid frame enables `NAK` again.
- One `CAN` byte cancels a transfer. The reference graceful-abort behavior uses two consecutive `CAN` bytes.
- An unexpected block number stops the receiver without sending the reference `CAN` abort sequence.
- Most protocol replies are not retried if the output queue is full. The application must drain the output queue promptly.
- YMODEM-G is not supported.

## Integration

```cmake
add_subdirectory(ymodem_helper)
target_link_libraries(your_target PRIVATE ymodem_helper)
```

Include the public header:

```c
#include "ymodem_helper.h"
```

## Public Types

- `ymodem_t`: Receiver instance.
- `ymodem_queue_t`: Byte queue used for protocol input or output.
- `ymodem_helper_cfg_t`: Receiver configuration with input queue, output queue, callback, and user context.

Treat `ymodem_t` and `ymodem_queue_t` as opaque objects. Do not access, copy, or move their members after initialization.

## Public Functions

| Function | Description |
| --- | --- |
| `ymodem_queue_init()` | Initializes a byte queue with caller-owned storage. |
| `ymodem_queue_write_byte()` | Adds one byte. Returns `false` if the queue is full. |
| `ymodem_queue_read_byte()` | Removes one byte. Returns `false` if the queue is empty. |
| `ymodem_queue_length()` | Returns the number of bytes in a queue. |
| `ymodem_helper_init()` | Configures the receiver and starts reception. |
| `ymodem_helper_task()` | Advances reception. Call this function repeatedly from the main loop. |

## Reports

The configured callback receives these reports:

| Report | Description |
| --- | --- |
| `YMODEM_START` | A transfer started. YMODEM provides file metadata; XMODEM provides `NULL, 0`. |
| `YMODEM_NEW_FRAME` | A validated 128-byte or 1024-byte data frame is available. The size includes padding. |
| `YMODEM_COMPLETE` | The transfer session is complete. |
| `YMODEM_CANCELLED` | The sender cancelled the transfer. |
| `YMODEM_ERROR` | A frame validation error occurred. |
| `YMODEM_TIME_OUT` | Reserved for timeout reporting. |

Callback data is valid only during the callback. Copy the data if the application must keep it.

## Basic Use

1. Create one `ymodem_t` instance and two `ymodem_queue_t` queues.
2. Initialize both queues with caller-owned buffers.
3. Set the queues and callback in `ymodem_helper_cfg_t`.
4. Call `ymodem_helper_init()` once.
5. Add received bytes to the input queue.
6. Call `ymodem_helper_task()` repeatedly.
7. Read protocol replies from the output queue and send them to the peer.
8. Handle received data and completion in the callback.

Use `YMODEM_COMPLETE`, not the return value of `ymodem_helper_task()`, to detect transfer completion.

The input queue must hold at least 133 bytes for 128-byte frames or 1029 bytes for 1K frames.

## Pseudocode Example

`transport_try_read()` and `transport_try_write()` represent application-provided transport functions.

```text
receiver: ymodem_t
input_queue: ymodem_queue_t
output_queue: ymodem_queue_t
input_buffer: byte[2048]
output_buffer: byte[64]
transfer_complete = false
pending_input = NONE
pending_output = NONE

function report_handler(context, report, data, size):
    if report == YMODEM_NEW_FRAME:
        process_or_copy(data, size)
    else if report == YMODEM_COMPLETE:
        transfer_complete = true

ymodem_queue_init(&input_queue, input_buffer, size(input_buffer))
ymodem_queue_init(&output_queue, output_buffer, size(output_buffer))

config = {
    ptIn: &input_queue,
    ptOut: &output_queue,
    fnHandler: report_handler,
    pObj: NULL
}

ymodem_helper_init(&receiver, &config)

loop forever:
    if pending_input == NONE:
        pending_input = transport_try_read()

    if pending_input != NONE:
        if ymodem_queue_write_byte(&input_queue, pending_input):
            pending_input = NONE

    ymodem_helper_task(&receiver)

    if pending_output == NONE:
        if ymodem_queue_read_byte(&output_queue, &byte):
            pending_output = byte

    if pending_output != NONE:
        if transport_try_write(pending_output):
            pending_output = NONE

    if transfer_complete and
       ymodem_queue_length(&output_queue) == 0 and
       pending_output == NONE:
        break
```
