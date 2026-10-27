# Phân tích lý thuyết cho từng ví dụ của "Mastering the FreeRTOS Real Time Kernel" v1.1.0.
# win: cửa sổ timeline mặc định (tick); tasks: bảng thiết lập; theory: các ý lý thuyết;
# book: đối chiếu với hình/kết quả trong sách; diff: chỗ khác so với listing gốc.

CHAPTERS = {
    4: 'Quản lý task',
    5: 'Queue',
    6: 'Software timer',
    7: 'Quản lý ngắt',
    8: 'Quản lý tài nguyên',
    9: 'Event group',
    10: 'Task notification',
}

N = {}

N['ex4_1'] = dict(
    title='4.1 Tạo task', win=(0, 8),
    goal='Hai task cùng ưu tiên 1, mỗi task in chuỗi rồi chạy vòng lặp rỗng (busy loop). Không task nào gọi API chặn.',
    tasks=[('Task 1', 1, 'in, busy loop, lặp lại'), ('Task 2', 1, 'giống Task 1')],
    theory=[
        'Scheduler chọn task Ready có ưu tiên cao nhất. Hai task cùng ưu tiên 1 và luôn Ready, nên áp dụng <b>time slicing</b> (configUSE_TIME_SLICING = 1): mỗi ngắt tick, kernel chuyển sang task kế tiếp cùng ưu tiên.',
        'Mỗi time slice dài đúng 1 tick (1 ms ở configTICK_RATE_HZ = 1000). Task bị ngắt giữa chừng vòng lặp vẫn ở trạng thái <b>Ready</b>; nó không bị Block.',
        'Idle task (ưu tiên 0) không bao giờ chạy, vì luôn có task ưu tiên 1 Ready.',
        'Tmr Svc (daemon task, ưu tiên 6) chạy một lần ở t = 0 rồi Block trên hàng đợi lệnh timer.',
    ],
    book='Hình 4.2/4.3: Task 1 và Task 2 luân phiên mỗi tick; output xen kẽ "Task 1 is running", "Task 2 is running".',
)
N['ex4_2'] = dict(
    title='4.2 Dùng tham số task', win=(0, 8),
    goal='Một hàm vTaskFunction tạo ra hai task; chuỗi cần in truyền qua pvParameters.',
    tasks=[('Task 1', 1, 'vTaskFunction("Task 1 is running")'), ('Task 2', 1, 'vTaskFunction("Task 2 is running")')],
    theory=[
        'Một hàm có thể là thân của nhiều task. Mỗi task có TCB và stack riêng, nên biến cục bộ (pcTaskName, ul) là bản riêng của từng task.',
        'Tham số phải còn hợp lệ khi task chạy: chuỗi là hằng tĩnh, không nằm trên stack của main() (stack này có thể bị tái sử dụng sau vTaskStartScheduler()).',
        'Hành vi lập lịch giống hệt 4.1: time slicing mỗi tick.',
    ],
    book='Output giống Hình 4.2 của Example 4.1.',
)
N['ex4_3'] = dict(
    title='4.3 Thử nghiệm ưu tiên', win=(0, 8),
    goal='Như 4.2 nhưng Task 2 có ưu tiên 2.',
    tasks=[('Task 1', 1, 'busy loop'), ('Task 2', 2, 'busy loop, không bao giờ Block')],
    theory=[
        'Scheduler luôn chọn task Ready ưu tiên cao nhất (fixed-priority preemptive). Task 2 không bao giờ rời trạng thái Ready/Running, nên Task 1 bị <b>starvation</b>: 0 % CPU.',
        'Tick vẫn xảy ra, nhưng time slicing chỉ chia CPU giữa các task <i>cùng</i> ưu tiên cao nhất; ở mức 2 chỉ có Task 2.',
        'Bài học: task ưu tiên cao phải Block (chờ sự kiện hoặc delay), không được polling.',
    ],
    book='Hình 4.5: chỉ có "Task 2 is running".',
)
N['ex4_4'] = dict(
    title='4.4 Dùng trạng thái Blocked để delay', win=(0, 520),
    goal='Thay busy loop bằng vTaskDelay(pdMS_TO_TICKS(250)).',
    tasks=[('Task 1', 1, 'in, vTaskDelay(250)'), ('Task 2', 2, 'in, vTaskDelay(250)')],
    theory=[
        'vTaskDelay(n) chuyển task sang <b>Blocked</b> trong n tick, tính từ lúc gọi. Task Blocked không dùng CPU.',
        'Ở t = 0, Task 2 (ưu tiên cao hơn) chạy trước, in rồi Block; sau đó Task 1 chạy, in rồi Block. Cả hai Blocked nên Idle task chạy gần 100 % thời gian.',
        'Ở tick 250, cả hai cùng hết delay; kernel đưa cả hai sang Ready và Task 2 chạy trước vì ưu tiên cao hơn.',
        'Task 1 không còn bị starvation: ưu tiên chỉ quyết định thứ tự khi cả hai cùng Ready.',
    ],
    book='Hình 4.7/4.8: Task 2 in trước Task 1 trong mỗi chu kỳ; Idle chạy phần còn lại.',
)
N['ex4_5'] = dict(
    title='4.5 Dùng vTaskDelayUntil()', win=(0, 520),
    goal='Thay vTaskDelay() bằng vTaskDelayUntil(&xLastWakeTime, 250).',
    tasks=[('Task 1', 1, 'in, vTaskDelayUntil(250)'), ('Task 2', 2, 'in, vTaskDelayUntil(250)')],
    theory=[
        'vTaskDelay() tính thời gian chờ tương đối từ lúc gọi, nên chu kỳ thực = 250 tick + thời gian thực thi; sai số tích luỹ.',
        'vTaskDelayUntil() tính mốc đánh thức tuyệt đối: xLastWakeTime += 250. Chu kỳ đúng 250 tick bất kể task chạy lâu hay bị chiếm bao nhiêu.',
        'Dùng cho task định kỳ cố định tần số (lấy mẫu ADC, vòng điều khiển).',
    ],
    book='Output giống 4.4 (Hình 4.8); khác biệt chỉ thấy khi thời gian thực thi đáng kể.',
)
N['ex4_6'] = dict(
    title='4.6 Kết hợp task block và không block', win=(0, 10),
    goal='Hai task liên tục (ưu tiên 1, không block) và một task định kỳ 3 ms (ưu tiên 2, vTaskDelayUntil).',
    tasks=[('Task 1', 1, 'in liên tục'), ('Task 2', 1, 'in liên tục'), ('Task 3', 2, 'in, vTaskDelayUntil(3 ms)')],
    theory=[
        'Task 1 và Task 2 chia CPU bằng time slicing mỗi tick.',
        'Mỗi 3 tick, Task 3 hết Blocked; vì ưu tiên 2 > 1 nên nó <b>chiếm quyền (preempt)</b> ngay trong ngắt tick, in một dòng rồi Block lại.',
        'Sau khi Task 3 Block, scheduler quay lại task ưu tiên 1 kế tiếp.',
        'Idle không chạy: luôn có task ưu tiên 1 Ready.',
    ],
    book='Hình 4.10/4.11: dòng "Periodic task is running" chen vào giữa các dòng của hai task liên tục.',
    diff='Vòng lặp in giảm còn 0xfffff để output gọn.',
)
N['ex4_7'] = dict(
    title='4.7 Idle task hook', win=(0, 520),
    goal='Thêm vApplicationIdleHook() đếm số lần Idle chạy; hai task in giá trị đếm mỗi 250 ms.',
    tasks=[('Task 1', 1, 'in ulIdleCycleCount, vTaskDelay(250)'), ('Task 2', 2, 'như Task 1'), ('IDLE', 0, 'gọi hook liên tục')],
    theory=[
        'Idle task được tạo tự động trong vTaskStartScheduler(), ưu tiên 0, chạy khi không có task nào khác Ready.',
        'Idle hook được gọi mỗi vòng lặp của Idle task. Hook không được Block (Idle phải luôn Ready) và phải ngắn.',
        'Ứng dụng: đo tải CPU, đưa MCU vào chế độ tiết kiệm năng lượng (tickless idle).',
        'Giá trị đếm giữa hai lần in phụ thuộc tốc độ CPU (sách: khoảng 4 triệu trên PC Windows).',
    ],
    book='Hình 4.13: ulIdleCycleCount tăng mỗi chu kỳ 250 ms.',
)
N['ex4_8'] = dict(
    title='4.8 Đổi ưu tiên task', win=(0, 10),
    goal='Task 1 (p2) nâng Task 2 lên p3; Task 2 tự hạ mình xuống p1.',
    tasks=[('Task 1', 2, 'in, vTaskPrioritySet(Task 2, 3)'), ('Task 2', 1, 'in, vTaskPrioritySet(NULL, 1)')],
    theory=[
        'vTaskPrioritySet() có hiệu lực ngay: nếu task được nâng có ưu tiên cao hơn task đang chạy, kernel chuyển ngữ cảnh ngay trong lời gọi API.',
        'Chu trình: Task 1 chạy, nâng Task 2 lên 3, nên Task 2 chạy ngay. Task 2 hạ mình xuống 1 (nhỏ hơn 2), nên Task 1 chạy lại. Chu kỳ lặp lại.',
        'Không task nào Block, nên Idle không chạy. Vòng tròn trên timeline đánh dấu từng lần đổi ưu tiên.',
    ],
    book='Hình 4.15/4.16: output xen kẽ "About to raise/lower the Task 2 priority".',
    diff='Thêm busy loop ngắn sau mỗi lần in để timeline dễ nhìn.',
)
N['ex4_9'] = dict(
    title='4.9 Xoá task', win=(0, 210),
    goal='Task 1 (p1) tạo Task 2 (p2) mỗi 100 ms; Task 2 tự xoá.',
    tasks=[('Task 1', 1, 'in, xTaskCreate(Task 2, p2), vTaskDelay(100)'), ('Task 2', 2, 'in, vTaskDelete(NULL)')],
    theory=[
        'Task 2 ưu tiên cao hơn nên chạy ngay khi được tạo, chiếm quyền của Task 1 ngay trong xTaskCreate().',
        'vTaskDelete() tự xoá: TCB và stack không được giải phóng ngay; Idle task giải phóng chúng sau. Vì vậy Idle phải được chạy, và heap phải hỗ trợ free (không dùng heap_1).',
        'Sau đó Task 1 tiếp tục, vTaskDelay(100), cả hệ thống rảnh và Idle chạy (giải phóng bộ nhớ của Task 2).',
    ],
    book='Hình 4.17/4.18: chuỗi Task 1 → Task 2 → Task 1 → Idle lặp lại mỗi 100 ms.',
)
N['ex5_1'] = dict(
    title='5.1 Block khi nhận từ queue', win=(0, 4),
    goal='Hai sender (p1) gửi số vào queue 5 phần tử; receiver (p2) nhận với timeout 100 ms.',
    tasks=[('Sender1', 1, 'xQueueSendToBack(100), taskYIELD()'), ('Sender2', 1, 'xQueueSendToBack(200), taskYIELD()'), ('Receiver', 2, 'xQueueReceive(timeout 100 ms)')],
    theory=[
        'Queue là FIFO, dữ liệu được copy vào queue (by copy). Task gọi xQueueReceive() trên queue rỗng sẽ Block (tối đa xTicksToWait).',
        'Receiver ưu tiên cao hơn, nên ngay khi một sender gửi một phần tử, Receiver được unblock và <b>chiếm quyền ngay trong xQueueSend()</b>, lấy phần tử ra rồi Block lại.',
        'Hệ quả: queue không bao giờ chứa quá 1 phần tử (hàng xQueue chỉ lên 1 rồi về 0).',
        'taskYIELD() làm hai sender cùng ưu tiên luân phiên mà không cần chờ tick.',
    ],
    book='Hình 5.2/5.3: "Received = 100", "Received = 200" xen kẽ.',
    diff='Thêm vòng trễ ngắn trong sender để output gọn.',
)
N['ex5_2'] = dict(
    title='5.2 Block khi gửi vào queue đầy, gửi struct', win=(0, 4),
    goal='Ngược 5.1: hai sender (p2) gửi struct vào queue 3 phần tử, timeout 100 ms; receiver (p1).',
    tasks=[('Sender1', 2, 'xQueueSendToBack(struct, 100 ms)'), ('Sender2', 2, 'như Sender1'), ('Receiver', 1, 'xQueueReceive(0)')],
    theory=[
        'Hai sender ưu tiên cao gửi đến khi queue đầy (3/3) rồi Block "chờ gửi". Lúc đó mới đến lượt Receiver.',
        'Mỗi lần Receiver lấy ra một phần tử, một sender được unblock và <b>chiếm quyền ngay</b> để lấp chỗ trống, nên Receiver luôn thấy queue đầy.',
        'Khi nhiều task Block cùng chờ một queue, task ưu tiên cao nhất được đánh thức trước; nếu cùng ưu tiên, task chờ lâu nhất được đánh thức.',
        'Gửi struct cho phép bên nhận biết nguồn dữ liệu (eDataSource).',
    ],
    book='Hình 5.5/5.6: "From Sender 1 = 100" và "From Sender 2 = 200" theo từng cặp.',
    diff='Thêm vòng trễ ngắn trong receiver để output gọn.',
)
N['ex5_3'] = dict(
    title='5.3 Queue set', win=(0, 420),
    goal='Receiver (p2) chờ đồng thời hai queue qua một queue set; Sender1 gửi mỗi 100 ms, Sender2 mỗi 200 ms.',
    tasks=[('Sender1', 1, 'vTaskDelay(100), xQueueSend(xQueue1)'), ('Sender2', 1, 'vTaskDelay(200), xQueueSend(xQueue2)'), ('Receiver', 2, 'xQueueSelectFromSet(), xQueueReceive(0)')],
    theory=[
        'Queue set cho phép một task Block trên nhiều queue/semaphore cùng lúc. Khi một thành viên nhận dữ liệu, handle của nó được gửi vào queue set.',
        'xQueueSelectFromSet() trả về handle của queue có dữ liệu; sau đó đọc queue đó với timeout 0.',
        'Kích thước set phải ≥ tổng độ dài các queue thành viên (1 + 1 = 2).',
        'Sách khuyên tránh queue set nếu có thể (dùng một queue với struct có trường nguồn) vì tốn RAM và phức tạp hơn.',
    ],
    book='Hình 5.11: mỗi 200 ms có 2 thông điệp của Sender1 và 1 của Sender2.',
)
N['ex6_1'] = dict(
    title='6.1 Timer one-shot và auto-reload', win=(0, 3500),
    goal='Timer one-shot 3333 ms và timer auto-reload 500 ms, mỗi timer một callback.',
    tasks=[('Tmr Svc', 6, 'daemon task: xử lý lệnh timer, chạy callback')],
    theory=[
        'Software timer không chạy trong ngắt. Callback chạy trong ngữ cảnh <b>daemon task</b> (Tmr Svc, configTIMER_TASK_PRIORITY).',
        'xTimerStart() gửi lệnh qua <b>timer command queue</b>. Gọi trước vTaskStartScheduler() thì lệnh nằm chờ trong queue đến khi daemon chạy.',
        'Auto-reload tự khởi động lại sau mỗi lần hết hạn: 500, 1000, 1500… One-shot chỉ hết hạn một lần (3333).',
        'Callback không được Block (nó chặn mọi timer khác).',
    ],
    book='Hình 6.6: callback auto-reload ở 500, 1000, 1500, …; one-shot ở 3333.',
)
N['ex6_2'] = dict(
    title='6.2 Tham số callback và timer ID', win=(0, 3500),
    goal='Một callback chung cho cả hai timer; timer ID làm bộ đếm; auto-reload dừng sau 5 lần.',
    tasks=[('Tmr Svc', 6, 'chạy prvTimerCallback()')],
    theory=[
        'Tham số xTimer của callback cho biết timer nào hết hạn, nên một callback dùng chung được.',
        'Timer ID (pvTimerGetTimerID/vTimerSetTimerID) là vùng lưu riêng của từng timer, dùng để đếm số lần chạy.',
        'xTimerStop() gọi trong callback phải dùng block time 0, vì callback chạy trong daemon task, task duy nhất đọc command queue; block ở đây sẽ deadlock.',
    ],
    book='Hình 6.7: auto-reload chạy 5 lần (500…2500) rồi dừng; one-shot ở 3333.',
)
N['ex6_3'] = dict(
    title='6.3 Reset software timer', win=(0, 9000),
    goal='Mô phỏng đèn nền: mỗi lần nhấn phím bật đèn và reset timer 5 s; timer hết hạn thì tắt đèn.',
    tasks=[('Key poll', 1, 'mỗi 50 ms đọc phím, xTimerReset()'), ('Tmr Svc', 6, 'callback tắt đèn')],
    theory=[
        'xTimerReset() khởi động lại timer: mốc hết hạn mới = thời điểm reset + chu kỳ. Nếu timer đang chạy, mốc cũ bị huỷ.',
        'Phím nhấn ở 1000, 3000, 3500 ms, nên timer hết hạn ở 3500 + 5000 = 8500 ms (không phải 6000 hay 8000).',
        'Mẫu "watchdog phần mềm/timeout không hoạt động": mỗi sự kiện đẩy lùi deadline.',
    ],
    book='Hình 6.10: đèn tắt 5 s sau lần nhấn phím cuối.',
    diff='_kbhit() của Windows được thay bằng phím giả lập (GPIO KEY) nhấn ở 1000, 3000, 3500 ms.',
)
N['ex7_1'] = dict(
    title='7.1 Đồng bộ task với ngắt bằng binary semaphore', win='irq',
    goal='Periodic (p1) tạo ngắt mỗi 500 ms; ISR "give" binary semaphore; Handler (p3) "take" rồi xử lý.',
    tasks=[('Handler', 3, 'xSemaphoreTake(portMAX_DELAY), xử lý'), ('Periodic', 1, 'vTaskDelay(500), tạo ngắt'), ('ISR IRQ3', '–', 'xSemaphoreGiveFromISR, portYIELD_FROM_ISR')],
    theory=[
        '<b>Deferred interrupt processing</b>: ISR làm tối thiểu (give semaphore), phần xử lý chính dời sang task. ISR ngắn, nên độ trễ ngắt của hệ thống thấp.',
        'xSemaphoreGiveFromISR() unblock Handler (p3 > p1) và đặt xHigherPriorityTaskWoken = pdTRUE.',
        'portYIELD_FROM_ISR(pdTRUE) yêu cầu chuyển ngữ cảnh khi ISR kết thúc: CPU quay về <b>Handler</b>, không về Periodic. Nhờ vậy Handler chạy liền sau ngắt, như thể chạy trong ISR.',
        'Không gọi portYIELD_FROM_ISR thì Handler chỉ chạy ở lần chuyển ngữ cảnh kế tiếp (tick hoặc khi Periodic Block).',
    ],
    book='Hình 7.4/7.5: "About to generate" → "Handler task - Processing event" → "Interrupt generated".',
)
N['ex7_2'] = dict(
    title='7.2 Counting semaphore', win='irq',
    goal='Như 7.1 nhưng dùng counting semaphore (max 10) và ISR give 3 lần.',
    tasks=[('Handler', 3, 'take, xử lý, lặp'), ('Periodic', 1, 'tạo ngắt'), ('ISR IRQ3', '–', 'give ×3')],
    theory=[
        'Binary semaphore chỉ chốt được 1 sự kiện: nhiều ngắt đến khi task chưa xử lý thì bị mất. Counting semaphore <b>đếm</b> sự kiện (0…uxMaxCount).',
        'ISR give 3 lần, count = 3. Handler take 3 lần liên tiếp không Block, xử lý 3 sự kiện, rồi Block khi count = 0.',
        'Hàng xCountingSemaphore trên timeline: 0 → 3 → 2 → 1 → 0.',
    ],
    book='Hình 7.8: ba dòng "Handler task - Processing event" cho mỗi ngắt.',
)
N['ex7_3'] = dict(
    title='7.3 Xử lý ngắt tập trung (daemon task)', win='irq',
    goal='ISR gọi xTimerPendFunctionCallFromISR() để daemon task chạy vDeferredHandlingFunction(); không tạo task riêng.',
    tasks=[('Periodic', 5, 'configTIMER_TASK_PRIORITY − 1; tạo ngắt'), ('Tmr Svc', 6, 'chạy hàm được pend'), ('ISR IRQ3', '–', 'pend function call')],
    theory=[
        'xTimerPendFunctionCallFromISR() gửi lệnh "chạy hàm" vào timer command queue; daemon task thực thi hàm đó.',
        'Ưu điểm: không tốn RAM cho một task riêng mỗi ngắt. Nhược điểm: mọi hàm deferred chạy cùng một ưu tiên (của daemon) và tuần tự.',
        'Daemon (p6) cao hơn Periodic (p5), nên hàm chạy ngay sau ISR, trước dòng "Interrupt generated".',
    ],
    book='Hình 7.10/7.11: "Handler function - Processing event 0, 1, 2…".',
)
N['ex7_4'] = dict(
    title='7.4 Gửi và nhận queue trong ISR', win='irq',
    goal='IntGen (p1) gửi 5 số vào xIntegerQueue rồi tạo ngắt; ISR chuyển mỗi số thành chuỗi vào xStringQueue; String (p2) in.',
    tasks=[('String', 2, 'xQueueReceive(xStringQueue), in'), ('IntGen', 1, 'mỗi 200 ms: 5 số, tạo ngắt'), ('ISR IRQ3', '–', 'ReceiveFromISR → SendToBackFromISR')],
    theory=[
        'ISR dùng xQueueReceiveFromISR/xQueueSendToBackFromISR: không có tham số chờ, vì ISR không được Block.',
        'ISR rút hết 5 số (xIntegerQueue 5 → 0) và đẩy 5 chuỗi (xStringQueue 0 → 5). String được unblock; portYIELD_FROM_ISR đưa nó chạy ngay, in 5 chuỗi rồi Block.',
        'Lưu ý của sách: queue trong ISR tiện nhưng kém hiệu quả nếu dữ liệu đến dày (mỗi byte một lần gọi); dùng DMA, stream buffer hoặc buffer vòng cho UART.',
    ],
    book='Hình 7.12/7.13: "String 0..3" in giữa "About to generate" và "Interrupt generated".',
)
N['ex8_1'] = dict(
    title='8.1 vPrintString() dùng mutex', win=(0, 60),
    goal='Hai task (p1, p2) in chuỗi dài qua prvNewPrintString(), được bảo vệ bằng mutex; delay ngẫu nhiên 0–31 tick.',
    tasks=[('Print1', 1, 'take mutex, printf, give, delay ngẫu nhiên'), ('Print2', 2, 'như Print1')],
    theory=[
        'stdout là tài nguyên dùng chung. Không có loại trừ tương hỗ (mutual exclusion) thì hai task có thể in xen ký tự của nhau.',
        'Mutex là binary semaphore có <b>priority inheritance</b>: khi Print2 (p2) chờ mutex do Print1 (p1) giữ, Print1 được tạm nâng lên p2 để trả mutex sớm, giảm thời gian priority inversion.',
        'Task phải take và give mutex trong cùng một task; không dùng mutex trong ISR.',
        'Hàng xMutex trên timeline: màu = task đang giữ.',
    ],
    book='Hình 8.6/8.7: các dòng "Task 1 ****" và "Task 2 ----" không bị trộn.',
    diff='Thêm busy loop sau printf để mô phỏng in chậm qua UART, giúp thấy mutex bị giữ.',
)
N['ex8_2'] = dict(
    title='8.2 Gatekeeper task', win=(0, 60),
    goal='Chỉ Gatekeeper (p0) được gọi printf; các task và tick hook gửi con trỏ chuỗi vào xPrintQueue.',
    tasks=[('Print1', 1, 'xQueueSendToBack, delay ngẫu nhiên'), ('Print2', 2, 'như Print1'), ('Gatekeeper', 0, 'xQueueReceive, printf'), ('Tick hook', '–', 'mỗi 200 tick: SendToFrontFromISR')],
    theory=[
        'Gatekeeper là task duy nhất sở hữu tài nguyên; mọi truy cập đi qua queue. Không có mutex, nên không có priority inversion hay deadlock.',
        'Gatekeeper ưu tiên 0 (bằng Idle): chỉ in khi các task khác rảnh. Output bị trì hoãn nhưng các task không bị chặn.',
        'Tick hook chạy trong ngắt tick, nên phải dùng ...FromISR. SendToFront đưa thông điệp của ngắt lên đầu hàng.',
    ],
    book='Hình 8.8: thông điệp tick hook xuất hiện mỗi 200 tick giữa các dòng của task.',
)
N['ex9_1'] = dict(
    title='9.1 Thử nghiệm event group', win='irq',
    goal='Bit Setter (p1) set bit 0, 1 xen kẽ mỗi 200 ms; ISR set bit 2 mỗi 500 ms; Bit Reader (p2) chờ bất kỳ bit nào (xWaitForAllBits = pdFALSE), xoá khi thoát.',
    tasks=[('Int Gen', 3, 'mỗi 500 ms tạo ngắt'), ('Bit Reader', 2, 'xEventGroupWaitBits(0x7, clear, any)'), ('Bit Setter', 1, 'set bit 0 / bit 1'), ('Tmr Svc', 6, 'thực hiện set bit từ ISR')],
    theory=[
        'Event group là tập bit; một task có thể chờ một tổ hợp bit (OR: xWaitForAllBits = pdFALSE, AND: pdTRUE).',
        'xClearOnExit = pdTRUE: các bit làm task thoát chờ được xoá nguyên tử khi thoát.',
        'xEventGroupSetBitsFromISR() không set bit ngay trong ISR (thao tác không tất định, vì có thể unblock nhiều task). Nó <b>pend</b> việc set sang daemon task, nên Tmr Svc chạy ngay sau ISR.',
        'Với pdTRUE (AND), Reader chỉ thoát khi đủ bit 0, 1 và 2.',
    ],
    book='Hình 9.2: mỗi "about to set bit n" được theo ngay bởi "Event bit n was set".',
)
N['ex9_2'] = dict(
    title='9.2 Đồng bộ task (xEventGroupSync)', win=(0, 9000),
    goal='Ba task cùng ưu tiên delay ngẫu nhiên 200–4200 ms, rồi gặp nhau tại điểm đồng bộ.',
    tasks=[('Task 1', 1, 'delay ngẫu nhiên, Sync(bit 0, chờ 0x7)'), ('Task 2', 1, 'Sync(bit 1)'), ('Task 3', 1, 'Sync(bit 2)')],
    theory=[
        'xEventGroupSync() set bit của mình và chờ đủ tất cả bit một cách <b>nguyên tử</b> (rendezvous/barrier).',
        'Task đến sớm Block; task đến cuối cùng set bit cuối, và cả ba được unblock cùng lúc; các bit tự xoá.',
        'Không dùng được xEventGroupSetBits() + xEventGroupWaitBits() riêng, vì giữa hai lời gọi có thể bị chiếm quyền và các bit đã bị xoá.',
    ],
    book='Hình 9.3: ba dòng "reached sync point" rồi ba dòng "exited sync point".',
)
N['ex10_1'] = dict(
    title='10.1 Task notification thay semaphore (cách 1)', win='irq',
    goal='Như 7.1, thay semaphore bằng vTaskNotifyGiveFromISR() / ulTaskNotifyTake(pdTRUE, …).',
    tasks=[('Handler', 3, 'ulTaskNotifyTake(clear = pdTRUE), xử lý hết'), ('Periodic', 1, 'tạo ngắt mỗi 500 ms'), ('ISR IRQ3', '–', 'vTaskNotifyGiveFromISR')],
    theory=[
        'Mỗi task có sẵn một notification value 32 bit. Gửi notification không cần tạo đối tượng kernel, nên không tốn RAM cho Queue_t. Theo sách, nhanh hơn semaphore khoảng 45 % và nhỏ hơn.',
        'Give = tăng notification value; ulTaskNotifyTake(pdTRUE) trả về giá trị đếm rồi <b>xoá về 0</b>, nên task phải xử lý tất cả sự kiện đã chốt.',
        'Giới hạn: chỉ một task nhận, ISR không thể là bên nhận, bên gửi không Block được.',
    ],
    book='Hình 10.3: giống 7.1.',
)
N['ex10_2'] = dict(
    title='10.2 Task notification thay semaphore (cách 2)', win='irq',
    goal='ISR give 3 lần; Handler dùng ulTaskNotifyTake(pdFALSE, …).',
    tasks=[('Handler', 3, 'ulTaskNotifyTake(clear = pdFALSE), xử lý 1'), ('Periodic', 1, 'tạo ngắt'), ('ISR IRQ3', '–', 'give ×3')],
    theory=[
        'xClearCountOnExit = pdFALSE: mỗi lần take chỉ <b>giảm 1</b>, giống counting semaphore. Handler gọi take 3 lần, mỗi lần xử lý 1 sự kiện.',
        'So với 10.1: cùng kết quả, khác cách tiêu thụ (xử lý hàng loạt so với từng cái một).',
    ],
    book='Hình 10.4: ba dòng "Processing event" cho mỗi ngắt, giống 7.2.',
)

N['ex6_1']['kinds'] = ['obj']
N['ex6_2']['kinds'] = ['obj']
N['ex6_3']['kinds'] = ['obj', 'gpio']
N['ex5_3']['kinds'] = ['obj']
N['ex9_2']['kinds'] = ['obj', 'block', 'ready']
