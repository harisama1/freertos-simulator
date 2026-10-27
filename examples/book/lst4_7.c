/* Listing 4.7 Tạo task từ bên trong một task khác sau khi scheduler đã chạy.
 *
 * Giống Example 4.1, nhưng main() chỉ tạo Task 1; Task 1 tạo Task 2 trước
 * khi vào vòng lặp vô hạn.
 * Quan sát trên Timeline: hàng Task 2 chỉ xuất hiện sau khi Task 1 chạy lần
 * đầu; từ đó hai task cùng ưu tiên 1 chia CPU theo time slice như 4.1.
 */
void vTask2( void * pvParameters );

void vTask1( void * pvParameters )
{
    const char * pcTaskName = "Task 1 is running\r\n";
    volatile uint32_t ul;

    xTaskCreate( vTask2, "Task 2", 1000, NULL, 1, NULL );

    for( ;; )
    {
        vPrintLine( pcTaskName );
        for( ul = 0; ul < mainDELAY_LOOP_COUNT; ul++ ) { }
    }
}

void vTask2( void * pvParameters )
{
    const char * pcTaskName = "Task 2 is running\r\n";
    volatile uint32_t ul;

    for( ;; )
    {
        vPrintLine( pcTaskName );
        for( ul = 0; ul < mainDELAY_LOOP_COUNT; ul++ ) { }
    }
}

int main( void )
{
    xTaskCreate( vTask1, "Task 1", 1000, NULL, 1, NULL );
    vTaskStartScheduler();
    for( ;; );
}
