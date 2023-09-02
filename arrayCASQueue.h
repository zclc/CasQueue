#pragma once

#include "dbg.h"

#ifdef __GNUC__
	#define CAS(a_ptr, a_oldVal, a_newVal) __sync_bool_compare_and_swap(a_ptr, a_oldVal, a_newVal)
	#define AtomicAdd(a_ptr,a_count) __sync_fetch_and_add (a_ptr, a_count)
	#define AtomicSub(a_ptr,a_count) __sync_fetch_and_sub (a_ptr, a_count)
	#include <sched.h> // sched_yield()
#else

#include <Windows.h>
#ifdef _WIN64
	#define CAS(a_ptr, a_oldVal, a_newVal) (a_oldVal == InterlockedCompareExchange64(a_ptr, a_newVal, a_oldVal))
	#define sched_yield()	SwitchToThread()
	#define AtomicAdd(a_ptr, num)	InterlockedIncrement64(a_ptr)
	#define AtomicSub(a_ptr, num)	InterlockedDecrement64(a_ptr)
#else
	#define CAS(a_ptr, a_oldVal, a_newVal) (a_oldVal == InterlockedCompareExchange(a_ptr, a_newVal, a_oldVal))
	#define sched_yield()	SwitchToThread()
	#define AtomicAdd(a_ptr, num)	InterlockedIncrement(a_ptr)
	#define AtomicSub(a_ptr, num)	InterlockedDecrement(a_ptr)
#endif

#endif


#define ARRAY_SIZE 65535

using u64INT = unsigned long;

template<class ELEM_T, u64INT Q_SIZE = ARRAY_SIZE>
class ArrayCASQueue
{
public:
    ArrayCASQueue();
    virtual ~ArrayCASQueue();

    u64INT size();

    bool enqueue(const ELEM_T& a_data);

    bool dequeue(ELEM_T& a_data);

private:

    ELEM_T m_thequeue[Q_SIZE];

    volatile u64INT m_count = 0;
    volatile u64INT m_writeIndex = 0;
    volatile u64INT m_readIndex = 0;
    volatile u64INT m_maximumReadIndex = 0;

    inline u64INT countToIndex(u64INT a_count);
};

#include "arrayCASQueue.h"

template <class ELEM_T, u64INT Q_SIZE>
inline ArrayCASQueue<ELEM_T, Q_SIZE>::ArrayCASQueue()
{
}

template <class ELEM_T, u64INT Q_SIZE>
inline ArrayCASQueue<ELEM_T, Q_SIZE>::~ArrayCASQueue()
{
}

template <class ELEM_T, u64INT Q_SIZE>
inline u64INT ArrayCASQueue<ELEM_T, Q_SIZE>::size()
{
    u64INT currentWriteIndex = m_writeIndex;
    u64INT currentReadIndex = m_readIndex;

    if (currentWriteIndex >= currentReadIndex)
        return currentWriteIndex - currentReadIndex;
    else 
        return Q_SIZE + currentWriteIndex - currentReadIndex;
}

template <class ELEM_T, u64INT Q_SIZE>
inline bool ArrayCASQueue<ELEM_T, Q_SIZE>::enqueue(const ELEM_T &a_data)
{
    u64INT currentWriteIndex;/* 获取写指针的位置 */
    u64INT currentReadIndex;
    do
    {
        currentWriteIndex = m_writeIndex;
        currentReadIndex = m_readIndex;
        if(countToIndex(currentWriteIndex+1) ==
            countToIndex(currentReadIndex))
        {
            return false;/* 队列已经满了 */
        }

    } while (!CAS(&m_writeIndex, currentWriteIndex, (currentWriteIndex+1)));
    
    m_thequeue[countToIndex(currentWriteIndex)] = a_data;

    while (!CAS(&m_maximumReadIndex, currentWriteIndex, (currentWriteIndex+1)))
    {
        sched_yield();
    }

    AtomicAdd(&m_count, 1);

    return true;
}

template <class ELEM_T, u64INT Q_SIZE>
inline bool ArrayCASQueue<ELEM_T, Q_SIZE>::dequeue(ELEM_T &a_data)
{
    u64INT currentMaximumReadIndex;
    u64INT currentReadIndex;

    do
    {
        currentReadIndex = m_readIndex;
        currentMaximumReadIndex = m_maximumReadIndex;

        if(countToIndex(currentReadIndex) ==
            countToIndex(currentMaximumReadIndex))
        {
            // 队列为空
            return false;
        }

        a_data = m_thequeue[countToIndex(currentReadIndex)];

        if(CAS(&m_readIndex, currentReadIndex, (currentReadIndex+1)))
        {
            AtomicSub(&m_count, 1);
            return true;
        }

    } while (true);

    return false;
}

template <class ELEM_T, u64INT Q_SIZE>
inline u64INT ArrayCASQueue<ELEM_T, Q_SIZE>::countToIndex(u64INT a_count)
{
    return a_count % Q_SIZE;
}