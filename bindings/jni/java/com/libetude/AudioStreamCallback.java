/**
 * LibEtude 오디오 스트림 콜백 인터페이스
 *
 * @author LibEtude Project
 * @version 1.0.0
 */
package com.libetude;

/**
 * 실시간 오디오 스트리밍을 위한 콜백 인터페이스
 *
 * 이 인터페이스를 구현하여 LibEtude 엔진에서 생성되는
 * 실시간 오디오 데이터를 받을 수 있습니다.
 */
public interface AudioStreamCallback {

    /**
     * 오디오 데이터가 생성되었을 때 호출됩니다
     *
     * 이 메서드는 네이티브 스레드에서 호출되므로 다음 사항에 주의해야 합니다:
     * - 빠른 처리를 위해 최소한의 작업만 수행
     * - UI 업데이트는 메인 스레드로 전달
     * - 예외 발생 시 스트리밍이 중단될 수 있음
     *
     * @param audioData 생성된 오디오 데이터 (float 배열, -1.0 ~ 1.0 범위)
     */
    void onAudioData(float[] audioData);
}