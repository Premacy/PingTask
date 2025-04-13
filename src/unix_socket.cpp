#include "unix_socket.h"

#include <arpa/inet.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/un.h>
#include <signal.h>
#include <stdlib.h>
#include <cerrno>
#include <exception>

#include "spdlog/spdlog.h"

UnixSocket::UnixSocket(std::string socket_path) 
    : sock(0)  // Инициализируем сокет нулём
{
    sock = socket(AF_UNIX, SOCK_DGRAM, 0);
    if(sock < 0) {
        throw std::runtime_error("Ошибка создания сокета");
    }

    sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));  // Очищаем структуру
    addr.sun_family = AF_UNIX;
    
    strncpy(addr.sun_path, socket_path.c_str(), sizeof(addr.sun_path) - 1);
    addr.sun_path[sizeof(addr.sun_path) - 1] = '\0';  // Гарантируем null-termination

    int err_bind = bind(sock, (struct sockaddr*)&addr, sizeof(addr));
    if(err_bind < 0) {
        close(sock);  // Закрываем сокет при ошибке
        throw std::runtime_error("Ошибка привязки сокета");
    }
}

UnixSocket::~UnixSocket() 
{
    if(sock != -1) {
        close(sock);
        spdlog::info("Сокет закрыт");
    }
}

std::string UnixSocket::Read() 
{
    if(!sock) {
        throw std::runtime_error("Сокет не инициализирован");
    }

    int bytes_read = recvfrom(sock, _buffer, BUFFER_SIZE, 0, nullptr, nullptr);
    if(bytes_read < 0) {
        throw std::runtime_error("Ошибка чтения из сокета");
    }

    return std::string(_buffer, bytes_read);
}

void UnixSocket::Write(std::string str) 
{
    if(!sock) {
        throw std::runtime_error("Сокет не инициализирован");
    }

    int bytes_sent = sendto(sock, str.c_str(), str.size(), 0, nullptr, 0);
    if(bytes_sent < 0) {
        throw std::runtime_error("Ошибка записи в сокет");
    }
}