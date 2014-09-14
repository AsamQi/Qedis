#ifndef BERT_MEMORYPOOL_H
#define BERT_MEMORYPOOL_H

#include "./Threads/IPC.h"
#include <vector>


class MemoryPool
{
    static const std::size_t ALIGN     = sizeof(void*);
    static const std::size_t BUCKETS   = 512;
    static const std::size_t MAX_SIZE  = ALIGN * BUCKETS;
    static const std::size_t TRUNK_NUM = 32;

    struct mem_node
    {
        struct mem_node * next_free;
    };

    static __thread mem_node* m_freelist[BUCKETS];

    static __thread void*    m_pool;
    static __thread std::size_t m_poolSize;

    static std::size_t _RoundUp(std::size_t size );
    static std::size_t _GetBucketIndex(std::size_t size);

    // ��ԭʼָ�����һ���������߳��˳�ͳһ�ͷ�
    // ��������һ�� PER-THREAD �̳߳�
    // ����ÿ���̶߳�Ҫ���� DESTRUCTOR
    static std::vector<void* > m_rawPtr;
    static Mutex m_mutex;
    
public:
    static void  Destructor();
    static void* allocate(std::size_t size);
    static void  deallocate(const void*, std::size_t size);

    static void* operator new(std::size_t size);
    static void* operator new[](std::size_t size);
    static void  operator delete( void * p , std::size_t size);
    static void  operator delete[]( void * p , std::size_t size);
};

#endif

