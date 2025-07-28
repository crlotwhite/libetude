/**
 * @file LibEtudePerformanceStats.mm
 * @brief LibEtude 성능 통계 구현
 * @author LibEtude Project
 * @version 1.0.0
 */

#import "LibEtudePerformanceStats.h"

@implementation LibEtudePerformanceStats

- (instancetype)initWithInferenceTime:(double)inferenceTimeMs
                         memoryUsage:(double)memoryUsageMb
                           cpuUsage:(double)cpuUsagePercent
                           gpuUsage:(double)gpuUsagePercent
                      activeThreads:(NSInteger)activeThreads {
    self = [super init];
    if (self) {
        _inferenceTimeMs = inferenceTimeMs;
        _memoryUsageMb = memoryUsageMb;
        _cpuUsagePercent = cpuUsagePercent;
        _gpuUsagePercent = gpuUsagePercent;
        _activeThreads = activeThreads;
    }
    return self;
}

- (BOOL)isMemoryUsageHighWithThreshold:(double)thresholdMb {
    return _memoryUsageMb > thresholdMb;
}

- (BOOL)isCpuUsageHighWithThreshold:(double)thresholdPercent {
    return _cpuUsagePercent > thresholdPercent;
}

- (BOOL)isGpuActive {
    return _gpuUsagePercent > 0.0;
}

- (BOOL)isPerformanceGood {
    return _inferenceTimeMs < 100.0 &&
           _memoryUsageMb < 100.0 &&
           _cpuUsagePercent < 80.0;
}

- (NSDictionary<NSString *, NSNumber *> *)toDictionary {
    return @{
        @"inferenceTimeMs": @(_inferenceTimeMs),
        @"memoryUsageMb": @(_memoryUsageMb),
        @"cpuUsagePercent": @(_cpuUsagePercent),
        @"gpuUsagePercent": @(_gpuUsagePercent),
        @"activeThreads": @(_activeThreads)
    };
}

- (NSString *)description {
    return [NSString stringWithFormat:
        @"LibEtudePerformanceStats{inferenceTime=%.2fms, memoryUsage=%.2fMB, "
        @"cpuUsage=%.1f%%, gpuUsage=%.1f%%, activeThreads=%ld}",
        _inferenceTimeMs, _memoryUsageMb, _cpuUsagePercent,
        _gpuUsagePercent, (long)_activeThreads];
}

@end