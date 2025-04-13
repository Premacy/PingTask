#include "utils.h"

#include <sstream>

namespace utils
{

std::vector<std::string> SplitByChar(const std::string& str, char delim){
	std::vector<std::string> result;
	std::istringstream stream(str);

	std::string buff;
	while(std::getline(stream, buff, delim)){
		result.push_back(std::move(buff));
	}
	return result;
}

} //namespace utils