/**
 * LibEtude Android 엔진 클래스
 *
 * @author LibEtude Project
 * @version 1.0.0
 */
package com.libetude;

/**
 * LibEtude 음성 합성 엔진의 Android 인터페이스
 *
 * 이 클래스는 LibEtude 네이티브 엔진에 대한 Java 래퍼를 제공합니다.
 * 텍스트-음성 변환, 노래 합성, 실시간 스트리밍 등의 기능을 지원합니다.
 */
public class Engine {

    // 네이티브 라이브러리 로드
    static {
        System.loadLibrary("etude");
    }

    // 품질 모드 상수
    public static final int QUALITY_FAST = 0;
    public static final int QUALITY_BALANCED = 1;
    public static final int QUALITY_HIGH = 2;

    // 로그 레벨 상수
    public static final int LOG_DEBUG = 0;
    public static final int LOG_INFO = 1;
    public static final int LOG_WARNING = 2;
    public static final int LOG_ERROR = 3;
    public static final int LOG_FATAL = 4;

    private long engineHandle = 0;
    private boolean isInitialized = false;

    /**
     * 엔진을 생성합니다
     *
     * @param modelPath 모델 파일 경로 (.lef 또는 .lefx)
     * @throws RuntimeException 엔진 생성 실패 시
     */
    public Engine(String modelPath) {
        if (modelPath == null || modelPath.isEmpty()) {
            throw new IllegalArgumentException("모델 경로가 유효하지 않습니다");
        }

        engineHandle = createEngine(modelPath);
        if (engineHandle == 0) {
            throw new RuntimeException("엔진 생성 실패: " + getLastError());
        }

        isInitialized = true;
    }

    /**
     * 엔진을 해제합니다
     *
     * 이 메서드는 자동으로 finalize에서 호출되지만, 명시적으로 호출하는 것을 권장합니다.
     */
    public void destroy() {
        if (isInitialized && engineHandle != 0) {
            // 스트리밍이 활성화되어 있으면 중지
            if (isStreaming()) {
                stopStreaming();
            }

            destroyEngine(engineHandle);
            engineHandle = 0;
            isInitialized = false;
        }
    }

    @Override
    protected void finalize() throws Throwable {
        try {
            destroy();
        } finally {
            super.finalize();
        }
    }

    /**
     * 엔진이 초기화되었는지 확인합니다
     *
     * @return 초기화 상태
     */
    public boolean isInitialized() {
        return isInitialized && engineHandle != 0;
    }

    /**
     * 텍스트를 음성으로 합성합니다
     *
     * @param text 합성할 텍스트
     * @return 생성된 오디오 데이터 (float 배열)
     * @throws RuntimeException 합성 실패 시
     */
    public float[] synthesizeText(String text) {
        checkInitialized();
        if (text == null || text.isEmpty()) {
            throw new IllegalArgumentException("텍스트가 유효하지 않습니다");
        }

        return synthesizeText(engineHandle, text);
    }

    /**
     * 가사와 음표를 노래로 합성합니다
     *
     * @param lyrics 가사
     * @param notes 음표 배열 (MIDI 노트 번호)
     * @return 생성된 오디오 데이터 (float 배열)
     * @throws RuntimeException 합성 실패 시
     */
    public float[] synthesizeSinging(String lyrics, float[] notes) {
        checkInitialized();
        if (lyrics == null || lyrics.isEmpty()) {
            throw new IllegalArgumentException("가사가 유효하지 않습니다");
        }
        if (notes == null || notes.length == 0) {
            throw new IllegalArgumentException("음표가 유효하지 않습니다");
        }

        return synthesizeSinging(engineHandle, lyrics, notes);
    }

    /**
     * 실시간 스트리밍을 시작합니다
     *
     * @param callback 오디오 데이터를 받을 콜백
     * @return 성공 여부
     */
    public boolean startStreaming(AudioStreamCallback callback) {
        checkInitialized();
        if (callback == null) {
            throw new IllegalArgumentException("콜백이 null입니다");
        }

        return startStreaming(engineHandle, callback);
    }

    /**
     * 스트리밍 중에 텍스트를 추가합니다
     *
     * @param text 합성할 텍스트
     * @return 성공 여부
     */
    public boolean streamText(String text) {
        checkInitialized();
        if (text == null || text.isEmpty()) {
            throw new IllegalArgumentException("텍스트가 유효하지 않습니다");
        }

        return streamText(engineHandle, text);
    }

    /**
     * 실시간 스트리밍을 중지합니다
     *
     * @return 성공 여부
     */
    public boolean stopStreaming() {
        checkInitialized();
        return stopStreaming(engineHandle);
    }

    /**
     * 스트리밍 상태를 확인합니다
     *
     * @return 스트리밍 활성화 여부
     */
    public boolean isStreaming() {
        if (!isInitialized()) return false;
        return isStreaming(engineHandle);
    }

    /**
     * 품질 모드를 설정합니다
     *
     * @param qualityMode 품질 모드 (QUALITY_FAST, QUALITY_BALANCED, QUALITY_HIGH)
     * @return 성공 여부
     */
    public boolean setQualityMode(int qualityMode) {
        checkInitialized();
        return setQualityMode(engineHandle, qualityMode);
    }

    /**
     * GPU 가속을 활성화합니다
     *
     * @return 성공 여부
     */
    public boolean enableGPUAcceleration() {
        checkInitialized();
        return enableGPUAcceleration(engineHandle);
    }

    /**
     * 성능 통계를 가져옵니다
     *
     * @return 성능 통계 객체
     */
    public PerformanceStats getPerformanceStats() {
        checkInitialized();
        return getPerformanceStats(engineHandle);
    }

    /**
     * 확장 모델을 로드합니다
     *
     * @param extensionPath 확장 모델 파일 경로
     * @return 성공 여부
     */
    public boolean loadExtension(String extensionPath) {
        checkInitialized();
        if (extensionPath == null || extensionPath.isEmpty()) {
            throw new IllegalArgumentException("확장 경로가 유효하지 않습니다");
        }

        return loadExtension(engineHandle, extensionPath);
    }

    /**
     * 모바일 최적화를 적용합니다
     *
     * @param lowMemory 저메모리 모드 활성화
     * @param batteryOptimized 배터리 최적화 활성화
     * @param maxThreads 최대 스레드 수
     * @return 성공 여부
     */
    public boolean applyMobileOptimizations(boolean lowMemory, boolean batteryOptimized, int maxThreads) {
        checkInitialized();
        return applyMobileOptimizations(engineHandle, lowMemory, batteryOptimized, maxThreads);
    }

    /**
     * 리소스 모니터링을 시작합니다
     */
    public void startResourceMonitoring() {
        checkInitialized();
        startResourceMonitoring(engineHandle);
    }

    // 정적 유틸리티 메서드들

    /**
     * LibEtude 버전을 가져옵니다
     *
     * @return 버전 문자열
     */
    public static String getVersion() {
        return getVersion();
    }

    /**
     * 하드웨어 기능을 가져옵니다
     *
     * @return 하드웨어 기능 플래그
     */
    public static int getHardwareFeatures() {
        return getHardwareFeatures();
    }

    /**
     * 마지막 오류 메시지를 가져옵니다
     *
     * @return 오류 메시지
     */
    public static String getLastError() {
        return getLastError();
    }

    /**
     * 로그 레벨을 설정합니다
     *
     * @param level 로그 레벨
     */
    public static void setLogLevel(int level) {
        setLogLevel(level);
    }

    /**
     * 현재 로그 레벨을 가져옵니다
     *
     * @return 로그 레벨
     */
    public static int getLogLevel() {
        return getLogLevel();
    }

    /**
     * 메모리 사용량 통계를 가져옵니다
     *
     * @return [사용량, 피크] 배열 (바이트 단위)
     */
    public static long[] getMemoryStats() {
        return getMemoryStats();
    }

    /**
     * 시스템 정보를 가져옵니다
     *
     * @return 시스템 정보 문자열
     */
    public static String getSystemInfo() {
        return getSystemInfo();
    }

    /**
     * 네이티브 라이브러리를 초기화합니다
     *
     * @return 성공 여부
     */
    public static boolean initializeNativeLibrary() {
        return initializeNativeLibrary();
    }

    /**
     * 네이티브 라이브러리를 정리합니다
     */
    public static void cleanupNativeLibrary() {
        cleanupNativeLibrary();
    }

    // 내부 유틸리티 메서드
    private void checkInitialized() {
        if (!isInitialized()) {
            throw new IllegalStateException("엔진이 초기화되지 않았습니다");
        }
    }

    // 네이티브 메서드 선언
    private native long createEngine(String modelPath);
    private native void destroyEngine(long engineHandle);
    private native float[] synthesizeText(long engineHandle, String text);
    private native float[] synthesizeSinging(long engineHandle, String lyrics, float[] notes);
    private native boolean startStreaming(long engineHandle, AudioStreamCallback callback);
    private native boolean streamText(long engineHandle, String text);
    private native boolean stopStreaming(long engineHandle);
    private native boolean isStreaming(long engineHandle);
    private native boolean setQualityMode(long engineHandle, int qualityMode);
    private native boolean enableGPUAcceleration(long engineHandle);
    private native PerformanceStats getPerformanceStats(long engineHandle);
    private native boolean loadExtension(long engineHandle, String extensionPath);
    private native boolean applyMobileOptimizations(long engineHandle, boolean lowMemory, boolean batteryOptimized, int maxThreads);
    private native void startResourceMonitoring(long engineHandle);

    // 정적 네이티브 메서드들
    private static native String getVersion();
    private static native int getHardwareFeatures();
    private static native String getLastError();
    private static native void setLogLevel(int level);
    private static native int getLogLevel();
    private static native long[] getMemoryStats();
    private static native String getSystemInfo();
    private static native boolean initializeNativeLibrary();
    private static native void cleanupNativeLibrary();
}