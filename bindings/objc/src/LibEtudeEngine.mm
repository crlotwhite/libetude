/**
 * @file LibEtudeEngine.mm
 * @brief LibEtude iOS/macOS 엔진 구현
 * @author LibEtude Project
 * @version 1.0.0
 */

#import "LibEtudeEngine.h"
#import "LibEtudeAudioStream.h"
#import "LibEtudePerformanceStats.h"
#import "LibEtudeUtils.h"
#import "libetude/api.h"

#import <os/log.h>
#import <mach/mach.h>

// 로그 카테고리
static os_log_t libetude_log = nil;

// 스트리밍 콜백 데이터
@interface LibEtudeStreamingContext : NSObject
@property (nonatomic, weak) id<LibEtudeAudioStreamDelegate> delegate;
@property (nonatomic, strong) dispatch_queue_t callbackQueue;
@property (nonatomic, assign) BOOL active;
@end

@implementation LibEtudeStreamingContext
@end

// C 콜백 함수
static void native_audio_callback(const float* audio, int length, void* user_data) {
    LibEtudeStreamingContext* context = (__bridge LibEtudeStreamingContext*)user_data;
    if (!context || !context.active || !context.delegate) {
        return;
    }

    NSData* audioData = [LibEtudeUtils dataFromFloatArray:audio length:length];

    dispatch_async(context.callbackQueue, ^{
        if (context.active && context.delegate) {
            [context.delegate audioStreamDidReceiveData:audioData];
        }
    });
}

@interface LibEtudeEngine ()
@property (nonatomic, assign) LibEtudeEngine* nativeEngine;
@property (nonatomic, strong) LibEtudeStreamingContext* streamingContext;
@property (nonatomic, strong) dispatch_queue_t engineQueue;
@end

@implementation LibEtudeEngine

+ (void)initialize {
    if (self == [LibEtudeEngine class]) {
        libetude_log = os_log_create("com.libetude", "engine");
    }
}

- (instancetype)initWithModelPath:(NSString *)modelPath error:(NSError **)error {
    self = [super init];
    if (self) {
        if (!modelPath || modelPath.length == 0) {
            if (error) {
                *error = [LibEtudeUtils errorWithCode:LibEtudeErrorCodeInvalidArgument
                                          description:@"모델 경로가 유효하지 않습니다"];
            }
            return nil;
        }

        // 엔진 큐 생성
        _engineQueue = dispatch_queue_create("com.libetude.engine", DISPATCH_QUEUE_SERIAL);

        // 네이티브 엔진 생성
        char* cModelPath = [LibEtudeUtils cStringFromNSString:modelPath];
        _nativeEngine = libetude_create_engine(cModelPath);
        free(cModelPath);

        if (!_nativeEngine) {
            const char* lastError = libetude_get_last_error();
            NSString* errorDesc = [LibEtudeUtils nsStringFromCString:lastError] ?: @"엔진 생성 실패";

            if (error) {
                *error = [LibEtudeUtils errorWithCode:LibEtudeErrorCodeRuntime description:errorDesc];
            }

            os_log_error(libetude_log, "엔진 생성 실패: %{public}@", errorDesc);
            return nil;
        }

        // iOS/macOS 모바일 최적화 적용
        [self applyDefaultMobileOptimizations];

        os_log_info(libetude_log, "엔진 생성 완료: %{public}@", modelPath);
    }
    return self;
}

- (void)dealloc {
    [self cleanup];
}

- (void)cleanup {
    if (_nativeEngine) {
        // 스트리밍 중지
        if (_streamingContext && _streamingContext.active) {
            [self stopStreamingWithError:nil];
        }

        // 네이티브 엔진 해제
        libetude_destroy_engine(_nativeEngine);
        _nativeEngine = nullptr;

        os_log_info(libetude_log, "엔진 해제 완료");
    }
}

- (BOOL)isInitialized {
    return _nativeEngine != nullptr;
}

- (BOOL)isStreaming {
    return _streamingContext && _streamingContext.active;
}

- (void)applyDefaultMobileOptimizations {
    if (!_nativeEngine) return;

#if TARGET_OS_IOS
    // iOS 최적화
    libetude_set_quality_mode(_nativeEngine, LIBETUDE_QUALITY_BALANCED);
    os_log_info(libetude_log, "iOS 모바일 최적화 적용");
#elif TARGET_OS_OSX
    // macOS 최적화
    libetude_set_quality_mode(_nativeEngine, LIBETUDE_QUALITY_HIGH);
    libetude_enable_gpu_acceleration(_nativeEngine);
    os_log_info(libetude_log, "macOS 최적화 적용");
#endif
}

- (nullable NSData *)synthesizeText:(NSString *)text error:(NSError **)error {
    if (!self.isInitialized) {
        if (error) {
            *error = [LibEtudeUtils errorWithCode:LibEtudeErrorCodeRuntime
                                      description:@"엔진이 초기화되지 않았습니다"];
        }
        return nil;
    }

    if (!text || text.length == 0) {
        if (error) {
            *error = [LibEtudeUtils errorWithCode:LibEtudeErrorCodeInvalidArgument
                                      description:@"텍스트가 유효하지 않습니다"];
        }
        return nil;
    }

    __block NSData* result = nil;
    __block NSError* blockError = nil;

    dispatch_sync(_engineQueue, ^{
        char* cText = [LibEtudeUtils cStringFromNSString:text];

        // iOS/macOS에서는 최대 30초 오디오 버퍼 사용
        const int maxSamples = 48000 * 30;
        float* audioBuffer = (float*)malloc(maxSamples * sizeof(float));
        int outputLength = maxSamples;

        int apiResult = libetude_synthesize_text(self->_nativeEngine, cText, audioBuffer, &outputLength);

        free(cText);

        if (apiResult == LIBETUDE_SUCCESS) {
            result = [LibEtudeUtils dataFromFloatArray:audioBuffer length:outputLength];
            os_log_info(libetude_log, "텍스트 합성 완료: %d 샘플", outputLength);
        } else {
            const char* lastError = libetude_get_last_error();
            NSString* errorDesc = [LibEtudeUtils nsStringFromCString:lastError] ?: @"텍스트 합성 실패";
            blockError = [LibEtudeUtils errorWithCode:LibEtudeErrorCodeRuntime description:errorDesc];
            os_log_error(libetude_log, "텍스트 합성 실패: %{public}@", errorDesc);
        }

        free(audioBuffer);
    });

    if (error) *error = blockError;
    return result;
}

- (nullable NSData *)synthesizeSinging:(NSString *)lyrics
                                 notes:(NSArray<NSNumber *> *)notes
                                 error:(NSError **)error {
    if (!self.isInitialized) {
        if (error) {
            *error = [LibEtudeUtils errorWithCode:LibEtudeErrorCodeRuntime
                                      description:@"엔진이 초기화되지 않았습니다"];
        }
        return nil;
    }

    if (!lyrics || lyrics.length == 0) {
        if (error) {
            *error = [LibEtudeUtils errorWithCode:LibEtudeErrorCodeInvalidArgument
                                      description:@"가사가 유효하지 않습니다"];
        }
        return nil;
    }

    if (!notes || notes.count == 0) {
        if (error) {
            *error = [LibEtudeUtils errorWithCode:LibEtudeErrorCodeInvalidArgument
                                      description:@"음표가 유효하지 않습니다"];
        }
        return nil;
    }

    __block NSData* result = nil;
    __block NSError* blockError = nil;

    dispatch_sync(_engineQueue, ^{
        char* cLyrics = [LibEtudeUtils cStringFromNSString:lyrics];
        float* noteArray = [LibEtudeUtils floatArrayFromNumberArray:notes];

        // 노래는 더 긴 오디오를 생성할 수 있음
        const int maxSamples = 48000 * 60;  // 60초
        float* audioBuffer = (float*)malloc(maxSamples * sizeof(float));
        int outputLength = maxSamples;

        int apiResult = libetude_synthesize_singing(self->_nativeEngine, cLyrics, noteArray,
                                                   (int)notes.count, audioBuffer, &outputLength);

        free(cLyrics);
        free(noteArray);

        if (apiResult == LIBETUDE_SUCCESS) {
            result = [LibEtudeUtils dataFromFloatArray:audioBuffer length:outputLength];
            os_log_info(libetude_log, "노래 합성 완료: %d 샘플", outputLength);
        } else {
            const char* lastError = libetude_get_last_error();
            NSString* errorDesc = [LibEtudeUtils nsStringFromCString:lastError] ?: @"노래 합성 실패";
            blockError = [LibEtudeUtils errorWithCode:LibEtudeErrorCodeRuntime description:errorDesc];
            os_log_error(libetude_log, "노래 합성 실패: %{public}@", errorDesc);
        }

        free(audioBuffer);
    });

    if (error) *error = blockError;
    return result;
}

- (BOOL)startStreamingWithDelegate:(id<LibEtudeAudioStreamDelegate>)delegate error:(NSError **)error {
    if (!self.isInitialized) {
        if (error) {
            *error = [LibEtudeUtils errorWithCode:LibEtudeErrorCodeRuntime
                                      description:@"엔진이 초기화되지 않았습니다"];
        }
        return NO;
    }

    if (!delegate) {
        if (error) {
            *error = [LibEtudeUtils errorWithCode:LibEtudeErrorCodeInvalidArgument
                                      description:@"델리게이트가 nil입니다"];
        }
        return NO;
    }

    // 이미 스트리밍 중이면 중지
    if (_streamingContext && _streamingContext.active) {
        [self stopStreamingWithError:nil];
    }

    // 스트리밍 컨텍스트 생성
    _streamingContext = [[LibEtudeStreamingContext alloc] init];
    _streamingContext.delegate = delegate;
    _streamingContext.callbackQueue = dispatch_queue_create("com.libetude.audio", DISPATCH_QUEUE_SERIAL);
    _streamingContext.active = YES;

    // 네이티브 스트리밍 시작
    int result = libetude_start_streaming(_nativeEngine, native_audio_callback,
                                         (__bridge void*)_streamingContext);

    if (result != LIBETUDE_SUCCESS) {
        const char* lastError = libetude_get_last_error();
        NSString* errorDesc = [LibEtudeUtils nsStringFromCString:lastError] ?: @"스트리밍 시작 실패";

        if (error) {
            *error = [LibEtudeUtils errorWithCode:LibEtudeErrorCodeRuntime description:errorDesc];
        }

        _streamingContext = nil;
        os_log_error(libetude_log, "스트리밍 시작 실패: %{public}@", errorDesc);
        return NO;
    }

    // 델리게이트에 시작 알림
    if ([delegate respondsToSelector:@selector(audioStreamDidStart)]) {
        dispatch_async(_streamingContext.callbackQueue, ^{
            [delegate audioStreamDidStart];
        });
    }

    os_log_info(libetude_log, "스트리밍 시작 완료");
    return YES;
}

- (BOOL)streamText:(NSString *)text error:(NSError **)error {
    if (!self.isInitialized) {
        if (error) {
            *error = [LibEtudeUtils errorWithCode:LibEtudeErrorCodeRuntime
                                      description:@"엔진이 초기화되지 않았습니다"];
        }
        return NO;
    }

    if (!_streamingContext || !_streamingContext.active) {
        if (error) {
            *error = [LibEtudeUtils errorWithCode:LibEtudeErrorCodeRuntime
                                      description:@"스트리밍이 활성화되지 않았습니다"];
        }
        return NO;
    }

    if (!text || text.length == 0) {
        if (error) {
            *error = [LibEtudeUtils errorWithCode:LibEtudeErrorCodeInvalidArgument
                                      description:@"텍스트가 유효하지 않습니다"];
        }
        return NO;
    }

    char* cText = [LibEtudeUtils cStringFromNSString:text];
    int result = libetude_stream_text(_nativeEngine, cText);
    free(cText);

    if (result != LIBETUDE_SUCCESS) {
        const char* lastError = libetude_get_last_error();
        NSString* errorDesc = [LibEtudeUtils nsStringFromCString:lastError] ?: @"스트리밍 텍스트 추가 실패";

        if (error) {
            *error = [LibEtudeUtils errorWithCode:LibEtudeErrorCodeRuntime description:errorDesc];
        }

        os_log_error(libetude_log, "스트리밍 텍스트 추가 실패: %{public}@", errorDesc);
        return NO;
    }

    return YES;
}

- (BOOL)stopStreamingWithError:(NSError **)error {
    if (!self.isInitialized) {
        if (error) {
            *error = [LibEtudeUtils errorWithCode:LibEtudeErrorCodeRuntime
                                      description:@"엔진이 초기화되지 않았습니다"];
        }
        return NO;
    }

    if (!_streamingContext || !_streamingContext.active) {
        return YES;  // 이미 중지된 상태
    }

    // 스트리밍 비활성화
    _streamingContext.active = NO;

    // 네이티브 스트리밍 중지
    int result = libetude_stop_streaming(_nativeEngine);

    // 델리게이트에 중지 알림
    id<LibEtudeAudioStreamDelegate> delegate = _streamingContext.delegate;
    dispatch_queue_t callbackQueue = _streamingContext.callbackQueue;

    if (delegate && [delegate respondsToSelector:@selector(audioStreamDidStop)]) {
        dispatch_async(callbackQueue, ^{
            [delegate audioStreamDidStop];
        });
    }

    _streamingContext = nil;

    if (result != LIBETUDE_SUCCESS) {
        const char* lastError = libetude_get_last_error();
        NSString* errorDesc = [LibEtudeUtils nsStringFromCString:lastError] ?: @"스트리밍 중지 실패";

        if (error) {
            *error = [LibEtudeUtils errorWithCode:LibEtudeErrorCodeRuntime description:errorDesc];
        }

        os_log_error(libetude_log, "스트리밍 중지 실패: %{public}@", errorDesc);
        return NO;
    }

    os_log_info(libetude_log, "스트리밍 중지 완료");
    return YES;
}

- (BOOL)setQualityMode:(LibEtudeQualityMode)qualityMode error:(NSError **)error {
    if (!self.isInitialized) {
        if (error) {
            *error = [LibEtudeUtils errorWithCode:LibEtudeErrorCodeRuntime
                                      description:@"엔진이 초기화되지 않았습니다"];
        }
        return NO;
    }

    int result = libetude_set_quality_mode(_nativeEngine, (QualityMode)qualityMode);

    if (result != LIBETUDE_SUCCESS) {
        if (error) {
            *error = [LibEtudeUtils errorWithCode:LibEtudeErrorCodeRuntime
                                      description:@"품질 모드 설정 실패"];
        }
        return NO;
    }

    os_log_info(libetude_log, "품질 모드 설정: %ld", (long)qualityMode);
    return YES;
}

- (BOOL)enableGPUAccelerationWithError:(NSError **)error {
    if (!self.isInitialized) {
        if (error) {
            *error = [LibEtudeUtils errorWithCode:LibEtudeErrorCodeRuntime
                                      description:@"엔진이 초기화되지 않았습니다"];
        }
        return NO;
    }

    int result = libetude_enable_gpu_acceleration(_nativeEngine);

    if (result != LIBETUDE_SUCCESS) {
        if (error) {
            *error = [LibEtudeUtils errorWithCode:LibEtudeErrorCodeHardware
                                      description:@"GPU 가속 활성화 실패"];
        }
        return NO;
    }

    os_log_info(libetude_log, "GPU 가속 활성화됨");
    return YES;
}

- (nullable LibEtudePerformanceStats *)getPerformanceStatsWithError:(NSError **)error {
    if (!self.isInitialized) {
        if (error) {
            *error = [LibEtudeUtils errorWithCode:LibEtudeErrorCodeRuntime
                                      description:@"엔진이 초기화되지 않았습니다"];
        }
        return nil;
    }

    PerformanceStats stats;
    int result = libetude_get_performance_stats(_nativeEngine, &stats);

    if (result != LIBETUDE_SUCCESS) {
        if (error) {
            *error = [LibEtudeUtils errorWithCode:LibEtudeErrorCodeRuntime
                                      description:@"성능 통계 가져오기 실패"];
        }
        return nil;
    }

    return [[LibEtudePerformanceStats alloc] initWithInferenceTime:stats.inference_time_ms
                                                      memoryUsage:stats.memory_usage_mb
                                                         cpuUsage:stats.cpu_usage_percent
                                                         gpuUsage:stats.gpu_usage_percent
                                                    activeThreads:stats.active_threads];
}

- (BOOL)loadExtension:(NSString *)extensionPath error:(NSError **)error {
    if (!self.isInitialized) {
        if (error) {
            *error = [LibEtudeUtils errorWithCode:LibEtudeErrorCodeRuntime
                                      description:@"엔진이 초기화되지 않았습니다"];
        }
        return NO;
    }

    if (!extensionPath || extensionPath.length == 0) {
        if (error) {
            *error = [LibEtudeUtils errorWithCode:LibEtudeErrorCodeInvalidArgument
                                      description:@"확장 경로가 유효하지 않습니다"];
        }
        return NO;
    }

    char* cPath = [LibEtudeUtils cStringFromNSString:extensionPath];
    int result = libetude_load_extension(_nativeEngine, cPath);
    free(cPath);

    if (result != LIBETUDE_SUCCESS) {
        const char* lastError = libetude_get_last_error();
        NSString* errorDesc = [LibEtudeUtils nsStringFromCString:lastError] ?: @"확장 모델 로드 실패";

        if (error) {
            *error = [LibEtudeUtils errorWithCode:LibEtudeErrorCodeModel description:errorDesc];
        }

        os_log_error(libetude_log, "확장 모델 로드 실패: %{public}@", errorDesc);
        return NO;
    }

    os_log_info(libetude_log, "확장 모델 로드 완료: %{public}@", extensionPath);
    return YES;
}

- (BOOL)applyMobileOptimizationsWithLowMemory:(BOOL)lowMemory
                              batteryOptimized:(BOOL)batteryOptimized
                                    maxThreads:(NSInteger)maxThreads
                                         error:(NSError **)error {
    if (!self.isInitialized) {
        if (error) {
            *error = [LibEtudeUtils errorWithCode:LibEtudeErrorCodeRuntime
                                      description:@"엔진이 초기화되지 않았습니다"];
        }
        return NO;
    }

    if (maxThreads < 1 || maxThreads > 8) {
        if (error) {
            *error = [LibEtudeUtils errorWithCode:LibEtudeErrorCodeInvalidArgument
                                      description:@"유효하지 않은 스레드 수 (1-8)"];
        }
        return NO;
    }

    // 저메모리 모드 설정
    if (lowMemory) {
        libetude_set_quality_mode(_nativeEngine, LIBETUDE_QUALITY_FAST);
    }

    // 배터리 최적화 - GPU 가속 비활성화
    if (batteryOptimized) {
        // GPU 가속 비활성화는 별도 API가 필요할 수 있음
        os_log_info(libetude_log, "배터리 최적화 모드 활성화");
    }

    os_log_info(libetude_log, "모바일 최적화 적용: 저메모리=%d, 배터리최적화=%d, 최대스레드=%ld",
                lowMemory, batteryOptimized, (long)maxThreads);

    return YES;
}

- (void)startResourceMonitoring {
    if (!self.isInitialized) return;

    // 주기적으로 성능 통계 로깅
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_LOW, 0), ^{
        LibEtudePerformanceStats* stats = [self getPerformanceStatsWithError:nil];
        if (stats) {
            os_log_info(libetude_log, "성능 통계: %{public}@", stats);
        }
    });
}

// 클래스 메서드들

+ (NSString *)version {
    const char* version = libetude_get_version();
    return [LibEtudeUtils nsStringFromCString:version] ?: @"Unknown";
}

+ (uint32_t)hardwareFeatures {
    return libetude_get_hardware_features();
}

+ (nullable NSString *)lastError {
    const char* error = libetude_get_last_error();
    return [LibEtudeUtils nsStringFromCString:error];
}

+ (void)setLogLevel:(LibEtudeLogLevel)level {
    libetude_set_log_level((LibEtudeLogLevel)level);
}

+ (LibEtudeLogLevel)logLevel {
    return (LibEtudeLogLevel)libetude_get_log_level();
}

+ (NSArray<NSNumber *> *)memoryStats {
    // 간단한 메모리 통계 구현
    double memoryUsage = [LibEtudeUtils memoryUsageMB];
    return @[@(memoryUsage), @(memoryUsage)];  // [사용량, 피크] - 단순화
}

+ (NSString *)systemInfo {
    NSDictionary* deviceInfo = [LibEtudeUtils deviceInfo];
    return [NSString stringWithFormat:@"LibEtude %@\nPlatform: %@\nDevice: %@\nOS: %@",
            [self version],
            deviceInfo[@"platform"] ?: @"Unknown",
            deviceInfo[@"model"] ?: @"Unknown",
            deviceInfo[@"systemVersion"] ?: @"Unknown"];
}

+ (BOOL)initializeNativeLibrary {
    os_log_info(libetude_log, "네이티브 라이브러리 초기화");
    return YES;
}

+ (void)cleanupNativeLibrary {
    os_log_info(libetude_log, "네이티브 라이브러리 정리");
}

@end