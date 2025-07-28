/**
 * @file LibEtudeUtils.h
 * @brief LibEtude 유틸리티 함수들
 * @author LibEtude Project
 * @version 1.0.0
 */

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

/**
 * LibEtude 오류 도메인
 */
extern NSErrorDomain const LibEtudeErrorDomain;

/**
 * LibEtude 오류 코드
 */
typedef NS_ENUM(NSInteger, LibEtudeErrorCode) {
    LibEtudeErrorCodeSuccess = 0,           ///< 성공
    LibEtudeErrorCodeInvalidArgument = -1,  ///< 유효하지 않은 인수
    LibEtudeErrorCodeOutOfMemory = -2,      ///< 메모리 부족
    LibEtudeErrorCodeIO = -3,               ///< 입출력 오류
    LibEtudeErrorCodeNotImplemented = -4,   ///< 구현되지 않음
    LibEtudeErrorCodeRuntime = -5,          ///< 런타임 오류
    LibEtudeErrorCodeHardware = -6,         ///< 하드웨어 오류
    LibEtudeErrorCodeModel = -7,            ///< 모델 오류
    LibEtudeErrorCodeTimeout = -8           ///< 타임아웃
};

/**
 * LibEtude 유틸리티 클래스
 *
 * 이 클래스는 LibEtude와 관련된 유틸리티 함수들을 제공합니다.
 */
@interface LibEtudeUtils : NSObject

/**
 * 오류 코드를 NSError 객체로 변환합니다
 *
 * @param errorCode 오류 코드
 * @param description 오류 설명 (선택적)
 * @return NSError 객체
 */
+ (NSError *)errorWithCode:(LibEtudeErrorCode)errorCode description:(nullable NSString *)description;

/**
 * 오류 코드를 문자열로 변환합니다
 *
 * @param errorCode 오류 코드
 * @return 오류 문자열
 */
+ (NSString *)stringFromErrorCode:(LibEtudeErrorCode)errorCode;

/**
 * float 배열을 NSData로 변환합니다
 *
 * @param floatArray float 배열
 * @param length 배열 길이
 * @return NSData 객체
 */
+ (NSData *)dataFromFloatArray:(const float *)floatArray length:(NSUInteger)length;

/**
 * NSData를 float 배열로 변환합니다
 *
 * @param data NSData 객체
 * @return float 배열 (호출자가 해제해야 함)
 */
+ (float *)floatArrayFromData:(NSData *)data;

/**
 * NSArray<NSNumber *>를 float 배열로 변환합니다
 *
 * @param numberArray NSNumber 배열
 * @return float 배열 (호출자가 해제해야 함)
 */
+ (float *)floatArrayFromNumberArray:(NSArray<NSNumber *> *)numberArray;

/**
 * 문자열을 UTF-8 C 문자열로 변환합니다
 *
 * @param string NSString 객체
 * @return C 문자열 (호출자가 해제해야 함)
 */
+ (char *)cStringFromNSString:(NSString *)string;

/**
 * C 문자열을 NSString으로 변환합니다
 *
 * @param cString C 문자열
 * @return NSString 객체, 실패 시 nil
 */
+ (nullable NSString *)nsStringFromCString:(const char *)cString;

/**
 * 현재 시간을 밀리초로 반환합니다
 *
 * @return 현재 시간 (밀리초)
 */
+ (uint64_t)currentTimeMillis;

/**
 * 메모리 사용량을 MB 단위로 반환합니다
 *
 * @return 메모리 사용량 (MB)
 */
+ (double)memoryUsageMB;

/**
 * CPU 사용률을 반환합니다
 *
 * @return CPU 사용률 (%)
 */
+ (double)cpuUsagePercent;

/**
 * 디바이스 정보를 반환합니다
 *
 * @return 디바이스 정보 딕셔너리
 */
+ (NSDictionary<NSString *, id> *)deviceInfo;

/**
 * 로그 메시지를 출력합니다
 *
 * @param level 로그 레벨
 * @param format 포맷 문자열
 * @param ... 가변 인수
 */
+ (void)logWithLevel:(NSInteger)level format:(NSString *)format, ... NS_FORMAT_FUNCTION(2,3);

@end

NS_ASSUME_NONNULL_END