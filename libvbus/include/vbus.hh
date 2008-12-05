#ifndef __VBUS_HH__
#define __VBUS_HH__

#include <boost/shared_ptr.hpp>
#include <string>

namespace VBus {

    class Queue;
    class Device;
    class Driver;
    typedef boost::shared_ptr<Queue> QueuePtr;
    typedef boost::shared_ptr<Device> DevicePtr;
    typedef boost::shared_ptr<Driver> DriverPtr;

    void MemoryBarrier();

    class Queue {
    public:
	virtual ~Queue() {}

	class Descriptor {
	public:
	    class Buffer {
	    public:
		virtual ~Buffer() {}

		void *m_data;
		size_t m_len;
	    };

	    typedef boost::shared_ptr<Buffer> BufferPtr;

	    enum OwnerType {
		OWNER_NORTH = 0,
		OWNER_SOUTH = 1,
	    };

	    virtual void Set(BufferPtr buf) = 0;
	    virtual void Reset() = 0;
	    virtual size_t Len() = 0;
	    virtual void Owner(OwnerType) = 0;
	    virtual OwnerType Owner() = 0;
	    virtual BufferPtr operator->() = 0;

	protected:
	    virtual ~Descriptor() {}
	};

	class Iterator {
	public:
	    virtual ~Iterator() {}

	    enum SeekType {
		ISEEK_TAIL,
		ISEEK_NEXT,
		ISEEK_HEAD,
		ISEEK_SET,
	    };

	    virtual void Seek(SeekType type, long offset) = 0;
	    virtual void Push() = 0;
	    virtual void Pop() = 0;
	    virtual Descriptor *operator->() = 0;
	};

	typedef boost::shared_ptr<Iterator> IteratorPtr;

	enum Index {
	    IDX_VALID,
	    IDX_INUSE
	};

	virtual void Start() = 0;
	virtual void Stop() = 0;
	virtual void Signal() = 0;
	virtual int Count(Index idx) = 0;
	virtual bool Full(Index idx) = 0;
	bool Empty(Index idx) { return Count(idx) == 0; }

	virtual IteratorPtr Iterator(Index idx, unsigned long flags) = 0;
    };

    class Device {
    public:
	virtual ~Device() {}

	virtual std::string Attr(const std::string &key) = 0;
	virtual void Attr(const std::string &key,
			  const std::string &val) = 0;

	virtual void Call(unsigned long func,
			  void *data,
			  size_t len,
			  unsigned long flags) = 0;

	virtual QueuePtr Queue(unsigned long id, size_t ringsize) = 0;
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
