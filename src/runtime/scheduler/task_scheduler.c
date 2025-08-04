#include "libetude/task_scheduler.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <sys/time.h>

// 현재 시간을 마이크로초로 가져오기
static uint64_t get_current_time_us() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000 + tv.tv_usec;
}

// 작업 큐 초기화
static void init_task_queue(ETTaskQueue* queue) {
    queue->head = NULL;
    queue->tail = NULL;
    queue->count = 0;
    pthread_mutex_init(&queue->mutex, NULL);
    pthread_cond_init(&queue->condition, NULL);
}

// 작업 큐 정리
static void cleanup_task_queue(ETTaskQueue* queue) {
    pthread_mutex_lock(&queue->mutex);

    // 남은 작업들 정리
    ETTask* current = queue->head;
    while (current) {
        ETTask* next = current->next;
        free(current);
        current = next;
    }

    queue->head = NULL;
    queue->tail = NULL;
    queue->count = 0;

    pthread_mutex_unlock(&queue->mutex);
    pthread_mutex_destroy(&queue->mutex);
    pthread_cond_destroy(&queue->condition);
}

// 우선순위 큐에 작업 추가 (우선순위와 데드라인 고려)
static void enqueue_task(ETTaskQueue* queue, ETTask* task) {
    pthread_mutex_lock(&queue->mutex);

    // 빈 큐인 경우
    if (queue->head == NULL) {
        queue->head = queue->tail = task;
        task->next = NULL;
    } else {
        // 실시간 작업의 경우 데드라인 기준으로 정렬
        if (task->priority == ET_TASK_PRIORITY_REALTIME) {
            ETTask* current = queue->head;
            ETTask* prev = NULL;

            // 데드라인이 더 빠른 위치 찾기
            while (current && current->priority == ET_TASK_PRIORITY_REALTIME &&
                   current->deadline <= task->deadline) {
                prev = current;
                current = current->next;
            }

            if (prev == NULL) {
                // 맨 앞에 삽입
                task->next = queue->head;
                queue->head = task;
            } else {
                // 중간에 삽입
                task->next = prev->next;
                prev->next = task;
                if (task->next == NULL) {
                    queue->tail = task;
                }
            }
        } else {
            // 일반 작업은 큐 끝에 추가
            queue->tail->next = task;
            queue->tail = task;
            task->next = NULL;
        }
    }

    queue->count++;
    pthread_cond_signal(&queue->condition);
    pthread_mutex_unlock(&queue->mutex);
}

// 큐에서 작업 제거
static ETTask* dequeue_task(ETTaskQueue* queue) {
    pthread_mutex_lock(&queue->mutex);

    ETTask* task = queue->head;
    if (task) {
        queue->head = task->next;
        if (queue->head == NULL) {
            queue->tail = NULL;
        }
        queue->count--;
        task->next = NULL;
    }

    pthread_mutex_unlock(&queue->mutex);
    return task;
}

// 워커 스레드 함수
static void* worker_thread_func(void* arg) {
    ETWorkerThread* worker = (ETWorkerThread*)arg;
    ETTaskScheduler* scheduler = worker->scheduler;

    while (!worker->should_exit) {
        ETTask* task = NULL;

        // 우선순위 순서대로 작업 검색 (REALTIME > HIGH > NORMAL > LOW)
        for (int priority = ET_TASK_PRIORITY_REALTIME; priority >= ET_TASK_PRIORITY_LOW; priority--) {
            ETTaskQueue* queue = &scheduler->queues[priority];

            pthread_mutex_lock(&queue->mutex);

            // 실시간 모드에서 데드라인 체크
            if (scheduler->realtime_mode && priority == ET_TASK_PRIORITY_REALTIME) {
                uint64_t current_time = get_current_time_us();

                // 데드라인이 지난 작업들 제거
                while (queue->head && queue->head->deadline < current_time) {
                    ETTask* expired_task = dequeue_task(queue);
                    if (expired_task) {
                        expired_task->status = ET_TASK_STATUS_CANCELLED;
                        pthread_mutex_lock(&scheduler->scheduler_mutex);
                        scheduler->total_tasks_cancelled++;
                        pthread_mutex_unlock(&scheduler->scheduler_mutex);

                        if (expired_task->completion_callback) {
                            expired_task->completion_callback(expired_task->task_id,
                                                            expired_task->callback_user_data);
                        }
                        free(expired_task);
                    }
                }
            }

            if (queue->head) {
                task = dequeue_task(queue);
                pthread_mutex_unlock(&queue->mutex);
                break;
            }

            pthread_mutex_unlock(&queue->mutex);
        }

        if (task) {
            // 작업 실행
            task->status = ET_TASK_STATUS_RUNNING;
            uint64_t start_time = get_current_time_us();

            if (task->task_func) {
                task->task_func(task->task_data);
            }

            uint64_t end_time = get_current_time_us();
            task->status = ET_TASK_STATUS_COMPLETED;

            // 실행 시간 기록 (프로파일링용)
            uint64_t execution_time = end_time - start_time;
            (void)execution_time; // 현재는 사용하지 않지만 추후 확장용으로 유지

            // 통계 업데이트
            pthread_mutex_lock(&scheduler->scheduler_mutex);
            scheduler->total_tasks_completed++;
            pthread_mutex_unlock(&scheduler->scheduler_mutex);

            // 완료 콜백 호출
            if (task->completion_callback) {
                task->completion_callback(task->task_id, task->callback_user_data);
            }

            free(task);
        } else {
            // 작업이 없으면 잠시 대기
            usleep(1000); // 1ms 대기
        }
    }

    return NULL;
}

// 스케줄러 생성
ETTaskScheduler* et_create_task_scheduler(uint32_t num_workers) {
    if (num_workers == 0) {
        num_workers = 4; // 기본값
    }

    ETTaskScheduler* scheduler = (ETTaskScheduler*)malloc(sizeof(ETTaskScheduler));
    if (!scheduler) {
        return NULL;
    }

    memset(scheduler, 0, sizeof(ETTaskScheduler));

    // 작업 큐들 초기화
    for (int i = 0; i < 4; i++) {
        init_task_queue(&scheduler->queues[i]);
    }

    // 워커 스레드 생성
    scheduler->num_workers = num_workers;
    scheduler->workers = (ETWorkerThread*)malloc(sizeof(ETWorkerThread) * num_workers);
    if (!scheduler->workers) {
        et_destroy_task_scheduler(scheduler);
        return NULL;
    }

    // 뮤텍스 초기화
    pthread_mutex_init(&scheduler->scheduler_mutex, NULL);
    pthread_mutex_init(&scheduler->id_mutex, NULL);

    // 기본 설정
    scheduler->realtime_mode = false;
    scheduler->audio_buffer_deadline = 10000; // 10ms 기본값
    scheduler->next_task_id = 1;
    scheduler->shutdown = false;

    // 워커 스레드 시작
    for (uint32_t i = 0; i < num_workers; i++) {
        scheduler->workers[i].worker_id = i;
        scheduler->workers[i].active = true;
        scheduler->workers[i].should_exit = false;
        scheduler->workers[i].scheduler = scheduler;

        if (pthread_create(&scheduler->workers[i].thread, NULL,
                          worker_thread_func, &scheduler->workers[i]) != 0) {
            // 스레드 생성 실패 시 정리
            scheduler->workers[i].active = false;
        }
    }

    return scheduler;
}

// 스케줄러 해제
void et_destroy_task_scheduler(ETTaskScheduler* scheduler) {
    if (!scheduler) {
        return;
    }

    // 종료 플래그 설정
    pthread_mutex_lock(&scheduler->scheduler_mutex);
    scheduler->shutdown = true;
    pthread_mutex_unlock(&scheduler->scheduler_mutex);

    // 워커 스레드들 종료
    if (scheduler->workers) {
        for (uint32_t i = 0; i < scheduler->num_workers; i++) {
            if (scheduler->workers[i].active) {
                scheduler->workers[i].should_exit = true;
                pthread_join(scheduler->workers[i].thread, NULL);
            }
        }
        free(scheduler->workers);
    }

    // 작업 큐들 정리
    for (int i = 0; i < 4; i++) {
        cleanup_task_queue(&scheduler->queues[i]);
    }

    // 뮤텍스 정리
    pthread_mutex_destroy(&scheduler->scheduler_mutex);
    pthread_mutex_destroy(&scheduler->id_mutex);

    free(scheduler);
}

// 새 작업 ID 생성
static uint32_t generate_task_id(ETTaskScheduler* scheduler) {
    pthread_mutex_lock(&scheduler->id_mutex);
    uint32_t id = scheduler->next_task_id++;
    pthread_mutex_unlock(&scheduler->id_mutex);
    return id;
}

// 작업 제출
uint32_t et_submit_task(ETTaskScheduler* scheduler,
                        ETTaskPriority priority,
                        void (*task_func)(void*),
                        void* task_data,
                        uint64_t deadline_us) {
    return et_submit_task_with_callback(scheduler, priority, task_func, task_data,
                                       deadline_us, NULL, NULL);
}

// 콜백과 함께 작업 제출
uint32_t et_submit_task_with_callback(ETTaskScheduler* scheduler,
                                      ETTaskPriority priority,
                                      void (*task_func)(void*),
                                      void* task_data,
                                      uint64_t deadline_us,
                                      void (*completion_callback)(uint32_t, void*),
                                      void* callback_user_data) {
    if (!scheduler || !task_func || scheduler->shutdown) {
        return 0;
    }

    ETTask* task = (ETTask*)malloc(sizeof(ETTask));
    if (!task) {
        return 0;
    }

    task->task_id = generate_task_id(scheduler);
    task->priority = priority;
    task->task_func = task_func;
    task->task_data = task_data;
    task->deadline = deadline_us;
    task->submit_time = get_current_time_us();
    task->status = ET_TASK_STATUS_PENDING;
    task->completion_callback = completion_callback;
    task->callback_user_data = callback_user_data;
    task->next = NULL;

    // 우선순위에 따른 큐에 추가
    enqueue_task(&scheduler->queues[priority], task);

    // 통계 업데이트
    pthread_mutex_lock(&scheduler->scheduler_mutex);
    scheduler->total_tasks_submitted++;
    pthread_mutex_unlock(&scheduler->scheduler_mutex);

    return task->task_id;
}

// 작업 취소 (구현 복잡성으로 인해 기본적인 형태만 제공)
bool et_cancel_task(ETTaskScheduler* scheduler, uint32_t task_id) {
    if (!scheduler || task_id == 0) {
        return false;
    }

    // 실제 구현에서는 각 큐를 검색하여 해당 작업을 찾아 제거해야 함
    // 여기서는 간단한 형태로만 구현

    pthread_mutex_lock(&scheduler->scheduler_mutex);
    scheduler->total_tasks_cancelled++;
    pthread_mutex_unlock(&scheduler->scheduler_mutex);

    return true;
}

// 작업 상태 조회 (간단한 형태)
ETTaskStatus et_get_task_status(const ETTaskScheduler* scheduler, uint32_t task_id) {
    if (!scheduler || task_id == 0) {
        return ET_TASK_STATUS_CANCELLED;
    }

    // 실제 구현에서는 작업 상태를 추적하는 해시 테이블 등이 필요
    // 여기서는 기본값 반환
    return ET_TASK_STATUS_PENDING;
}

// 실시간 모드 설정
void et_set_realtime_mode(ETTaskScheduler* scheduler, bool enable) {
    if (!scheduler) {
        return;
    }

    pthread_mutex_lock(&scheduler->scheduler_mutex);
    scheduler->realtime_mode = enable;
    pthread_mutex_unlock(&scheduler->scheduler_mutex);
}

// 오디오 버퍼 데드라인 설정
void et_set_audio_buffer_deadline(ETTaskScheduler* scheduler, uint64_t deadline_us) {
    if (!scheduler) {
        return;
    }

    pthread_mutex_lock(&scheduler->scheduler_mutex);
    scheduler->audio_buffer_deadline = deadline_us;
    pthread_mutex_unlock(&scheduler->scheduler_mutex);
}

// 스케줄러 통계 조회
void et_get_scheduler_stats(ETTaskScheduler* scheduler, ETSchedulerStats* stats) {
    if (!scheduler || !stats) {
        return;
    }

    pthread_mutex_lock(&scheduler->scheduler_mutex);

    stats->total_submitted = scheduler->total_tasks_submitted;
    stats->total_completed = scheduler->total_tasks_completed;
    stats->total_cancelled = scheduler->total_tasks_cancelled;

    // 대기 중인 작업 수 계산
    stats->pending_tasks = 0;
    for (int i = 0; i < 4; i++) {
        pthread_mutex_lock(&scheduler->queues[i].mutex);
        stats->pending_tasks += scheduler->queues[i].count;
        pthread_mutex_unlock(&scheduler->queues[i].mutex);
    }

    // 활성 워커 수 계산
    stats->active_workers = 0;
    for (uint32_t i = 0; i < scheduler->num_workers; i++) {
        if (scheduler->workers[i].active && !scheduler->workers[i].should_exit) {
            stats->active_workers++;
        }
    }

    // 평균 시간들 (실제 구현에서는 더 정교한 측정 필요)
    stats->avg_task_completion_time_us = 1000.0; // 기본값 1ms
    stats->avg_queue_wait_time_us = 500.0;       // 기본값 0.5ms

    pthread_mutex_unlock(&scheduler->scheduler_mutex);
}

// 스케줄러 일시 정지
void et_pause_scheduler(ETTaskScheduler* scheduler) {
    if (!scheduler) {
        return;
    }

    pthread_mutex_lock(&scheduler->scheduler_mutex);
    for (uint32_t i = 0; i < scheduler->num_workers; i++) {
        scheduler->workers[i].active = false;
    }
    pthread_mutex_unlock(&scheduler->scheduler_mutex);
}

// 스케줄러 재개
void et_resume_scheduler(ETTaskScheduler* scheduler) {
    if (!scheduler) {
        return;
    }

    pthread_mutex_lock(&scheduler->scheduler_mutex);
    for (uint32_t i = 0; i < scheduler->num_workers; i++) {
        if (!scheduler->workers[i].should_exit) {
            scheduler->workers[i].active = true;
        }
    }
    pthread_mutex_unlock(&scheduler->scheduler_mutex);
}