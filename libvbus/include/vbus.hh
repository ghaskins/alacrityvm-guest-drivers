#ifndef __VBUS_HH__
#define __VBUS_HH__

#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/condition.hpp>
#include <boost/thread/detail/lock.hpp>
#include <map>
#include <string>

namespace Virtual {

	typedef boost::mutex                                     Mutex;
	typedef boost::condition                                 CondVar;
	typedef boost::detail::thread::scoped_lock<boost::mutex> Lock;

	class Bus {
	public:
		Bus();

		class Device {
		public:
			typedef unsigned long Id;
		
			Id          m_id;
			std::string m_type;
		};

		typedef boost::shared_ptr<Device> DevicePtr;

	private:
		typedef std::map<Device::Id, DevicePtr> DeviceMap;

		Mutex m_mutex;
		DeviceMap m_devicemap; 
	};

};

#endif // __VBUS_HH__
