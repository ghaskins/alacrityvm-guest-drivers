
#include <vbus.hh>
#include <iostream>
#include <fstream>
#include <sstream>
#include <exception>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/thread/thread.hpp>
#include <boost/bind.hpp>

#include "privatevbus.hh"

namespace fs = boost::filesystem;

using namespace VBus;

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

Impl::Bus::Bus() :
    m_version(CastString<unsigned long>(ReadAttr("/sys/vbus/version")))
{
    if (m_version != 1) {
	std::ostringstream os;
	
	os << "Version " << m_version << " unsupported";
	throw std::runtime_error(os.str());
    }
    
    m_path = "/sys/vbus/instances/" + ReadAttr("/proc/self/vbus");
    
    if (!fs::exists(m_path))
	throw std::runtime_error("no virtual-bus connected");
    
    fs::directory_iterator end_iter;
    for ( fs::directory_iterator dev_iter(m_path + "/devices");
	  dev_iter != end_iter;
	  ++dev_iter )
    {
	Impl::Device::Id id =
	    CastString<Impl::Device::Id>(dev_iter->path().leaf());
	
	DevicePtr dev(new Impl::Device(id, dev_iter->path().string()));

	m_devicemap[id] = dev;
    }
    
}

static Impl::Bus g_bus;

void Impl::Bus::Refresh(const std::string &name)
{
    Lock l(m_mutex);
    Driver::TypePtr type;
    
    {
	TypeMap::iterator iter(m_typemap.find(name));
	
	if (iter == m_typemap.end())
	    throw std::runtime_error("Could not refresh type " + name);
	
	type = iter->second;
    }
    
    for(DeviceMap::iterator iter(m_devicemap.begin());
	iter != m_devicemap.end();
	++iter)
    {
	DevicePtr dev(iter->second);
	Impl::Device *_dev(dynamic_cast<Impl::Device*>(dev.get()));
	
	if (_dev->Attr("type") == name && _dev->m_driver == NULL) 
	    _dev->m_driver = type->Probe(dev);
    }
}

class RefreshThread {
public:
    RefreshThread(const std::string &type) : m_type(type) {}

    void operator()()
	{
	    g_bus.Refresh(m_type);
	}

private:
    std::string m_type;
};

void Impl::Bus::Register(const std::string &name, Driver::TypePtr type)
{
    Lock l(m_mutex);

    if (m_typemap.find(name) != m_typemap.end())
	throw std::runtime_error("driver " + name + " already registered");

    m_typemap[name] = type;  

    boost::thread t(RefreshThread(name));
}

void Driver::Type::Register(const std::string &name, Driver::TypePtr type)
{
    g_bus.Register(name, type);
}

Impl::Device::Device(Id id, const std::string &path) :
    m_id(id), m_path(path)
{

}

std::string Impl::Device::Attr(const std::string &key)
{
    return ReadAttr(m_path + "/" + key);
}

void Impl::Device::Attr(const std::string &key, const std::string &val)
{
    std::string fqp(m_path + "/" + key);
    std::ofstream os(fqp.c_str());
    
    if (!os.is_open())
	throw std::runtime_error("could not open " + fqp);

    os << val;
}
