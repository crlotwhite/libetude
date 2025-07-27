#ifndef LIBETUDE_TASK_SCHEDULER_H
#define LIBETUDE_TASK_SCHEDULER_H

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

// 작업 우선순위 정의
typedef enum {
    ET_TASK_PRIORITY_LOW = 0,
    ET_TASK_PRIORITY_NORMAL = 1,
    ET_TASK_PRIORITY_HIGH = 2,
    ET_TASK_PRIORITY_REALTIME = 3
} ETTaskPriority;

// 작업 상태 정의
typedef enum {
    ET_TASK_STATUS_PENDING = 0,
    ET_TASK_STATUS_RUNNING = 1,
    ET_TASK_STATUS_COMPLETED = 2,
    ET_TASK_STATUS_CANCELLED = 3
} ETTaskStatus;

// 작업 구조체
typedef struct {
    uint32_t task_id;                    // 작업 고유 ID
    ETTaskPriority priority;             // 작업 우선순위
    void (*task_func)(void*);            // 작업 함수
    void* task_data;                     // 작업 데이터
    uint64_t deadline;                   // 데드라인 (실시간용, 마이크로초)
    uint64_t submit_time;                // 제출 시간
    ETTaskStatus status;                 // 작업 상태

    // 콜백 함수들
    void (*completion_callback)(uint32_t task_id, void* user_data);
    void* callback_user_data;

    // 내부 사용
    struct ETTask* next;                 // 큐에서 다음 작업
} ETTask;

// 작업 큐 구조체
typedef struct {
    ETTask* head;                        // 큐 헤드
    ETTask* tail;                        // 큐 테일
    uint32_t count;                      // 큐 내 작업 수
    pthread_mutex_t mutex;               // 큐 뮤텍스
    pthread_cond_t condition;            // 조건 변수
} ETTaskQueue;

// 워커 스레드 구조체
typedef struct {
    pthread_t thread;                    // 스레드 핸들
    uint32_t worker_id;                  // 워커 ID
    bool active;                         // 활성 상태
    bool should_exit;                    // 종료 플래그
    struct ETTaskScheduler* scheduler;   // 스케줄러 참조
} ETWorkerThread;

// 스케줄러 구조체
typedef struct ETTaskScheduler {
    // 작업 큐들 (우선순위별)
    ETTaskQueue queues[4];               // 우선순위별 큐 (LOW, NORMAL, HIGH, REALTIME)

    // 워커 스레드 풀
    ETWorkerThread* workers;             // 워커 스레드 배열
    uint32_t num_workers;                // 워커 스레드 수

    // 실시간 스케줄링
    bool realtime_mode;                  // 실시간 모드 활성화
    uint64_t audio_buffer_deadline;      // 오디오 버퍼 데드라인 (마이크로초)

    // 통계 및 모니터링
    uint64_t total_tasks_submitted;      // 제출된 총 작업 수
    uint64_t total_tasks_completed;      // 완료된 총 작업 수
    uint64_t total_tasks_cancelled;      // 취소된 총 작업 수

    // 동기화
    pthread_mutex_t scheduler_mutex;     // 스케줄러 뮤텍스
    bool shutdown;                       // 종료 플래그

    // 작업 ID 생성
    uint32_t next_task_id;               // 다음 작업 ID
    pthread_mutex_t id_mutex;            // ID 생성 뮤텍스
} ETTaskScheduler;

// 스케줄러 생성 및 해제
ETTaskScheduler* et_create_task_scheduler(uint32_t num_workers);
void et_destroy_task_scheduler(ETTaskScheduler* scheduler);

// 작업 제출 및 관리
uint32_t et_submit_task(ETTaskScheduler* scheduler,
                        ETTaskPriority priority,
                        void (*task_func)(void*),
                        void* task_data,
                        uint64_t deadline_us);

uint32_t et_submit_task_with_callback(ETTaskScheduler* scheduler,
                                      ETTaskPriority priority,
                                      void (*task_func)(void*),
                                      void* task_data,
                                      uint64_t deadline_us,
                                      void (*completion_callback)(uint32_t, void*),
                                      void* callback_user_data);

bool et_cancel_task(ETTaskScheduler* scheduler, uint32_t task_id);
ETTaskStatus et_get_task_status(ETTaskScheduler* scheduler, uint32_t task_id);

// 실시간 스케줄링 제어
void et_set_realtime_mode(ETTaskScheduler* scheduler, bool enable);
void et_set_audio_buffer_deadline(ETTaskScheduler* scheduler, uint64_t deadline_us);

// 스케줄러 통계
typedef struct {
    uint64_t total_submitted;
    uint64_t total_completed;
    uint64_t total_cancelled;
    uint32_t pending_tasks;
    uint32_t active_workers;
    double avg_task_completion_time_us;
    double avg_queue_wait_time_us;
} ETSchedulerStats;

void et_get_scheduler_stats(ETTaskScheduler* scheduler, ETSchedulerStats* stats);

// 워커 스레드 제어
void et_pause_scheduler(ETTaskScheduler* scheduler);
void et_resume_scheduler(ETTaskScheduler* scheduler);

#ifdef __cplusplus
}
#endif

#endif // LIBETUDE_TASK_SCHEDULER_H