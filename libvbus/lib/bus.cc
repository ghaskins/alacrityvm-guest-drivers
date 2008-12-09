
#include <vbus.hh>
#include <iostream>
#include <fstream>
#include <sstream>
#include <exception>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/thread/thread.hpp>
#include <boost/bind.hpp>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/vbus.h>

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
    m_version(CastString<unsigned long>(ReadAttr("/sys/vbus/version"))),
    m_quiesce(0)
{
    if (m_version != 1) {
	std::ostringstream os;
	
	os << "Version " << m_version << " unsupported";
	throw std::runtime_error(os.str());
    }
    
    m_path = "/sys/vbus/instances/" + ReadAttr("/proc/self/vbus");
    
    if (!fs::exists(m_path))
	throw std::runtime_error("no virtual-bus connected");

    m_fd = open("/dev/vbus", 0);
    if (m_fd < 0)
	throw Impl::ErrnoException("failed to open /dev/vbus");
    
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

Impl::Bus g_bus;

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

    m_quiesce--;
    if (!m_quiesce)
	m_cv.notify_all();
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
    m_quiesce++;

    RefreshThread s(name);
    boost::thread t(s);
}

void Driver::Type::Register(const std::string &name,
			    Driver::TypePtr type,
			    Flags flags)
{
    ValidateFlags(0, flags);

    g_bus.Register(name, type);
}

void Impl::Bus::Quiesce()
{
    Lock l(m_mutex);

    while (m_quiesce)
	m_cv.wait(l);
}

int Impl::Bus::Ioctl(unsigned long func, void *args)
{
    int ret = ioctl(m_fd, func, args);
    if (ret < 0)
	throw Impl::ErrnoException("failed to complete ioctl");

    return ret;
}

void Impl::Bus::Register(unsigned long id, Impl::Queue *q)
{
    Lock l(m_mutex);

    if (m_queuemap.find(id) != m_queuemap.end())
	throw std::runtime_error("queue already registered");

    m_queuemap[id] = q;
}

void Impl::Bus::Unregister(Impl::Queue *q)
{
    Lock l(m_mutex);

    m_queuemap.erase(q->Id());
}

void VBus::Quiesce()
{
    g_bus.Quiesce();
}

Impl::Device::Device(Id id, const std::string &path) :
    m_id(id), m_path(path)
{
    struct vbus_deviceopen args;

    args.devid = m_id;

    g_bus.Ioctl(VBUS_DEVICEOPEN, &args);

    m_handle = args.handle;
}

Impl::Device::~Device()
{
    try {
	g_bus.Ioctl(VBUS_DEVICECLOSE, &m_handle);
    } catch(...) {}
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

void Impl::Device::Call(unsigned long func,
			void *data,
			size_t len,
			Flags flags)
{  
    ValidateFlags(0, flags);

    struct vbus_devicecall args;

    args.devh  = m_handle;
    args.func  = func;
    args.len   = len;
    args.datap = (__u64)data;
    args.flags = flags;

    g_bus.Ioctl(VBUS_DEVICECALL, &args);
}

VBus::QueuePtr Impl::Device::Queue(unsigned long id,
				   size_t ringsize,
				   Flags flags)
{
    ValidateFlags(0, flags);

    VBus::QueuePtr q(new Impl::Queue(m_handle, id, ringsize));

    return q;
}
