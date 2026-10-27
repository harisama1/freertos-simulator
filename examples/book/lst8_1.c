/* Listing 8.1 Read-modify-write không được bảo vệ: mất cập nhật biến chung.
 *
 * Listing 8.1 (PORTA |= 0x01) gồm 3 bước đọc - sửa - ghi. Nếu tick ngắt
 * giữa bước đọc và ghi, task kia chạy và ghi trước; khi task này quay lại,
 * nó ghi đè giá trị cũ + 1 -> một lần tăng của task kia bị mất.
 * Kết quả: "mất N lần tăng" rất lớn: task bị ngắt giữa đọc và ghi giữ giá
 * trị cũ suốt time slice của task kia, nên mỗi lần chuyển task làm mất cả
 * trăm lần tăng của task kia. Sửa: lst8_4.c, lst8_9.c.
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
        ulTmp = ulShared;                       /* read   */
        for( ul = 0; ul < 2000; ul++ ) { }      /* modify (chậm) */
        ulShared = ulTmp + 1;                   /* write  */
        ulDone[ i ]++;
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
