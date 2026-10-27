/*
 * Scheduler hợp tác (cooperative) kiểu bare-metal: bảng task + tick, không RTOS,
 * giống vòng lặp chính trên STM32. Mỗi task chạy đến hết rồi mới nhường CPU.
 *
 * Thời gian ảo: work(n) "tiêu" n tick CPU. Không cần phần cứng.
 * Timeline: st_task() tạo hàng, st_run()/st_block()/st_idle() đổi trạng thái.
 * Thử: tăng thời gian của task_comm lên 6 tick và xem task_led trễ hạn.
 */

#define NTASK 3

typedef struct
{
    const char * name;
    void ( * fn )( void );
    uint32_t period;      /* tick */
    uint32_t next;        /* next release time */
    uint32_t release;     /* release time of the current job */
    int pending;
    int id;               /* timeline row */
} task_t;

static uint64_t now;                  /* virtual tick counter */

static void tick( void )
{
    now++;
    st_time( now );
    st_tick();
}

static void work( unsigned n )        /* busy for n ticks */
{
    while( n-- )
        tick();
}

static void task_led( void )    { work( 1 ); }
static void task_sensor( void ) { work( 3 ); }
static void task_comm( void )   { work( 4 ); }

/* Table order = priority: the first pending task wins at each decision point. */
static task_t tasks[ NTASK ] =
{
    { "LED (5)",     task_led,    5,  0 },
    { "Sensor (10)", task_sensor, 10, 0 },
    { "Comm (20)",   task_comm,   20, 0 },
};

int main( void )
{
    int i;

    st_time( 0 );
    for( i = 0; i < NTASK; i++ )
    {
        tasks[ i ].id = st_task( tasks[ i ].name, NTASK - i );
        st_block( tasks[ i ].id );
    }

    while( now < 60 )
    {
        /* release jobs that are due */
        for( i = 0; i < NTASK; i++ )
        {
            if( now >= tasks[ i ].next && !tasks[ i ].pending )
            {
                tasks[ i ].pending = 1;
                tasks[ i ].release = tasks[ i ].next;
                tasks[ i ].next += tasks[ i ].period;
                st_ready( tasks[ i ].id );
            }
        }

        /* pick the first pending task in table order */
        for( i = 0; i < NTASK && !tasks[ i ].pending; i++ )
        {
        }

        if( i == NTASK )
        {
            st_idle();
            tick();
            continue;
        }

        st_run( tasks[ i ].id );
        tasks[ i ].fn();                      /* runs to completion: no preemption */
        tasks[ i ].pending = 0;
        st_block( tasks[ i ].id );

        if( now - tasks[ i ].release > tasks[ i ].period )
        {
            printf( "t=%3llu  %s finished late (latency %llu > period %u)\n",
                    ( unsigned long long ) now, tasks[ i ].name,
                    ( unsigned long long ) ( now - tasks[ i ].release ), tasks[ i ].period );
            st_mark( "late" );
        }
    }

    printf( "done at t=%llu\n", ( unsigned long long ) now );
    return 0;
}
