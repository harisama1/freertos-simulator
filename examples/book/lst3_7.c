/* Listing 3.7-3.11 Theo dõi heap: xPortGetFreeHeapSize(), xPortGetMinimumEverFreeHeapSize(), vPortGetHeapStats(), malloc failed hook.
 *
 * Task Alloc lần lượt cấp phát rồi giải phóng các khối có kích thước khác
 * nhau và in trạng thái heap sau mỗi bước:
 *   free now     xPortGetFreeHeapSize()           byte còn trống hiện tại
 *   min ever     xPortGetMinimumEverFreeHeapSize() mức trống thấp nhất từng có
 *   largest      vPortGetHeapStats()              khối liền lớn nhất (phân mảnh)
 * Cuối cùng xin một khối lớn hơn cả heap: pvPortMalloc() trả NULL và gọi
 * vApplicationMallocFailedHook() (Listing 3.11; cần configUSE_MALLOC_FAILED_HOOK = 1).
 * Chỉ heap_4 và heap_5 có vPortGetHeapStats() (sách viết nhầm thành vPortGetHeapStatus); chọn heap_2 sẽ báo lỗi biên dịch.
 * Quan sát: tab Thống kê > Heap; "largest" nhỏ hơn "free now" khi có lỗ
 * hổng (phân mảnh) sau khi giải phóng khối B ở giữa. Gợi ý "Hết heap" ở
 * tab Kết quả là kết quả mong đợi của bước cuối.
 */
void vApplicationMallocFailedHook( void )
{
    printf( "[%4lu ms] vApplicationMallocFailedHook(): pvPortMalloc() trả NULL\n", ( unsigned long ) xTaskGetTickCount() );
}

static void prvShow( const char * pcStep )
{
    HeapStats_t xStats;

    vPortGetHeapStats( &xStats );
    printf( "free now %7u  min ever %7u  largest %7u  blocks free %u  <- %s\n",
            ( unsigned ) xPortGetFreeHeapSize(), ( unsigned ) xPortGetMinimumEverFreeHeapSize(),
            ( unsigned ) xStats.xSizeOfLargestFreeBlockInBytes, ( unsigned ) xStats.xNumberOfFreeBlocks, pcStep );
}

static void vAlloc( void * pv )
{
    void * pvA, * pvB, * pvC, * pvBig;

    prvShow( "ban đầu" );
    pvA = pvPortMalloc( 100000 );
    prvShow( "malloc A 100000" );
    pvB = pvPortMalloc( 200000 );
    prvShow( "malloc B 200000" );
    pvC = pvPortMalloc( 100000 );
    prvShow( "malloc C 100000" );
    vPortFree( pvB );
    prvShow( "free B (lỗ ở giữa)" );
    vPortFree( pvA );
    prvShow( "free A (gộp với lỗ B)" );
    vPortFree( pvC );
    prvShow( "free C" );
    pvBig = pvPortMalloc( 2 * configTOTAL_HEAP_SIZE );
    prvShow( pvBig == NULL ? "malloc 2x heap -> NULL" : "malloc 2x heap" );
    vTaskDelete( NULL );
}

int main( void )
{
    xTaskCreate( vAlloc, "Alloc", 1000, NULL, 1, NULL );
    vTaskStartScheduler();
    for( ;; );
}
