/**
 * @file LibEtudePerformanceStats.h
 * @brief LibEtude 성능 통계 클래스
 * @author LibEtude Project
 * @version 1.0.0
 */

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

/**
 * LibEtude 엔진의 성능 통계 정보를 담는 클래스
 *
 * 이 클래스는 엔진의 추론 시간, 메모리 사용량, CPU/GPU 사용률 등의
 * 성능 관련 정보를 제공합니다.
 */
@interface LibEtudePerformanceStats : NSObject

/**
 * 추론 시간 (밀리초)
 */
@property (nonatomic, readonly) double inferenceTimeMs;

/**
 * 메모리 사용량 (MB)
 */
@property (nonatomic, readonly) double memoryUsageMb;

/**
 * CPU 사용률 (%)
 */
@property (nonatomic, readonly) double cpuUsagePercent;

/**
 * GPU 사용률 (%)
 */
@property (nonatomic, readonly) double gpuUsagePercent;

/**
 * 활성 스레드 수
 */
@property (nonatomic, readonly) NSInteger activeThreads;

/**
 * 성능 통계 객체를 생성합니다
 *
 * @param inferenceTimeMs 추론 시간 (밀리초)
 * @param memoryUsageMb 메모리 사용량 (MB)
 * @param cpuUsagePercent CPU 사용률 (%)
 * @param gpuUsagePercent GPU 사용률 (%)
 * @param activeThreads 활성 스레드 수
 * @return 초기화된 성능 통계 객체
 */
- (instancetype)initWithInferenceTime:(double)inferenceTimeMs
                         memoryUsage:(double)memoryUsageMb
                           cpuUsage:(double)cpuUsagePercent
                           gpuUsage:(double)gpuUsagePercent
                      activeThreads:(NSInteger)activeThreads;

/**
 * 메모리 사용량이 임계치를 초과하는지 확인합니다
 *
 * @param thresholdMb 임계치 (MB)
 * @return 초과 여부
 */
- (BOOL)isMemoryUsageHighWithThreshold:(double)thresholdMb;

/**
 * CPU 사용률이 임계치를 초과하는지 확인합니다
 *
 * @param thresholdPercent 임계치 (%)
 * @return 초과 여부
 */
- (BOOL)isCpuUsageHighWithThreshold:(double)thresholdPercent;

/**
 * GPU가 활성화되어 있는지 확인합니다
 *
 * @return GPU 활성화 여부
 */
- (BOOL)isGpuActive;

/**
 * 성능이 양호한지 확인합니다
 *
 * 다음 조건을 모두 만족하면 양호한 것으로 판단합니다:
 * - 추론 시간 < 100ms
 * - 메모리 사용량 < 100MB
 * - CPU 사용률 < 80%
 *
 * @return 성능 양호 여부
 */
- (BOOL)isPerformanceGood;

/**
 * 성능 통계를 딕셔너리로 반환합니다
 *
 * @return 성능 통계 딕셔너리
 */
- (NSDictionary<NSString *, NSNumber *> *)toDictionary;

@end

NS_ASSUME_NONNULL_END