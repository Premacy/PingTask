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
#include <future>

#include <arpa/inet.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/un.h>

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

	struct host_stat{
		std::string ip;
		int times;	 // общее число попыток
		int stimes;  // число успешных попыток 
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

	int recv_echo_reply(int sock, int ident)
	{
	    // allocate buffer
	    char buffer[MTU];
	    struct sockaddr_in peer_addr;

	    // receive another packet
	    socklen_t addr_len = sizeof(peer_addr);
	    int bytes = recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr*)&peer_addr, &addr_len);
	    if (bytes == -1) {
	        // normal return when timeout
	        if (errno == EAGAIN || errno == EWOULDBLOCK) {
	            return 0;
	        }

	        return -1;
	    }

	    // find icmp packet in ip packet
	    struct icmp_echo* icmp = (struct icmp_echo*)(buffer + 20);

	    // check type
	    if (icmp->type != 0 || icmp->code != 0) {
	        return 0;
	    }

	    // match identifier
	    if (ntohs(icmp->ident) != ident) {
	        return 0;
	    }

	    // print info
	    printf("%s seq=%d %5.2fms\n",
	        inet_ntoa(peer_addr.sin_addr),
	        ntohs(icmp->seq),
	        (get_timestamp() - icmp->sending_ts) * 1000
	    );

	    return 0;
	}

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
	        return -1;
	    }

	    // set socket timeout option
	    struct timeval tv;
	    tv.tv_sec = 0;
	    tv.tv_usec = RECV_TIMEOUT_USEC;
	    int ret = setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
	    if (ret == -1) {
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
	        }

	        // try to receive and print reply
	        ret = recv_echo_reply(sock, ident);
	        if (ret == -1) {
	            perror("Receive failed");
	        }
	    }

	    return 0;
	}

}

namespace utils{
	#include <sstream>

	std::vector<std::string> split(const std::string& str, char delim){
		std::string istringstream ss(str);
		std::vector<std::string> result;

		while(std::getline(ss, str, delim)){
			result.push_back(std::move(str));
		}

		return result;
	}
};

struct command_t{
	std::string name;
	std::map<std::string, std::string> arguments;
};

command_t parse_command(const std::string& str){
	std::vector<std::string> params = utils::split(str, ' ');
	command_t command;

	command.name = params[0];

	for(int i = 1; i < params.size(); i+=2){
		std::string key = params[i];
		std::string value = params[i+1];

		command.arguments.emplace(key, value);
	}

	return command;
}

class Handler{
public:
	Handler(command_t command_): command(command_) {};
	virtual void process() = 0;

private:
	command_t command;
};

class PingHandler : public Handler{

};

class StatHandler : public Handler{

};

class HandlerFabric{
public:
	static std::shared_ptr<Handler> handler(const command_t& command){

	}
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
		std::string std_command;

		while(_run){
			str_command = sock->Read();
			auto command = parse_command(str_command);

			if (command.name == "ping") {
				host_stat stat;
			} else if (command.name == "statistic"){

			}
		}
	}

	std::vector<host_stat&> statistic;

	std::shared_ptr<UnixSocket> sock;
	std::atomic<bool> _run;

	std::map<std::string, std::function<>>;
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