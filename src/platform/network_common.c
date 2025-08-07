/**
 * @file network_common.c
 * @brief 네트워크 추상화 레이어 공통 구현
 * @author LibEtude Project
 * @version 1.0.0
 *
 * 플랫폼별 네트워크 구현에서 공통으로 사용되는 함수들을 구현합니다.
 */

#include "libetude/platform/network.h"
#include "libetude/platform/common.h"
#include "libetude/platform/factory.h"
#include "libetude/error.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <netdb.h>
    #include <errno.h>
#endif

// ============================================================================
// 전역 변수
// ============================================================================

static const ETNetworkInterface* g_network_interface = NULL;
static bool g_network_initialized = false;

// ============================================================================
// 오류 매핑 테이블
// ============================================================================

/**
 * @brief 플랫폼별 네트워크 오류 매핑 테이블
 */
static const ETErrorMapping network_error_mappings[] = {
#ifdef _WIN32
    { WSAEACCES, ET_ERROR_INVALID_ARGUMENT, "권한 거부" },
    { WSAEADDRINUSE, ET_ERROR_INVALID_STATE, "주소가 이미 사용 중" },
    { WSAEADDRNOTAVAIL, ET_ERROR_NOT_FOUND, "주소를 사용할 수 없음" },
    { WSAEAFNOSUPPORT, ET_ERROR_UNSUPPORTED, "주소 패밀리가 지원되지 않음" },
    { WSAEALREADY, ET_ERROR_INVALID_STATE, "이미 진행 중인 작업" },
    { WSAEBADF, ET_ERROR_INVALID_ARGUMENT, "잘못된 소켓" },
    { WSAECONNABORTED, ET_ERROR_IO, "연결이 중단됨" },
    { WSAECONNREFUSED, ET_ERROR_IO, "연결이 거부됨" },
    { WSAECONNRESET, ET_ERROR_IO, "연결이 재설정됨" },
    { WSAEFAULT, ET_ERROR_INVALID_ARGUMENT, "잘못된 주소" },
    { WSAEHOSTDOWN, ET_ERROR_IO, "호스트가 다운됨" },
    { WSAEHOSTUNREACH, ET_ERROR_IO, "호스트에 도달할 수 없음" },
    { WSAEINPROGRESS, ET_ERROR_TIMEOUT, "작업이 진행 중" },
    { WSAEINTR, ET_ERROR_RUNTIME, "시스템 호출이 중단됨" },
    { WSAEINVAL, ET_ERROR_INVALID_ARGUMENT, "잘못된 인수" },
    { WSAEISCONN, ET_ERROR_INVALID_STATE, "이미 연결됨" },
    { WSAEMFILE, ET_ERROR_OUT_OF_MEMORY, "파일 디스크립터 부족" },
    { WSAEMSGSIZE, ET_ERROR_INVALID_ARGUMENT, "메시지가 너무 큼" },
    { WSAENETDOWN, ET_ERROR_IO, "네트워크가 다운됨" },
    { WSAENETUNREACH, ET_ERROR_IO, "네트워크에 도달할 수 없음" },
    { WSAENOBUFS, ET_ERROR_OUT_OF_MEMORY, "버퍼 공간 부족" },
    { WSAENOPROTOOPT, ET_ERROR_UNSUPPORTED, "프로토콜 옵션이 지원되지 않음" },
    { WSAENOTCONN, ET_ERROR_INVALID_STATE, "연결되지 않음" },
    { WSAENOTSOCK, ET_ERROR_INVALID_ARGUMENT, "소켓이 아님" },
    { WSAEOPNOTSUPP, ET_ERROR_UNSUPPORTED, "작업이 지원되지 않음" },
    { WSAEPROTONOSUPPORT, ET_ERROR_UNSUPPORTED, "프로토콜이 지원되지 않음" },
    { WSAEPROTOTYPE, ET_ERROR_INVALID_ARGUMENT, "잘못된 프로토콜 타입" },
    { WSAESHUTDOWN, ET_ERROR_INVALID_STATE, "소켓이 종료됨" },
    { WSAESOCKTNOSUPPORT, ET_ERROR_UNSUPPORTED, "소켓 타입이 지원되지 않음" },
    { WSAETIMEDOUT, ET_ERROR_TIMEOUT, "연결 시간 초과" },
    { WSAEWOULDBLOCK, ET_ERROR_TIMEOUT, "작업이 블록됨" },
    { WSANOTINITIALISED, ET_ERROR_NOT_INITIALIZED, "Winsock이 초기화되지 않음" }
#else
    { EACCES, ET_ERROR_INVALID_ARGUMENT, "권한 거부" },
    { EADDRINUSE, ET_ERROR_INVALID_STATE, "주소가 이미 사용 중" },
    { EADDRNOTAVAIL, ET_ERROR_NOT_FOUND, "주소를 사용할 수 없음" },
    { EAFNOSUPPORT, ET_ERROR_UNSUPPORTED, "주소 패밀리가 지원되지 않음" },
    { EAGAIN, ET_ERROR_TIMEOUT, "리소스를 일시적으로 사용할 수 없음" },
    { EALREADY, ET_ERROR_INVALID_STATE, "이미 진행 중인 작업" },
    { EBADF, ET_ERROR_INVALID_ARGUMENT, "잘못된 파일 디스크립터" },
    { ECONNABORTED, ET_ERROR_IO, "연결이 중단됨" },
    { ECONNREFUSED, ET_ERROR_IO, "연결이 거부됨" },
    { ECONNRESET, ET_ERROR_IO, "연결이 재설정됨" },
    { EFAULT, ET_ERROR_INVALID_ARGUMENT, "잘못된 주소" },
    { EHOSTDOWN, ET_ERROR_IO, "호스트가 다운됨" },
    { EHOSTUNREACH, ET_ERROR_IO, "호스트에 도달할 수 없음" },
    { EINPROGRESS, ET_ERROR_TIMEOUT, "작업이 진행 중" },
    { EINTR, ET_ERROR_RUNTIME, "시스템 호출이 중단됨" },
    { EINVAL, ET_ERROR_INVALID_ARGUMENT, "잘못된 인수" },
    { EISCONN, ET_ERROR_INVALID_STATE, "이미 연결됨" },
    { EMFILE, ET_ERROR_OUT_OF_MEMORY, "파일 디스크립터 부족" },
    { EMSGSIZE, ET_ERROR_INVALID_ARGUMENT, "메시지가 너무 큼" },
    { ENETDOWN, ET_ERROR_IO, "네트워크가 다운됨" },
    { ENETUNREACH, ET_ERROR_IO, "네트워크에 도달할 수 없음" },
    { ENOBUFS, ET_ERROR_OUT_OF_MEMORY, "버퍼 공간 부족" },
    { ENOPROTOOPT, ET_ERROR_UNSUPPORTED, "프로토콜 옵션이 지원되지 않음" },
    { ENOTCONN, ET_ERROR_INVALID_STATE, "연결되지 않음" },
    { ENOTSOCK, ET_ERROR_INVALID_ARGUMENT, "소켓이 아님" },
    { EOPNOTSUPP, ET_ERROR_UNSUPPORTED, "작업이 지원되지 않음" },
    { EPROTONOSUPPORT, ET_ERROR_UNSUPPORTED, "프로토콜이 지원되지 않음" },
    { EPROTOTYPE, ET_ERROR_INVALID_ARGUMENT, "잘못된 프로토콜 타입" },
    { ESHUTDOWN, ET_ERROR_INVALID_STATE, "소켓이 종료됨" },
    { ESOCKTNOSUPPORT, ET_ERROR_UNSUPPORTED, "소켓 타입이 지원되지 않음" },
    { ETIMEDOUT, ET_ERROR_TIMEOUT, "연결 시간 초과" },
    { EWOULDBLOCK, ET_ERROR_TIMEOUT, "작업이 블록됨" }
#endif
};

static const size_t network_error_mapping_count =
    sizeof(network_error_mappings) / sizeof(network_error_mappings[0]);

// ============================================================================
// 내부 함수 선언
// ============================================================================

static ETResult validate_address(const ETSocketAddress* addr);
static ETResult parse_ipv4_string(const char* str, uint32_t* addr);
static ETResult parse_ipv6_string(const char* str, uint8_t* addr);

// ============================================================================
// 공개 함수 구현
// ============================================================================

ETResult et_network_initialize(void)
{
    if (g_network_initialized) {
        ET_LOG_WARNING("네트워크 추상화 레이어가 이미 초기화됨");
        return ET_SUCCESS;
    }

    // 플랫폼별 네트워크 인터페이스 가져오기
    const ETPlatformFactory* factory = et_get_platform_factory();
    if (!factory || !factory->create_network_interface) {
        ET_SET_ERROR(ET_ERROR_NOT_IMPLEMENTED, "네트워크 인터페이스가 구현되지 않음");
        return ET_ERROR_NOT_IMPLEMENTED;
    }

    ETResult result = factory->create_network_interface((void**)&g_network_interface);
    if (result != ET_SUCCESS) {
        ET_SET_ERROR(result, "네트워크 인터페이스 생성 실패");
        return result;
    }

    g_network_initialized = true;
    ET_LOG_INFO("네트워크 추상화 레이어 초기화 완료");

    return ET_SUCCESS;
}

const ETNetworkInterface* et_get_network_interface(void)
{
    if (!g_network_initialized) {
        ET_SET_ERROR(ET_ERROR_NOT_INITIALIZED, "네트워크 추상화 레이어가 초기화되지 않음");
        return NULL;
    }

    return g_network_interface;
}

void et_network_finalize(void)
{
    if (!g_network_initialized) {
        return;
    }

    // 플랫폼별 정리 작업은 팩토리에서 처리
    const ETPlatformFactory* factory = et_get_platform_factory();
    if (factory && factory->destroy_network_interface) {
        factory->destroy_network_interface((void*)g_network_interface);
    }

    g_network_interface = NULL;
    g_network_initialized = false;

    ET_LOG_INFO("네트워크 추상화 레이어 정리 완료");
}

ETResult et_network_error_to_common(int platform_error)
{
    // 오류 매핑 테이블에서 검색
    for (size_t i = 0; i < network_error_mapping_count; i++) {
        if (network_error_mappings[i].platform_error == platform_error) {
            return network_error_mappings[i].common_error;
        }
    }

    // 매핑되지 않은 오류는 일반적인 시스템 오류로 처리
    return ET_ERROR_SYSTEM;
}

ETResult et_create_ipv4_address(const char* ip_str, uint16_t port, ETSocketAddress* addr)
{
    if (!ip_str || !addr) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    memset(addr, 0, sizeof(ETSocketAddress));
    addr->family = ET_AF_INET;
    addr->ipv4.port = port;

    // "any" 주소 처리
    if (strcmp(ip_str, "0.0.0.0") == 0 || strcmp(ip_str, "any") == 0) {
        addr->ipv4.addr = 0; // INADDR_ANY
        return ET_SUCCESS;
    }

    // "localhost" 처리
    if (strcmp(ip_str, "localhost") == 0) {
        addr->ipv4.addr = htonl(0x7F000001); // 127.0.0.1
        return ET_SUCCESS;
    }

    // IP 주소 문자열 파싱
    return parse_ipv4_string(ip_str, &addr->ipv4.addr);
}

ETResult et_create_ipv6_address(const char* ip_str, uint16_t port, ETSocketAddress* addr)
{
    if (!ip_str || !addr) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    memset(addr, 0, sizeof(ETSocketAddress));
    addr->family = ET_AF_INET6;
    addr->ipv6.port = port;

    // "any" 주소 처리
    if (strcmp(ip_str, "::") == 0 || strcmp(ip_str, "any") == 0) {
        memset(addr->ipv6.addr, 0, 16); // IN6ADDR_ANY
        return ET_SUCCESS;
    }

    // "localhost" 처리
    if (strcmp(ip_str, "::1") == 0 || strcmp(ip_str, "localhost") == 0) {
        memset(addr->ipv6.addr, 0, 15);
        addr->ipv6.addr[15] = 1; // ::1
        return ET_SUCCESS;
    }

    // IP 주소 문자열 파싱
    return parse_ipv6_string(ip_str, addr->ipv6.addr);
}

ETResult et_create_unix_address(const char* path, ETSocketAddress* addr)
{
    if (!path || !addr) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    size_t path_len = strlen(path);
    if (path_len >= sizeof(addr->unix_addr.path)) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    memset(addr, 0, sizeof(ETSocketAddress));
    addr->family = ET_AF_UNIX;
    strncpy(addr->unix_addr.path, path, sizeof(addr->unix_addr.path) - 1);

    return ET_SUCCESS;
}

bool et_is_valid_address(const ETSocketAddress* addr)
{
    if (!addr) {
        return false;
    }

    switch (addr->family) {
        case ET_AF_INET:
            return true; // IPv4는 모든 값이 유효

        case ET_AF_INET6:
            return true; // IPv6도 모든 값이 유효

        case ET_AF_UNIX:
            return strlen(addr->unix_addr.path) > 0;

        default:
            return false;
    }
}

bool et_compare_addresses(const ETSocketAddress* addr1, const ETSocketAddress* addr2)
{
    if (!addr1 || !addr2) {
        return false;
    }

    if (addr1->family != addr2->family) {
        return false;
    }

    switch (addr1->family) {
        case ET_AF_INET:
            return (addr1->ipv4.addr == addr2->ipv4.addr) &&
                   (addr1->ipv4.port == addr2->ipv4.port);

        case ET_AF_INET6:
            return (memcmp(addr1->ipv6.addr, addr2->ipv6.addr, 16) == 0) &&
                   (addr1->ipv6.port == addr2->ipv6.port) &&
                   (addr1->ipv6.flowinfo == addr2->ipv6.flowinfo) &&
                   (addr1->ipv6.scope_id == addr2->ipv6.scope_id);

        case ET_AF_UNIX:
            return strcmp(addr1->unix_addr.path, addr2->unix_addr.path) == 0;

        default:
            return false;
    }
}

// ============================================================================
// 내부 함수 구현
// ============================================================================

static ETResult validate_address(const ETSocketAddress* addr)
{
    if (!addr) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    switch (addr->family) {
        case ET_AF_INET:
        case ET_AF_INET6:
        case ET_AF_UNIX:
            return ET_SUCCESS;
        default:
            return ET_ERROR_INVALID_ARGUMENT;
    }
}

static ETResult parse_ipv4_string(const char* str, uint32_t* addr)
{
    if (!str || !addr) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    unsigned int a, b, c, d;
    if (sscanf(str, "%u.%u.%u.%u", &a, &b, &c, &d) != 4) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    if (a > 255 || b > 255 || c > 255 || d > 255) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    *addr = htonl((a << 24) | (b << 16) | (c << 8) | d);
    return ET_SUCCESS;
}

static ETResult parse_ipv6_string(const char* str, uint8_t* addr)
{
    if (!str || !addr) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

#ifdef _WIN32
    // Windows에서는 InetPton 사용
    if (InetPtonA(AF_INET6, str, addr) == 1) {
        return ET_SUCCESS;
    }
#else
    // POSIX에서는 inet_pton 사용
    if (inet_pton(AF_INET6, str, addr) == 1) {
        return ET_SUCCESS;
    }
#endif

    return ET_ERROR_INVALID_ARGUMENT;
}