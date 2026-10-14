#pragma once

#include "concurrency/OSThread.h"
#include "freertosinc.h"
#include <cassert>
#ifdef ARCH_ZEPHYR
#include <zephyr/kernel.h>
#endif

/**
 * A static circular buffer queue for pointers.
 * This provides the same interface as PointerQueue but uses a statically allocated
 * buffer instead of dynamic allocation.
 */
template <class T, int MaxElements> class StaticPointerQueue
{
    static_assert(MaxElements > 0, "MaxElements must be greater than 0");

    T *buffer[MaxElements];
    int head = 0;
    int tail = 0;
    int count = 0;
    concurrency::OSThread *reader = nullptr;
#ifdef ARCH_ZEPHYR
    mutable struct k_spinlock lock {
    };
    class Guard
    {
        struct k_spinlock *lock;
        k_spinlock_key_t key;

      public:
        explicit Guard(struct k_spinlock *value) : lock(value), key(k_spin_lock(value)) {}
        ~Guard() { k_spin_unlock(lock, key); }
    };
#endif

  public:
    StaticPointerQueue()
    {
        // Initialize all buffer elements to nullptr to silence warnings and ensure clean state
        for (int i = 0; i < MaxElements; i++) {
            buffer[i] = nullptr;
        }
    }

    int numFree() const { return MaxElements - numUsed(); }

    bool isEmpty() const { return numUsed() == 0; }

    int numUsed() const
    {
#ifdef ARCH_ZEPHYR
        Guard guard(&lock);
#endif
        return count;
    }

    bool enqueue(T *x, TickType_t maxWait = portMAX_DELAY)
    {
#ifdef ARCH_ZEPHYR
        concurrency::OSThread *wakeReader;
        {
            Guard guard(&lock);
            if (count >= MaxElements) {
                return false; // Queue is full
            }
            buffer[tail] = x;
            tail = (tail + 1) % MaxElements;
            count++;
            wakeReader = reader;
        }
        if (wakeReader) {
            wakeReader->setInterval(0);
            concurrency::mainDelay.interrupt();
        }
        return true;
#else
        if (count >= MaxElements) {
            return false;
        }
        if (reader) {
            reader->setInterval(0);
            concurrency::mainDelay.interrupt();
        }
        buffer[tail] = x;
        tail = (tail + 1) % MaxElements;
        count++;
        return true;
#endif
    }

    bool dequeue(T **p, TickType_t maxWait = portMAX_DELAY)
    {
#ifdef ARCH_ZEPHYR
        Guard guard(&lock);
#endif
        if (count == 0) {
            return false; // Queue is empty
        }

        *p = buffer[head];
        head = (head + 1) % MaxElements;
        count--;
        return true;
    }

    // returns a ptr or null if the queue was empty
    T *dequeuePtr(TickType_t maxWait = portMAX_DELAY)
    {
        T *p;
        return dequeue(&p, maxWait) ? p : nullptr;
    }

    void setReader(concurrency::OSThread *t)
    {
#ifdef ARCH_ZEPHYR
        Guard guard(&lock);
#endif
        reader = t;
    }

    // For compatibility with PointerQueue interface
    int getMaxLen() const { return MaxElements; }
};
