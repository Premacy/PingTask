// Создать демон
// Отправлять icmp ping пакеты
// Проверить, что созданный демон - один в системе
// Упаковать все в deb пакет
// Не забываем отловить сигнал

#include <exception>
#include <string>
#include <iostream>
#include <memory>
#include <atomic>
#include <map>
#include <unordered_map>
#include <thread>
#include <sstream>
#include <vector>

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

#include "spdlog/spdlog.h"

namespace utils{
	std::vector<std::string> SplitByChar(const std::string& str, char delim){
		std::vector<std::string> result;
		std::istringstream stream(str);

		std::string buff;
		while(std::getline(stream, buff, delim)){
			result.push_back(std::move(buff));
		}

		return result;
	}
};

struct host_stat{
	std::string ip;
	int times;	 // общее число попыток
	int stimes;  // число успешных попыток 
};

namespace network{
	#define MAGIC "1234567890"
	#define MAGIC_LEN 11
	#define MTU 1500
	#define RECV_TIMEOUT_USEC 5

	struct icmp_echo {
	    // header
	    uint8_t type;
	    uint8_t code;
	    uint16_t checksum;

	    uint16_t ident;
	    uint16_t seq;

	    // data
	    double sending_ts;
	    char magic[MAGIC_LEN];
	};

	double get_timestamp()
	{
	    struct timeval tv;
	    gettimeofday(&tv, NULL);
	    return tv.tv_sec + ((double)tv.tv_usec) / 1000000;
	}

	uint16_t calculate_checksum(unsigned char* buffer, int bytes)
	{
	    uint32_t checksum = 0;
	    unsigned char* end = buffer + bytes;

	    // odd bytes add last byte and reset end
	    if (bytes % 2 == 1) {
	        end = buffer + bytes - 1;
	        checksum += (*end) << 8;
	    }

	    // add words of two bytes, one by one
	    while (buffer < end) {
	        checksum += buffer[0] << 8;
	        checksum += buffer[1];
	        buffer += 2;
	    }

	    // add carry if any
	    uint32_t carray = checksum >> 16;
	    while (carray) {
	        checksum = (checksum & 0xffff) + carray;
	        carray = checksum >> 16;
	    }

	    // negate it
	    checksum = ~checksum;

	    return checksum & 0xffff;
	}

	int send_echo_request(int sock, struct sockaddr_in* addr, int ident, int seq)
	{
	    // allocate memory for icmp packet
	    struct icmp_echo icmp;
	    bzero(&icmp, sizeof(icmp));

	    // fill header files
	    icmp.type = 8;
	    icmp.code = 0;
	    icmp.ident = htons(ident);
	    icmp.seq = htons(seq);

	    // fill magic string
	    strncpy(icmp.magic, MAGIC, MAGIC_LEN);

	    // fill sending timestamp
	    icmp.sending_ts = get_timestamp();

	    // calculate and fill checksum
	    icmp.checksum = htons(
	        calculate_checksum((unsigned char*)&icmp, sizeof(icmp))
	    );

	    // send it
	    int bytes = sendto(sock, &icmp, sizeof(icmp), 0,
	        (struct sockaddr*)addr, sizeof(*addr));
	    if (bytes == -1) {
	        return -1;
	    }

	    return 0;
	}

	// int recv_echo_reply(int sock, int ident)
	// {
	//     // allocate buffer
	//     char buffer[MTU];
	//     struct sockaddr_in peer_addr;

	//     // receive another packet
	//     socklen_t addr_len = sizeof(peer_addr);
	//     int bytes = recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr*)&peer_addr, &addr_len);
	//     if (bytes == -1) {
	//         // normal return when timeout
	//         if (errno == EAGAIN || errno == EWOULDBLOCK) {
	//             return 0;
	//         }

	//         return -1;
	//     }

	//     // find icmp packet in ip packet
	//     struct icmp_echo* icmp = (struct icmp_echo*)(buffer + 20);

	//     // check type
	//     if (icmp->type != 0 || icmp->code != 0) {
	//         return 0;
	//     }

	//     // match identifier
	//     if (ntohs(icmp->ident) != ident) {
	//         return 0;
	//     }

	//     // print info
	//     printf("%s seq=%d %5.2fms\n",
	//         inet_ntoa(peer_addr.sin_addr),
	//         ntohs(icmp->seq),
	//         (get_timestamp() - icmp->sending_ts) * 1000
	//     );

	//     return 0;
	// }

	int ping(host_stat& hstat)
	{
	    // for store destination address
	    struct sockaddr_in addr;
	    bzero(&addr, sizeof(addr));

	    // fill address, set port to 0
	    addr.sin_family = AF_INET;
	    addr.sin_port = 0;
	    if (inet_aton(hstat.ip.c_str(), (struct in_addr*)&addr.sin_addr.s_addr) == 0) {
	        return -1;
	    };

	    // create raw socket for icmp protocol
	    int sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
	    if (sock == -1) {
	    	spdlog::error("Error create raw socket for icmp protocol");
	        return -1;
	    }

	    // set socket timeout option
	    struct timeval tv;
	    tv.tv_sec = 0;
	    tv.tv_usec = RECV_TIMEOUT_USEC;

	    int ret = setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
	    if (ret == -1) {
	    	spdlog::error("Error set socket timeout option");
	        return -1;
	    }

	    double next_ts = get_timestamp();
	    int ident = getpid();
	    int seq = 1;
	    int tries = 0;

	    while (tries < hstat.times) {
	        // time to send another packet
	        if (get_timestamp() >= next_ts) {
	            // send it
	           	spdlog::info("Try to send echo request to ip {}", hstat.ip);
	            spdlog::info("Send {}/{} ping message", hstat.times, tries);

	            ret = send_echo_request(sock, &addr, ident, seq);
	            if (ret == -1) {
	                perror("Send failed");
	            }else{
	            	hstat.stimes++;
	            }

	            // update next sendint timestamp to one second later
	            next_ts += 1;
	            // increase sequence number
	            seq += 1;
	          	tries++;
	        }	        // try to receive and print reply
	        // ret = recv_echo_reply(sock, ident);
	        // if (ret == -1) {
	        //     perror("Receive failed");
	        // }
	    }

	    return 0;
	}

}

struct command_t{
	enum class TYPE{
		GET_PING,
		GET_STATISTIC
	};

	std::string name;
	std::map<std::string, std::string> arguments;
	TYPE type;
};

class UnixSocket{
public:
	UnixSocket(std::string socket_path){
		sock = socket(AF_UNIX, SOCK_DGRAM, 0);

		if(sock < 0){
			throw std::logic_error("Erorr create socket");
		}

		sockaddr_un addr;
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

	// void Close(){
	// 	close(sock);
	// }

	~UnixSocket(){
		spdlog::info("UnixSocket closed");
		close(sock);
	}

private:
	constexpr std::size_t BUFFER_SIZE = 1024;
	//constexpr std::size_t BUFFER_SIZE = 1024;
	char _buffer[BUFFER_SIZE];
	int sock;
};

// class State{
// public:
// 	std::unordered_map<int, &network::host_stat> statistic;
// };

command_t parse_command(const std::string& str_command){
	auto v = utils::SplitByChar(str_command, ' ');

	command_t command;

	if(v.size() == 1){
		command.type = command_t::TYPE::GET_STATISTIC;
	}else if(v.size() == 2){
		command.type = command_t::TYPE::GET_PING;
		command.arguments["ip"] = v[0];
		command.arguments["times"] = v[1];
	}

	return command;
}

class Daemon{
public:
	Daemon(std::string socket_path): 
		sock(std::make_shared<UnixSocket>(socket_path)) 
	{	
	}

	void Start(){
		std::thread ListenLoopThread(Daemon::UnixSocketListen, this);
		std::thread TerminalLoopThread(Daemon::TerminalLoop, this);

		ListenLoopThread.join();
		TerminalLoopThread.join();
	}

private:

	void UnixSocketListen(){
		std::string str_command;

		while(_run){
			str_command = sock->Read();
			auto command = parse_command(str_command);

			std::string command_type = ( command.type == command_t::TYPE::GET_PING ? "Ping" : "Get statistic" );

			spdlog::info("Accept new command {}", command_type);

			if(command.type == command_t::TYPE::GET_PING){
				spdlog::info("Start ping session");
				// TODO Thread pool or boost asio os something else
				std::thread ping_sessin([](){
					host_stat hstat{"192.168.0.107", 5, 0};
					int res = network::ping(hstat);
					if(res != 0){
						spdlog::error("Ping session error");
					}
				});

				ping_sessin.detach();

			}else if(command.type == command_t::TYPE::GET_STATISTIC){
				// oh sheet...
				spdlog::info("Start GET_STATISTIC command");
			}
		}
	}

	void TerminalLoop(){
		while(_run_terminal){
			std::cout << "Enter command:" << std::endl;
			// TODO
		}
	}

	void Usage(){

	}

	void Daemonize(){
		 daemon(0, 0);
	}

	std::shared_ptr<UnixSocket> sock;

	std::atomic<bool> _run{true};
	std::atomic<bool> _run_terminal{true};
};

void signal_handler(int signum){
	if(signum == SIGINT){
		spdlog::info("Daemon stop");
		exit(0);
	}
}

int main(int argc, char** argv){

	signal(SIGINT, signal_handler);

	try{
		spdlog::info("Daemon start");

		Daemon daemon(argv[1]);
		daemon.Start();

	}catch(std::exception& exception){
		std::cout << exception.what() << std::endl;
	}

	return 0;
};


/*
command1:
	ip send_count
	statistic
*/