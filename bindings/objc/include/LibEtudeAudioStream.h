/**
 * @file LibEtudeAudioStream.h
 * @brief LibEtude 오디오 스트리밍 델리게이트 프로토콜
 * @author LibEtude Project
 * @version 1.0.0
 */

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

/**
 * LibEtude 오디오 스트리밍 델리게이트 프로토콜
 *
 * 이 프로토콜을 구현하여 LibEtude 엔진에서 생성되는
 * 실시간 오디오 데이터를 받을 수 있습니다.
 */
@protocol LibEtudeAudioStreamDelegate <NSObject>

@required

/**
 * 오디오 데이터가 생성되었을 때 호출됩니다
 *
 * 이 메서드는 백그라운드 스레드에서 호출되므로 다음 사항에 주의해야 합니다:
 * - 빠른 처리를 위해 최소한의 작업만 수행
 * - UI 업데이트는 메인 스레드로 전달
 * - 예외 발생 시 스트리밍이 중단될 수 있음
 *
 * @param audioData 생성된 오디오 데이터 (float 값들, -1.0 ~ 1.0 범위)
 */
- (void)audioStreamDidReceiveData:(NSData *)audioData;

@optional

/**
 * 스트리밍이 시작되었을 때 호출됩니다
 */
- (void)audioStreamDidStart;

/**
 * 스트리밍이 중지되었을 때 호출됩니다
 */
- (void)audioStreamDidStop;

/**
 * 스트리밍 중 오류가 발생했을 때 호출됩니다
 *
 * @param error 발생한 오류
 */
- (void)audioStreamDidEncounterError:(NSError *)error;

/**
 * 스트리밍 버퍼 상태가 변경되었을 때 호출됩니다
 *
 * @param bufferLevel 버퍼 레벨 (0.0 ~ 1.0)
 */
- (void)audioStreamBufferLevelChanged:(float)bufferLevel;

@end

NS_ASSUME_NONNULL_END