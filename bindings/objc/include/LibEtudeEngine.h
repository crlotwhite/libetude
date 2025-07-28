/**
 * @file LibEtudeEngine.h
 * @brief LibEtude iOS/macOS Objective-C 엔진 인터페이스
 * @author LibEtude Project
 * @version 1.0.0
 */

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@class LibEtudePerformanceStats;
@protocol LibEtudeAudioStreamDelegate;

/**
 * 품질 모드 열거형
 */
typedef NS_ENUM(NSInteger, LibEtudeQualityMode) {
    LibEtudeQualityModeFast = 0,      ///< 빠른 처리 (낮은 품질)
    LibEtudeQualityModeBalanced = 1,  ///< 균형 모드
    LibEtudeQualityModeHigh = 2       ///< 고품질 (느린 처리)
};

/**
 * 로그 레벨 열거형
 */
typedef NS_ENUM(NSInteger, LibEtudeLogLevel) {
    LibEtudeLogLevelDebug = 0,    ///< 디버그
    LibEtudeLogLevelInfo = 1,     ///< 정보
    LibEtudeLogLevelWarning = 2,  ///< 경고
    LibEtudeLogLevelError = 3,    ///< 오류
    LibEtudeLogLevelFatal = 4     ///< 치명적 오류
};

/**
 * LibEtude 음성 합성 엔진 클래스
 *
 * 이 클래스는 LibEtude 네이티브 엔진에 대한 Objective-C 래퍼를 제공합니다.
 * 텍스트-음성 변환, 노래 합성, 실시간 스트리밍 등의 기능을 지원합니다.
 */
@interface LibEtudeEngine : NSObject

/**
 * 엔진이 초기화되었는지 여부
 */
@property (nonatomic, readonly) BOOL isInitialized;

/**
 * 현재 스트리밍 중인지 여부
 */
@property (nonatomic, readonly) BOOL isStreaming;

/**
 * 지정된 모델 파일로 엔진을 초기화합니다
 *
 * @param modelPath 모델 파일 경로 (.lef 또는 .lefx)
 * @param error 오류 정보를 받을 포인터 (선택적)
 * @return 초기화된 엔진 인스턴스, 실패 시 nil
 */
- (nullable instancetype)initWithModelPath:(NSString *)modelPath error:(NSError **)error;

/**
 * 텍스트를 음성으로 합성합니다
 *
 * @param text 합성할 텍스트
 * @param error 오류 정보를 받을 포인터 (선택적)
 * @return 생성된 오디오 데이터, 실패 시 nil
 */
- (nullable NSData *)synthesizeText:(NSString *)text error:(NSError **)error;

/**
 * 가사와 음표를 노래로 합성합니다
 *
 * @param lyrics 가사
 * @param notes 음표 배열 (NSNumber 배열, MIDI 노트 번호)
 * @param error 오류 정보를 받을 포인터 (선택적)
 * @return 생성된 오디오 데이터, 실패 시 nil
 */
- (nullable NSData *)synthesizeSinging:(NSString *)lyrics
                                 notes:(NSArray<NSNumber *> *)notes
                                 error:(NSError **)error;

/**
 * 실시간 스트리밍을 시작합니다
 *
 * @param delegate 오디오 데이터를 받을 델리게이트
 * @param error 오류 정보를 받을 포인터 (선택적)
 * @return 성공 여부
 */
- (BOOL)startStreamingWithDelegate:(id<LibEtudeAudioStreamDelegate>)delegate error:(NSError **)error;

/**
 * 스트리밍 중에 텍스트를 추가합니다
 *
 * @param text 합성할 텍스트
 * @param error 오류 정보를 받을 포인터 (선택적)
 * @return 성공 여부
 */
- (BOOL)streamText:(NSString *)text error:(NSError **)error;

/**
 * 실시간 스트리밍을 중지합니다
 *
 * @param error 오류 정보를 받을 포인터 (선택적)
 * @return 성공 여부
 */
- (BOOL)stopStreamingWithError:(NSError **)error;

/**
 * 품질 모드를 설정합니다
 *
 * @param qualityMode 품질 모드
 * @param error 오류 정보를 받을 포인터 (선택적)
 * @return 성공 여부
 */
- (BOOL)setQualityMode:(LibEtudeQualityMode)qualityMode error:(NSError **)error;

/**
 * GPU 가속을 활성화합니다
 *
 * @param error 오류 정보를 받을 포인터 (선택적)
 * @return 성공 여부
 */
- (BOOL)enableGPUAccelerationWithError:(NSError **)error;

/**
 * 성능 통계를 가져옵니다
 *
 * @param error 오류 정보를 받을 포인터 (선택적)
 * @return 성능 통계 객체, 실패 시 nil
 */
- (nullable LibEtudePerformanceStats *)getPerformanceStatsWithError:(NSError **)error;

/**
 * 확장 모델을 로드합니다
 *
 * @param extensionPath 확장 모델 파일 경로
 * @param error 오류 정보를 받을 포인터 (선택적)
 * @return 성공 여부
 */
- (BOOL)loadExtension:(NSString *)extensionPath error:(NSError **)error;

/**
 * 모바일 최적화를 적용합니다
 *
 * @param lowMemory 저메모리 모드 활성화
 * @param batteryOptimized 배터리 최적화 활성화
 * @param maxThreads 최대 스레드 수
 * @param error 오류 정보를 받을 포인터 (선택적)
 * @return 성공 여부
 */
- (BOOL)applyMobileOptimizationsWithLowMemory:(BOOL)lowMemory
                              batteryOptimized:(BOOL)batteryOptimized
                                    maxThreads:(NSInteger)maxThreads
                                         error:(NSError **)error;

/**
 * 리소스 모니터링을 시작합니다
 */
- (void)startResourceMonitoring;

// 클래스 메서드들

/**
 * LibEtude 버전을 반환합니다
 *
 * @return 버전 문자열
 */
+ (NSString *)version;

/**
 * 하드웨어 기능을 반환합니다
 *
 * @return 하드웨어 기능 플래그
 */
+ (uint32_t)hardwareFeatures;

/**
 * 마지막 오류 메시지를 반환합니다
 *
 * @return 오류 메시지
 */
+ (nullable NSString *)lastError;

/**
 * 로그 레벨을 설정합니다
 *
 * @param level 로그 레벨
 */
+ (void)setLogLevel:(LibEtudeLogLevel)level;

/**
 * 현재 로그 레벨을 반환합니다
 *
 * @return 로그 레벨
 */
+ (LibEtudeLogLevel)logLevel;

/**
 * 메모리 사용량 통계를 반환합니다
 *
 * @return [사용량, 피크] 배열 (NSNumber 배열, 바이트 단위)
 */
+ (NSArray<NSNumber *> *)memoryStats;

/**
 * 시스템 정보를 반환합니다
 *
 * @return 시스템 정보 문자열
 */
+ (NSString *)systemInfo;

/**
 * 네이티브 라이브러리를 초기화합니다
 *
 * @return 성공 여부
 */
+ (BOOL)initializeNativeLibrary;

/**
 * 네이티브 라이브러리를 정리합니다
 */
+ (void)cleanupNativeLibrary;

@end

NS_ASSUME_NONNULL_END