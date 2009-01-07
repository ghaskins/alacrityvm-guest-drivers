
#include <linux/vbus.h>

#include "privatevbus.hh"

using namespace VBus;

Impl::Queue::Descriptor::Descriptor(struct ioq_ring_desc *desc) :
    m_desc(desc)
{
    
}

void Impl::Queue::Descriptor::Buffer(Queue::Descriptor::BufferPtr buf,
				     Flags flags)
{
    ValidateFlags(0, flags);

    m_buf = buf;

    if (m_desc->ptr != (__u64)m_buf->m_data)
	m_desc->ptr = (__u64)m_buf->m_data;

    // This will set the length field, if necessary
    this->Reset();
}

Queue::Descriptor::BufferPtr Impl::Queue::Descriptor::Buffer()
{
    return m_buf;
}

void Impl::Queue::Descriptor::Reset()
{
    /*
     * We only update len if it changed to avoid dirtying the cacheline
     * unecessarily
     */
    if (m_desc->len != m_buf->m_len)
	m_desc->len = m_buf->m_len; 
}

size_t Impl::Queue::Descriptor::Len()
{
    return m_desc->len;
}

void Impl::Queue::Descriptor::Owner(Queue::Descriptor::OwnerType owner)
{
    m_desc->sown = owner == Impl::Queue::Descriptor::OWNER_NORTH ? 0 : 1;
}

Queue::Descriptor::OwnerType Impl::Queue::Descriptor::Owner()
{
    return m_desc->sown ?
	Impl::Queue::Descriptor::OWNER_SOUTH :
	Impl::Queue::Descriptor::OWNER_NORTH;
}

void Impl::Queue::Descriptor::Valid(bool val)
{
    m_desc->valid = val ? 1 : 0;
}

bool Impl::Queue::Descriptor::Valid()
{
    return m_desc->valid;
}

Impl::Queue::Queue(Device::Handle devh, unsigned long qid, size_t count) :
    m_handle(0)
{
    m_head = (struct ioq_ring_head*)new char[sizeof(struct ioq_ring_head)];

    memset(m_head, 0, sizeof(*m_head));

    m_head->magic = IOQ_RING_MAGIC;
    m_head->ver   = IOQ_RING_VER;
    m_head->count = count;

    size_t ringlen(sizeof(struct ioq_ring_desc) * count);
    m_ring = (struct ioq_ring_desc*)new char[ringlen];

    for (int i(0); i < count; i++)
    {
	struct ioq_ring_desc *desc(&m_ring[i]);

	memset(desc, 0, sizeof(*desc));
	
	desc->cookie = (__u64)new Impl::Queue::Descriptor(desc);
    }

    struct vbus_queuecreate args;

    args.devh   = devh;
    args.qid    = qid;
    args.flags  = 0;
    args.head   = (__u64)m_head;
    m_head->ptr = (__u64)m_ring;

    g_bus.Ioctl(VBUS_QUEUECREATE, &args);

    m_handle = args.handle;

    g_bus.Register(m_handle, this);
}

Impl::Queue::~Queue()
{
    if (m_handle)
	g_bus.Unregister(this);

    delete[] m_head;
    delete[] m_ring;
}

void Impl::Queue::Signal(Flags flags)
{
    ValidateFlags(0, flags);

    struct ioq_irq *irq = &m_head->irq[Descriptor::OWNER_SOUTH];

    irq->pending = 1;
    MemoryBarrier();

    if (irq->enabled)
	g_bus.Ioctl(VBUS_QUEUESIGNAL, &m_handle);
}

static inline void ValidateIndex(Queue::Index idx)
{
    if (idx < Queue::IDX_VALID || idx > Queue::IDX_INUSE)
	throw std::runtime_error("illegal index");
}

int Impl::Queue::Count(struct ioq_ring_idx *idx)
{
    if (idx->full && (idx->head == idx->tail))
	return m_head->count;
    else if (idx->head >= idx->tail)
	return idx->head - idx->tail;
    else
	return (idx->head + m_head->count) - idx->tail;
}

int Impl::Queue::Count(Queue::Index idx)
{
    ValidateIndex(idx);
    
    return Count(&m_head->idx[idx]);
}

bool Impl::Queue::Full(Queue::Index idx)
{
    ValidateIndex(idx);

    return m_head->idx[idx].full;
}

Queue::IteratorPtr
Impl::Queue::IteratorCreate(Queue::Index idx, Flags flags)
{
    ValidateIndex(idx);
    ValidateFlags(Queue::ITER_AUTO_UPDATE, flags);

    bool update(flags & Queue::ITER_AUTO_UPDATE);

    Queue::IteratorPtr iter(new Impl::Queue::Iterator(this, idx, update));

    return iter;
}

void
Impl::Queue::Wakeup()
{
    Lock l(m_mutex);

    m_cv.notify_all();
}

void
Impl::Queue::Wait()
{
    Lock l(m_mutex);

    struct ioq_irq *irq = &m_head->irq[Descriptor::OWNER_NORTH];

    irq->enabled = 1;
    MemoryBarrier();

    while (!irq->pending)
	m_cv.wait(l);

    irq->enabled = 0;
    MemoryBarrier();

    irq->pending = 0;
}

Impl::Queue::Iterator::Iterator(Queue *queue, Queue::Index idx, bool update) :
    m_queue(queue), m_update(update)
{
    ValidateIndex(idx);

    m_idx = &m_queue->m_head->idx[idx];
}

void Impl::Queue::Iterator::Seek(Queue::Iterator::SeekType type,
				 long offset,
				 Flags flags)
{
    ValidateFlags(0, flags);

    struct ioq_ring_head *head = m_queue->m_head;
    unsigned long pos;
    
    switch (type) {
	case Queue::Iterator::ISEEK_NEXT:
	    pos = m_pos + 1;
	    pos %= head->count;
	    break;
	case Queue::Iterator::ISEEK_TAIL:
	    pos = m_idx->tail;
	    break;
	case Queue::Iterator::ISEEK_HEAD:
	    pos = m_idx->head;
	    break;
	case Queue::Iterator::ISEEK_SET:
	    pos = offset;
	    break;
	default:
	    throw std::runtime_error("illegal seek type");
    }
  
    if (pos >= m_queue->m_head->count)
	throw std::runtime_error("illegal position");
    
    m_pos = pos;
    m_desc = (Impl::Queue::Descriptor*)m_queue->m_ring[pos].cookie;
}

void Impl::Queue::Iterator::Push(Flags flags)
{
    ValidateFlags(0, flags);

    struct ioq_ring_head *head = m_queue->m_head;
    int ret = -ENOSPC;
    
    /*
     * Its only valid to push if we are currently pointed at the head
     */
    if (m_pos != m_idx->head)
	throw std::runtime_error("illegal push");
    
    if (m_queue->Count(m_idx) < head->count) {
	m_idx->head++;
	m_idx->head %= head->count;
	
	if (m_idx->head == m_idx->tail)
	    m_idx->full = 1;
	
	MemoryBarrier();
	
	Seek(Queue::Iterator::ISEEK_NEXT, 0, 0);
	
	if (m_update)
	    m_queue->Signal(0);
    }
}

void Impl::Queue::Iterator::Pop(Flags flags)
{
    ValidateFlags(0, flags);

    struct ioq_ring_head *head = m_queue->m_head;
    
    /*
     * Its only valid to pop if we are currently pointed at the tail
     */
    if (m_pos != m_idx->tail)
	throw std::runtime_error("illegal pop");
    
    if (m_queue->Count(m_idx) != 0) {
	m_idx->tail++;
	m_idx->tail %= head->count;
	
	m_idx->full = 0;
	
	MemoryBarrier();
	
	Seek(Queue::Iterator::ISEEK_NEXT, 0, 0);
	
	if (m_update)
	    m_queue->Signal(0);
    }
}

VBus::Queue::Descriptor *Impl::Queue::Iterator::Desc()
{
    return dynamic_cast<VBus::Queue::Descriptor*>(m_desc);
}
