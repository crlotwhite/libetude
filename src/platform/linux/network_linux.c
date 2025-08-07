/**
 * @file network_linux.c
 * @brief Linux 네트워크 구현체
 * @author LibEtude Project
 * @version 1.0.0
 *
 * Linux 소켓 API를 사용한 네트워크 추상화 레이어 구현입니다.
 * epoll을 통한 비동기 I/O를 지원합니다.
 */

#ifdef __linux__

#include "libetude/platform/network.h"
#include "libetude/platform/common.h"
#include "libetude/error.h"
#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// Linux 특화 구조체 정의
// ============================================================================

/**
 * @brief Linux 소켓 구조체
 */
typedef struct ETLinuxSocket {
    int socket_fd;                  /**< Linux 소켓 파일 디스크립터 */
    ETSocketType type;              /**< 소켓 타입 */
    ETSocketState state;            /**< 소켓 상태 */
    ETSocketAddress local_addr;     /**< 로컬 주소 */
    ETSocketAddress remote_addr;    /**< 원격 주소 */
    bool is_nonblocking;            /**< 논블로킹 모드 여부 */
    ETNetworkStats stats;           /**< 네트워크 통계 */
} ETLinuxSocket;

/**
 * @brief Linux I/O 컨텍스트 구조체 (epoll 기반)
 */
typedef struct ETLinuxIOContext {
    int epoll_fd;                   /**< epoll 파일 디스크립터 */
    bool is_running;                /**< 실행 중 여부 */
    pthread_mutex_t lock;           /**< 동기화용 뮤텍스 */
} ETLinuxIOContext;

/**
 * @brief epoll 이벤트 데이터
 */
typedef struct ETEpollEventData {
    ETSocket* socket;               /**< 관련 소켓 */
    void* user_data;                /**< 사용자 데이터 */
} ETEpollEventData;

// ============================================================================
// 내부 함수 선언
// ============================================================================

static ETResult socket_address_to_sockaddr(const ETSocketAddress* et_addr,
                                          struct sockaddr* sockaddr, socklen_t* sockaddr_len);
static ETResult sockaddr_to_socket_address(const struct sockaddr* sockaddr,
                                          socklen_t sockaddr_len, ETSocketAddress* et_addr);
static int get_socket_family(ETSocketType type);
static int get_socket_type(ETSocketType type);
static int get_socket_protocol(ETSocketType type);
static ETResult set_socket_nonblocking(int socket_fd, bool nonblocking);
static ETResult handle_socket_error(const char* operation);
static uint32_t et_events_to_epoll_events(ETIOEvents events);
static ETIOEvents epoll_events_to_et_events(uint32_t epoll_events);

// ============================================================================
// 소켓 관리 함수 구현
// ============================================================================

static ETResult linux_create_socket(ETSocketType type, ETSocket** socket)
{
    if (!socket) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // Linux 소켓 구조체 할당
    ETLinuxSocket* linux_socket = (ETLinuxSocket*)calloc(1, sizeof(ETLinuxSocket));
    if (!linux_socket) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    // 소켓 생성
    int family = get_socket_family(type);
    int sock_type = get_socket_type(type);
    int protocol = get_socket_protocol(type);

    linux_socket->socket_fd = socket(family, sock_type, protocol);
    if (linux_socket->socket_fd < 0) {
        free(linux_socket);
        return handle_socket_error("socket");
    }

    linux_socket->type = type;
    linux_socket->state = ET_SOCKET_CLOSED;
    linux_socket->is_nonblocking = false;
    memset(&linux_socket->stats, 0, sizeof(ETNetworkStats));

    *socket = (ETSocket*)linux_socket;
    return ET_SUCCESS;
}

static ETResult linux_bind_socket(ETSocket* socket, const ETSocketAddress* addr)
{
    if (!socket || !addr) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ETLinuxSocket* linux_socket = (ETLinuxSocket*)socket;

    struct sockaddr_storage sockaddr;
    socklen_t sockaddr_len;
    ETResult result = socket_address_to_sockaddr(addr, (struct sockaddr*)&sockaddr, &sockaddr_len);
    if (result != ET_SUCCESS) {
        return result;
    }

    if (bind(linux_socket->socket_fd, (struct sockaddr*)&sockaddr, sockaddr_len) < 0) {
        return handle_socket_error("bind");
    }

    linux_socket->local_addr = *addr;
    linux_socket->state = ET_SOCKET_BOUND;
    return ET_SUCCESS;
}

static ETResult linux_listen_socket(ETSocket* socket, int backlog)
{
    if (!socket) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ETLinuxSocket* linux_socket = (ETLinuxSocket*)socket;

    if (linux_socket->state != ET_SOCKET_BOUND) {
        return ET_ERROR_INVALID_STATE;
    }

    if (listen(linux_socket->socket_fd, backlog) < 0) {
        return handle_socket_error("listen");
    }

    linux_socket->state = ET_SOCKET_LISTENING;
    return ET_SUCCESS;
}

static ETResult linux_accept_socket(ETSocket* socket, ETSocket** client, ETSocketAddress* addr)
{
    if (!socket || !client) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ETLinuxSocket* linux_socket = (ETLinuxSocket*)socket;

    if (linux_socket->state != ET_SOCKET_LISTENING) {
        return ET_ERROR_INVALID_STATE;
    }

    struct sockaddr_storage client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    int client_fd = accept(linux_socket->socket_fd, (struct sockaddr*)&client_addr, &client_addr_len);
    if (client_fd < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return ET_ERROR_TIMEOUT;
        }
        return handle_socket_error("accept");
    }

    // 클라이언트 소켓 구조체 생성
    ETLinuxSocket* linux_client = (ETLinuxSocket*)calloc(1, sizeof(ETLinuxSocket));
    if (!linux_client) {
        close(client_fd);
        return ET_ERROR_OUT_OF_MEMORY;
    }

    linux_client->socket_fd = client_fd;
    linux_client->type = linux_socket->type;
    linux_client->state = ET_SOCKET_CONNECTED;
    linux_client->is_nonblocking = false;
    memset(&linux_client->stats, 0, sizeof(ETNetworkStats));

    // 클라이언트 주소 변환
    if (addr) {
        sockaddr_to_socket_address((struct sockaddr*)&client_addr, client_addr_len, addr);
        linux_client->remote_addr = *addr;
    }

    *client = (ETSocket*)linux_client;
    return ET_SUCCESS;
}

static ETResult linux_connect_socket(ETSocket* socket, const ETSocketAddress* addr)
{
    if (!socket || !addr) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ETLinuxSocket* linux_socket = (ETLinuxSocket*)socket;

    struct sockaddr_storage sockaddr;
    socklen_t sockaddr_len;
    ETResult result = socket_address_to_sockaddr(addr, (struct sockaddr*)&sockaddr, &sockaddr_len);
    if (result != ET_SUCCESS) {
        return result;
    }

    linux_socket->state = ET_SOCKET_CONNECTING;

    if (connect(linux_socket->socket_fd, (struct sockaddr*)&sockaddr, sockaddr_len) < 0) {
        if (errno == EINPROGRESS) {
            // 논블로킹 모드에서는 진행 중 상태 유지
            return ET_SUCCESS;
        }
        linux_socket->state = ET_SOCKET_ERROR;
        return handle_socket_error("connect");
    }

    linux_socket->remote_addr = *addr;
    linux_socket->state = ET_SOCKET_CONNECTED;
    return ET_SUCCESS;
}

static void linux_close_socket(ETSocket* socket)
{
    if (!socket) {
        return;
    }

    ETLinuxSocket* linux_socket = (ETLinuxSocket*)socket;

    if (linux_socket->socket_fd >= 0) {
        close(linux_socket->socket_fd);
        linux_socket->socket_fd = -1;
    }

    linux_socket->state = ET_SOCKET_CLOSED;
    free(linux_socket);
}

// ============================================================================
// 데이터 전송 함수 구현
// ============================================================================

static ETResult linux_send_data(ETSocket* socket, const void* data, size_t size, size_t* sent)
{
    if (!socket || !data || !sent) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ETLinuxSocket* linux_socket = (ETLinuxSocket*)socket;

    if (linux_socket->state != ET_SOCKET_CONNECTED) {
        return ET_ERROR_INVALID_STATE;
    }

    ssize_t result = send(linux_socket->socket_fd, data, size, MSG_NOSIGNAL);
    if (result < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            *sent = 0;
            return ET_SUCCESS;
        }
        return handle_socket_error("send");
    }

    *sent = (size_t)result;
    linux_socket->stats.bytes_sent += *sent;
    linux_socket->stats.packets_sent++;
    return ET_SUCCESS;
}

static ETResult linux_receive_data(ETSocket* socket, void* buffer, size_t size, size_t* received)
{
    if (!socket || !buffer || !received) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ETLinuxSocket* linux_socket = (ETLinuxSocket*)socket;

    if (linux_socket->state != ET_SOCKET_CONNECTED) {
        return ET_ERROR_INVALID_STATE;
    }

    ssize_t result = recv(linux_socket->socket_fd, buffer, size, 0);
    if (result < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            *received = 0;
            return ET_SUCCESS;
        }
        return handle_socket_error("recv");
    }

    if (result == 0) {
        // 연결이 정상적으로 종료됨
        linux_socket->state = ET_SOCKET_CLOSED;
        *received = 0;
        return ET_SUCCESS;
    }

    *received = (size_t)result;
    linux_socket->stats.bytes_received += *received;
    linux_socket->stats.packets_received++;
    return ET_SUCCESS;
}

static ETResult linux_send_to(ETSocket* socket, const void* data, size_t size,
                             const ETSocketAddress* addr, size_t* sent)
{
    if (!socket || !data || !addr || !sent) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ETLinuxSocket* linux_socket = (ETLinuxSocket*)socket;

    if (linux_socket->type != ET_SOCKET_UDP) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    struct sockaddr_storage sockaddr;
    socklen_t sockaddr_len;
    ETResult result = socket_address_to_sockaddr(addr, (struct sockaddr*)&sockaddr, &sockaddr_len);
    if (result != ET_SUCCESS) {
        return result;
    }

    ssize_t send_result = sendto(linux_socket->socket_fd, data, size, MSG_NOSIGNAL,
                                (struct sockaddr*)&sockaddr, sockaddr_len);
    if (send_result < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            *sent = 0;
            return ET_SUCCESS;
        }
        return handle_socket_error("sendto");
    }

    *sent = (size_t)send_result;
    linux_socket->stats.bytes_sent += *sent;
    linux_socket->stats.packets_sent++;
    return ET_SUCCESS;
}

static ETResult linux_receive_from(ETSocket* socket, void* buffer, size_t size,
                                  ETSocketAddress* addr, size_t* received)
{
    if (!socket || !buffer || !received) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ETLinuxSocket* linux_socket = (ETLinuxSocket*)socket;

    if (linux_socket->type != ET_SOCKET_UDP) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    struct sockaddr_storage from_addr;
    socklen_t from_addr_len = sizeof(from_addr);

    ssize_t recv_result = recvfrom(linux_socket->socket_fd, buffer, size, 0,
                                  (struct sockaddr*)&from_addr, &from_addr_len);
    if (recv_result < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            *received = 0;
            return ET_SUCCESS;
        }
        return handle_socket_error("recvfrom");
    }

    *received = (size_t)recv_result;
    linux_socket->stats.bytes_received += *received;
    linux_socket->stats.packets_received++;

    // 송신자 주소 변환
    if (addr) {
        sockaddr_to_socket_address((struct sockaddr*)&from_addr, from_addr_len, addr);
    }

    return ET_SUCCESS;
}

// ============================================================================
// 소켓 옵션 및 상태 함수 구현
// ============================================================================

static ETResult linux_set_socket_option(ETSocket* socket, ETSocketOption option,
                                       const void* value, size_t size)
{
    if (!socket || !value) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ETLinuxSocket* linux_socket = (ETLinuxSocket*)socket;
    int level, optname;
    const void* optval = value;
    socklen_t optlen = (socklen_t)size;

    switch (option) {
        case ET_SOCKET_OPT_REUSEADDR:
            level = SOL_SOCKET;
            optname = SO_REUSEADDR;
            break;
        case ET_SOCKET_OPT_REUSEPORT:
            level = SOL_SOCKET;
            optname = SO_REUSEPORT;
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
            return set_socket_nonblocking(linux_socket->socket_fd, *(bool*)value);
        case ET_SOCKET_OPT_RCVBUF:
            level = SOL_SOCKET;
            optname = SO_RCVBUF;
            break;
        case ET_SOCKET_OPT_SNDBUF:
            level = SOL_SOCKET;
            optname = SO_SNDBUF;
            break;
        case ET_SOCKET_OPT_RCVTIMEO: {
            level = SOL_SOCKET;
            optname = SO_RCVTIMEO;
            // 밀리초를 timeval로 변환
            int timeout_ms = *(int*)value;
            struct timeval tv;
            tv.tv_sec = timeout_ms / 1000;
            tv.tv_usec = (timeout_ms % 1000) * 1000;
            optval = &tv;
            optlen = sizeof(tv);
            break;
        }
        case ET_SOCKET_OPT_SNDTIMEO: {
            level = SOL_SOCKET;
            optname = SO_SNDTIMEO;
            // 밀리초를 timeval로 변환
            int timeout_ms = *(int*)value;
            struct timeval tv;
            tv.tv_sec = timeout_ms / 1000;
            tv.tv_usec = (timeout_ms % 1000) * 1000;
            optval = &tv;
            optlen = sizeof(tv);
            break;
        }
        default:
            return ET_ERROR_UNSUPPORTED;
    }

    if (setsockopt(linux_socket->socket_fd, level, optname, optval, optlen) < 0) {
        return handle_socket_error("setsockopt");
    }

    if (option == ET_SOCKET_OPT_NONBLOCK) {
        linux_socket->is_nonblocking = *(bool*)value;
    }

    return ET_SUCCESS;
}

static ETResult linux_get_socket_option(ETSocket* socket, ETSocketOption option,
                                       void* value, size_t* size)
{
    if (!socket || !value || !size) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ETLinuxSocket* linux_socket = (ETLinuxSocket*)socket;
    int level, optname;
    void* optval = value;
    socklen_t optlen = (socklen_t)*size;

    switch (option) {
        case ET_SOCKET_OPT_REUSEADDR:
            level = SOL_SOCKET;
            optname = SO_REUSEADDR;
            break;
        case ET_SOCKET_OPT_REUSEPORT:
            level = SOL_SOCKET;
            optname = SO_REUSEPORT;
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
            *(bool*)value = linux_socket->is_nonblocking;
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
        case ET_SOCKET_OPT_RCVTIMEO: {
            level = SOL_SOCKET;
            optname = SO_RCVTIMEO;
            struct timeval tv;
            socklen_t tv_len = sizeof(tv);
            if (getsockopt(linux_socket->socket_fd, level, optname, &tv, &tv_len) < 0) {
                return handle_socket_error("getsockopt");
            }
            // timeval을 밀리초로 변환
            *(int*)value = tv.tv_sec * 1000 + tv.tv_usec / 1000;
            *size = sizeof(int);
            return ET_SUCCESS;
        }
        case ET_SOCKET_OPT_SNDTIMEO: {
            level = SOL_SOCKET;
            optname = SO_SNDTIMEO;
            struct timeval tv;
            socklen_t tv_len = sizeof(tv);
            if (getsockopt(linux_socket->socket_fd, level, optname, &tv, &tv_len) < 0) {
                return handle_socket_error("getsockopt");
            }
            // timeval을 밀리초로 변환
            *(int*)value = tv.tv_sec * 1000 + tv.tv_usec / 1000;
            *size = sizeof(int);
            return ET_SUCCESS;
        }
        default:
            return ET_ERROR_UNSUPPORTED;
    }

    if (getsockopt(linux_socket->socket_fd, level, optname, optval, &optlen) < 0) {
        return handle_socket_error("getsockopt");
    }

    *size = (size_t)optlen;
    return ET_SUCCESS;
}

static ETSocketState linux_get_socket_state(const ETSocket* socket)
{
    if (!socket) {
        return ET_SOCKET_ERROR;
    }

    const ETLinuxSocket* linux_socket = (const ETLinuxSocket*)socket;
    return linux_socket->state;
}

static ETResult linux_get_local_address(const ETSocket* socket, ETSocketAddress* addr)
{
    if (!socket || !addr) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    const ETLinuxSocket* linux_socket = (const ETLinuxSocket*)socket;

    struct sockaddr_storage sockaddr;
    socklen_t sockaddr_len = sizeof(sockaddr);

    if (getsockname(linux_socket->socket_fd, (struct sockaddr*)&sockaddr, &sockaddr_len) < 0) {
        return handle_socket_error("getsockname");
    }

    return sockaddr_to_socket_address((struct sockaddr*)&sockaddr, sockaddr_len, addr);
}

static ETResult linux_get_remote_address(const ETSocket* socket, ETSocketAddress* addr)
{
    if (!socket || !addr) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    const ETLinuxSocket* linux_socket = (const ETLinuxSocket*)socket;

    if (linux_socket->state != ET_SOCKET_CONNECTED) {
        return ET_ERROR_INVALID_STATE;
    }

    struct sockaddr_storage sockaddr;
    socklen_t sockaddr_len = sizeof(sockaddr);

    if (getpeername(linux_socket->socket_fd, (struct sockaddr*)&sockaddr, &sockaddr_len) < 0) {
        return handle_socket_error("getpeername");
    }

    return sockaddr_to_socket_address((struct sockaddr*)&sockaddr, sockaddr_len, addr);
}

// ============================================================================
// 비동기 I/O 함수 구현 (epoll 기반)
// ============================================================================

static ETResult linux_create_io_context(ETIOContext** context)
{
    if (!context) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ETLinuxIOContext* linux_context = (ETLinuxIOContext*)calloc(1, sizeof(ETLinuxIOContext));
    if (!linux_context) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    // epoll 인스턴스 생성
    linux_context->epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (linux_context->epoll_fd < 0) {
        free(linux_context);
        return handle_socket_error("epoll_create1");
    }

    linux_context->is_running = true;
    if (pthread_mutex_init(&linux_context->lock, NULL) != 0) {
        close(linux_context->epoll_fd);
        free(linux_context);
        return ET_ERROR_SYSTEM;
    }

    *context = (ETIOContext*)linux_context;
    return ET_SUCCESS;
}

static ETResult linux_register_socket(ETIOContext* context, ETSocket* socket,
                                     ETIOEvents events, void* user_data)
{
    if (!context || !socket) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ETLinuxIOContext* linux_context = (ETLinuxIOContext*)context;
    ETLinuxSocket* linux_socket = (ETLinuxSocket*)socket;

    // 이벤트 데이터 구조체 생성
    ETEpollEventData* event_data = (ETEpollEventData*)malloc(sizeof(ETEpollEventData));
    if (!event_data) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    event_data->socket = socket;
    event_data->user_data = user_data;

    // epoll 이벤트 설정
    struct epoll_event epoll_event;
    epoll_event.events = et_events_to_epoll_events(events);
    epoll_event.data.ptr = event_data;

    if (epoll_ctl(linux_context->epoll_fd, EPOLL_CTL_ADD, linux_socket->socket_fd, &epoll_event) < 0) {
        free(event_data);
        return handle_socket_error("epoll_ctl");
    }

    return ET_SUCCESS;
}

static ETResult linux_modify_socket_events(ETIOContext* context, ETSocket* socket, ETIOEvents events)
{
    if (!context || !socket) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ETLinuxIOContext* linux_context = (ETLinuxIOContext*)context;
    ETLinuxSocket* linux_socket = (ETLinuxSocket*)socket;

    struct epoll_event epoll_event;
    epoll_event.events = et_events_to_epoll_events(events);
    epoll_event.data.ptr = NULL; // 기존 데이터 유지

    if (epoll_ctl(linux_context->epoll_fd, EPOLL_CTL_MOD, linux_socket->socket_fd, &epoll_event) < 0) {
        return handle_socket_error("epoll_ctl");
    }

    return ET_SUCCESS;
}

static ETResult linux_unregister_socket(ETIOContext* context, ETSocket* socket)
{
    if (!context || !socket) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ETLinuxIOContext* linux_context = (ETLinuxIOContext*)context;
    ETLinuxSocket* linux_socket = (ETLinuxSocket*)socket;

    if (epoll_ctl(linux_context->epoll_fd, EPOLL_CTL_DEL, linux_socket->socket_fd, NULL) < 0) {
        return handle_socket_error("epoll_ctl");
    }

    return ET_SUCCESS;
}

static ETResult linux_wait_events(ETIOContext* context, ETIOEvent* events,
                                 int max_events, int timeout, int* num_events)
{
    if (!context || !events || !num_events) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ETLinuxIOContext* linux_context = (ETLinuxIOContext*)context;
    *num_events = 0;

    struct epoll_event epoll_events[max_events];
    int result = epoll_wait(linux_context->epoll_fd, epoll_events, max_events, timeout);

    if (result < 0) {
        if (errno == EINTR) {
            return ET_SUCCESS; // 시그널에 의한 중단
        }
        return handle_socket_error("epoll_wait");
    }

    for (int i = 0; i < result; i++) {
        ETEpollEventData* event_data = (ETEpollEventData*)epoll_events[i].data.ptr;
        if (event_data) {
            events[i].socket = event_data->socket;
            events[i].events = epoll_events_to_et_events(epoll_events[i].events);
            events[i].user_data = event_data->user_data;
            events[i].error_code = 0;
        }
    }

    *num_events = result;
    return ET_SUCCESS;
}

static void linux_destroy_io_context(ETIOContext* context)
{
    if (!context) {
        return;
    }

    ETLinuxIOContext* linux_context = (ETLinuxIOContext*)context;

    linux_context->is_running = false;

    if (linux_context->epoll_fd >= 0) {
        close(linux_context->epoll_fd);
    }

    pthread_mutex_destroy(&linux_context->lock);
    free(linux_context);
}

// ============================================================================
// 주소 처리 함수 구현
// ============================================================================

static ETResult linux_string_to_address(ETAddressFamily family, const char* str_addr, ETSocketAddress* addr)
{
    if (!str_addr || !addr) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    memset(addr, 0, sizeof(ETSocketAddress));
    addr->family = family;

    switch (family) {
        case ET_AF_INET: {
            struct in_addr in_addr;
            if (inet_pton(AF_INET, str_addr, &in_addr) != 1) {
                return ET_ERROR_INVALID_ARGUMENT;
            }
            addr->ipv4.addr = in_addr.s_addr;
            return ET_SUCCESS;
        }

        case ET_AF_INET6: {
            struct in6_addr in6_addr;
            if (inet_pton(AF_INET6, str_addr, &in6_addr) != 1) {
                return ET_ERROR_INVALID_ARGUMENT;
            }
            memcpy(addr->ipv6.addr, &in6_addr, 16);
            return ET_SUCCESS;
        }

        default:
            return ET_ERROR_UNSUPPORTED;
    }
}

static ETResult linux_address_to_string(const ETSocketAddress* addr, char* str_addr, size_t size)
{
    if (!addr || !str_addr || size == 0) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    switch (addr->family) {
        case ET_AF_INET: {
            struct in_addr in_addr;
            in_addr.s_addr = addr->ipv4.addr;
            if (inet_ntop(AF_INET, &in_addr, str_addr, size) == NULL) {
                return ET_ERROR_INVALID_ARGUMENT;
            }
            return ET_SUCCESS;
        }

        case ET_AF_INET6: {
            struct in6_addr in6_addr;
            memcpy(&in6_addr, addr->ipv6.addr, 16);
            if (inet_ntop(AF_INET6, &in6_addr, str_addr, size) == NULL) {
                return ET_ERROR_INVALID_ARGUMENT;
            }
            return ET_SUCCESS;
        }

        default:
            return ET_ERROR_UNSUPPORTED;
    }
}

static ETResult linux_resolve_hostname(const char* hostname, ETAddressFamily family,
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
        ETResult conv_result = sockaddr_to_socket_address(ptr->ai_addr, ptr->ai_addrlen, &addresses[count]);
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

static ETResult linux_get_network_stats(const ETSocket* socket, ETNetworkStats* stats)
{
    if (!stats) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    if (socket) {
        const ETLinuxSocket* linux_socket = (const ETLinuxSocket*)socket;
        *stats = linux_socket->stats;
    } else {
        // 전체 시스템 네트워크 통계는 구현하지 않음
        memset(stats, 0, sizeof(ETNetworkStats));
    }

    return ET_SUCCESS;
}

static int linux_get_last_network_error(void)
{
    return errno;
}

static const char* linux_get_network_error_string(int error_code)
{
    return strerror(error_code);
}

// ============================================================================
// Linux 네트워크 인터페이스 구조체
// ============================================================================

static const ETNetworkInterface linux_network_interface = {
    // 소켓 관리
    .create_socket = linux_create_socket,
    .bind_socket = linux_bind_socket,
    .listen_socket = linux_listen_socket,
    .accept_socket = linux_accept_socket,
    .connect_socket = linux_connect_socket,
    .close_socket = linux_close_socket,

    // 데이터 전송
    .send_data = linux_send_data,
    .receive_data = linux_receive_data,
    .send_to = linux_send_to,
    .receive_from = linux_receive_from,

    // 소켓 옵션 및 상태
    .set_socket_option = linux_set_socket_option,
    .get_socket_option = linux_get_socket_option,
    .get_socket_state = linux_get_socket_state,
    .get_local_address = linux_get_local_address,
    .get_remote_address = linux_get_remote_address,

    // 비동기 I/O
    .create_io_context = linux_create_io_context,
    .register_socket = linux_register_socket,
    .modify_socket_events = linux_modify_socket_events,
    .unregister_socket = linux_unregister_socket,
    .wait_events = linux_wait_events,
    .destroy_io_context = linux_destroy_io_context,

    // 주소 처리
    .string_to_address = linux_string_to_address,
    .address_to_string = linux_address_to_string,
    .resolve_hostname = linux_resolve_hostname,

    // 유틸리티
    .get_network_stats = linux_get_network_stats,
    .get_last_network_error = linux_get_last_network_error,
    .get_network_error_string = linux_get_network_error_string,

    .platform_data = NULL
};

// ============================================================================
// 공개 함수 구현
// ============================================================================

const ETNetworkInterface* et_get_linux_network_interface(void)
{
    return &linux_network_interface;
}

// ============================================================================
// 내부 함수 구현
// ============================================================================

static ETResult socket_address_to_sockaddr(const ETSocketAddress* et_addr,
                                          struct sockaddr* sockaddr, socklen_t* sockaddr_len)
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

        case ET_AF_UNIX: {
            struct sockaddr_un* sun = (struct sockaddr_un*)sockaddr;
            memset(sun, 0, sizeof(struct sockaddr_un));
            sun->sun_family = AF_UNIX;
            strncpy(sun->sun_path, et_addr->unix_addr.path, sizeof(sun->sun_path) - 1);
            *sockaddr_len = sizeof(struct sockaddr_un);
            return ET_SUCCESS;
        }

        default:
            return ET_ERROR_UNSUPPORTED;
    }
}

static ETResult sockaddr_to_socket_address(const struct sockaddr* sockaddr,
                                          socklen_t sockaddr_len, ETSocketAddress* et_addr)
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

        case AF_UNIX: {
            const struct sockaddr_un* sun = (const struct sockaddr_un*)sockaddr;
            et_addr->family = ET_AF_UNIX;
            strncpy(et_addr->unix_addr.path, sun->sun_path, sizeof(et_addr->unix_addr.path) - 1);
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

static ETResult set_socket_nonblocking(int socket_fd, bool nonblocking)
{
    int flags = fcntl(socket_fd, F_GETFL, 0);
    if (flags < 0) {
        return handle_socket_error("fcntl");
    }

    if (nonblocking) {
        flags |= O_NONBLOCK;
    } else {
        flags &= ~O_NONBLOCK;
    }

    if (fcntl(socket_fd, F_SETFL, flags) < 0) {
        return handle_socket_error("fcntl");
    }

    return ET_SUCCESS;
}

static ETResult handle_socket_error(const char* operation)
{
    int error = errno;
    ETResult result = et_network_error_to_common(error);

    ET_SET_ERROR(result, "%s 실패: %s (%d)", operation, strerror(error), error);

    return result;
}

static uint32_t et_events_to_epoll_events(ETIOEvents events)
{
    uint32_t epoll_events = 0;

    if (events & ET_IO_EVENT_READ) {
        epoll_events |= EPOLLIN;
    }
    if (events & ET_IO_EVENT_WRITE) {
        epoll_events |= EPOLLOUT;
    }
    if (events & ET_IO_EVENT_ERROR) {
        epoll_events |= EPOLLERR;
    }
    if (events & ET_IO_EVENT_CLOSE) {
        epoll_events |= EPOLLHUP | EPOLLRDHUP;
    }

    return epoll_events;
}

static ETIOEvents epoll_events_to_et_events(uint32_t epoll_events)
{
    ETIOEvents events = ET_IO_EVENT_NONE;

    if (epoll_events & EPOLLIN) {
        events |= ET_IO_EVENT_READ;
    }
    if (epoll_events & EPOLLOUT) {
        events |= ET_IO_EVENT_WRITE;
    }
    if (epoll_events & EPOLLERR) {
        events |= ET_IO_EVENT_ERROR;
    }
    if (epoll_events & (EPOLLHUP | EPOLLRDHUP)) {
        events |= ET_IO_EVENT_CLOSE;
    }

    return events;
}

#endif // __linux__