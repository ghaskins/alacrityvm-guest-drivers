#ifndef __PRIVATE_VBUS_HH__
#define __PRIVATE_VBUS_HH__

#include <errno.h>
#include <exception>

#include <vbus.hh>
#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/condition.hpp>
#include <boost/thread/detail/lock.hpp>
#include <map>

namespace VBus {
    namespace Impl {

	typedef boost::mutex                                     Mutex;
	typedef boost::condition                                 CondVar;
	typedef boost::detail::thread::scoped_lock<boost::mutex> Lock;

	class ErrnoString : public std::string
	{
	public:
	    ErrnoString()
		{
		    std::ostringstream os;
		    
		    os << strerror(errno) << " (errno = " << errno << ")";
		    assign(os.str());
		}
	};

	class ErrnoException : public std::runtime_error
	{
	public:
	    ErrnoException(std::string what) :
		std::runtime_error(what + ": " + ErrnoString()) {}
	};

	class Device : public VBus::Device {
	public:
	    typedef unsigned long Id;

	    Device(Id id, const std::string &path);
	    
	    std::string Attr(const std::string &key);
	    void Attr(const std::string &key, const std::string &val);
	    
	    void Call(unsigned long func,
		      void *data,
		      size_t len,
		      unsigned long flags);

	    DriverPtr m_driver;

	private:
	    Id m_id;
	    std::string m_path;
	};
	
	class Bus {
	public:
	    Bus();

	    void Register(const std::string &name, VBus::Driver::TypePtr type);
	    void Refresh(const std::string &name);
	    void Quiesce();
	    void Call(Device::Id id,
		      unsigned long func,
		      void *data,
		      size_t len,
		      unsigned long flags);

	private:
	    typedef std::map<std::string, VBus::Driver::TypePtr> TypeMap;
	    typedef std::map<Device::Id, VBus::DevicePtr> DeviceMap;
	    
	    Mutex m_mutex;
	    unsigned long m_version;
	    std::string m_path;
	    TypeMap m_typemap;
	    DeviceMap m_devicemap;
	    int m_quiesce;
	    CondVar m_cv;
	    int m_fd;
	};
    };
};

#endif // __PRIVATE_VBUS_HH__
