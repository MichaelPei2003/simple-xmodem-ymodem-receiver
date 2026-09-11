# ymodem_helper 使用说明

`ymodem_helper` 用于从字节流接收 XMODEM / YMODEM 数据。应用程序负责收发字节，
模块负责协议应答和帧校验，并通过回报函数交付数据。

使用时只需包含 `ymodem_helper.h`，通过公开函数操作对象，无需了解模块内部结构。

## 1. 接入工程

将整个 `ymodem_helper` 文件夹复制到工程中。需要 C11 编译器和 C 标准库。
模块不要求特定的芯片、串口驱动或操作系统。

在应用工程的 CMake 中加入：

```cmake
add_subdirectory(ymodem_helper)
target_link_libraries(your_target PRIVATE ymodem_helper)
```

将 `your_target` 替换为应用的目标名称。源文件只需包含：

```c
#include "ymodem_helper.h"
```

当前版本是接收端，使用 CRC16，支持 128 字节和 1024 字节数据帧。
发送端应选择 XMODEM-CRC、XMODEM-1K，或与本版本结束流程相符的 YMODEM 模式。
本版本不支持校验和模式和 YMODEM-G。

**YMODEM 兼容条件：发送端每个文件结束只发送一次 EOT；模块随后输出 `NAK ACK C`，
发送端继续发送下一个文件的 0 号帧，或空文件名的 0 号结束帧。**
本版本没有实现标准双 EOT 等待流程，不能假定任意 YMODEM 发送工具都兼容。

## 2. 准备对象和缓冲区

每条独立接收通道需要以下资源：

| 资源 | 用途 |
| --- | --- |
| 一个 `ymodem_t` | 保存该通道的接收状态 |
| 一个 `ymodem_helper_cfg_t` | 配置输入输出队列、回报函数和上下文 |
| 一个输入 `ymodem_queue_t` 和缓冲区 | 存放发送端传入的原始字节 |
| 一个输出 `ymodem_queue_t` 和缓冲区 | 存放模块生成的协议应答，供应用发送给对端 |
| 一个回报函数，可选 | 处理文件信息、数据帧和接收结果 |
| 一个用户上下文，可选 | 保存应用自己的统计值或文件处理状态 |

输入缓冲区在 128 字节模式下至少需要 **133 字节**，支持 1K 时至少需要 **1029 字节**。
下面的示例使用 2048 字节输入缓冲区和 64 字节输出缓冲区。
队列容量参数为 `uint16_t`，有效范围为 1～65535。

实例、队列、缓冲区及回报上下文必须在使用期间保持有效。可使用静态变量，也可放在
生命周期足够长的应用对象中。初始化后的实例和队列不能通过赋值或 `memcpy` 复制、移动。

虽然头文件提供了完整类型，应用仍应将 `ymodem_t` 和 `ymodem_queue_t` 视为黑盒：
不要读写其成员，所有操作均使用公开函数。其他库的队列类型不能强转后传入。

## 3. 最小接收示例

以下代码统计整个接收会话的数据量，不把收到的内容写入文件。
需要保存数据时，在 `YMODEM_NEW_FRAME` 分支中及时处理或复制数据。

`transport_try_read` 和 `transport_try_write` 是需要应用实现的传输适配函数，
不是模块提供的接口。两者必须立即返回：

- `transport_try_read` 返回 `true` 表示取到一个字节；返回 `false` 表示暂时没有数据。
- `transport_try_write` 返回 `true` 表示传输层已接收该字节；返回 `false` 表示未接收，稍后重试。

```c
#include "ymodem_helper.h"

/* Implement these two functions for the application transport. */
bool transport_try_read(uint8_t *pchByte);
bool transport_try_write(uint8_t chByte);

typedef struct receive_result_t {
    size_t uBytes;
    size_t uFiles;
    size_t uErrors;
    bool bComplete;
    bool bCancelled;
} receive_result_t;

static ymodem_t s_tReceiver;
static ymodem_queue_t s_tIn;
static ymodem_queue_t s_tOut;
static uint8_t s_chIn[2048];
static uint8_t s_chOut[64];
static receive_result_t s_tResult;

/* Application-owned pending bytes, separate from module state. */
static uint8_t s_chRx;
static uint8_t s_chTx;
static bool s_bRxPending;
static bool s_bTxPending;

static void receive_report(
    void *pObj,
    ymodem_report_t tReport,
    uint8_t *pchData,
    size_t uSize)
{
    receive_result_t *ptResult = (receive_result_t *)pObj;
    (void)pchData;

    switch (tReport) {
        case YMODEM_START:
            ptResult->uFiles++;
            break;

        case YMODEM_NEW_FRAME:
            ptResult->uBytes += uSize;
            /* Process or copy pchData[0 .. uSize-1] here. */
            break;

        case YMODEM_COMPLETE:
            ptResult->bComplete = true;
            break;

        case YMODEM_CANCELLED:
            ptResult->bCancelled = true;
            break;

        case YMODEM_ERROR:
            ptResult->uErrors++;
            break;

        case YMODEM_TIME_OUT:
            break;
    }
}

void receiver_init(void)
{
    ymodem_helper_cfg_t tCFG = {
        .ptIn = &s_tIn,
        .ptOut = &s_tOut,
        .fnHandler = receive_report,
        .pObj = &s_tResult,
    };

    s_tResult = (receive_result_t){0};
    s_bRxPending = false;
    s_bTxPending = false;

    ymodem_queue_init(&s_tIn, s_chIn, sizeof(s_chIn));
    ymodem_queue_init(&s_tOut, s_chOut, sizeof(s_chOut));
    ymodem_helper_init(&s_tReceiver, &tCFG);
}

void receiver_poll(void)
{
    if (!s_bRxPending) {
        s_bRxPending = transport_try_read(&s_chRx);
    }
    if (s_bRxPending && ymodem_queue_write_byte(&s_tIn, s_chRx)) {
        s_bRxPending = false;
    }

    (void)ymodem_helper_task(&s_tReceiver);

    if (!s_bTxPending) {
        s_bTxPending = ymodem_queue_read_byte(&s_tOut, &s_chTx);
    }
    if (s_bTxPending && transport_try_write(s_chTx)) {
        s_bTxPending = false;
    }
}
```

系统准备好传输通道后调用 `receiver_init()` 一次，再在超级循环中反复调用
`receiver_poll()`，同时调度其他任务。初始化会启动接收，后续轮询会发起握手。
不要每轮都调用初始化，也不要等待 `task` 返回某个值才运行其他任务。

示例每轮最多向模块提交一个字节、向传输层发送一个字节。轮询频率必须满足接收速率。
需要提高吞吐量时，可将输入和输出操作改为每轮处理有限数量的字节；
仍需保留暂时未能提交或发送的字节，并给其他任务执行机会。

输入队列满时，示例暂停读取新字节。传输驱动仍需要足够的接收缓冲或流控，
否则暂存一个字节并不能避免硬件接收溢出。

## 4. 公开接口

| 调用 | 用法与结果 |
| --- | --- |
| `ymodem_queue_init(ptQueue, pBuffer, hwSize)` | 初始化队列，返回 `ptQueue`；会清空原队列状态 |
| `ymodem_queue_write_byte(ptQueue, chByte)` | 写入一个字节；成功为 `true`，队列满为 `false` |
| `ymodem_queue_read_byte(ptQueue, pchByte)` | 取出并移除一个字节；成功为 `true`，队列空为 `false` |
| `ymodem_queue_length(ptQueue)` | 返回当前队列中的字节数 |
| `ymodem_helper_init(ptThis, ptCFG)` | 复制配置并启动接收，返回 `ptThis`；不会清空输入、输出队列 |
| `ymodem_helper_task(ptThis)` | 推进接收并生成应答，应反复调用 |

实例、队列和缓冲区参数必须有效，不能传入 `NULL`。
读取应答时必须提供有效的字节地址；不要传 `NULL`，否则成功读取的字节会被丢弃。

通过配置结构体的 `.ptIn`、`.ptOut`、`.fnHandler`、`.pObj` 设置接收参数，然后调用 `init`。
`ptCFG`、`ptIn` 和 `ptOut` 必须非空。`pObj` 原样传给回报函数，不需要上下文时可以为 `NULL`；
`fnHandler` 为 `NULL` 时不产生用户回报，协议接收仍会继续。
配置在初始化时按值复制，配置变量本身无需一直保留，但它引用的队列、缓冲区和上下文必须保持有效。
修改原配置变量不会改变已初始化实例；需要应用新配置时，在会话边界重新调用 `init`。
配置结构体是允许用户设置的公开参数，不应直接访问接收实例内部的 `tCFG`。

**`ymodem_fsm_rt_cpl` 只表示一次调度任务完成，不表示文件或会话接收完成。**
应用通过 `YMODEM_COMPLETE` 回报判断接收结束。

## 5. 如何处理回报

| 回报 | 应用收到的内容 | 建议处理 |
| --- | --- | --- |
| `YMODEM_START` | XMODEM 为 `NULL, 0`；YMODEM 为 0 号帧的数据区 | 开始一个文件；需要时保存文件名和元信息 |
| `YMODEM_NEW_FRAME` | 已通过校验的数据区，长度为 128 或 1024 | 处理文件数据，累加长度 |
| `YMODEM_COMPLETE` | 接收结束通知 | 完成统计或后续处理，忽略该回报的数据参数 |
| `YMODEM_CANCELLED` | 接收取消通知 | 标记取消，忽略该回报的数据参数 |
| `YMODEM_ERROR` | 帧校验错误通知 | 记录错误，忽略该回报的数据参数；不等同于会话已终止 |
| `YMODEM_TIME_OUT` | 预留通知，当前不会产生 | 如有业务超时要求，由应用自行计时 |

XMODEM 收到第一个有效数据帧时会依次回报 `START` 和 `NEW_FRAME`。
YMODEM 的非空文件名 0 号帧只作为 `START` 回报，不计入数据长度。
正常数据帧重复发送时会得到协议应答，但不会重复回报 `NEW_FRAME`。

XMODEM 收到 EOT 后回报 `COMPLETE`。YMODEM 每个文件开始时回报 `START`，
收到空文件名 0 号帧才回报整个批次的 `COMPLETE`；当前没有逐文件 EOT 回报。
示例中的 `uBytes` 累加整个会话，而不是只统计最后一个文件。

数据长度包含末尾填充，不一定等于原始文件长度。模块不会自动解析文件名、
按文件长度去除填充或创建文件。需要精确保存 YMODEM 文件时，应用应解析 `START`
中的长度字段，并限制实际写入长度；XMODEM 原始长度需由应用另行确定。

YMODEM 文件名从 `pchData[0]` 开始，以 `\0` 分隔后续元信息。
解析前必须在 `uSize` 范围内确认终止符，不要直接对未经检查的数据使用 `strlen` 或 `%s`。
若要在传输结束时打印文件名，应在 `START` 回报中复制到应用自己的缓冲区。

回报在 `task` 调用期间同步执行。数据指针只在该次回报期间有效，应按只读数据使用；
不要保存指针供后续使用。回报函数必须及时返回，不应等待串口、Flash 或其他任务完成。
需要延后处理时，先复制到应用自己的缓冲区，并确保缓冲容量和处理速度足够。
回报函数没有返回值，应用无法通过它要求模块等待数据保存完成或重新交付数据。

不要在回报函数内重新初始化、递归调用同一实例的 `task`，或读取其输入队列。

## 6. 运行条件与再次接收

- 输出队列只能用于协议应答，内容必须原样发送给对端。日志和统计结果应使用独立通道，
  或等本次协议通信完全结束后再发送。
- 必须及时取出并发送应答。当前版本在输出队列满时可能丢失应答，不会完整重试。
  一次 YMODEM 结束握手需要 3 字节空间，应另外为其他待发送应答预留余量。
  示例的发送暂存字节只解决传输层暂时拒收，不能解决输出队列长期满的问题。
- 收到 `COMPLETE` 后仍应发送剩余应答。输出队列为空不等于串口已经发送完毕：
  示例中的 `s_bTxPending` 以及驱动自己的发送缓冲也需处理完毕。
- 当前连续错误帧仅首次发送 NAK，收到有效帧后才允许再次发送；发送端需能处理这种重试行为。
- 调度调用次数会影响握手重试间隔，不应把它视为固定毫秒定时器。
- 同一实例的队列操作、初始化、回报设置和 `task` 应串行执行。
  从中断或多个线程直接并发调用时，应用必须自行提供同步。

再次接收应在旧会话结束、应答发送完毕后进行。先处理旧输入残留，再传入配置初始化对象。
上面的 `receiver_init()` 会清空应用的两个队列和暂存字节，因此不要在传输期间调用。
模块当前没有对外提供单独的主动取消接口。

多实例使用时，为每条独立通道分别准备接收对象、队列、缓冲区和上下文，分别调用 `task`。
模块不会自动识别多个实例共享的字节流归属，不能直接让两个接收实例轮流消费同一个输入队列。
