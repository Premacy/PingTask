// Создать демон
// Отправлять icmp ping пакеты
// Проверить, что созданный демон - один в системе
// Упаковать все в deb пакет
// Не забываем отловить сигнал

#include <exception>
#include <string>
#include <iostream>
#include <memory>

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

struct host_t{
	std::string ip;
	int port;
};

struct ping_t{
	host_t host;
	int times;
};

struct ping_stat_t{
	host_t host;

	int times;
	int success_count;
};

ping_stat_t send_ping(const ping_t& ping) {
	int count = 0;
	int sock = 0;
	int wait_ms = 100;

	sock = socket(AF_UNIX, SOCK_DGRAM, 0);

	// тут нужно установить опции сокета
	while(count < ping.times){

		++count;
	}
}

class Handler{
public:
	Handler(std::string ip_): ip(ip_) {};

private:
	std::string ip;
};

constexpr std::size_t BUFFER_SIZE = 1024;

class UnixSocket{
public:
	UnixSocket(std::string socket_path){
		sock = socket(AF_UNIX, SOCK_DGRAM, 0);
		//int error = socketpair(AF_UNIX, SOCK_STREAN, 0, int *sv);

		if(sock < 0){
			throw std::logic_error("Erorr create socket");
		}

		struct sockaddr_un addr;
		addr.sun_family = AF_UNIX;

		memcpy(addr.sun_path, socket_path.data(), socket_path.size());

		int err_conn = bind(sock, (const sockaddr*)&addr, sizeof(addr));

		if(err_conn < 0){
			throw std::logic_error("Erorr create socket");
		}
	}

	std::string Read(){
		int result = read(sock, _buffer, BUFFER_SIZE);
		return _buffer;
	}

	void Write(std::string str){
		memcpy(_buffer, str.data(), str.size());
		int write = read(sock, _buffer, BUFFER_SIZE);
	}

private:
	//constexpr std::size_t BUFFER_SIZE = 1024;
	char _buffer[BUFFER_SIZE];
	int sock;
};

class Daemon{
public:
	Daemon(std::string socket_path): 
		sock(std::make_shared<UnixSocket>(socket_path)) 
	{

	}

	void Start(){
		std::cout << "Start daemon" << std::endl;
		//	daemon(0, 0);

		MainLoop();
	}

private:

	void MainLoop(){
		std::string command;

		while(_run){
			command = sock->Read();	
			// if(!command.empty()) 
			std::cout << command << std::endl;
		}
	}

	std::shared_ptr<UnixSocket> sock;
	bool _run;
};

int main(int argc, char** argv){
	try{
		Daemon daemon(argv[1]);
		daemon.Start();

	}catch(std::exception& exception){
		std::cout << exception.what() << std::endl;
	}

	return 0;
};