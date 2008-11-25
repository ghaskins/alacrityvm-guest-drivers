
#include <vbus.hh>
#include <iostream>
#include <fstream>
#include <sstream>
#include <exception>

using namespace Virtual;

static void CheckVersion()
{
	std::ifstream is("/sys/vbus/version");

	if (!is.is_open())
		throw std::runtime_error("cannot detect vbus feature in the kernel");

	unsigned long version;

	is >> version;

	if (version != 1) {
		std::ostringstream os;
		
		os << "Version " << version << " unsupported";
		throw std::runtime_error(os.str());
	}
}

Bus::Bus()
{
	CheckVersion();

	
}
