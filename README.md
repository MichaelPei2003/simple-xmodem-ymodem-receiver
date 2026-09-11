# ymodem_helper

可独立复制的 C11 接收模块，只依赖 C 标准库。内部包含 window checker、
X/YMODEM 状态机、字节队列和事件，不依赖 Pico SDK 或工程根目录的 `app_cfg.h`。

## 模块边界

```text
ymodem_helper/
├── ymodem_helper.h          外部唯一包含入口
├── ymodem_helper.c          初始化、调度和回报转发
├── app_cfg.h                整个子树的配置入口
├── ymodem_helper_types.h    父子模块共用的返回值和回报类型
├── __common.h              顶层实现私有头
├── window_checker/         内部识别与 peek 调度
├── xmodem/                 内部协议状态机和 CRC
├── queue/                  自带队列
└── fsm_event/              实例内事件
```

顶层接口通过 `./app_cfg.h` 引入配置，子模块通过 `../app_cfg.h` 引入同一份配置，
不依赖应用工程的同名头文件。配置覆盖仍可通过编译定义提供。

每个子模块具有同名 `.h`、`.c` 和私有 `__common.h`。配置由顶层统一提供。
外部只包含 `ymodem_helper.h`，使用 `ymodem_helper_*` 和 `ymodem_queue_*` 接口。
`ymodem_internal_*` 仅供子树内部使用。

`ymodem_t` 和 `ymodem_queue_t` 的完整定义用于调用者分配存储空间。
外部不得直接访问它们的成员。顶层头包含必要的子模块类型以确定实例布局；
子模块头不反向包含顶层接口。初始化后的实例和队列不能复制或移动。

## 接入

调用者提供两个 `ymodem_queue_t`、各自的缓冲区，以及一个 `ymodem_t`。
这些对象在接收期间必须保持有效。自带队列与原工程的 `byte_queue_t` 是不同类型，
不能通过强制类型转换混用。

```c
#include "ymodem_helper.h"

typedef struct receive_result_t {
    size_t uBytes;
    bool bComplete;
} receive_result_t;

static ymodem_t s_tReceiver;
static ymodem_queue_t s_tIn;
static ymodem_queue_t s_tOut;
static uint8_t s_chIn[2048];
static uint8_t s_chOut[64];
static receive_result_t s_tResult;

static void report_handler(
    void *pObj, ymodem_report_t tReport, uint8_t *pchData, size_t uSize)
{
    receive_result_t *ptResult = (receive_result_t *)pObj;
    (void)pchData;

    if (YMODEM_NEW_FRAME == tReport) {
        ptResult->uBytes += uSize;
    } else if (YMODEM_COMPLETE == tReport) {
        ptResult->bComplete = true;
    }
}

static void receive_init(void)
{
    ymodem_helper_cfg_t tCFG = {
        .ptIn = &s_tIn,
        .ptOut = &s_tOut,
        .fnHandler = report_handler,
        .pObj = &s_tResult,
    };

    s_tResult = (receive_result_t){0};
    ymodem_queue_init(&s_tIn, s_chIn, sizeof(s_chIn));
    ymodem_queue_init(&s_tOut, s_chOut, sizeof(s_chOut));
    ymodem_helper_init(&s_tReceiver, &tCFG);
}
```

初始化会启动接收。在超级循环中执行以下操作，每轮只做有限工作：

1. 将串口收到的字节通过 `ymodem_queue_write_byte(&s_tIn, chByte)` 放入输入队列。
   返回 `false` 表示队列已满，调用者必须保留该字节并处理背压。
2. 调用 `ymodem_helper_task(&s_tReceiver)` 一次。
3. 串口具备发送能力时，通过 `ymodem_queue_read_byte(&s_tOut, &chByte)`
   取出应答并发送。一次读取成功后，该字节已从队列中移除。

`ymodem_helper_task` 返回内部 `xmodem_task` 的调度结果。
`ymodem_fsm_rt_cpl` 不表示文件传输完成；使用 `YMODEM_COMPLETE` 回报判断结束。
需要重新接收时，在会话边界处理完残留输入，再传入配置重新初始化。
多个实例各自保存状态；各实例需要各自的输入流，不会自动判断共享字节属于哪个实例。
队列操作和 task 需要由调用者串行调度，模块不提供中断或线程同步。

## 回报

通过 `ymodem_helper_cfg_t` 配置 `ptIn`、`ptOut`、`fnHandler` 和 `pObj`，再传入初始化函数。
配置按值复制到实例，局部配置变量可以在初始化后离开作用域；所引用的队列、缓冲区和上下文必须保持有效。
`pObj` 原样转交给回报函数；`fnHandler` 为 `NULL` 时不产生用户回报，协议应答仍由模块处理。

| 回报 | 数据含义 |
| --- | --- |
| `YMODEM_START` | XMODEM 为 `NULL, 0`；YMODEM 为非空文件名的 0 号帧数据区 |
| `YMODEM_NEW_FRAME` | 已校验的数据区，长度为 128 或 1024，包含文件末尾填充 |
| `YMODEM_COMPLETE` | XMODEM 的 EOT，或 YMODEM 的空文件名 0 号结束帧 |
| `YMODEM_CANCELLED` | 接收取消通知 |
| `YMODEM_ERROR` | 帧校验错误通知 |
| `YMODEM_TIME_OUT` | 预留，当前不会产生 |

只有 `NEW_FRAME` 应计入文件数据统计；`START` 的文件名和元信息不计入长度。
YMODEM 每个文件的非空 0 号帧产生一次 `START`，整个批次结束才产生 `COMPLETE`。
对 `COMPLETE`、`CANCELLED`、`ERROR`，不要把回报携带的数据当作新文件数据使用。

数据指针仅在回报函数执行期间有效。需要延后处理时，在回报函数内复制所需内容。
回报函数同步执行，必须及时返回，不能递归调用同一实例的 task/init，
也不能在回报中读取输入队列或操作内部状态。

## 保留的协议行为与容量要求

- 使用 CRC16，支持 128 字节和 1024 字节数据帧。
- 缺少数据时返回并允许重新 peek；YIELD 时保留当前解析进度。
  输入队列必须容纳一整个未确认帧：128 字节模式至少 133 字节，1K 至少 1029 字节。
- 当前 YMODEM 发送方只发送一次 EOT，模块连续输出 `NAK ACK C`。
  这不是标准的双 EOT 等待流程；更换为双 EOT 发送方时需另行调整。
- 连续错误帧仅首次发送 NAK，收到有效帧后允许再次发送，保留原有设计。
- 多数协议应答写入仍沿用原实现，不在队列满时重试。调用者应及时排空输出队列，
  在应答前保证足够空间，单次 YMODEM 结束应答需要 3 字节空间。
- `YMODEM_HELPER_CFG_RETRY_POLLS` 默认 100000，以 task 轮询次数计数，不是毫秒定时器。
  可通过编译定义覆盖；应对整个库使用一致的配置。

## 构建

现有工程可以通过以下方式接入，无需添加子模块目录作为 include 搜索路径：

```cmake
add_subdirectory(ymodem_helper)
target_link_libraries(your_target PRIVATE ymodem_helper)
```

也可使用主机 C 编译器独立构建：

```text
cmake -S ymodem_helper -B build/ymodem_helper-host
cmake --build build/ymodem_helper-host
```
