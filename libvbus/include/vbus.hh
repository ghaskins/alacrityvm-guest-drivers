#ifndef __VBUS_HH__
#define __VBUS_HH__

#include <boost/shared_ptr.hpp>
#include <string>

namespace VBus {

    class Device;
    class Driver;
    typedef boost::shared_ptr<Device> DevicePtr;
    typedef boost::shared_ptr<Driver> DriverPtr;

    class Device {
    public:
	virtual ~Device() {}

	virtual std::string Attr(const std::string &key) = 0;
	virtual void Attr(const std::string &key,
			  const std::string &val) = 0;
    };
    
    class Driver {
    public:
	virtual ~Driver() {}

	class Type;
	typedef boost::shared_ptr<Type> TypePtr;

	class Type {
	public:
	    virtual ~Type() {}
	    
	    virtual DriverPtr Probe(DevicePtr device) = 0;

	    static void Register(const std::string &name, TypePtr type);
	};
    
	virtual void Remove() = 0;
    };

    void Quiesce();
};

#endif // __VBUS_HH__
