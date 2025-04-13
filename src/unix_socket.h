#pragma once

#include <string>

// FIX_ME - для многопоточной среды...
class UnixSocket {
public:
	UnixSocket(std::string socket_path);
	~UnixSocket();

	std::string Read();
	void Write(std::string str);

private:
	const std::size_t BUFFER_SIZE = 1024; //FIX_ME
	char _buffer[1024];
	int sock = 0;
};