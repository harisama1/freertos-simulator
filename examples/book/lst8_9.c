/* Listing 8.7-8.9 Tạm dừng scheduler (vTaskSuspendAll) bảo vệ read-modify-write.
 *
 * Cùng chương trình với lst8_1.c, nhưng dùng vTaskSuspendAll()/
 * xTaskResumeAll(): ngắt vẫn chạy (tick vẫn được đếm) nhưng không chuyển
 * task; việc chuyển task bị dồn lại tới xTaskResumeAll().
 * Kết quả: "mất 0 lần tăng". Listing 8.9 dùng đúng cơ chế này cho
 * vPrintString() để printf() của các task không lẫn vào nhau.
 * Hai task cùng ưu tiên 1 (time slicing) cùng tăng biến chung ulShared theo
 * kiểu read-modify-write (đọc -> tính -> ghi); vòng lặp rỗng giữa đọc và
 * ghi làm cửa sổ tranh chấp rộng ra để dễ thấy. Mỗi task đếm riêng số lần
 * nó đã tăng; task Monitor (p2) mỗi 1000 ms so sánh tổng với ulShared.
 */
static volatile uint32_t ulShared;
static volatile uint32_t ulDone[ 2 ];

static void vIncrementTask( void * pv )
{
    const uint32_t i = ( uint32_t ) ( uintptr_t ) pv;
    volatile uint32_t ul;
    uint32_t ulTmp;

    for( ;; )
    {
        vTaskSuspendAll();
        ulTmp = ulShared;                       /* read   */
        for( ul = 0; ul < 2000; ul++ ) { }      /* modify (chậm) */
        ulShared = ulTmp + 1;                   /* write  */
        ulDone[ i ]++;
        xTaskResumeAll();
    }
}

static void vMonitor( void * pv )
{
    for( ;; )
    {
        vTaskDelay( pdMS_TO_TICKS( 1000 ) );
        printf( "[%5lu ms] Task1 %lu + Task2 %lu = %lu, ulShared = %lu -> mất %ld lần tăng\n",
                ( unsigned long ) xTaskGetTickCount(), ( unsigned long ) ulDone[ 0 ], ( unsigned long ) ulDone[ 1 ],
                ( unsigned long ) ( ulDone[ 0 ] + ulDone[ 1 ] ), ( unsigned long ) ulShared,
                ( long ) ( ulDone[ 0 ] + ulDone[ 1 ] ) - ( long ) ulShared );
    }
}

int main( void )
{
    xTaskCreate( vIncrementTask, "Task1", 1000, ( void * ) 0, 1, NULL );
    xTaskCreate( vIncrementTask, "Task2", 1000, ( void * ) 1, 1, NULL );
    xTaskCreate( vMonitor, "Monitor", 1000, NULL, 2, NULL );
    vTaskStartScheduler();
    for( ;; );
}
