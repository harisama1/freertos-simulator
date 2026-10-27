# FreeRTOS Simulator

Chạy chương trình FreeRTOS cho MCU nhỏ (STM32, RP2040…) trên laptop, không cần board. Gồm một IDE web và một CLI.

| Chế độ | Đầu vào | Chạy bằng |
|---|---|---|
| **FreeRTOS** | File C dùng API FreeRTOS (`.c`/`.txt`) | Kernel FreeRTOS thật, POSIX port, build 32-bit (`gcc -m32`) để con trỏ, `StackType_t`, TCB và khối heap có kích thước như MCU 32-bit. Ngắt và ngoại vi giả lập qua [sim/sim_hw.h](sim/sim_hw.h) |
| **C** | Chương trình C bất kỳ: superloop bare-metal, scheduler tự viết | gcc; timeline qua [sim/simtrace.h](sim/simtrace.h) |

Chế độ được tự nhận: có `xTaskCreate`/`vTaskStartScheduler` thì là FreeRTOS, còn lại là C.

## Cài đặt

Cần Linux x86-64, `python3`, `gcc` với hỗ trợ 32-bit (`gcc-multilib`). Kernel FreeRTOS đặt cạnh thư mục simulator, hoặc chỉ đường dẫn bằng biến `FREERTOS_KERNEL`:

```bash
sudo apt install gcc-multilib python3
git clone https://github.com/FreeRTOS/FreeRTOS-Kernel.git      # đã thử ở commit 640d0c4
git clone https://github.com/harisama1/freertos-simulator.git simulator
python3 simulator/frsim.py serve
```

## Dùng

`frsim` dưới đây = `python3 frsim.py`.

```bash
frsim serve                      # IDE: http://127.0.0.1:8765, kéo-thả file vào cửa sổ là chạy
frsim run file.c                 # CLI: output, bảng task/đối tượng/ISR, stack/RAM, timeline ASCII
frsim run file.c --mcu stm32f103c8 --heap 4 --duration 2
frsim run file.c --json          # JSON đầy đủ
```

Tuỳ chọn ghi ngay trong file (ưu tiên hơn ô điều khiển của IDE; cờ CLI gõ tay ưu tiên hơn cả hai):

```c
/* frsim: mcu=stm32f103c8 heap=4 duration=2 tick_rate=1000 */
```

Khoá: `mcu preemption time_slicing idle_should_yield tick_rate max_priorities static_alloc heap heap_size duration timestamp`.

Mặc định: chạy **10 s**, heap **1 MB**. Mục đích là xem code chạy thế nào, nên một lần chạy không bị hỏng vì thiếu bộ nhớ. Giới hạn của chip chỉ được báo cáo, xem "Preset MCU".

| Tab | Nội dung |
|---|---|
| Kết quả | Output chương trình, gợi ý lỗi (heap, stack, API sai trong ISR) |
| Build | Lỗi/cảnh báo gcc; bấm vào dòng để nhảy tới code |
| Timeline | Task (Running / Ready / Blocked kèm lý do), ISR, queue/semaphore (số phần tử theo thời gian), mutex (task đang giữ), timer (lúc hết hạn), event group (set/clear bit), GPIO (dạng sóng) |
| Thống kê | CPU % và lý do block từng task; đối tượng kernel; ngắt; RAM và stack theo MCU; heap |

Tham số URL: `?example=freertos/ch8_mutex_priority_inheritance.c&run=1&tab=tl&all=1`.

## Debug từng bước (record & replay)

Nút **🐞 Debug** (Ctrl+Shift+Enter, CLI `--debug 1`) chạy chương trình một lần và ghi lại mọi bước. Sau đó có thể tua tới/lùi tự do:

| Nguồn | Cách ghi |
|---|---|
| Khối lệnh đã chạy | File của bạn được build với `-fsanitize-coverage=trace-pc`; địa chỉ khối được đổi ra dòng bằng bảng dòng DWARF (`objdump`). Vòng lặp không gọi API được gộp thành một bước "lặp lại N khối lệnh" |
| Lời gọi API | `sim_prelude.h` bọc các API kernel, `printf`/`vPrint*` và `sim_*` để ghi đúng `__LINE__` của lời gọi |
| Kernel | Chuyển trạng thái task (lý do Blocked), queue/semaphore/mutex, notification, event group, timer, ISR, GPIO, heap |
| Output | Vị trí byte trong stdout sau mỗi lần in |

Tab **Debug** gồm:
- nút ⏮ / ◀ Lùi / Bước ▶ (phím ← →), ⏭ Sự kiện kernel kế tiếp, ▶▶ Tới breakpoint (bấm số dòng trong editor để đặt), bộ lọc "Theo dõi" theo task, thanh tua;
- thẻ thao tác hiện tại kèm giải thích lý thuyết;
- bảng task (trạng thái, lý do Blocked, dòng đang đứng), mức queue/semaphore, ai giữ mutex, ISR đang chạy, GPIO, heap còn trống;
- nhật ký bước và output tính đến bước đó.

Editor tô dòng của bước hiện tại bằng màu của task/ISR đang thực thi; ô vuông màu ở lề cho biết mỗi task đang đứng ở dòng nào. Tab Timeline có vạch đỏ tại thời điểm của bước.

Giới hạn: tối đa 40 000 bước (chương trình in liên tục có thể chạm giới hạn trước 10 s; khi đó giảm ô "Chạy"); trong một khối lệnh không có rẽ nhánh, các dòng không gọi API được báo chung một bước; chỉ áp dụng cho chế độ FreeRTOS.

## Chế độ FreeRTOS

| IDE | CLI | Macro |
|---|---|---|
| MCU | `--mcu` | preset: RAM, heap mặc định, khung context |
| Preemption | `--preemption` | `configUSE_PREEMPTION` |
| Time slicing | `--time-slicing` | `configUSE_TIME_SLICING` |
| Idle yield | `--idle-should-yield` | `configIDLE_SHOULD_YIELD` |
| Tick | `--tick-rate` | `configTICK_RATE_HZ` |
| Heap | `--heap 1..5` | `portable/MemMang/heap_N.c` |
| Size | `--heap-size` | `configTOTAL_HEAP_SIZE` |
| Static alloc | `--static-alloc` | `configSUPPORT_STATIC_ALLOCATION` |
| Chạy (s) | `--duration` | thời gian chạy rồi dừng tiến trình |

Các macro khác nằm trong [sim/FreeRTOSConfig.h](sim/FreeRTOSConfig.h): `configMINIMAL_STACK_SIZE` = 128 word, `configTIMER_TASK_STACK_DEPTH` = 256 word, `configTIMER_TASK_PRIORITY` = `configMAX_PRIORITIES - 1`; queue, mutex, counting/recursive semaphore, queue set, notification, timer, event group đều bật.

- Không cần `#include`: [sim/sim_prelude.h](sim/sim_prelude.h) chèn các header FreeRTOS và `sim_hw.h`. Code chép từ sách chạy được, kể cả chương 7 dùng API của Windows port (`vPortSetInterruptHandler`, `vPortGenerateSimulatedInterrupt`).
- Có `vPrintString`, `vPrintStringAndNumber`, `vPrintTwoStrings`… `printf`/`puts`/`putchar` được chuyển sang hàm in an toàn (critical section + `write()`).
- Hook (`vApplicationIdleHook`, `TickHook`, `MallocFailedHook`, `GetIdleTaskMemory`, `GetTimerTaskMemory`) có bản mặc định weak.
- Tên đối tượng trên timeline lấy theo biến được gán ở dòng tạo (`xQueue = xQueueCreate(...)` → `xQueue`).

### Preset MCU

| `mcu` | Lõi | RAM | Khung context |
|---|---|---|---|
| `host` | – | không giới hạn | 64 B |
| `stm32f030f4` | Cortex-M0 | 4 KB | 64 B |
| `stm32f103c8` | Cortex-M3 | 20 KB | 64 B |
| `stm32f411ce` | Cortex-M4F | 128 KB | 68 B |
| `rp2040` | Cortex-M0+ (1 core) | 264 KB | 64 B |

Chọn MCU không đổi heap hay cách chạy; nó chỉ dùng cho báo cáo. **RAM cần** = `.data` + `.bss` của kernel và chương trình, cộng **heap dùng thực tế (đỉnh)**, cộng main stack (MSP). Con số heap đỉnh cũng là giá trị `configTOTAL_HEAP_SIZE` tối thiểu khi đưa code lên chip.

**Stack** từng task là ước lượng tĩnh, không phải đo: độ sâu lớn nhất theo đồ thị lời gọi (`gcc -Os -fstack-usage -fcallgraph-info`) tính từ hàm của task, cộng khung context mà port Cortex-M đẩy lên stack khi chuyển task. So với stack depth truyền cho `xTaskCreate` × 4 byte, kết quả là OK, SÁT (dư < 20 %) hoặc TRÀN. Timer task được tính thêm callback lớn nhất. Không tính đệ quy, con trỏ hàm, alloca. `printf` và các hàm cùng họ giả định ≈ 400 B (newlib-nano). Frame được tính bằng gcc i386, không phải arm-none-eabi-gcc, nên chỉ là ước lượng cùng bậc độ lớn.

### Ngắt và ngoại vi giả lập ([sim/sim_hw.h](sim/sim_hw.h))

Thời gian tính bằng ms kể từ `vTaskStartScheduler()`. Sự kiện ngoài được phát trong ngắt tick, nên độ phân giải bằng 1 tick.

```c
sim_irq_attach(irq, "TIM2", isr);        /* void isr(void); trong ISR chỉ dùng ...FromISR() */
sim_irq_raise(irq);                      /* từ task: ISR chạy ngay (như NVIC_SetPendingIRQ) */
sim_irq_at(irq, ms);                     /* một sự kiện ngoài */
sim_irq_every(irq, period_ms, first_ms); /* timer phần cứng */

sim_gpio_name(pin, "LED PA5");  sim_gpio_write(pin, 1);  sim_gpio_toggle(pin);  sim_gpio_read(pin);
sim_gpio_input(pin, "100:1 250:0", irq); /* mức vào theo thời gian, irq ở mỗi cạnh (-1: không) */
sim_button(pin, press_ms, hold_ms, bounces, irq);   /* nút active-low, có dội phím */

sim_uart_rx(at_ms, "LED ON\n", irq);     /* mỗi ms một byte, mỗi byte một ngắt */
sim_uart_getc();                         /* trong ISR: byte nhận được, -1 nếu rỗng */

sim_adc_input(ch, "0:0 500:4095 1000:0");  /* tín hiệu tuyến tính từng đoạn (ms:giá trị) */
sim_adc_read(ch);
```

Kết thúc ISR bằng `portYIELD_FROM_ISR(xHigherPriorityTaskWoken)`. Simulator phát hiện:
- API của task (`xQueueSend`, `xSemaphoreGive`, `xTaskNotify`, `xEventGroupSetBits`…) gọi trong ISR: cảnh báo;
- API có thể block gọi trong ISR: dừng chương trình (exit code 4).

### Khác với board thật

| Khác biệt | Hệ quả |
|---|---|
| Tick là timer Linux | Chu kỳ ≈ 1,05–1,1 ms thay vì đúng 1 ms; thứ tự lập lịch vẫn đúng |
| Busy loop chạy theo CPU laptop | Thời gian vòng `for` khác MCU |
| Task chạy trên stack của pthread | Tràn stack không gây lỗi khi mô phỏng; `uxTaskGetStackHighWaterMark()` vô nghĩa. Dùng bảng stack tĩnh |
| Ngắt ngoài chỉ đến ở biên tick; không có ưu tiên/lồng ngắt NVIC | Không mô phỏng được độ trễ ngắt dưới 1 tick, `configMAX_SYSCALL_INTERRUPT_PRIORITY` |
| Một core | Không mô phỏng FreeRTOS SMP (ESP32, RP2040 2 core) |

## Chế độ C: superloop / scheduler tự viết

Không cần `#include`: `simtrace.h`, `stdio.h`, `stdint.h`, `stdlib.h`, `string.h` được chèn tự động.

```c
int  st_task(const char *name, int prio);   /* tạo một hàng timeline, trả về id */
void st_run(int id);      /* id đang chạy; task chạy trước đó chuyển sang Ready */
void st_block(int id);    /* id Blocked (chờ, ngủ, suspend) */
void st_ready(int id);    /* id Ready */
void st_idle(void);       /* CPU rảnh */
void st_exit(int id);     /* id kết thúc */
void st_time(unsigned long long t);   /* thời gian ảo (bộ đếm tick của bạn); không gọi = thời gian thực, µs */
void st_tick(void);       /* vạch tick */
void st_mark(const char *label);      /* ghi chú tại thời điểm hiện tại */
```

## Ví dụ

`book/exN_M.c`: 25 Example của *Mastering the FreeRTOS Real Time Kernel* (4.1–10.2).
`book/lstN_M.c`: các Listing có mã chạy được nhưng không phải Example, đã làm cho chạy được:

| File | Listing | Nội dung |
|---|---|---|
| `lst3_7` | 3.7–3.11 | Thống kê heap, phân mảnh, malloc failed hook |
| `lst4_7` | 4.7 | Tạo task từ trong task |
| `lst5_13` | 5.13–5.15 | Pointer queue, buffer cấp phát động |
| `lst5_14_pool` | 5.14–5.15 | Pointer queue, pool buffer tĩnh + queue buffer rảnh |
| `lst5_14_stack_bug` | 5.5.1 | Lỗi: gửi con trỏ tới biến trên stack |
| `lst5_16` | 5.16–5.20 | `IPStackEvent_t`: một queue, nhiều kiểu dữ liệu |
| `lst5_27` | 5.27 | Queue set gồm 2 queue + binary semaphore |
| `lst5_28` | 5.28–5.32 | Mailbox: `xQueueOverwrite()` / `xQueuePeek()` |
| `lst6_13` | 6.13 | `xTimerChangePeriod()` |
| `lst7_10` | 7.10 | Deferred task UART RX: đọc hết FIFO, timeout |
| `lst8_1`, `lst8_4`, `lst8_9` | 8.1, 8.4, 8.7–8.9 | Read-modify-write: không bảo vệ / critical section / suspend scheduler |
| `lst8_14` | 8.14 | Recursive mutex |
| `lst8_15`, `lst8_16` | 8.15, 8.16 | Mutex trong vòng lặp chặt; sửa bằng `taskYIELD()` |
| `lst10_11`, `lst10_12` | 10.11, 10.12 | Driver UART TX/RX dùng task notification |
| `lst10_13` | 10.13 | `xTaskNotifyFromISR()` gửi giá trị ADC |
| `lst10_14` | 10.14–10.18 | Server task: request qua queue, trả lời bằng notification |
| `lst12_12` | 12.4–12.12 | Run-time stats, `vTaskListTasks()` |

Không đưa vào: 4.30 (TLS pointer), 9.11 (mã giả, trùng Example 9.2), chương 11 (tickless idle — POSIX port không hỗ trợ), 13.x (stack hook — task chạy trên stack pthread; dùng ước lượng tĩnh).

| File | Nội dung |
|---|---|
| `freertos/ch3_heap_usage.txt` | heap_1..5, cấp phát/giải phóng |
| `freertos/ex4_*.txt` | Example 4.1, 4.6, 4.8, 4.9 |
| `freertos/ch5_1_…`, `ch5_2_…` | Queue: nhận block (5.1), gửi block khi đầy (5.2) |
| `freertos/ch6_software_timers.c` | Timer one-shot và auto-reload, callback trong daemon task |
| `freertos/ch7_1_…` | Example 7.1: deferred interrupt với binary semaphore (API Windows port) |
| `freertos/ch7_uart_isr_to_queue.c` | ISR UART đưa byte vào queue |
| `freertos/ch8_mutex_priority_inheritance.c` | Priority inversion và priority inheritance |
| `freertos/ch9_event_groups_sync.c` | Event group: nút bấm (ISR) + task cảm biến |
| `freertos/ch10_notify_from_timer_isr.c` | Task notification từ ISR timer, ADC, LED |
| `freertos/mcu_button_debounce.c` | Nút dội phím, chống dội bằng software timer (STM32F103) |
| `freertos/mcu_stack_overflow_f103.c` | Ước lượng stack và RAM; task tràn stack |
| `c/01_coop_superloop.c` | Scheduler cooperative bare-metal |

## Cấu trúc

```
frsim.py           CLI + web server; build kernel (cache build/), chạy, đọc trace, phân tích stack/RAM
sim/               FreeRTOSConfig.h (trace macro), sim_prelude.h, sim_support.c (in, hook, trace),
                   sim_hw.h/.c (ngắt, GPIO, UART, ADC); simtrace.h/.c (chế độ C)
web/index.html     IDE
examples/          freertos/, c/
```

Định dạng trace (một dòng mỗi sự kiện, `<thời điểm> <mã> <số…> [tên]`) được mô tả trong docstring của `parse_trace()` trong [frsim.py](frsim.py).
