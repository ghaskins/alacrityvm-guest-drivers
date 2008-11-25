
#include <vbus.hh>
#include <iostream>
#include <fstream>
#include <sstream>
#include <exception>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>

namespace fs = boost::filesystem;

using namespace Virtual;

template <class T>
static T CastString(const std::string &s)
{
    std::istringstream is(s);
    T v;
    
    is >> v;	
    
    return v;	
}

static std::string ReadAttr(const std::string &path)
{
    std::ifstream is(path.c_str());
    std::string v;
    
    if (!is.is_open())
	throw std::runtime_error("could not open " + path);
    
    is >> v;
    
    return v;
}

Bus::Bus() : m_version(CastString<unsigned long>(ReadAttr("/sys/vbus/version")))
{
    if (m_version != 1) {
	std::ostringstream os;
	
	os << "Version " << m_version << " unsupported";
	throw std::runtime_error(os.str());
    }
    
    std::string buspath("/sys/vbus/instances/" +
			ReadAttr("/proc/self/vbus"));
    
    if (!fs::exists(buspath))
	throw std::runtime_error("no virtual-bus connected");
    
    fs::directory_iterator end_iter;
    for ( fs::directory_iterator dev_iter(buspath + "/devices");
	  dev_iter != end_iter;
	  ++dev_iter )
    {
	Bus::Device::Id id =
	    CastString<Bus::Device::Id>(dev_iter->path().leaf());
	
	Bus::DevicePtr dev(new Bus::Device(id, dev_iter->path().string()));

	m_devicemap[id] = dev;
    }
    
}

Bus::Device::Device(Bus::Device::Id id, const std::string &path) :
    m_id(id), m_path(path)
{

}

std::string Bus::Device::Attr(const std::string &key)
{
    return ReadAttr(m_path + "/" + key);
}

void Bus::Device::Attr(const std::string &key, const std::string &val)
{
    std::string fqp(m_path + "/" + key);
    std::ofstream os(fqp.c_str());
    
    if (!os.is_open())
	throw std::runtime_error("could not open " + fqp);

    os << val;
}
