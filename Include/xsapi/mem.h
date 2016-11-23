//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************
#pragma once
#include <new>
#include <stddef.h>
#include "xsapi/system.h"

namespace xbox { namespace services { namespace system {

class xsapi_memory
{
public:
    static _Ret_maybenull_ _Post_writable_byte_size_(dwSize) void* mem_alloc(
        _In_ size_t dwSize
        );

    static void mem_free(
        _In_ void* pAddress
        );

private:
    xsapi_memory();
    xsapi_memory(const xsapi_memory&);
    xsapi_memory& operator=(const xsapi_memory&);
};

class xsapi_memory_buffer
{
public:
    xsapi_memory_buffer(_In_ size_t dwSize)
    {
        m_pBuffer = xsapi_memory::mem_alloc(dwSize);
    }

    ~xsapi_memory_buffer()
    {
        xsapi_memory::mem_free(m_pBuffer);
        m_pBuffer = nullptr;
    }

    void* get()
    {
        return m_pBuffer;
    }

private:
    void* m_pBuffer;
};

}}}

template<typename T>
class xsapi_stl_allocator
{
public:
    xsapi_stl_allocator() { }

    template<typename Other> xsapi_stl_allocator(const xsapi_stl_allocator<Other> &) { }

    template<typename Other>
    struct rebind
    {
        typedef xsapi_stl_allocator<Other> other;
    };

    typedef size_t      size_type;
    typedef ptrdiff_t   difference_type;
    typedef T*          pointer;
    typedef const T*    const_pointer;
    typedef T&          reference;
    typedef const T&    const_reference;
    typedef T           value_type;

    pointer allocate(size_type n, const void * = 0)
    {
        pointer p = reinterpret_cast<pointer>(xbox::services::system::xsapi_memory::mem_alloc(n * sizeof(T)));
        if (p == NULL)
        {
            throw std::bad_alloc();
        }
        return p;
    }

    void deallocate(_In_opt_ void* p, size_type)
    {
        xbox::services::system::xsapi_memory::mem_free(p);
    }

    char* _Charalloc(size_type n)
    {
        char* p = reinterpret_cast<char*>(xbox::services::system::xsapi_memory::mem_alloc(n));
        if (p == NULL)
        {
            throw std::bad_alloc();
        }
        return p;
    }

    void construct(_In_ pointer p, const_reference t)
    {
        new ((void*)p) T(t);
    }

    void destroy(_In_ pointer p)
    {
#if _WIN32
        std::_Destroy(p);
#else
        p->~T();
#endif
    }

    size_t max_size() const
    {
        size_t n = (size_t)(-1) / sizeof(T);
        return (0 < n ? n : 1);
    }
};

template<typename T1, typename T2>
inline bool operator==(const xsapi_stl_allocator<T1>&, const xsapi_stl_allocator<T2>&)
{
    return true;
}

template<typename T1, typename T2>
bool operator!=(const xsapi_stl_allocator<T1>&, const xsapi_stl_allocator<T2>&)
{
    return false;
}

#define xsapi_internal_string std::basic_string<char_t, std::char_traits<char_t>, xsapi_stl_allocator<char_t> >

#define xsapi_internal_vector(T) std::vector<T, xsapi_stl_allocator<T> >

#define xsapi_internal_unordered_map(Key, T) std::unordered_map<Key, T, std::hash<Key>, std::equal_to<Key>, xsapi_stl_allocator< std::pair< const Key, T > > >

#define xsapi_internal_dequeue(T) std::deque<T, xsapi_stl_allocator<T> >