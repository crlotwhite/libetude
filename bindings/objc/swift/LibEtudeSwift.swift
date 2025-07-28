/**
 * LibEtude Swift 래퍼 및 예제
 *
 * @author LibEtude Project
 * @version 1.0.0
 */

import Foundation

/**
 * LibEtude Swift 래퍼 클래스
 *
 * Objective-C API를 Swift에서 더 사용하기 쉽게 만드는 래퍼입니다.
 */
@objc public class LibEtudeSwift: NSObject {

    private let engine: LibEtudeEngine

    /**
     * 엔진을 초기화합니다
     *
     * - Parameter modelPath: 모델 파일 경로
     * - Throws: 초기화 실패 시 오류
     */
    public init(modelPath: String) throws {
        var error: NSError?
        guard let engine = LibEtudeEngine(modelPath: modelPath, error: &error) else {
            throw error ?? NSError(domain: LibEtudeErrorDomain, code: LibEtudeErrorCode.runtime.rawValue, userInfo: nil)
        }

        self.engine = engine
        super.init()
    }

    deinit {
        engine.cleanup()
    }

    /**
     * 엔진 초기화 상태
     */
    public var isInitialized: Bool {
        return engine.isInitialized
    }

    /**
     * 스트리밍 상태
     */
    public var isStreaming: Bool {
        return engine.isStreaming
    }

    /**
     * 텍스트를 음성으로 합성합니다
     *
     * - Parameter text: 합성할 텍스트
     * - Returns: 생성된 오디오 데이터
     * - Throws: 합성 실패 시 오류
     */
    public func synthesizeText(_ text: String) throws -> Data {
        var error: NSError?
        guard let audioData = engine.synthesizeText(text, error: &error) else {
            throw error ?? NSError(domain: LibEtudeErrorDomain, code: LibEtudeErrorCode.runtime.rawValue, userInfo: nil)
        }
        return audioData
    }

    /**
     * 가사와 음표를 노래로 합성합니다
     *
     * - Parameters:
     *   - lyrics: 가사
     *   - notes: 음표 배열 (MIDI 노트 번호)
     * - Returns: 생성된 오디오 데이터
     * - Throws: 합성 실패 시 오류
     */
    public func synthesizeSinging(lyrics: String, notes: [Float]) throws -> Data {
        let noteNumbers = notes.map { NSNumber(value: $0) }

        var error: NSError?
        guard let audioData = engine.synthesizeSinging(lyrics, notes: noteNumbers, error: &error) else {
            throw error ?? NSError(domain: LibEtudeErrorDomain, code: LibEtudeErrorCode.runtime.rawValue, userInfo: nil)
        }
        return audioData
    }

    /**
     * 실시간 스트리밍을 시작합니다
     *
     * - Parameter delegate: 오디오 데이터를 받을 델리게이트
     * - Throws: 스트리밍 시작 실패 시 오류
     */
    public func startStreaming(delegate: LibEtudeAudioStreamDelegate) throws {
        var error: NSError?
        guard engine.startStreaming(with: delegate, error: &error) else {
            throw error ?? NSError(domain: LibEtudeErrorDomain, code: LibEtudeErrorCode.runtime.rawValue, userInfo: nil)
        }
    }

    /**
     * 스트리밍 중에 텍스트를 추가합니다
     *
     * - Parameter text: 합성할 텍스트
     * - Throws: 텍스트 추가 실패 시 오류
     */
    public func streamText(_ text: String) throws {
        var error: NSError?
        guard engine.streamText(text, error: &error) else {
            throw error ?? NSError(domain: LibEtudeErrorDomain, code: LibEtudeErrorCode.runtime.rawValue, userInfo: nil)
        }
    }

    /**
     * 실시간 스트리밍을 중지합니다
     *
     * - Throws: 스트리밍 중지 실패 시 오류
     */
    public func stopStreaming() throws {
        var error: NSError?
        guard engine.stopStreaming(error: &error) else {
            throw error ?? NSError(domain: LibEtudeErrorDomain, code: LibEtudeErrorCode.runtime.rawValue, userInfo: nil)
        }
    }

    /**
     * 품질 모드를 설정합니다
     *
     * - Parameter mode: 품질 모드
     * - Throws: 설정 실패 시 오류
     */
    public func setQualityMode(_ mode: LibEtudeQualityMode) throws {
        var error: NSError?
        guard engine.setQualityMode(mode, error: &error) else {
            throw error ?? NSError(domain: LibEtudeErrorDomain, code: LibEtudeErrorCode.runtime.rawValue, userInfo: nil)
        }
    }

    /**
     * GPU 가속을 활성화합니다
     *
     * - Throws: 활성화 실패 시 오류
     */
    public func enableGPUAcceleration() throws {
        var error: NSError?
        guard engine.enableGPUAcceleration(error: &error) else {
            throw error ?? NSError(domain: LibEtudeErrorDomain, code: LibEtudeErrorCode.hardware.rawValue, userInfo: nil)
        }
    }

    /**
     * 성능 통계를 가져옵니다
     *
     * - Returns: 성능 통계 객체
     * - Throws: 통계 가져오기 실패 시 오류
     */
    public func getPerformanceStats() throws -> LibEtudePerformanceStats {
        var error: NSError?
        guard let stats = engine.getPerformanceStats(error: &error) else {
            throw error ?? NSError(domain: LibEtudeErrorDomain, code: LibEtudeErrorCode.runtime.rawValue, userInfo: nil)
        }
        return stats
    }

    /**
     * 확장 모델을 로드합니다
     *
     * - Parameter extensionPath: 확장 모델 파일 경로
     * - Throws: 로드 실패 시 오류
     */
    public func loadExtension(path: String) throws {
        var error: NSError?
        guard engine.loadExtension(path, error: &error) else {
            throw error ?? NSError(domain: LibEtudeErrorDomain, code: LibEtudeErrorCode.model.rawValue, userInfo: nil)
        }
    }

    /**
     * 모바일 최적화를 적용합니다
     *
     * - Parameters:
     *   - lowMemory: 저메모리 모드 활성화
     *   - batteryOptimized: 배터리 최적화 활성화
     *   - maxThreads: 최대 스레드 수
     * - Throws: 최적화 적용 실패 시 오류
     */
    public func applyMobileOptimizations(lowMemory: Bool, batteryOptimized: Bool, maxThreads: Int) throws {
        var error: NSError?
        guard engine.applyMobileOptimizations(withLowMemory: lowMemory,
                                            batteryOptimized: batteryOptimized,
                                            maxThreads: maxThreads,
                                            error: &error) else {
            throw error ?? NSError(domain: LibEtudeErrorDomain, code: LibEtudeErrorCode.runtime.rawValue, userInfo: nil)
        }
    }

    /**
     * 리소스 모니터링을 시작합니다
     */
    public func startResourceMonitoring() {
        engine.startResourceMonitoring()
    }

    // MARK: - 정적 메서드들

    /**
     * LibEtude 버전을 반환합니다
     */
    public static var version: String {
        return LibEtudeEngine.version()
    }

    /**
     * 하드웨어 기능을 반환합니다
     */
    public static var hardwareFeatures: UInt32 {
        return LibEtudeEngine.hardwareFeatures()
    }

    /**
     * 마지막 오류 메시지를 반환합니다
     */
    public static var lastError: String? {
        return LibEtudeEngine.lastError()
    }

    /**
     * 로그 레벨을 설정합니다
     */
    public static func setLogLevel(_ level: LibEtudeLogLevel) {
        LibEtudeEngine.setLogLevel(level)
    }

    /**
     * 현재 로그 레벨을 반환합니다
     */
    public static var logLevel: LibEtudeLogLevel {
        return LibEtudeEngine.logLevel()
    }

    /**
     * 메모리 사용량 통계를 반환합니다
     */
    public static var memoryStats: [NSNumber] {
        return LibEtudeEngine.memoryStats()
    }

    /**
     * 시스템 정보를 반환합니다
     */
    public static var systemInfo: String {
        return LibEtudeEngine.systemInfo()
    }

    /**
     * 네이티브 라이브러리를 초기화합니다
     */
    public static func initializeNativeLibrary() -> Bool {
        return LibEtudeEngine.initializeNativeLibrary()
    }

    /**
     * 네이티브 라이브러리를 정리합니다
     */
    public static func cleanupNativeLibrary() {
        LibEtudeEngine.cleanupNativeLibrary()
    }
}

/**
 * Swift용 오디오 스트림 델리게이트 래퍼
 */
public protocol LibEtudeAudioStreamSwiftDelegate: AnyObject {
    func audioStreamDidReceiveData(_ audioData: Data)
    func audioStreamDidStart()
    func audioStreamDidStop()
    func audioStreamDidEncounterError(_ error: Error)
    func audioStreamBufferLevelChanged(_ bufferLevel: Float)
}

/**
 * Objective-C 델리게이트를 Swift 델리게이트로 브리징하는 클래스
 */
@objc public class LibEtudeAudioStreamDelegateBridge: NSObject, LibEtudeAudioStreamDelegate {

    public weak var swiftDelegate: LibEtudeAudioStreamSwiftDelegate?

    public init(swiftDelegate: LibEtudeAudioStreamSwiftDelegate) {
        self.swiftDelegate = swiftDelegate
        super.init()
    }

    public func audioStreamDidReceiveData(_ audioData: Data) {
        swiftDelegate?.audioStreamDidReceiveData(audioData)
    }

    public func audioStreamDidStart() {
        swiftDelegate?.audioStreamDidStart()
    }

    public func audioStreamDidStop() {
        swiftDelegate?.audioStreamDidStop()
    }

    public func audioStreamDidEncounterError(_ error: Error) {
        swiftDelegate?.audioStreamDidEncounterError(error)
    }

    public func audioStreamBufferLevelChanged(_ bufferLevel: Float) {
        swiftDelegate?.audioStreamBufferLevelChanged(bufferLevel)
    }
}