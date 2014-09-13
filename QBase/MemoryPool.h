#ifndef BERT_MEMORYPOOL_H
#define BERT_MEMORYPOOL_H

#include "./Threads/IPC.h"
#include <vector>


class MemoryPool
{
    static const unsigned ALIGN     = sizeof(void*);
    static const unsigned BUCKETS   = 512;
    static const unsigned MAX_SIZE  = ALIGN * BUCKETS;
    static const unsigned TRUNK_NUM = 32;

    struct mem_node
    {
        struct mem_node * next_free;
    };

    static __thread mem_node* m_freelist[BUCKETS];

    static __thread void*    m_pool;
    static __thread unsigned m_poolSize;

    static unsigned _RoundUp( unsigned size );
    static unsigned _GetBucketIndex(unsigned size);

    // ��ԭʼָ�����һ���������߳��˳�ͳһ�ͷ�
    // ��������һ�� PER-THREAD �̳߳�
    // ����ÿ���̶߳�Ҫ���� DESTRUCTOR
    static std::vector<void* > m_rawPtr;
    static Mutex m_mutex;
    
public:
    static void  Destructor();
    static void* allocate(unsigned size);
    static void  deallocate(const void*, unsigned size);

    static void* operator new(std::size_t size);
    static void* operator new[](std::size_t size);
    static void  operator delete( void * p , std::size_t size);
    static void  operator delete[]( void * p , std::size_t size);
};

#endif

