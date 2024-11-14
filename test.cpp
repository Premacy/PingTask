#include <exception>
#include <string>
#include <iostream>
#include <memory>

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>


int main(int argc, char* argv[]){

	if(argc < 2){
		std::cout << "Error : must be 2 arguments given" << std::endl;
		exit(-1);
	}

	int sock;

	sock = socket(AF_UNIX, SOCK_DGRAM, 0);
		//int error = socketpair(AF_UNIX, SOCK_STREAN, 0, int *sv);

	if(sock < 0){
		std::cout << "Error : could not create socket" << std::endl;
		exit(-1);
	}

	struct sockaddr_un addr;
	addr.sun_family = AF_UNIX;

	std::string socket_path = argv[1];

	memcpy(addr.sun_path, socket_path.data(), socket_path.size());

	int err_conn = connect(sock, (const sockaddr*)&addr, sizeof(addr));

	if(err_conn < 0){
		std::cout << "Error : connection to socket_path " << socket_path << " could not established" << std::endl;
		exit(-1);
	}

	std::string command;
	while(true){
		std::getline(std::cin, command);

		int bytes_write = write(sock, command.data(), command.size());
		std::cout << bytes_write << std::endl;
	}

	exit(0);
}