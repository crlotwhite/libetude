/**
 * LibEtude 성능 통계 클래스
 *
 * @author LibEtude Project
 * @version 1.0.0
 */
package com.libetude;

/**
 * LibEtude 엔진의 성능 통계 정보를 담는 클래스
 *
 * 이 클래스는 엔진의 추론 시간, 메모리 사용량, CPU/GPU 사용률 등의
 * 성능 관련 정보를 제공합니다.
 */
public class PerformanceStats {

    /** 추론 시간 (밀리초) */
    public final double inferenceTimeMs;

    /** 메모리 사용량 (MB) */
    public final double memoryUsageMb;

    /** CPU 사용률 (%) */
    public final double cpuUsagePercent;

    /** GPU 사용률 (%) */
    public final double gpuUsagePercent;

    /** 활성 스레드 수 */
    public final int activeThreads;

    /**
     * 성능 통계 객체를 생성합니다
     *
     * 이 생성자는 JNI에서 호출됩니다.
     *
     * @param inferenceTimeMs 추론 시간 (밀리초)
     * @param memoryUsageMb 메모리 사용량 (MB)
     * @param cpuUsagePercent CPU 사용률 (%)
     * @param gpuUsagePercent GPU 사용률 (%)
     * @param activeThreads 활성 스레드 수
     */
    public PerformanceStats(double inferenceTimeMs, double memoryUsageMb,
                           double cpuUsagePercent, double gpuUsagePercent,
                           int activeThreads) {
        this.inferenceTimeMs = inferenceTimeMs;
        this.memoryUsageMb = memoryUsageMb;
        this.cpuUsagePercent = cpuUsagePercent;
        this.gpuUsagePercent = gpuUsagePercent;
        this.activeThreads = activeThreads;
    }

    /**
     * 성능 통계를 문자열로 반환합니다
     *
     * @return 성능 통계 문자열
     */
    @Override
    public String toString() {
        return String.format(
            "PerformanceStats{" +
            "inferenceTime=%.2fms, " +
            "memoryUsage=%.2fMB, " +
            "cpuUsage=%.1f%%, " +
            "gpuUsage=%.1f%%, " +
            "activeThreads=%d" +
            "}",
            inferenceTimeMs, memoryUsageMb, cpuUsagePercent,
            gpuUsagePercent, activeThreads
        );
    }

    /**
     * 메모리 사용량이 임계치를 초과하는지 확인합니다
     *
     * @param thresholdMb 임계치 (MB)
     * @return 초과 여부
     */
    public boolean isMemoryUsageHigh(double thresholdMb) {
        return memoryUsageMb > thresholdMb;
    }

    /**
     * CPU 사용률이 임계치를 초과하는지 확인합니다
     *
     * @param thresholdPercent 임계치 (%)
     * @return 초과 여부
     */
    public boolean isCpuUsageHigh(double thresholdPercent) {
        return cpuUsagePercent > thresholdPercent;
    }

    /**
     * GPU가 활성화되어 있는지 확인합니다
     *
     * @return GPU 활성화 여부
     */
    public boolean isGpuActive() {
        return gpuUsagePercent > 0.0;
    }

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
    public boolean isPerformanceGood() {
        return inferenceTimeMs < 100.0 &&
               memoryUsageMb < 100.0 &&
               cpuUsagePercent < 80.0;
    }
}