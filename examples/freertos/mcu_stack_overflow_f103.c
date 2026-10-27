/*
 * MCU: stack và RAM trên STM32F103C8 (Cortex-M3, 20 KB RAM).
 *   - mỗi task được cấp 128 word = 512 B stack;
 *   - task Format dùng mảng cục bộ 600 B + printf -> vượt 512 B.
 *
 * frsim: mcu=stm32f103c8
 *
 * Quan sát:
 *  - tab Thống kê, mục "Stack": Format = TRÀN, Blink = OK;
 *  - mục "RAM": heap dùng thực tế + .bss + main stack so với 20 KB;
 *  - tab Kết quả: gợi ý tăng stack depth.
 * Lưu ý: trên PC task chạy trên stack của pthread nên chương trình vẫn chạy;
 * trên chip thật, tràn stack ghi đè TCB/heap kề bên -> HardFault hoặc lỗi
 * ngẫu nhiên. Đây là ước lượng tĩnh (gcc -fstack-usage), không phải đo.
 * Thử:
 *  1. tăng stack của Format lên 320 word -> hết cảnh báo;
 *  2. đổi mcu=stm32f030f4 (4 KB RAM): mục "RAM" cho thấy chương trình cần
 *     nhiều RAM hơn chip có (mô phỏng vẫn chạy, chip thật thì không).
 */
#define PIN_LED    4

static void vBlink( void * pv )
{
    for( ;; )
    {
        sim_gpio_toggle( PIN_LED );
        vTaskDelay( pdMS_TO_TICKS( 50 ) );
    }
}

static void vFormat( void * pv )
{
    char report[ 600 ];
    uint32_t n = 0;

    for( ;; )
    {
        snprintf( report, sizeof( report ), "report #%lu: free heap %u B", ( unsigned long ) n++,
                  ( unsigned ) xPortGetFreeHeapSize() );
        printf( "%s\n", report );
        vTaskDelay( pdMS_TO_TICKS( 100 ) );
    }
}

int main( void )
{
    sim_gpio_name( PIN_LED, "LED PA4" );
    if( xTaskCreate( vBlink, "Blink", 128, NULL, 1, NULL ) != pdPASS )
        printf( "Blink: không đủ heap\n" );
    if( xTaskCreate( vFormat, "Format", 128, NULL, 2, NULL ) != pdPASS )
        printf( "Format: không đủ heap\n" );
    printf( "heap còn trống trước scheduler: %u B\n", ( unsigned ) xPortGetFreeHeapSize() );
    vTaskStartScheduler();
    printf( "vTaskStartScheduler() trả về: không đủ heap cho Idle/Timer task\n" );
    for( ;; );
}
