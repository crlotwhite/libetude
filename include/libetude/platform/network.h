/**
 * @file network.h
 * @brief 네트워크 추상화 레이어 인터페이스
 * @author LibEtude Project
 * @version 1.0.0
 *
 * 플랫폼별 네트워크 API 차이를 숨기고 일관된 소켓 인터페이스를 제공합니다.
 * Windows Winsock, Linux/macOS 소켓 API를 추상화하며,
 * 비동기 I/O (IOCP, epoll, kqueue)를 통합된 인터페이스로 제공합니다.
 */

#ifndef LIBETUDE_PLATFORM_NETWORK_H
#define LIBETUDE_PLATFORM_NETWORK_H

#include "libetude/platform/common.h"
#include "libetude/types.h"
#include "libetude/error.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// 네트워크 타입 정의
// ============================================================================

/**
 * @brief 소켓 타입 열거형
 */
typedef enum {
    ET_SOCKET_TCP = 0,          /**< TCP 소켓 */
    ET_SOCKET_UDP = 1,          /**< UDP 소켓 */
    ET_SOCKET_RAW = 2           /**< Raw 소켓 */
} ETSocketType;

/**
 * @brief 소켓 상태 열거형
 */
typedef enum {
    ET_SOCKET_CLOSED = 0,       /**< 닫힌 상태 */
    ET_SOCKET_BOUND = 1,        /**< 바인드된 상태 */
    ET_SOCKET_LISTENING = 2,    /**< 리스닝 상태 */
    ET_SOCKET_CONNECTING = 3,   /**< 연결 중 */
    ET_SOCKET_CONNECTED = 4,    /**< 연결됨 */
    ET_SOCKET_ERROR = 5         /**< 오류 상태 */
} ETSocketState;

/**
 * @brief 소켓 옵션 열거형
 */
typedef enum {
    ET_SOCKET_OPT_REUSEADDR = 0,    /**< 주소 재사용 */
    ET_SOCKET_OPT_REUSEPORT = 1,    /**< 포트 재사용 */
    ET_SOCKET_OPT_KEEPALIVE = 2,    /**< Keep-alive */
    ET_SOCKET_OPT_NODELAY = 3,      /**< Nagle 알고리즘 비활성화 */
    ET_SOCKET_OPT_NONBLOCK = 4,     /**< 논블로킹 모드 */
    ET_SOCKET_OPT_RCVBUF = 5,       /**< 수신 버퍼 크기 */
    ET_SOCKET_OPT_SNDBUF = 6,       /**< 송신 버퍼 크기 */
    ET_SOCKET_OPT_RCVTIMEO = 7,     /**< 수신 타임아웃 */
    ET_SOCKET_OPT_SNDTIMEO = 8      /**< 송신 타임아웃 */
} ETSocketOption;

/**
 * @brief I/O 이벤트 플래그
 */
typedef enum {
    ET_IO_EVENT_NONE = 0,           /**< 이벤트 없음 */
    ET_IO_EVENT_READ = 1 << 0,      /**< 읽기 가능 */
    ET_IO_EVENT_WRITE = 1 << 1,     /**< 쓰기 가능 */
    ET_IO_EVENT_ERROR = 1 << 2,     /**< 오류 발생 */
    ET_IO_EVENT_CLOSE = 1 << 3      /**< 연결 종료 */
} ETIOEvents;

/**
 * @brief 주소 패밀리 열거형
 */
typedef enum {
    ET_AF_INET = 0,             /**< IPv4 */
    ET_AF_INET6 = 1,            /**< IPv6 */
    ET_AF_UNIX = 2              /**< Unix 도메인 소켓 */
} ETAddressFamily;

// ============================================================================
// 네트워크 구조체 정의
// ============================================================================

/**
 * @brief 소켓 주소 구조체
 */
typedef struct {
    ETAddressFamily family;     /**< 주소 패밀리 */
    union {
        struct {
            uint32_t addr;      /**< IPv4 주소 (네트워크 바이트 순서) */
            uint16_t port;      /**< 포트 번호 (호스트 바이트 순서) */
        } ipv4;
        struct {
            uint8_t addr[16];   /**< IPv6 주소 */
            uint16_t port;      /**< 포트 번호 */
            uint32_t flowinfo;  /**< 플로우 정보 */
            uint32_t scope_id;  /**< 스코프 ID */
        } ipv6;
        struct {
            char path[108];     /**< Unix 소켓 경로 */
        } unix_addr;
    };
} ETSocketAddress;

/**
 * @brief I/O 이벤트 구조체
 */
typedef struct {
    void* socket;               /**< 소켓 포인터 */
    ETIOEvents events;          /**< 발생한 이벤트 */
    void* user_data;            /**< 사용자 데이터 */
    int error_code;             /**< 오류 코드 (오류 발생 시) */
} ETIOEvent;

/**
 * @brief 네트워크 통계 구조체
 */
typedef struct {
    uint64_t bytes_sent;        /**< 송신된 바이트 수 */
    uint64_t bytes_received;    /**< 수신된 바이트 수 */
    uint64_t packets_sent;      /**< 송신된 패킷 수 */
    uint64_t packets_received;  /**< 수신된 패킷 수 */
    uint64_t errors;            /**< 오류 발생 횟수 */
    uint64_t timeouts;          /**< 타임아웃 발생 횟수 */
} ETNetworkStats;

// ============================================================================
// 불투명한 포인터 타입 정의
// ============================================================================

/**
 * @brief 소켓 핸들 (플랫폼별 구현에 따라 다름)
 */
typedef struct ETSocket ETSocket;

/**
 * @brief I/O 컨텍스트 핸들 (비동기 I/O용)
 */
typedef struct ETIOContext ETIOContext;

// ============================================================================
// 네트워크 인터페이스 구조체
// ============================================================================

/**
 * @brief 네트워크 추상화 인터페이스
 */
typedef struct ETNetworkInterface {
    // ========================================================================
    // 소켓 관리 함수들
    // ========================================================================

    /**
     * @brief 소켓을 생성합니다
     * @param type 소켓 타입 (TCP/UDP/RAW)
     * @param socket 생성된 소켓 포인터를 저장할 변수
     * @return 성공시 ET_SUCCESS, 실패시 오류 코드
     */
    ETResult (*create_socket)(ETSocketType type, ETSocket** socket);

    /**
     * @brief 소켓을 주소에 바인드합니다
     * @param socket 소켓 포인터
     * @param addr 바인드할 주소
     * @return 성공시 ET_SUCCESS, 실패시 오류 코드
     */
    ETResult (*bind_socket)(ETSocket* socket, const ETSocketAddress* addr);

    /**
     * @brief 소켓을 리스닝 상태로 만듭니다
     * @param socket 소켓 포인터
     * @param backlog 대기열 크기
     * @return 성공시 ET_SUCCESS, 실패시 오류 코드
     */
    ETResult (*listen_socket)(ETSocket* socket, int backlog);

    /**
     * @brief 클라이언트 연결을 수락합니다
     * @param socket 서버 소켓 포인터
     * @param client 클라이언트 소켓 포인터를 저장할 변수
     * @param addr 클라이언트 주소를 저장할 변수 (NULL 가능)
     * @return 성공시 ET_SUCCESS, 실패시 오류 코드
     */
    ETResult (*accept_socket)(ETSocket* socket, ETSocket** client, ETSocketAddress* addr);

    /**
     * @brief 서버에 연결합니다
     * @param socket 소켓 포인터
     * @param addr 연결할 서버 주소
     * @return 성공시 ET_SUCCESS, 실패시 오류 코드
     */
    ETResult (*connect_socket)(ETSocket* socket, const ETSocketAddress* addr);

    /**
     * @brief 소켓을 닫습니다
     * @param socket 소켓 포인터
     */
    void (*close_socket)(ETSocket* socket);

    // ========================================================================
    // 데이터 전송 함수들
    // ========================================================================

    /**
     * @brief 데이터를 송신합니다
     * @param socket 소켓 포인터
     * @param data 송신할 데이터
     * @param size 데이터 크기
     * @param sent 실제 송신된 바이트 수를 저장할 변수
     * @return 성공시 ET_SUCCESS, 실패시 오류 코드
     */
    ETResult (*send_data)(ETSocket* socket, const void* data, size_t size, size_t* sent);

    /**
     * @brief 데이터를 수신합니다
     * @param socket 소켓 포인터
     * @param buffer 수신 버퍼
     * @param size 버퍼 크기
     * @param received 실제 수신된 바이트 수를 저장할 변수
     * @return 성공시 ET_SUCCESS, 실패시 오류 코드
     */
    ETResult (*receive_data)(ETSocket* socket, void* buffer, size_t size, size_t* received);

    /**
     * @brief UDP 데이터를 특정 주소로 송신합니다
     * @param socket 소켓 포인터
     * @param data 송신할 데이터
     * @param size 데이터 크기
     * @param addr 목적지 주소
     * @param sent 실제 송신된 바이트 수를 저장할 변수
     * @return 성공시 ET_SUCCESS, 실패시 오류 코드
     */
    ETResult (*send_to)(ETSocket* socket, const void* data, size_t size,
                       const ETSocketAddress* addr, size_t* sent);

    /**
     * @brief UDP 데이터를 수신하고 송신자 주소를 가져옵니다
     * @param socket 소켓 포인터
     * @param buffer 수신 버퍼
     * @param size 버퍼 크기
     * @param addr 송신자 주소를 저장할 변수
     * @param received 실제 수신된 바이트 수를 저장할 변수
     * @return 성공시 ET_SUCCESS, 실패시 오류 코드
     */
    ETResult (*receive_from)(ETSocket* socket, void* buffer, size_t size,
                            ETSocketAddress* addr, size_t* received);

    // ========================================================================
    // 소켓 옵션 및 상태 함수들
    // ========================================================================

    /**
     * @brief 소켓 옵션을 설정합니다
     * @param socket 소켓 포인터
     * @param option 설정할 옵션
     * @param value 옵션 값
     * @param size 값의 크기
     * @return 성공시 ET_SUCCESS, 실패시 오류 코드
     */
    ETResult (*set_socket_option)(ETSocket* socket, ETSocketOption option,
                                 const void* value, size_t size);

    /**
     * @brief 소켓 옵션을 가져옵니다
     * @param socket 소켓 포인터
     * @param option 가져올 옵션
     * @param value 옵션 값을 저장할 버퍼
     * @param size 버퍼 크기 (입력), 실제 크기 (출력)
     * @return 성공시 ET_SUCCESS, 실패시 오류 코드
     */
    ETResult (*get_socket_option)(ETSocket* socket, ETSocketOption option,
                                 void* value, size_t* size);

    /**
     * @brief 소켓 상태를 가져옵니다
     * @param socket 소켓 포인터
     * @return 소켓 상태
     */
    ETSocketState (*get_socket_state)(const ETSocket* socket);

    /**
     * @brief 소켓의 로컬 주소를 가져옵니다
     * @param socket 소켓 포인터
     * @param addr 주소를 저장할 변수
     * @return 성공시 ET_SUCCESS, 실패시 오류 코드
     */
    ETResult (*get_local_address)(const ETSocket* socket, ETSocketAddress* addr);

    /**
     * @brief 소켓의 원격 주소를 가져옵니다
     * @param socket 소켓 포인터
     * @param addr 주소를 저장할 변수
     * @return 성공시 ET_SUCCESS, 실패시 오류 코드
     */
    ETResult (*get_remote_address)(const ETSocket* socket, ETSocketAddress* addr);

    // ========================================================================
    // 비동기 I/O 함수들
    // ========================================================================

    /**
     * @brief I/O 컨텍스트를 생성합니다
     * @param context 생성된 컨텍스트 포인터를 저장할 변수
     * @return 성공시 ET_SUCCESS, 실패시 오류 코드
     */
    ETResult (*create_io_context)(ETIOContext** context);

    /**
     * @brief 소켓을 I/O 컨텍스트에 등록합니다
     * @param context I/O 컨텍스트
     * @param socket 등록할 소켓
     * @param events 관심있는 이벤트 플래그
     * @param user_data 사용자 데이터
     * @return 성공시 ET_SUCCESS, 실패시 오류 코드
     */
    ETResult (*register_socket)(ETIOContext* context, ETSocket* socket,
                               ETIOEvents events, void* user_data);

    /**
     * @brief 소켓의 이벤트 관심사를 수정합니다
     * @param context I/O 컨텍스트
     * @param socket 수정할 소켓
     * @param events 새로운 이벤트 플래그
     * @return 성공시 ET_SUCCESS, 실패시 오류 코드
     */
    ETResult (*modify_socket_events)(ETIOContext* context, ETSocket* socket, ETIOEvents events);

    /**
     * @brief 소켓을 I/O 컨텍스트에서 제거합니다
     * @param context I/O 컨텍스트
     * @param socket 제거할 소켓
     * @return 성공시 ET_SUCCESS, 실패시 오류 코드
     */
    ETResult (*unregister_socket)(ETIOContext* context, ETSocket* socket);

    /**
     * @brief I/O 이벤트를 대기합니다
     * @param context I/O 컨텍스트
     * @param events 이벤트를 저장할 배열
     * @param max_events 최대 이벤트 수
     * @param timeout 타임아웃 (밀리초, -1은 무한대기)
     * @param num_events 실제 발생한 이벤트 수를 저장할 변수
     * @return 성공시 ET_SUCCESS, 실패시 오류 코드
     */
    ETResult (*wait_events)(ETIOContext* context, ETIOEvent* events,
                           int max_events, int timeout, int* num_events);

    /**
     * @brief I/O 컨텍스트를 해제합니다
     * @param context I/O 컨텍스트
     */
    void (*destroy_io_context)(ETIOContext* context);

    // ========================================================================
    // 주소 처리 함수들
    // ========================================================================

    /**
     * @brief 문자열 IP 주소를 바이너리로 변환합니다
     * @param family 주소 패밀리
     * @param str_addr 문자열 주소
     * @param addr 변환된 주소를 저장할 구조체
     * @return 성공시 ET_SUCCESS, 실패시 오류 코드
     */
    ETResult (*string_to_address)(ETAddressFamily family, const char* str_addr, ETSocketAddress* addr);

    /**
     * @brief 바이너리 IP 주소를 문자열로 변환합니다
     * @param addr 주소 구조체
     * @param str_addr 문자열을 저장할 버퍼
     * @param size 버퍼 크기
     * @return 성공시 ET_SUCCESS, 실패시 오류 코드
     */
    ETResult (*address_to_string)(const ETSocketAddress* addr, char* str_addr, size_t size);

    /**
     * @brief 호스트명을 IP 주소로 해석합니다
     * @param hostname 호스트명
     * @param family 주소 패밀리
     * @param addresses 해석된 주소들을 저장할 배열
     * @param max_addresses 최대 주소 수
     * @param num_addresses 실제 해석된 주소 수를 저장할 변수
     * @return 성공시 ET_SUCCESS, 실패시 오류 코드
     */
    ETResult (*resolve_hostname)(const char* hostname, ETAddressFamily family,
                                ETSocketAddress* addresses, int max_addresses, int* num_addresses);

    // ========================================================================
    // 유틸리티 함수들
    // ========================================================================

    /**
     * @brief 네트워크 통계를 가져옵니다
     * @param socket 소켓 포인터 (NULL이면 전체 통계)
     * @param stats 통계를 저장할 구조체
     * @return 성공시 ET_SUCCESS, 실패시 오류 코드
     */
    ETResult (*get_network_stats)(const ETSocket* socket, ETNetworkStats* stats);

    /**
     * @brief 마지막 네트워크 오류를 가져옵니다
     * @return 플랫폼별 오류 코드
     */
    int (*get_last_network_error)(void);

    /**
     * @brief 네트워크 오류 코드를 문자열로 변환합니다
     * @param error_code 오류 코드
     * @return 오류 설명 문자열
     */
    const char* (*get_network_error_string)(int error_code);

    // ========================================================================
    // 플랫폼별 확장 데이터
    // ========================================================================

    /**
     * @brief 플랫폼별 확장 데이터
     */
    void* platform_data;

} ETNetworkInterface;

// ============================================================================
// 공통 함수 선언
// ============================================================================

/**
 * @brief 네트워크 추상화 레이어를 초기화합니다
 * @return 성공시 ET_SUCCESS, 실패시 오류 코드
 */
ETResult et_network_initialize(void);

/**
 * @brief 현재 플랫폼의 네트워크 인터페이스를 가져옵니다
 * @return 네트워크 인터페이스 포인터
 */
const ETNetworkInterface* et_get_network_interface(void);

/**
 * @brief 네트워크 추상화 레이어를 정리합니다
 */
void et_network_finalize(void);

/**
 * @brief 플랫폼별 네트워크 오류를 공통 오류로 변환합니다
 * @param platform_error 플랫폼별 오류 코드
 * @return 공통 오류 코드
 */
ETResult et_network_error_to_common(int platform_error);

/**
 * @brief IPv4 주소를 생성합니다
 * @param ip_str IP 주소 문자열 (예: "192.168.1.1")
 * @param port 포트 번호
 * @param addr 생성된 주소를 저장할 구조체
 * @return 성공시 ET_SUCCESS, 실패시 오류 코드
 */
ETResult et_create_ipv4_address(const char* ip_str, uint16_t port, ETSocketAddress* addr);

/**
 * @brief IPv6 주소를 생성합니다
 * @param ip_str IP 주소 문자열 (예: "::1")
 * @param port 포트 번호
 * @param addr 생성된 주소를 저장할 구조체
 * @return 성공시 ET_SUCCESS, 실패시 오류 코드
 */
ETResult et_create_ipv6_address(const char* ip_str, uint16_t port, ETSocketAddress* addr);

/**
 * @brief Unix 도메인 소켓 주소를 생성합니다
 * @param path 소켓 파일 경로
 * @param addr 생성된 주소를 저장할 구조체
 * @return 성공시 ET_SUCCESS, 실패시 오류 코드
 */
ETResult et_create_unix_address(const char* path, ETSocketAddress* addr);

/**
 * @brief 주소가 유효한지 확인합니다
 * @param addr 확인할 주소
 * @return 유효하면 true, 아니면 false
 */
bool et_is_valid_address(const ETSocketAddress* addr);

/**
 * @brief 두 주소가 같은지 비교합니다
 * @param addr1 첫 번째 주소
 * @param addr2 두 번째 주소
 * @return 같으면 true, 다르면 false
 */
bool et_compare_addresses(const ETSocketAddress* addr1, const ETSocketAddress* addr2);

#ifdef __cplusplus
}
#endif

#endif // LIBETUDE_PLATFORM_NETWORK_H