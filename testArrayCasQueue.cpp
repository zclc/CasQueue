#include <iostream>
#include <vector>
#include <deque>
#include <list>
#include <string>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>

#include "dbg.h"

#include "arrayCASQueue.h"

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


using namespace std;



static u64INT s_queue_item_num = 2000000; // 每个线程插入的元素个数
static int s_producer_thread_num = 1;   // 生产者线程数量
static int s_consumer_thread_num = 1;   // 消费线程数量

// 有锁队列
std::list<int> s_list;
std::mutex s_mutex;

// 无锁队列
ArrayCASQueue<int, 65535> arraylockfree;

atomic<int> s_count_push = 0;
atomic<int> s_count_pop  = 0;

void CasProducer()
{
    auto start = chrono::steady_clock::now();
    int writeFailedCount = 0;

    for (size_t i = 0; i < s_queue_item_num;)
    {
        if (arraylockfree.enqueue(s_count_push))
        {
            s_count_push++;
            i++;
        }
        else
        {
            writeFailedCount++;
            sched_yield();
        }
    }
    auto end = chrono::steady_clock::now();

    auto ms = chrono::duration_cast<chrono::milliseconds>(end-start).count();

    log_info("producer time used : [ %ld ] ms", ms);

}

void CasConsumer()
{
    int last_value = 0;
    int value = 0;
    int read_failed_count = 0;

    auto start = chrono::steady_clock::now();
    while (true)
    {
        if (arraylockfree.dequeue(value))
        {
            s_count_pop++;
        }
        else
        {
            read_failed_count++;
            sched_yield();
        }

        if (s_count_pop >= s_queue_item_num * s_producer_thread_num)
        {
            break;
        }
    }

    auto end = chrono::steady_clock::now();

    auto ms = chrono::duration_cast<chrono::milliseconds>(end-start).count();
    
    log_info("Consumer time used : [ %ld ] ms", ms);
}

int main(int argc, char const *argv[])
{
    if(argc != 3)
    {
        cout <<"usage: ./a.out consumer_num produce_num";
        return -1;
    }

    s_consumer_thread_num = atoi(argv[1]);
    s_producer_thread_num = atoi(argv[2]);

    vector<thread> v_consumer;
    vector<thread> v_producer;

    for (size_t i = 0; i < s_consumer_thread_num; i++)
    {
        v_consumer.push_back(thread(CasConsumer));
    }
    
    for (size_t i = 0; i < s_consumer_thread_num; i++)
    {
        v_consumer.push_back(thread(CasProducer));
    }

    for(auto& i : v_consumer)
    {
        i.join();
    }

    for(auto& i : v_producer)
    {
        i.join();
    }

    return 0;
}