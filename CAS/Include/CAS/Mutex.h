#include <atomic>
#include <mutex>

#if defined(__x86_64__) || defined(__i386__)
#include <immintrin.h>
#elif defined(__aarch64__) || defined(__arm__)
#include <arm_acle.h>
#endif

#if defined(__linux__) && defined(__GLIBC__)
#include <pthread.h>
#endif

inline void cpu_relax() noexcept
{
#if defined(__x86_64__) || defined(__i386__)
    _mm_pause();          // x86 PAUSE hint
#elif defined(__aarch64__) || defined(__arm__)
    __yield();            // ARM YIELD hint
#endif
}

namespace CAS 
{

#if defined(__linux__) && defined(__GLIBC__)

    class Spinlock
    {
    public:
        Spinlock() noexcept
        {
            pthread_spin_init(&m_mutex, PTHREAD_PROCESS_PRIVATE);
        }

        ~Spinlock() noexcept
        {
            pthread_spin_destroy(&m_mutex);
        }

        Spinlock(const Spinlock&) = delete;
        Spinlock& operator=(const Spinlock&) = delete;

        void lock() noexcept
        {
            pthread_spin_lock(&m_mutex);
        }
        void unlock() noexcept
        {
            pthread_spin_unlock(&m_mutex); 
        }

    private:
        pthread_spinlock_t m_mutex{};
    };

#else

/**
 * 一个锁要保证两件事：

 * 互斥（mutual exclusion）：同一时刻只有一个线程能进入临界区。
 * 可见性（visibility）：线程 A 在临界区内写的所有数据，线程 B 下一次拿到锁后必须能看到。
 */
    class Spinlock
    {
    public:
        using Lock = std::lock_guard<Spinlock>;
        
        Spinlock() noexcept = default;
        ~Spinlock() noexcept = default;

        Spinlock(const Spinlock&) = delete;
        Spinlock& operator=(const Spinlock&) = delete;

        void lock() noexcept
        {
            while(m_mutex.test_and_set(std::memory_order_acquire))
            {
                while(m_mutex.test(std::memory_order_relaxed))
                {
                    cpu_relax();
                }
            }
        }

        [[nodiscard]] bool try_lock() noexcept
        {
            return !m_mutex.test_and_set(std::memory_order_acquire);
        }

        void unlock() noexcept
        {
            m_mutex.clear(std::memory_order_release);
        }

    private:
        std::atomic_flag m_mutex = ATOMIC_FLAG_INIT;
    };
#endif
};