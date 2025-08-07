/**
 * @file network_windows.c
 * @brief Windows 네트워크 구현체
 * @author LibEtude Project
 * @version 1.0.0
 *
 * Windows Winsock API를 사용한 네트워크 추상화 레이어 구현입니다.
 * IOCP(I/O Completion Port)를 통한 비동기 I/O를 지원합니다.
 */

#ifdef _WIN32

#include "libetude/platform/network.h"
#include "libetude/platform/common.h"
#include "libetude/error.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mswsock.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Winsock 라이브러리 링크
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "mswsock.lib")

// ============================================================================
// Windows 특화 구조체 정의
// ============================================================================

/**
 * @brief Windows 소켓 구조체
 */
typedef struct ETWindowsSocket {
    SOCKET socket;                  /**< Windows 소켓 핸들 */
    ETSocketType type;              /**< 소켓 타입 */
    ETSocketState state;            /**< 소켓 상태 */
    ETSocketAddress local_addr;     /**< 로컬 주소 */
    ETSocketAddress remote_addr;    /**< 원격 주소 */
    bool is_nonblocking;            /**< 논블로킹 모드 여부 */
    ETNetworkStats stats;           /**< 네트워크 통계 */
} ETWindowsSocket;

/**
 * @brief Windows I/O 컨텍스트 구조체
 */
typedef struct ETWindowsIOContext {
    HANDLE completion_port;         /**< I/O Completion Port */
    bool is_running;                /**< 실행 중 여부 */
    CRITICAL_SECTION lock;          /**< 동기화용 크리티컬 섹션 */
} ETWindowsIOContext;

/**
 * @brief IOCP 오버랩 구조체
 */
typedef struct ETIOCPOverlapped {
    OVERLAPPED overlapped;          /**< Windows OVERLAPPED 구조체 */
    ETSocket* socket;               /**< 관련 소켓 */
    ETIOEvents events;              /**< 이벤트 타입 */
    void* user_data;                /**< 사용자 데이터 */
    WSABUF wsa_buf;                 /**< WSA 버퍼 */
    char buffer[8192];              /**< 데이터 버퍼 */
    DWORD bytes_transferred;        /**< 전송된 바이트 수 */
    DWORD flags;                    /**< WSA 플래그 */
} ETIOCPOverlapped;

// ============================================================================
// 전역 변수
// ============================================================================

static bool g_winsock_initialized = false;
static WSADATA g_wsa_data;

// ============================================================================
// 내부 함수 선언
// ============================================================================

static ETResult initialize_winsock(void);
static void cleanup_winsock(void);
static ETResult socket_address_to_sockaddr(const ETSocketAddress* et_addr,
                                          struct sockaddr* sockaddr, int* sockaddr_len);
static ETResult sockaddr_to_socket_address(const struct sockaddr* sockaddr,
                                          int sockaddr_len, ETSocketAddress* et_addr);
static int get_socket_family(ETSocketType type);
static int get_socket_type(ETSocketType type);
static int get_socket_protocol(ETSocketType type);
static ETResult set_socket_nonblocking(SOCKET socket, bool nonblocking);
static ETResult handle_winsock_error(const char* operation);

// ============================================================================
// 소켓 관리 함수 구현
// ============================================================================

static ETResult windows_create_socket(ETSocketType type, ETSocket** socket)
{
    if (!socket) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ETResult result = initialize_winsock();
    if (result != ET_SUCCESS) {
        return result;
    }

    // Windows 소켓 구조체 할당
    ETWindowsSocket* win_socket = (ETWindowsSocket*)calloc(1, sizeof(ETWindowsSocket));
    if (!win_socket) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    // 소켓 생성
    int family = get_socket_family(type);
    int sock_type = get_socket_type(type);
    int protocol = get_socket_protocol(type);

    win_socket->socket = WSASocket(family, sock_type, protocol, NULL, 0, WSA_FLAG_OVERLAPPED);
    if (win_socket->socket == INVALID_SOCKET) {
        free(win_socket);
        return handle_winsock_error("WSASocket");
    }

    win_socket->type = type;
    win_socket->state = ET_SOCKET_CLOSED;
    win_socket->is_nonblocking = false;
    memset(&win_socket->stats, 0, sizeof(ETNetworkStats));

    *socket = (ETSocket*)win_socket;
    return ET_SUCCESS;
}

static ETResult windows_bind_socket(ETSocket* socket, const ETSocketAddress* addr)
{
    if (!socket || !addr) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ETWindowsSocket* win_socket = (ETWindowsSocket*)socket;

    struct sockaddr_storage sockaddr;
    int sockaddr_len;
    ETResult result = socket_address_to_sockaddr(addr, (struct sockaddr*)&sockaddr, &sockaddr_len);
    if (result != ET_SUCCESS) {
        return result;
    }

    if (bind(win_socket->socket, (struct sockaddr*)&sockaddr, sockaddr_len) == SOCKET_ERROR) {
        return handle_winsock_error("bind");
    }

    win_socket->local_addr = *addr;
    win_socket->state = ET_SOCKET_BOUND;
    return ET_SUCCESS;
}

static ETResult windows_listen_socket(ETSocket* socket, int backlog)
{
    if (!socket) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ETWindowsSocket* win_socket = (ETWindowsSocket*)socket;

    if (win_socket->state != ET_SOCKET_BOUND) {
        return ET_ERROR_INVALID_STATE;
    }

    if (listen(win_socket->socket, backlog) == SOCKET_ERROR) {
        return handle_winsock_error("listen");
    }

    win_socket->state = ET_SOCKET_LISTENING;
    return ET_SUCCESS;
}

static ETResult windows_accept_socket(ETSocket* socket, ETSocket** client, ETSocketAddress* addr)
{
    if (!socket || !client) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ETWindowsSocket* win_socket = (ETWindowsSocket*)socket;

    if (win_socket->state != ET_SOCKET_LISTENING) {
        return ET_ERROR_INVALID_STATE;
    }

    struct sockaddr_storage client_addr;
    int client_addr_len = sizeof(client_addr);

    SOCKET client_socket = accept(win_socket->socket, (struct sockaddr*)&client_addr, &client_addr_len);
    if (client_socket == INVALID_SOCKET) {
        return handle_winsock_error("accept");
    }

    // 클라이언트 소켓 구조체 생성
    ETWindowsSocket* win_client = (ETWindowsSocket*)calloc(1, sizeof(ETWindowsSocket));
    if (!win_client) {
        closesocket(client_socket);
        return ET_ERROR_OUT_OF_MEMORY;
    }

    win_client->socket = client_socket;
    win_client->type = win_socket->type;
    win_client->state = ET_SOCKET_CONNECTED;
    win_client->is_nonblocking = false;
    memset(&win_client->stats, 0, sizeof(ETNetworkStats));

    // 클라이언트 주소 변환
    if (addr) {
        sockaddr_to_socket_address((struct sockaddr*)&client_addr, client_addr_len, addr);
        win_client->remote_addr = *addr;
    }

    *client = (ETSocket*)win_client;
    return ET_SUCCESS;
}

static ETResult windows_connect_socket(ETSocket* socket, const ETSocketAddress* addr)
{
    if (!socket || !addr) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ETWindowsSocket* win_socket = (ETWindowsSocket*)socket;

    struct sockaddr_storage sockaddr;
    int sockaddr_len;
    ETResult result = socket_address_to_sockaddr(addr, (struct sockaddr*)&sockaddr, &sockaddr_len);
    if (result != ET_SUCCESS) {
        return result;
    }

    win_socket->state = ET_SOCKET_CONNECTING;

    if (connect(win_socket->socket, (struct sockaddr*)&sockaddr, sockaddr_len) == SOCKET_ERROR) {
        int error = WSAGetLastError();
        if (error == WSAEWOULDBLOCK || error == WSAEINPROGRESS) {
            // 논블로킹 모드에서는 진행 중 상태 유지
            return ET_SUCCESS;
        }
        win_socket->state = ET_SOCKET_ERROR;
        return handle_winsock_error("connect");
    }

    win_socket->remote_addr = *addr;
    win_socket->state = ET_SOCKET_CONNECTED;
    return ET_SUCCESS;
}

static void windows_close_socket(ETSocket* socket)
{
    if (!socket) {
        return;
    }

    ETWindowsSocket* win_socket = (ETWindowsSocket*)socket;

    if (win_socket->socket != INVALID_SOCKET) {
        closesocket(win_socket->socket);
        win_socket->socket = INVALID_SOCKET;
    }

    win_socket->state = ET_SOCKET_CLOSED;
    free(win_socket);
}

// ============================================================================
// 데이터 전송 함수 구현
// ============================================================================

static ETResult windows_send_data(ETSocket* socket, const void* data, size_t size, size_t* sent)
{
    if (!socket || !data || !sent) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ETWindowsSocket* win_socket = (ETWindowsSocket*)socket;

    if (win_socket->state != ET_SOCKET_CONNECTED) {
        return ET_ERROR_INVALID_STATE;
    }

    int result = send(win_socket->socket, (const char*)data, (int)size, 0);
    if (result == SOCKET_ERROR) {
        int error = WSAGetLastError();
        if (error == WSAEWOULDBLOCK) {
            *sent = 0;
            return ET_SUCCESS;
        }
        return handle_winsock_error("send");
    }

    *sent = (size_t)result;
    win_socket->stats.bytes_sent += *sent;
    win_socket->stats.packets_sent++;
    return ET_SUCCESS;
}

static ETResult windows_receive_data(ETSocket* socket, void* buffer, size_t size, size_t* received)
{
    if (!socket || !buffer || !received) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ETWindowsSocket* win_socket = (ETWindowsSocket*)socket;

    if (win_socket->state != ET_SOCKET_CONNECTED) {
        return ET_ERROR_INVALID_STATE;
    }

    int result = recv(win_socket->socket, (char*)buffer, (int)size, 0);
    if (result == SOCKET_ERROR) {
        int error = WSAGetLastError();
        if (error == WSAEWOULDBLOCK) {
            *received = 0;
            return ET_SUCCESS;
        }
        return handle_winsock_error("recv");
    }

    if (result == 0) {
        // 연결이 정상적으로 종료됨
        win_socket->state = ET_SOCKET_CLOSED;
        *received = 0;
        return ET_SUCCESS;
    }

    *received = (size_t)result;
    win_socket->stats.bytes_received += *received;
    win_socket->stats.packets_received++;
    return ET_SUCCESS;
}

static ETResult windows_send_to(ETSocket* socket, const void* data, size_t size,
                               const ETSocketAddress* addr, size_t* sent)
{
    if (!socket || !data || !addr || !sent) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ETWindowsSocket* win_socket = (ETWindowsSocket*)socket;

    if (win_socket->type != ET_SOCKET_UDP) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    struct sockaddr_storage sockaddr;
    int sockaddr_len;
    ETResult result = socket_address_to_sockaddr(addr, (struct sockaddr*)&sockaddr, &sockaddr_len);
    if (result != ET_SUCCESS) {
        return result;
    }

    int send_result = sendto(win_socket->socket, (const char*)data, (int)size, 0,
                            (struct sockaddr*)&sockaddr, sockaddr_len);
    if (send_result == SOCKET_ERROR) {
        int error = WSAGetLastError();
        if (error == WSAEWOULDBLOCK) {
            *sent = 0;
            return ET_SUCCESS;
        }
        return handle_winsock_error("sendto");
    }

    *sent = (size_t)send_result;
    win_socket->stats.bytes_sent += *sent;
    win_socket->stats.packets_sent++;
    return ET_SUCCESS;
}

static ETResult windows_receive_from(ETSocket* socket, void* buffer, size_t size,
                                    ETSocketAddress* addr, size_t* received)
{
    if (!socket || !buffer || !received) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ETWindowsSocket* win_socket = (ETWindowsSocket*)socket;

    if (win_socket->type != ET_SOCKET_UDP) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    struct sockaddr_storage from_addr;
    int from_addr_len = sizeof(from_addr);

    int recv_result = recvfrom(win_socket->socket, (char*)buffer, (int)size, 0,
                              (struct sockaddr*)&from_addr, &from_addr_len);
    if (recv_result == SOCKET_ERROR) {
        int error = WSAGetLastError();
        if (error == WSAEWOULDBLOCK) {
            *received = 0;
            return ET_SUCCESS;
        }
        return handle_winsock_error("recvfrom");
    }

    *received = (size_t)recv_result;
    win_socket->stats.bytes_received += *received;
    win_socket->stats.packets_received++;

    // 송신자 주소 변환
    if (addr) {
        sockaddr_to_socket_address((struct sockaddr*)&from_addr, from_addr_len, addr);
    }

    return ET_SUCCESS;
}

// ============================================================================
// 소켓 옵션 및 상태 함수 구현
// ============================================================================

static ETResult windows_set_socket_option(ETSocket* socket, ETSocketOption option,
                                         const void* value, size_t size)
{
    if (!socket || !value) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ETWindowsSocket* win_socket = (ETWindowsSocket*)socket;
    int level, optname;
    const char* optval = (const char*)value;
    int optlen = (int)size;

    switch (option) {
        case ET_SOCKET_OPT_REUSEADDR:
            level = SOL_SOCKET;
            optname = SO_REUSEADDR;
            break;
        case ET_SOCKET_OPT_KEEPALIVE:
            level = SOL_SOCKET;
            optname = SO_KEEPALIVE;
            break;
        case ET_SOCKET_OPT_NODELAY:
            level = IPPROTO_TCP;
            optname = TCP_NODELAY;
            break;
        case ET_SOCKET_OPT_NONBLOCK:
            return set_socket_nonblocking(win_socket->socket, *(bool*)value);
        case ET_SOCKET_OPT_RCVBUF:
            level = SOL_SOCKET;
            optname = SO_RCVBUF;
            break;
        case ET_SOCKET_OPT_SNDBUF:
            level = SOL_SOCKET;
            optname = SO_SNDBUF;
            break;
        case ET_SOCKET_OPT_RCVTIMEO:
            level = SOL_SOCKET;
            optname = SO_RCVTIMEO;
            break;
        case ET_SOCKET_OPT_SNDTIMEO:
            level = SOL_SOCKET;
            optname = SO_SNDTIMEO;
            break;
        default:
            return ET_ERROR_UNSUPPORTED;
    }

    if (setsockopt(win_socket->socket, level, optname, optval, optlen) == SOCKET_ERROR) {
        return handle_winsock_error("setsockopt");
    }

    if (option == ET_SOCKET_OPT_NONBLOCK) {
        win_socket->is_nonblocking = *(bool*)value;
    }

    return ET_SUCCESS;
}

static ETResult windows_get_socket_option(ETSocket* socket, ETSocketOption option,
                                        void* value, size_t* size)
{
    if (!socket || !value || !size) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ETWindowsSocket* win_socket = (ETWindowsSocket*)socket;
    int level, optname;
    char* optval = (char*)value;
    int optlen = (int)*size;

    switch (option) {
        case ET_SOCKET_OPT_REUSEADDR:
            level = SOL_SOCKET;
            optname = SO_REUSEADDR;
            break;
        case ET_SOCKET_OPT_KEEPALIVE:
            level = SOL_SOCKET;
            optname = SO_KEEPALIVE;
            break;
        case ET_SOCKET_OPT_NODELAY:
            level = IPPROTO_TCP;
            optname = TCP_NODELAY;
            break;
        case ET_SOCKET_OPT_NONBLOCK:
            *(bool*)value = win_socket->is_nonblocking;
            *size = sizeof(bool);
            return ET_SUCCESS;
        case ET_SOCKET_OPT_RCVBUF:
            level = SOL_SOCKET;
            optname = SO_RCVBUF;
            break;
        case ET_SOCKET_OPT_SNDBUF:
            level = SOL_SOCKET;
            optname = SO_SNDBUF;
            break;
        case ET_SOCKET_OPT_RCVTIMEO:
            level = SOL_SOCKET;
            optname = SO_RCVTIMEO;
            break;
        case ET_SOCKET_OPT_SNDTIMEO:
            level = SOL_SOCKET;
            optname = SO_SNDTIMEO;
            break;
        default:
            return ET_ERROR_UNSUPPORTED;
    }

    if (getsockopt(win_socket->socket, level, optname, optval, &optlen) == SOCKET_ERROR) {
        return handle_winsock_error("getsockopt");
    }

    *size = (size_t)optlen;
    return ET_SUCCESS;
}

static ETSocketState windows_get_socket_state(const ETSocket* socket)
{
    if (!socket) {
        return ET_SOCKET_ERROR;
    }

    const ETWindowsSocket* win_socket = (const ETWindowsSocket*)socket;
    return win_socket->state;
}

static ETResult windows_get_local_address(const ETSocket* socket, ETSocketAddress* addr)
{
    if (!socket || !addr) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    const ETWindowsSocket* win_socket = (const ETWindowsSocket*)socket;

    struct sockaddr_storage sockaddr;
    int sockaddr_len = sizeof(sockaddr);

    if (getsockname(win_socket->socket, (struct sockaddr*)&sockaddr, &sockaddr_len) == SOCKET_ERROR) {
        return handle_winsock_error("getsockname");
    }

    return sockaddr_to_socket_address((struct sockaddr*)&sockaddr, sockaddr_len, addr);
}

static ETResult windows_get_remote_address(const ETSocket* socket, ETSocketAddress* addr)
{
    if (!socket || !addr) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    const ETWindowsSocket* win_socket = (const ETWindowsSocket*)socket;

    if (win_socket->state != ET_SOCKET_CONNECTED) {
        return ET_ERROR_INVALID_STATE;
    }

    struct sockaddr_storage sockaddr;
    int sockaddr_len = sizeof(sockaddr);

    if (getpeername(win_socket->socket, (struct sockaddr*)&sockaddr, &sockaddr_len) == SOCKET_ERROR) {
        return handle_winsock_error("getpeername");
    }

    return sockaddr_to_socket_address((struct sockaddr*)&sockaddr, sockaddr_len, addr);
}

// ============================================================================
// 비동기 I/O 함수 구현 (IOCP 기반)
// ============================================================================

static ETResult windows_create_io_context(ETIOContext** context)
{
    if (!context) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ETWindowsIOContext* win_context = (ETWindowsIOContext*)calloc(1, sizeof(ETWindowsIOContext));
    if (!win_context) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    // I/O Completion Port 생성
    win_context->completion_port = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
    if (!win_context->completion_port) {
        free(win_context);
        return ET_ERROR_SYSTEM;
    }

    win_context->is_running = true;
    InitializeCriticalSection(&win_context->lock);

    *context = (ETIOContext*)win_context;
    return ET_SUCCESS;
}

static ETResult windows_register_socket(ETIOContext* context, ETSocket* socket,
                                       ETIOEvents events, void* user_data)
{
    if (!context || !socket) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ETWindowsIOContext* win_context = (ETWindowsIOContext*)context;
    ETWindowsSocket* win_socket = (ETWindowsSocket*)socket;

    // 소켓을 IOCP에 연결
    HANDLE result = CreateIoCompletionPort((HANDLE)win_socket->socket,
                                          win_context->completion_port,
                                          (ULONG_PTR)socket, 0);
    if (!result) {
        return ET_ERROR_SYSTEM;
    }

    return ET_SUCCESS;
}

static ETResult windows_modify_socket_events(ETIOContext* context, ETSocket* socket, ETIOEvents events)
{
    // Windows IOCP에서는 이벤트 수정이 필요하지 않음
    // 모든 I/O 작업이 완료 포트를 통해 처리됨
    return ET_SUCCESS;
}

static ETResult windows_unregister_socket(ETIOContext* context, ETSocket* socket)
{
    // Windows IOCP에서는 소켓을 명시적으로 제거할 필요 없음
    // 소켓이 닫히면 자동으로 제거됨
    return ET_SUCCESS;
}

static ETResult windows_wait_events(ETIOContext* context, ETIOEvent* events,
                                   int max_events, int timeout, int* num_events)
{
    if (!context || !events || !num_events) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ETWindowsIOContext* win_context = (ETWindowsIOContext*)context;
    *num_events = 0;

    DWORD timeout_ms = (timeout < 0) ? INFINITE : (DWORD)timeout;

    for (int i = 0; i < max_events && win_context->is_running; i++) {
        DWORD bytes_transferred;
        ULONG_PTR completion_key;
        LPOVERLAPPED overlapped;

        BOOL result = GetQueuedCompletionStatus(win_context->completion_port,
                                               &bytes_transferred,
                                               &completion_key,
                                               &overlapped,
                                               (i == 0) ? timeout_ms : 0);

        if (!result && !overlapped) {
            // 타임아웃 또는 오류
            if (GetLastError() == WAIT_TIMEOUT) {
                break;
            }
            return ET_ERROR_SYSTEM;
        }

        if (overlapped) {
            ETIOCPOverlapped* iocp_overlapped = CONTAINING_RECORD(overlapped, ETIOCPOverlapped, overlapped);

            events[i].socket = iocp_overlapped->socket;
            events[i].events = iocp_overlapped->events;
            events[i].user_data = iocp_overlapped->user_data;
            events[i].error_code = result ? 0 : GetLastError();

            (*num_events)++;
            free(iocp_overlapped);
        }
    }

    return ET_SUCCESS;
}

static void windows_destroy_io_context(ETIOContext* context)
{
    if (!context) {
        return;
    }

    ETWindowsIOContext* win_context = (ETWindowsIOContext*)context;

    win_context->is_running = false;

    if (win_context->completion_port) {
        CloseHandle(win_context->completion_port);
    }

    DeleteCriticalSection(&win_context->lock);
    free(win_context);
}

// ============================================================================
// 주소 처리 함수 구현
// ============================================================================

static ETResult windows_string_to_address(ETAddressFamily family, const char* str_addr, ETSocketAddress* addr)
{
    if (!str_addr || !addr) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    memset(addr, 0, sizeof(ETSocketAddress));
    addr->family = family;

    switch (family) {
        case ET_AF_INET: {
            struct sockaddr_in sin;
            memset(&sin, 0, sizeof(sin));
            sin.sin_family = AF_INET;

            if (InetPtonA(AF_INET, str_addr, &sin.sin_addr) != 1) {
                return ET_ERROR_INVALID_ARGUMENT;
            }

            addr->ipv4.addr = sin.sin_addr.s_addr;
            return ET_SUCCESS;
        }

        case ET_AF_INET6: {
            struct sockaddr_in6 sin6;
            memset(&sin6, 0, sizeof(sin6));
            sin6.sin6_family = AF_INET6;

            if (InetPtonA(AF_INET6, str_addr, &sin6.sin6_addr) != 1) {
                return ET_ERROR_INVALID_ARGUMENT;
            }

            memcpy(addr->ipv6.addr, &sin6.sin6_addr, 16);
            return ET_SUCCESS;
        }

        default:
            return ET_ERROR_UNSUPPORTED;
    }
}

static ETResult windows_address_to_string(const ETSocketAddress* addr, char* str_addr, size_t size)
{
    if (!addr || !str_addr || size == 0) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    switch (addr->family) {
        case ET_AF_INET: {
            struct in_addr in_addr;
            in_addr.s_addr = addr->ipv4.addr;

            if (InetNtopA(AF_INET, &in_addr, str_addr, (ULONG)size) == NULL) {
                return ET_ERROR_INVALID_ARGUMENT;
            }
            return ET_SUCCESS;
        }

        case ET_AF_INET6: {
            struct in6_addr in6_addr;
            memcpy(&in6_addr, addr->ipv6.addr, 16);

            if (InetNtopA(AF_INET6, &in6_addr, str_addr, (ULONG)size) == NULL) {
                return ET_ERROR_INVALID_ARGUMENT;
            }
            return ET_SUCCESS;
        }

        default:
            return ET_ERROR_UNSUPPORTED;
    }
}

static ETResult windows_resolve_hostname(const char* hostname, ETAddressFamily family,
                                        ETSocketAddress* addresses, int max_addresses, int* num_addresses)
{
    if (!hostname || !addresses || !num_addresses) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    struct addrinfo hints, *result, *ptr;
    memset(&hints, 0, sizeof(hints));

    switch (family) {
        case ET_AF_INET:
            hints.ai_family = AF_INET;
            break;
        case ET_AF_INET6:
            hints.ai_family = AF_INET6;
            break;
        default:
            hints.ai_family = AF_UNSPEC;
            break;
    }

    int ret = getaddrinfo(hostname, NULL, &hints, &result);
    if (ret != 0) {
        return ET_ERROR_NOT_FOUND;
    }

    int count = 0;
    for (ptr = result; ptr != NULL && count < max_addresses; ptr = ptr->ai_next) {
        ETResult conv_result = sockaddr_to_socket_address(ptr->ai_addr, (int)ptr->ai_addrlen, &addresses[count]);
        if (conv_result == ET_SUCCESS) {
            count++;
        }
    }

    freeaddrinfo(result);
    *num_addresses = count;
    return (count > 0) ? ET_SUCCESS : ET_ERROR_NOT_FOUND;
}

// ============================================================================
// 유틸리티 함수 구현
// ============================================================================

static ETResult windows_get_network_stats(const ETSocket* socket, ETNetworkStats* stats)
{
    if (!stats) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    if (socket) {
        const ETWindowsSocket* win_socket = (const ETWindowsSocket*)socket;
        *stats = win_socket->stats;
    } else {
        // 전체 시스템 네트워크 통계는 구현하지 않음
        memset(stats, 0, sizeof(ETNetworkStats));
    }

    return ET_SUCCESS;
}

static int windows_get_last_network_error(void)
{
    return WSAGetLastError();
}

static const char* windows_get_network_error_string(int error_code)
{
    static char error_buffer[256];

    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                   NULL, error_code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                   error_buffer, sizeof(error_buffer), NULL);

    return error_buffer;
}

// ============================================================================
// Windows 네트워크 인터페이스 구조체
// ============================================================================

static const ETNetworkInterface windows_network_interface = {
    // 소켓 관리
    .create_socket = windows_create_socket,
    .bind_socket = windows_bind_socket,
    .listen_socket = windows_listen_socket,
    .accept_socket = windows_accept_socket,
    .connect_socket = windows_connect_socket,
    .close_socket = windows_close_socket,

    // 데이터 전송
    .send_data = windows_send_data,
    .receive_data = windows_receive_data,
    .send_to = windows_send_to,
    .receive_from = windows_receive_from,

    // 소켓 옵션 및 상태
    .set_socket_option = windows_set_socket_option,
    .get_socket_option = windows_get_socket_option,
    .get_socket_state = windows_get_socket_state,
    .get_local_address = windows_get_local_address,
    .get_remote_address = windows_get_remote_address,

    // 비동기 I/O
    .create_io_context = windows_create_io_context,
    .register_socket = windows_register_socket,
    .modify_socket_events = windows_modify_socket_events,
    .unregister_socket = windows_unregister_socket,
    .wait_events = windows_wait_events,
    .destroy_io_context = windows_destroy_io_context,

    // 주소 처리
    .string_to_address = windows_string_to_address,
    .address_to_string = windows_address_to_string,
    .resolve_hostname = windows_resolve_hostname,

    // 유틸리티
    .get_network_stats = windows_get_network_stats,
    .get_last_network_error = windows_get_last_network_error,
    .get_network_error_string = windows_get_network_error_string,

    .platform_data = NULL
};

// ============================================================================
// 공개 함수 구현
// ============================================================================

const ETNetworkInterface* et_get_windows_network_interface(void)
{
    return &windows_network_interface;
}

// ============================================================================
// 내부 함수 구현
// ============================================================================

static ETResult initialize_winsock(void)
{
    if (g_winsock_initialized) {
        return ET_SUCCESS;
    }

    int result = WSAStartup(MAKEWORD(2, 2), &g_wsa_data);
    if (result != 0) {
        return ET_ERROR_SYSTEM;
    }

    g_winsock_initialized = true;
    return ET_SUCCESS;
}

static void cleanup_winsock(void)
{
    if (g_winsock_initialized) {
        WSACleanup();
        g_winsock_initialized = false;
    }
}

static ETResult socket_address_to_sockaddr(const ETSocketAddress* et_addr,
                                          struct sockaddr* sockaddr, int* sockaddr_len)
{
    if (!et_addr || !sockaddr || !sockaddr_len) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    switch (et_addr->family) {
        case ET_AF_INET: {
            struct sockaddr_in* sin = (struct sockaddr_in*)sockaddr;
            memset(sin, 0, sizeof(struct sockaddr_in));
            sin->sin_family = AF_INET;
            sin->sin_addr.s_addr = et_addr->ipv4.addr;
            sin->sin_port = htons(et_addr->ipv4.port);
            *sockaddr_len = sizeof(struct sockaddr_in);
            return ET_SUCCESS;
        }

        case ET_AF_INET6: {
            struct sockaddr_in6* sin6 = (struct sockaddr_in6*)sockaddr;
            memset(sin6, 0, sizeof(struct sockaddr_in6));
            sin6->sin6_family = AF_INET6;
            memcpy(&sin6->sin6_addr, et_addr->ipv6.addr, 16);
            sin6->sin6_port = htons(et_addr->ipv6.port);
            sin6->sin6_flowinfo = et_addr->ipv6.flowinfo;
            sin6->sin6_scope_id = et_addr->ipv6.scope_id;
            *sockaddr_len = sizeof(struct sockaddr_in6);
            return ET_SUCCESS;
        }

        default:
            return ET_ERROR_UNSUPPORTED;
    }
}

static ETResult sockaddr_to_socket_address(const struct sockaddr* sockaddr,
                                          int sockaddr_len, ETSocketAddress* et_addr)
{
    if (!sockaddr || !et_addr) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    memset(et_addr, 0, sizeof(ETSocketAddress));

    switch (sockaddr->sa_family) {
        case AF_INET: {
            const struct sockaddr_in* sin = (const struct sockaddr_in*)sockaddr;
            et_addr->family = ET_AF_INET;
            et_addr->ipv4.addr = sin->sin_addr.s_addr;
            et_addr->ipv4.port = ntohs(sin->sin_port);
            return ET_SUCCESS;
        }

        case AF_INET6: {
            const struct sockaddr_in6* sin6 = (const struct sockaddr_in6*)sockaddr;
            et_addr->family = ET_AF_INET6;
            memcpy(et_addr->ipv6.addr, &sin6->sin6_addr, 16);
            et_addr->ipv6.port = ntohs(sin6->sin6_port);
            et_addr->ipv6.flowinfo = sin6->sin6_flowinfo;
            et_addr->ipv6.scope_id = sin6->sin6_scope_id;
            return ET_SUCCESS;
        }

        default:
            return ET_ERROR_UNSUPPORTED;
    }
}

static int get_socket_family(ETSocketType type)
{
    switch (type) {
        case ET_SOCKET_TCP:
        case ET_SOCKET_UDP:
            return AF_INET;
        case ET_SOCKET_RAW:
            return AF_INET;
        default:
            return AF_INET;
    }
}

static int get_socket_type(ETSocketType type)
{
    switch (type) {
        case ET_SOCKET_TCP:
            return SOCK_STREAM;
        case ET_SOCKET_UDP:
            return SOCK_DGRAM;
        case ET_SOCKET_RAW:
            return SOCK_RAW;
        default:
            return SOCK_STREAM;
    }
}

static int get_socket_protocol(ETSocketType type)
{
    switch (type) {
        case ET_SOCKET_TCP:
            return IPPROTO_TCP;
        case ET_SOCKET_UDP:
            return IPPROTO_UDP;
        case ET_SOCKET_RAW:
            return IPPROTO_RAW;
        default:
            return 0;
    }
}

static ETResult set_socket_nonblocking(SOCKET socket, bool nonblocking)
{
    u_long mode = nonblocking ? 1 : 0;
    if (ioctlsocket(socket, FIONBIO, &mode) == SOCKET_ERROR) {
        return handle_winsock_error("ioctlsocket");
    }
    return ET_SUCCESS;
}

static ETResult handle_winsock_error(const char* operation)
{
    int error = WSAGetLastError();
    ETResult result = et_network_error_to_common(error);

    ET_SET_ERROR(result, "%s 실패: %s (%d)", operation,
                windows_get_network_error_string(error), error);

    return result;
}

#endif // _WIN32