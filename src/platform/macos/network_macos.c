/**
 * @file network_macos.c
 * @brief macOS 네트워크 구현체
 * @author LibEtude Project
 * @version 1.0.0
 *
 * macOS 소켓 API를 사용한 네트워크 추상화 레이어 구현입니다.
 * kqueue를 통한 비동기 I/O를 지원합니다.
 */

#ifdef __APPLE__

#include "libetude/platform/network.h"
#include "libetude/platform/common.h"
#include "libetude/error.h"
#include <sys/socket.h>
#include <sys/event.h>
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
// macOS 특화 구조체 정의
// ============================================================================

/**
 * @brief macOS 소켓 구조체
 */
typedef struct ETMacOSSocket {
    int socket_fd;                  /**< macOS 소켓 파일 디스크립터 */
    ETSocketType type;              /**< 소켓 타입 */
    ETSocketState state;            /**< 소켓 상태 */
    ETSocketAddress local_addr;     /**< 로컬 주소 */
    ETSocketAddress remote_addr;    /**< 원격 주소 */
    bool is_nonblocking;            /**< 논블로킹 모드 여부 */
    ETNetworkStats stats;           /**< 네트워크 통계 */
} ETMacOSSocket;

/**
 * @brief macOS I/O 컨텍스트 구조체 (kqueue 기반)
 */
typedef struct ETMacOSIOContext {
    int kqueue_fd;                  /**< kqueue 파일 디스크립터 */
    bool is_running;                /**< 실행 중 여부 */
    pthread_mutex_t lock;           /**< 동기화용 뮤텍스 */
} ETMacOSIOContext;

/**
 * @brief kqueue 이벤트 데이터
 */
typedef struct ETKqueueEventData {
    ETSocket* socket;               /**< 관련 소켓 */
    void* user_data;                /**< 사용자 데이터 */
} ETKqueueEventData;

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
static int16_t et_events_to_kqueue_filter(ETIOEvents events);
static ETIOEvents kqueue_filter_to_et_events(int16_t filter, uint16_t flags);

// ============================================================================
// 소켓 관리 함수 구현
// ============================================================================

static ETResult macos_create_socket(ETSocketType type, ETSocket** socket)
{
    if (!socket) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    // macOS 소켓 구조체 할당
    ETMacOSSocket* macos_socket = (ETMacOSSocket*)calloc(1, sizeof(ETMacOSSocket));
    if (!macos_socket) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    // 소켓 생성
    int family = get_socket_family(type);
    int sock_type = get_socket_type(type);
    int protocol = get_socket_protocol(type);

    macos_socket->socket_fd = socket(family, sock_type, protocol);
    if (macos_socket->socket_fd < 0) {
        free(macos_socket);
        return handle_socket_error("socket");
    }

    macos_socket->type = type;
    macos_socket->state = ET_SOCKET_CLOSED;
    macos_socket->is_nonblocking = false;
    memset(&macos_socket->stats, 0, sizeof(ETNetworkStats));

    *socket = (ETSocket*)macos_socket;
    return ET_SUCCESS;
}

static ETResult macos_bind_socket(ETSocket* socket, const ETSocketAddress* addr)
{
    if (!socket || !addr) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ETMacOSSocket* macos_socket = (ETMacOSSocket*)socket;

    struct sockaddr_storage sockaddr;
    socklen_t sockaddr_len;
    ETResult result = socket_address_to_sockaddr(addr, (struct sockaddr*)&sockaddr, &sockaddr_len);
    if (result != ET_SUCCESS) {
        return result;
    }

    if (bind(macos_socket->socket_fd, (struct sockaddr*)&sockaddr, sockaddr_len) < 0) {
        return handle_socket_error("bind");
    }

    macos_socket->local_addr = *addr;
    macos_socket->state = ET_SOCKET_BOUND;
    return ET_SUCCESS;
}

static ETResult macos_listen_socket(ETSocket* socket, int backlog)
{
    if (!socket) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ETMacOSSocket* macos_socket = (ETMacOSSocket*)socket;

    if (macos_socket->state != ET_SOCKET_BOUND) {
        return ET_ERROR_INVALID_STATE;
    }

    if (listen(macos_socket->socket_fd, backlog) < 0) {
        return handle_socket_error("listen");
    }

    macos_socket->state = ET_SOCKET_LISTENING;
    return ET_SUCCESS;
}

static ETResult macos_accept_socket(ETSocket* socket, ETSocket** client, ETSocketAddress* addr)
{
    if (!socket || !client) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ETMacOSSocket* macos_socket = (ETMacOSSocket*)socket;

    if (macos_socket->state != ET_SOCKET_LISTENING) {
        return ET_ERROR_INVALID_STATE;
    }

    struct sockaddr_storage client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    int client_fd = accept(macos_socket->socket_fd, (struct sockaddr*)&client_addr, &client_addr_len);
    if (client_fd < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return ET_ERROR_TIMEOUT;
        }
        return handle_socket_error("accept");
    }

    // 클라이언트 소켓 구조체 생성
    ETMacOSSocket* macos_client = (ETMacOSSocket*)calloc(1, sizeof(ETMacOSSocket));
    if (!macos_client) {
        close(client_fd);
        return ET_ERROR_OUT_OF_MEMORY;
    }

    macos_client->socket_fd = client_fd;
    macos_client->type = macos_socket->type;
    macos_client->state = ET_SOCKET_CONNECTED;
    macos_client->is_nonblocking = false;
    memset(&macos_client->stats, 0, sizeof(ETNetworkStats));

    // 클라이언트 주소 변환
    if (addr) {
        sockaddr_to_socket_address((struct sockaddr*)&client_addr, client_addr_len, addr);
        macos_client->remote_addr = *addr;
    }

    *client = (ETSocket*)macos_client;
    return ET_SUCCESS;
}

static ETResult macos_connect_socket(ETSocket* socket, const ETSocketAddress* addr)
{
    if (!socket || !addr) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ETMacOSSocket* macos_socket = (ETMacOSSocket*)socket;

    struct sockaddr_storage sockaddr;
    socklen_t sockaddr_len;
    ETResult result = socket_address_to_sockaddr(addr, (struct sockaddr*)&sockaddr, &sockaddr_len);
    if (result != ET_SUCCESS) {
        return result;
    }

    macos_socket->state = ET_SOCKET_CONNECTING;

    if (connect(macos_socket->socket_fd, (struct sockaddr*)&sockaddr, sockaddr_len) < 0) {
        if (errno == EINPROGRESS) {
            // 논블로킹 모드에서는 진행 중 상태 유지
            return ET_SUCCESS;
        }
        macos_socket->state = ET_SOCKET_ERROR;
        return handle_socket_error("connect");
    }

    macos_socket->remote_addr = *addr;
    macos_socket->state = ET_SOCKET_CONNECTED;
    return ET_SUCCESS;
}

static void macos_close_socket(ETSocket* socket)
{
    if (!socket) {
        return;
    }

    ETMacOSSocket* macos_socket = (ETMacOSSocket*)socket;

    if (macos_socket->socket_fd >= 0) {
        close(macos_socket->socket_fd);
        macos_socket->socket_fd = -1;
    }

    macos_socket->state = ET_SOCKET_CLOSED;
    free(macos_socket);
}

// ============================================================================
// 데이터 전송 함수 구현
// ============================================================================

static ETResult macos_send_data(ETSocket* socket, const void* data, size_t size, size_t* sent)
{
    if (!socket || !data || !sent) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ETMacOSSocket* macos_socket = (ETMacOSSocket*)socket;

    if (macos_socket->state != ET_SOCKET_CONNECTED) {
        return ET_ERROR_INVALID_STATE;
    }

    ssize_t result = send(macos_socket->socket_fd, data, size, 0);
    if (result < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            *sent = 0;
            return ET_SUCCESS;
        }
        return handle_socket_error("send");
    }

    *sent = (size_t)result;
    macos_socket->stats.bytes_sent += *sent;
    macos_socket->stats.packets_sent++;
    return ET_SUCCESS;
}

static ETResult macos_receive_data(ETSocket* socket, void* buffer, size_t size, size_t* received)
{
    if (!socket || !buffer || !received) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ETMacOSSocket* macos_socket = (ETMacOSSocket*)socket;

    if (macos_socket->state != ET_SOCKET_CONNECTED) {
        return ET_ERROR_INVALID_STATE;
    }

    ssize_t result = recv(macos_socket->socket_fd, buffer, size, 0);
    if (result < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            *received = 0;
            return ET_SUCCESS;
        }
        return handle_socket_error("recv");
    }

    if (result == 0) {
        // 연결이 정상적으로 종료됨
        macos_socket->state = ET_SOCKET_CLOSED;
        *received = 0;
        return ET_SUCCESS;
    }

    *received = (size_t)result;
    macos_socket->stats.bytes_received += *received;
    macos_socket->stats.packets_received++;
    return ET_SUCCESS;
}

static ETResult macos_send_to(ETSocket* socket, const void* data, size_t size,
                             const ETSocketAddress* addr, size_t* sent)
{
    if (!socket || !data || !addr || !sent) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ETMacOSSocket* macos_socket = (ETMacOSSocket*)socket;

    if (macos_socket->type != ET_SOCKET_UDP) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    struct sockaddr_storage sockaddr;
    socklen_t sockaddr_len;
    ETResult result = socket_address_to_sockaddr(addr, (struct sockaddr*)&sockaddr, &sockaddr_len);
    if (result != ET_SUCCESS) {
        return result;
    }

    ssize_t send_result = sendto(macos_socket->socket_fd, data, size, 0,
                                (struct sockaddr*)&sockaddr, sockaddr_len);
    if (send_result < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            *sent = 0;
            return ET_SUCCESS;
        }
        return handle_socket_error("sendto");
    }

    *sent = (size_t)send_result;
    macos_socket->stats.bytes_sent += *sent;
    macos_socket->stats.packets_sent++;
    return ET_SUCCESS;
}

static ETResult macos_receive_from(ETSocket* socket, void* buffer, size_t size,
                                  ETSocketAddress* addr, size_t* received)
{
    if (!socket || !buffer || !received) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ETMacOSSocket* macos_socket = (ETMacOSSocket*)socket;

    if (macos_socket->type != ET_SOCKET_UDP) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    struct sockaddr_storage from_addr;
    socklen_t from_addr_len = sizeof(from_addr);

    ssize_t recv_result = recvfrom(macos_socket->socket_fd, buffer, size, 0,
                                  (struct sockaddr*)&from_addr, &from_addr_len);
    if (recv_result < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            *received = 0;
            return ET_SUCCESS;
        }
        return handle_socket_error("recvfrom");
    }

    *received = (size_t)recv_result;
    macos_socket->stats.bytes_received += *received;
    macos_socket->stats.packets_received++;

    // 송신자 주소 변환
    if (addr) {
        sockaddr_to_socket_address((struct sockaddr*)&from_addr, from_addr_len, addr);
    }

    return ET_SUCCESS;
}

// ============================================================================
// 소켓 옵션 및 상태 함수 구현
// ============================================================================

static ETResult macos_set_socket_option(ETSocket* socket, ETSocketOption option,
                                       const void* value, size_t size)
{
    if (!socket || !value) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ETMacOSSocket* macos_socket = (ETMacOSSocket*)socket;
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
            return set_socket_nonblocking(macos_socket->socket_fd, *(bool*)value);
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

    if (setsockopt(macos_socket->socket_fd, level, optname, optval, optlen) < 0) {
        return handle_socket_error("setsockopt");
    }

    if (option == ET_SOCKET_OPT_NONBLOCK) {
        macos_socket->is_nonblocking = *(bool*)value;
    }

    return ET_SUCCESS;
}

static ETResult macos_get_socket_option(ETSocket* socket, ETSocketOption option,
                                       void* value, size_t* size)
{
    if (!socket || !value || !size) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ETMacOSSocket* macos_socket = (ETMacOSSocket*)socket;
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
            *(bool*)value = macos_socket->is_nonblocking;
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
            if (getsockopt(macos_socket->socket_fd, level, optname, &tv, &tv_len) < 0) {
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
            if (getsockopt(macos_socket->socket_fd, level, optname, &tv, &tv_len) < 0) {
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

    if (getsockopt(macos_socket->socket_fd, level, optname, optval, &optlen) < 0) {
        return handle_socket_error("getsockopt");
    }

    *size = (size_t)optlen;
    return ET_SUCCESS;
}

static ETSocketState macos_get_socket_state(const ETSocket* socket)
{
    if (!socket) {
        return ET_SOCKET_ERROR;
    }

    const ETMacOSSocket* macos_socket = (const ETMacOSSocket*)socket;
    return macos_socket->state;
}

static ETResult macos_get_local_address(const ETSocket* socket, ETSocketAddress* addr)
{
    if (!socket || !addr) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    const ETMacOSSocket* macos_socket = (const ETMacOSSocket*)socket;

    struct sockaddr_storage sockaddr;
    socklen_t sockaddr_len = sizeof(sockaddr);

    if (getsockname(macos_socket->socket_fd, (struct sockaddr*)&sockaddr, &sockaddr_len) < 0) {
        return handle_socket_error("getsockname");
    }

    return sockaddr_to_socket_address((struct sockaddr*)&sockaddr, sockaddr_len, addr);
}

static ETResult macos_get_remote_address(const ETSocket* socket, ETSocketAddress* addr)
{
    if (!socket || !addr) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    const ETMacOSSocket* macos_socket = (const ETMacOSSocket*)socket;

    if (macos_socket->state != ET_SOCKET_CONNECTED) {
        return ET_ERROR_INVALID_STATE;
    }

    struct sockaddr_storage sockaddr;
    socklen_t sockaddr_len = sizeof(sockaddr);

    if (getpeername(macos_socket->socket_fd, (struct sockaddr*)&sockaddr, &sockaddr_len) < 0) {
        return handle_socket_error("getpeername");
    }

    return sockaddr_to_socket_address((struct sockaddr*)&sockaddr, sockaddr_len, addr);
}

// ============================================================================
// 비동기 I/O 함수 구현 (kqueue 기반)
// ============================================================================

static ETResult macos_create_io_context(ETIOContext** context)
{
    if (!context) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ETMacOSIOContext* macos_context = (ETMacOSIOContext*)calloc(1, sizeof(ETMacOSIOContext));
    if (!macos_context) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    // kqueue 인스턴스 생성
    macos_context->kqueue_fd = kqueue();
    if (macos_context->kqueue_fd < 0) {
        free(macos_context);
        return handle_socket_error("kqueue");
    }

    macos_context->is_running = true;
    if (pthread_mutex_init(&macos_context->lock, NULL) != 0) {
        close(macos_context->kqueue_fd);
        free(macos_context);
        return ET_ERROR_SYSTEM;
    }

    *context = (ETIOContext*)macos_context;
    return ET_SUCCESS;
}

static ETResult macos_register_socket(ETIOContext* context, ETSocket* socket,
                                     ETIOEvents events, void* user_data)
{
    if (!context || !socket) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ETMacOSIOContext* macos_context = (ETMacOSIOContext*)context;
    ETMacOSSocket* macos_socket = (ETMacOSSocket*)socket;

    // 이벤트 데이터 구조체 생성
    ETKqueueEventData* event_data = (ETKqueueEventData*)malloc(sizeof(ETKqueueEventData));
    if (!event_data) {
        return ET_ERROR_OUT_OF_MEMORY;
    }

    event_data->socket = socket;
    event_data->user_data = user_data;

    struct kevent kev[2];
    int nev = 0;

    // 읽기 이벤트 등록
    if (events & ET_IO_EVENT_READ) {
        EV_SET(&kev[nev], macos_socket->socket_fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, event_data);
        nev++;
    }

    // 쓰기 이벤트 등록
    if (events & ET_IO_EVENT_WRITE) {
        EV_SET(&kev[nev], macos_socket->socket_fd, EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, 0, event_data);
        nev++;
    }

    if (nev > 0 && kevent(macos_context->kqueue_fd, kev, nev, NULL, 0, NULL) < 0) {
        free(event_data);
        return handle_socket_error("kevent");
    }

    return ET_SUCCESS;
}

static ETResult macos_modify_socket_events(ETIOContext* context, ETSocket* socket, ETIOEvents events)
{
    if (!context || !socket) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ETMacOSIOContext* macos_context = (ETMacOSIOContext*)context;
    ETMacOSSocket* macos_socket = (ETMacOSSocket*)socket;

    struct kevent kev[4];
    int nev = 0;

    // 기존 이벤트 삭제
    EV_SET(&kev[nev], macos_socket->socket_fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
    nev++;
    EV_SET(&kev[nev], macos_socket->socket_fd, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
    nev++;

    // 새 이벤트 추가
    if (events & ET_IO_EVENT_READ) {
        EV_SET(&kev[nev], macos_socket->socket_fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, NULL);
        nev++;
    }

    if (events & ET_IO_EVENT_WRITE) {
        EV_SET(&kev[nev], macos_socket->socket_fd, EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, 0, NULL);
        nev++;
    }

    if (kevent(macos_context->kqueue_fd, kev, nev, NULL, 0, NULL) < 0) {
        return handle_socket_error("kevent");
    }

    return ET_SUCCESS;
}

static ETResult macos_unregister_socket(ETIOContext* context, ETSocket* socket)
{
    if (!context || !socket) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ETMacOSIOContext* macos_context = (ETMacOSIOContext*)context;
    ETMacOSSocket* macos_socket = (ETMacOSSocket*)socket;

    struct kevent kev[2];
    EV_SET(&kev[0], macos_socket->socket_fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
    EV_SET(&kev[1], macos_socket->socket_fd, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);

    if (kevent(macos_context->kqueue_fd, kev, 2, NULL, 0, NULL) < 0) {
        return handle_socket_error("kevent");
    }

    return ET_SUCCESS;
}

static ETResult macos_wait_events(ETIOContext* context, ETIOEvent* events,
                                 int max_events, int timeout, int* num_events)
{
    if (!context || !events || !num_events) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    ETMacOSIOContext* macos_context = (ETMacOSIOContext*)context;
    *num_events = 0;

    struct kevent kqueue_events[max_events];
    struct timespec timeout_spec;
    struct timespec* timeout_ptr = NULL;

    if (timeout >= 0) {
        timeout_spec.tv_sec = timeout / 1000;
        timeout_spec.tv_nsec = (timeout % 1000) * 1000000;
        timeout_ptr = &timeout_spec;
    }

    int result = kevent(macos_context->kqueue_fd, NULL, 0, kqueue_events, max_events, timeout_ptr);

    if (result < 0) {
        if (errno == EINTR) {
            return ET_SUCCESS; // 시그널에 의한 중단
        }
        return handle_socket_error("kevent");
    }

    for (int i = 0; i < result; i++) {
        ETKqueueEventData* event_data = (ETKqueueEventData*)kqueue_events[i].udata;
        if (event_data) {
            events[i].socket = event_data->socket;
            events[i].events = kqueue_filter_to_et_events(kqueue_events[i].filter, kqueue_events[i].flags);
            events[i].user_data = event_data->user_data;
            events[i].error_code = (kqueue_events[i].flags & EV_ERROR) ? (int)kqueue_events[i].data : 0;
        }
    }

    *num_events = result;
    return ET_SUCCESS;
}

static void macos_destroy_io_context(ETIOContext* context)
{
    if (!context) {
        return;
    }

    ETMacOSIOContext* macos_context = (ETMacOSIOContext*)context;

    macos_context->is_running = false;

    if (macos_context->kqueue_fd >= 0) {
        close(macos_context->kqueue_fd);
    }

    pthread_mutex_destroy(&macos_context->lock);
    free(macos_context);
}

// ============================================================================
// 주소 처리 함수 구현
// ============================================================================

static ETResult macos_string_to_address(ETAddressFamily family, const char* str_addr, ETSocketAddress* addr)
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

static ETResult macos_address_to_string(const ETSocketAddress* addr, char* str_addr, size_t size)
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

static ETResult macos_resolve_hostname(const char* hostname, ETAddressFamily family,
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

static ETResult macos_get_network_stats(const ETSocket* socket, ETNetworkStats* stats)
{
    if (!stats) {
        return ET_ERROR_INVALID_ARGUMENT;
    }

    if (socket) {
        const ETMacOSSocket* macos_socket = (const ETMacOSSocket*)socket;
        *stats = macos_socket->stats;
    } else {
        // 전체 시스템 네트워크 통계는 구현하지 않음
        memset(stats, 0, sizeof(ETNetworkStats));
    }

    return ET_SUCCESS;
}

static int macos_get_last_network_error(void)
{
    return errno;
}

static const char* macos_get_network_error_string(int error_code)
{
    return strerror(error_code);
}

// ============================================================================
// macOS 네트워크 인터페이스 구조체
// ============================================================================

static const ETNetworkInterface macos_network_interface = {
    // 소켓 관리
    .create_socket = macos_create_socket,
    .bind_socket = macos_bind_socket,
    .listen_socket = macos_listen_socket,
    .accept_socket = macos_accept_socket,
    .connect_socket = macos_connect_socket,
    .close_socket = macos_close_socket,

    // 데이터 전송
    .send_data = macos_send_data,
    .receive_data = macos_receive_data,
    .send_to = macos_send_to,
    .receive_from = macos_receive_from,

    // 소켓 옵션 및 상태
    .set_socket_option = macos_set_socket_option,
    .get_socket_option = macos_get_socket_option,
    .get_socket_state = macos_get_socket_state,
    .get_local_address = macos_get_local_address,
    .get_remote_address = macos_get_remote_address,

    // 비동기 I/O
    .create_io_context = macos_create_io_context,
    .register_socket = macos_register_socket,
    .modify_socket_events = macos_modify_socket_events,
    .unregister_socket = macos_unregister_socket,
    .wait_events = macos_wait_events,
    .destroy_io_context = macos_destroy_io_context,

    // 주소 처리
    .string_to_address = macos_string_to_address,
    .address_to_string = macos_address_to_string,
    .resolve_hostname = macos_resolve_hostname,

    // 유틸리티
    .get_network_stats = macos_get_network_stats,
    .get_last_network_error = macos_get_last_network_error,
    .get_network_error_string = macos_get_network_error_string,

    .platform_data = NULL
};

// ============================================================================
// 공개 함수 구현
// ============================================================================

const ETNetworkInterface* et_get_macos_network_interface(void)
{
    return &macos_network_interface;
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

static int16_t et_events_to_kqueue_filter(ETIOEvents events)
{
    // kqueue는 필터별로 개별 등록이 필요하므로 이 함수는 사용하지 않음
    (void)events;
    return 0;
}

static ETIOEvents kqueue_filter_to_et_events(int16_t filter, uint16_t flags)
{
    ETIOEvents events = ET_IO_EVENT_NONE;

    if (filter == EVFILT_READ) {
        events |= ET_IO_EVENT_READ;
    }
    if (filter == EVFILT_WRITE) {
        events |= ET_IO_EVENT_WRITE;
    }
    if (flags & EV_ERROR) {
        events |= ET_IO_EVENT_ERROR;
    }
    if (flags & EV_EOF) {
        events |= ET_IO_EVENT_CLOSE;
    }

    return events;
}

#endif // __APPLE__