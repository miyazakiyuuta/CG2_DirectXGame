#include "Logger.h"
#include <debugapi.h>

void Logger::Log(const std::string& message) {
	OutputDebugStringA(message.c_str());
}
