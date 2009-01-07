
#include <boost/thread/thread.hpp>
#include <boost/bind.hpp>
#include <boost/functional.hpp>

#include <exception>
#include <iostream>
#include <vbus.hh>

#include <linux/venet.h>

#define QLEN 32

/*
 * This function represents the egress point of our fictitious stack
 */
void RxPacket(VBus::Queue::Descriptor::BufferPtr buf, size_t len)
{
    std::cout << "rx packet of length " << len << std::endl;
}

/*
 * This class represents our per-descriptor buffer container
 */
class VEnetBuffer : public VBus::Queue::Descriptor::Buffer
{
public:
    VEnetBuffer(size_t len)
	{
	    m_data = new char[len];
	    m_len = len;
	} 

    ~VEnetBuffer() { delete[] (char*)m_data; }
};

/*
 * This class is instantiated whenever a new venet device is added to
 * our bus
 */
class VEnetDriver : public VBus::Driver {
public:
    VEnetDriver(VBus::DevicePtr dev) :
	m_dev(dev),
	m_rxq(m_dev->Queue(VENET_QUEUE_RX, QLEN)),
	m_txq(m_dev->Queue(VENET_QUEUE_TX, QLEN)),
	m_mtu(1500)
	{
	    std::cout << "new device found" << std::endl;

	    VBus::Queue::IteratorPtr iter;
	    
	    /*
	     * We want to iterate on the "valid" index.  By default the
	     * iterator will not "autoupdate" which means it will not
	     * system-call the host with our changes.  This is good, because
	     * we are really just initializing stuff here anyway.  Note that
	     * you can always manually signal the host with q->Signal() if
	     * the autoupdate feature is not used.
	     */
	    iter = m_rxq->IteratorCreate(VBus::Queue::IDX_VALID);
	    
	    /*
	     * Seek to the head of the valid index (which should be our first
	     * item, since the queue is brand-new)
	     */
	    iter->Seek(VBus::Queue::Iterator::ISEEK_HEAD, 0);
	    
	    /*
	     * Now populate each descriptor with an empty buffer
	     */
	    while (!iter->Desc()->Valid()) {
		AllocRxDesc(iter->Desc());
		
		/*
		 * This push operation will simultaneously advance the
		 * valid-head index and increment our position in the queue
		 * by one.
		 */
		iter->Push();
	    }

	    /*
	     * We are now ready to process packets, so tell the kernel that
	     * the link is up
	     */
	    m_dev->Call(VENET_FUNC_LINKUP, NULL, 0, 0);

	    /*
	     * And finally, fire up our Rx thread to handle incoming packets
	     */
	    using namespace boost;
	    thread t(bind(mem_fn(&VEnetDriver::RxThread), this));
	}

    /*
     * This function is launched as a thread to handle our RX notifications
     */
    void RxThread()
	{
	    VBus::Queue::IteratorPtr iter;

	    /* We want to iterate on the tail of the in-use index */
	    iter = m_rxq->IteratorCreate(VBus::Queue::IDX_INUSE);

	    iter->Seek(VBus::Queue::Iterator::ISEEK_TAIL,
		       VBus::Queue::ITER_AUTO_UPDATE);

	    for (;;)
	    {
		
		while (iter->Desc()->Owner() == 
		       VBus::Queue::Descriptor::OWNER_SOUTH)
		    m_rxq->Wait();

		/* Pass the buffer up to the stack */
		RxPacket(iter->Desc()->Buffer(), iter->Desc()->Len());
		VBus::MemoryBarrier();
		
		/* Grab a new buffer to put in the ring */
		AllocRxDesc(iter->Desc());

		/* Advance the in-use tail */
		iter->Pop();
	    }
	}

    /*
     * This is called by libvbus if the device we are communicating with
     * is removed from the bus
     */
    void Remove()
	{
	    std::cout << "device removed" << std::endl;
	}

private:

    void AllocRxDesc(VBus::Queue::Descriptor *desc)
	{
	    VBus::Queue::Descriptor::BufferPtr buf(new VEnetBuffer(m_mtu));
	    
	    desc->Buffer(buf);
	    VBus::MemoryBarrier();

	    desc->Valid(true);
	    desc->Owner(VBus::Queue::Descriptor::OWNER_SOUTH);
	}


    VBus::DevicePtr m_dev;
    VBus::QueuePtr  m_rxq;
    VBus::QueuePtr  m_txq;
    int             m_mtu;
};

/*
 * This Driver::Type class is registered with the bus so that we receive
 * a Type::Probe() callback whenever a device of interest is discovered
 * (or dynamically added) on our bus
 */
class VEnetType : public VBus::Driver::Type {
public:
    VBus::DriverPtr Probe(VBus::DevicePtr dev)
	{
	    VBus::DriverPtr drv(new VEnetDriver(dev));

	    return drv;
	}
};

int main()
{
	try {
	    VBus::Driver::Type::Register("virtual-ethernet",
					 VBus::Driver::TypePtr(new VEnetType));

	    VBus::Quiesce();

	    while(1) wait();

	} catch(std::exception &e) {
		
		std::cerr << "Exception: " << e.what() << std::endl;
		return -1;
	}

	return 0;
}
