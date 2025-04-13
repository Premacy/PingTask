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
#include <sstream>

#include "spdlog/spdlog.h"

#include "network.h"
#include "utils.h"
#include "unix_socket.h"
#include "types.h"

std::string toString(const host_stat& stat)
{	
	std::stringstream ss;
	ss << "ip: " << stat.ip << "\n";
	ss << "times: " << stat.times << "\n";
	ss << "successful_times: " << stat.stimes;

	return ss.str();
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

// class State{
// public:
// 	std::unordered_map<int, &network::host_stat> statistic;
// };

command_t parse_command(const std::string& str_command){
	auto v = utils::SplitByChar(str_command, ' ');

	command_t command;

	if (v.size() == 1) {
		command.type = command_t::TYPE::GET_STATISTIC;
	} else if (v.size() == 2) {
		command.type = command_t::TYPE::GET_PING;
		command.arguments["ip"] = v[0];
		command.arguments["times"] = v[1];
	}

	return command;
}

class ICommandHandler 
{
public:
	virtual ~ICommandHandler() = default;
	virtual void process(const command_t& command) = 0;
};

class GetPingHandler : public ICommandHandler
{
public:
	void process(const command_t& command) override
	{
		spdlog::info("Start ping session");
		std::string ip_ping = "127.0.0.1";
		host_stat hstat{command["ip"].c_str(), 5, 0}; // FIX_ME
		int res = network::ping(hstat);
		if (res != 0) {
			spdlog::error("Ping session error");
		} else {
			spdlog::info("End ping session");
			spdlog::info("Info {}", toString(hstat));
		}
	}
};

class GetStatisticHandler : public ICommandHandler
{
public:
	void process(const command_t& command) override
	{
		spdlog::info("Start GET_STATISTIC command");
	}
};

class CommandHandlerFabric
{
	using handler_ptr = std::shared_ptr<ICommandHandler>;
public:
	handler_ptr createHandler(const command_t& command) const
	{
		// TODO: fix shared_ptr -> make_shared
		if (command.type == command_t::TYPE::GET_PING) {
			return std::shared_ptr<ICommandHandler>(new GetPingHandler{});
		} else if (command.type == command_t::TYPE::GET_STATISTIC) {
			return std::shared_ptr<ICommandHandler>(new GetStatisticHandler{});
		}
		return nullptr;
	}
};

class Daemon
{
public:
	Daemon(std::string socket_path)
	{
		try{
			sock = std::make_shared<UnixSocket>(socket_path);
		} catch(std::logic_error &e) {
			spdlog::info("Error create unis_socket on path {}", socket_path);
		}
		spdlog::info("Open unix socket on path {}", socket_path);
	}

	void Start()
	{
		std::thread ListenLoopThread(&Daemon::UnixSocketListen, this);
		std::thread TerminalLoopThread(&Daemon::TerminalLoop, this);

		ListenLoopThread.join();
		//TerminalLoopThread.join();
	}

private:
	void UnixSocketListen() 
	{
		std::string str_command;
		CommandHandlerFabric handlerFabric;
		while (_run) {
			str_command = sock->Read();
			auto command = parse_command(str_command);

			std::string command_type = 
				(command.type == command_t::TYPE::GET_PING ? "Ping" : "Get statistic");

			spdlog::info("Accept new command {}", command_type);
			std::shared_ptr<ICommandHandler> handler = 
				std::shared_ptr<ICommandHandler>(handlerFabric.createHandler(command););

			if (command_type == command_t::TYPE::GET_PING) { // SWAP TO THREAD_POOL
				std::thread worker([&]() {
					handler->process(command);
				});
				worker.detach();
			} else {
				handler->process(command);
			}
		}
	}

	void TerminalLoop() 
	{
		std::string terminal_command;
		while (_run_terminal) {
			std::cout << "..." << std::endl;
			std::getline(std::cin, terminal_command);
		}
	}

	void Daemonize()
	{
		 daemon(0, 0);
	}

	std::shared_ptr<UnixSocket> sock;

	std::atomic<bool> _run{true};
	std::atomic<bool> _run_terminal{true};
};

void signal_handler(int signum) 
{
	if (signum == SIGINT) {
		spdlog::info("Daemon stop");
		exit(0);
	}
}

void usage(const char* progname) 
{
	std::cout << "Usage: " << progname << " <unix_socket_path>" <<std::endl;
}

int main(int argc, char** argv)
{
	signal(SIGINT, signal_handler);

	if (argc < 2) {
		usage(argv[0]);
		exit(-1);
	}
	try{
		spdlog::info("Daemon start");

		Daemon daemon(argv[1]);
		daemon.Start();

	} catch (std::exception& exception) {
		std::cout << exception.what() << std::endl;
	}

	return 0;
}
