#pragma once

#include <atomic>
#include <cassert>
#include <cstdint>

class NonCopyable {
   public:
    NonCopyable() = default;
    ~NonCopyable() = default;

    NonCopyable(const NonCopyable&) = delete;
    NonCopyable& operator=(const NonCopyable&) = delete;

    NonCopyable(NonCopyable&&) = default;
    NonCopyable& operator=(NonCopyable&&) = default;
};

template <class T>
class Ref;
template <class T>
class RefConst;

template <class T>
class RefTarget {
   public:
    inline RefTarget() = default;
    inline RefTarget(const RefTarget&) { /* Do not copy ref count */ }
    inline ~RefTarget() {
        uint32_t value = m_ref_count.load(std::memory_order_relaxed);
        assert((value == 0 || value == EMBEDDED) && ("Object deleted while still referenced!"));
    }

    inline void set_embedded() const {
        uint32_t old = m_ref_count.fetch_add(EMBEDDED, std::memory_order_relaxed);
        assert(old < EMBEDDED && "Object is already embedded!");
    }

    inline RefTarget& operator=(const RefTarget&) { return *this; /* Don't copy ref count */ }

    uint32_t get_ref_count() const { return m_ref_count.load(std::memory_order_relaxed); }

    inline void add_ref() const { m_ref_count.fetch_add(1, std::memory_order_relaxed); }

    inline void release() const {
        uint32_t old_value = m_ref_count.fetch_sub(1, std::memory_order_release);
        if (old_value == 1) {
            std::atomic_thread_fence(std::memory_order_acquire);
            delete static_cast<const T*>(this);
        }
        assert(old_value != 0 && old_value != EMBEDDED && "Too many calls to release!");
    }

   protected:
    static constexpr uint32_t EMBEDDED = 0x0ebedded;
    mutable std::atomic<uint32_t> m_ref_count{0};
};

template <class T>
class Ref {
   public:
    inline Ref() : m_ptr(nullptr) {}
    inline Ref(T* ptr) : m_ptr(ptr) { add_ref(); }
    inline Ref(const Ref<T>& other) : m_ptr(other.m_ptr) { add_ref(); }
    inline Ref(Ref<T>&& other) noexcept : m_ptr(other.m_ptr) { other.m_ptr = nullptr; }
    inline ~Ref() { release(); }

    inline Ref<T>& operator=(T* ptr) {
        if (m_ptr != ptr) {
            release();
            m_ptr = ptr;
            add_ref();
        }
        return *this;
    }

    inline Ref<T>& operator=(const Ref<T>& other) {
        if (m_ptr != other.m_ptr) {
            release();
            m_ptr = other.m_ptr;
            add_ref();
        }
        return *this;
    }

    inline Ref<T>& operator=(Ref<T>&& other) noexcept {
        if (m_ptr != other.m_ptr) {
            release();
            m_ptr = other.m_ptr;
            other.m_ptr = nullptr;
        }
        return *this;
    }

    inline T* operator->() const { return m_ptr; }
    inline T& operator*() const { return *m_ptr; }
    inline explicit operator bool() const { return m_ptr != nullptr; }
    inline T* get() const { return m_ptr; }

    inline bool operator==(const Ref<T>& other) const { return m_ptr == other.m_ptr; }
    inline bool operator!=(const Ref<T>& other) const { return m_ptr != other.m_ptr; }
    inline bool operator==(const T* ptr) const { return m_ptr == ptr; }
    inline bool operator!=(const T* ptr) const { return m_ptr != ptr; }

   private:
    template <class T2>
    friend class RefConst;

    inline void add_ref() {
        if (m_ptr != nullptr) m_ptr->add_ref();
    }
    inline void release() {
        if (m_ptr != nullptr) m_ptr->release();
    }

    T* m_ptr;
};

template <class T>
class RefConst {
   public:
    inline RefConst() : m_ptr(nullptr) {}
    inline RefConst(const T* ptr) : m_ptr(ptr) { add_ref(); }
    inline RefConst(const RefConst<T>& other) : m_ptr(other.m_ptr) { add_ref(); }
    inline RefConst(const Ref<T>& other) : m_ptr(other.m_ptr) { add_ref(); }
    inline ~RefConst() { release(); }

    inline RefConst<T>& operator=(const T* ptr) {
        if (m_ptr != ptr) {
            release();
            m_ptr = ptr;
            add_ref();
        }
        return *this;
    }

    inline RefConst<T>& operator=(const RefConst<T>& other) {
        if (m_ptr != other.m_ptr) {
            release();
            m_ptr = other.m_ptr;
            add_ref();
        }
        return *this;
    }

    inline RefConst<T>& operator=(const Ref<T>& other) {
        if (m_ptr != other.m_ptr) {
            release();
            m_ptr = other.m_ptr;
            add_ref();
        }
        return *this;
    }

    inline const T* operator->() const { return m_ptr; }
    inline const T& operator*() const { return *m_ptr; }
    inline explicit operator bool() const { return m_ptr != nullptr; }
    inline const T* get() const { return m_ptr; }

   private:
    inline void add_ref() {
        if (m_ptr != nullptr) m_ptr->add_ref();
    }
    inline void release() {
        if (m_ptr != nullptr) m_ptr->release();
    }

    const T* m_ptr;
};
