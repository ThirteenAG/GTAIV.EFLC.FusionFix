#pragma once
/*
MIT License

Copyright (c) 2022 Morgan McGuire and Zander Majercik

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#define USE_SSE_MEMCPY 1

#include <string>
#include <stdint.h>
#include <assert.h>
#include <algorithm>
#include <cstring>
#include <iterator>
#include <cstddef>
#include <limits>
#include <ios>
#include <iostream>
#include <string_view>
#include <initializer_list>
#include <errno.h>

#if defined(USE_SSE_MEMCPY) && USE_SSE_MEMCPY
#   if defined(__i386__) || defined(_M_IX86) || defined(__x86_64__) || defined(_M_X64)
#       define SSE_x64
#       include <immintrin.h>
        typedef __m128i u64x2_t;
#   elif  defined(__ARM_NEON) || defined(__arm__) || defined(_M_ARM)
#       include <arm_neon.h>
        typedef uint64x2_t u64x2_t;
#   endif 
#endif

#if defined(USE_G3D_ALLOCATOR) || (G3D_ALLOCATOR == 1)
#   include <G3D-base/System.h>
#endif

#ifdef G3D_System_h
#define TEMPLATE template<size_t INTERNAL_SIZE = 64, class Allocator = G3D::g3d_allocator<char>>
#else
#define TEMPLATE template<size_t INTERNAL_SIZE = 64, class Allocator = ::std::allocator<char>>
#endif

bool inConstSegment(const char* c);

constexpr size_t SSO_ALIGNMENT = 16;

/**
   \brief Very fast string class that follows the std::string/std::basic_string interface.

   - Recognizes constant segment strings and avoids copying them
   - Stores small strings internally to avoid heap allocation
   - Uses SSE instructions to copy internal strings
   - Uses the G3D free-list/block allocator when heap allocation is required

   INTERNAL_SIZE is in bytes. It should be chosen to be a multiple of 16.
*/
TEMPLATE
class
    /** This inline storage is used when strings are small */

alignas(SSO_ALIGNMENT)
SIMDString {
public:

    typedef char                                    value_type;
    typedef std::char_traits<value_type>            traits_type;
    typedef value_type&                             reference;
    typedef const value_type&                       const_reference;
    typedef value_type*                             pointer;
    typedef const value_type*                       const_pointer;
    typedef ptrdiff_t                               difference_type;
    typedef size_t                                  size_type;

    // We use custom iterators for this function, because otherwise 
    // SIMDString(char*, 0) is ambiguously overloaded
    template<typename StrType>
    class Const_Iterator {
    public:
        using iterator_category = std::random_access_iterator_tag;
        using value_type = typename StrType::value_type;
        using reference = const value_type&;
        using pointer = typename StrType::const_pointer;
        using difference_type = typename StrType::difference_type;
    private:
        pointer m_ptr; 
    public:

        Const_Iterator() : m_ptr(nullptr) {}
        Const_Iterator(pointer ptr) : m_ptr(ptr) {}
        Const_Iterator(const Const_Iterator& i) : m_ptr(i.m_ptr) {}

        inline reference operator*() const { return *m_ptr; }
        inline pointer operator->() const { std::pointer_traits<pointer>::pointer_to(**this); }
        inline reference operator[](difference_type rhs) { return m_ptr[rhs]; }

        inline Const_Iterator& operator+=(difference_type rhs) {m_ptr += rhs; return *this;}
        inline Const_Iterator& operator++() { m_ptr++; return *this; }  
        inline Const_Iterator operator++(int) { Const_Iterator tmp (*this); ++m_ptr; return tmp; }
        inline Const_Iterator& operator-=(difference_type rhs) {m_ptr -= rhs; return *this;}
        inline Const_Iterator& operator--() { m_ptr--; return *this; }  
        inline Const_Iterator operator--(int) {  Const_Iterator tmp(*this); --(*this); return tmp; }

        inline difference_type operator-(const Const_Iterator& rhs) const {return m_ptr - rhs.m_ptr;}
        inline Const_Iterator operator-(difference_type rhs) const { Const_Iterator tmp (*this); return tmp -= rhs; }
        inline Const_Iterator operator+(difference_type rhs) const { Const_Iterator tmp (*this); return tmp += rhs; }
        friend inline Const_Iterator operator+(difference_type lhs, Const_Iterator<StrType> rhs){ return rhs += lhs; }

        inline bool operator== (const Const_Iterator& rhs) const { return m_ptr == rhs.m_ptr; };
        inline bool operator!= (const Const_Iterator& rhs) const { return m_ptr != rhs.m_ptr; };  
        inline bool operator< (const Const_Iterator& rhs) const { return m_ptr < rhs.m_ptr; };
        inline bool operator<= (const Const_Iterator& rhs) const { return m_ptr <= rhs.m_ptr; };  
        inline bool operator> (const Const_Iterator& rhs) const { return m_ptr > rhs.m_ptr; };
        inline bool operator>= (const Const_Iterator& rhs) const { return m_ptr >= rhs.m_ptr; }; 
    };

    template<typename StrType>
    class Iterator: public Const_Iterator<StrType> {
    public:
        using super = Const_Iterator<StrType>;
        using iterator_category = std::random_access_iterator_tag;
        using value_type = typename StrType::value_type;
        using reference = value_type&;
        using pointer = typename StrType::pointer;
        using difference_type = typename StrType::difference_type;

        using super::super;

        inline reference operator*() {  return const_cast<reference>(super::operator*()); }
        inline pointer operator->() { std::pointer_traits<pointer>::pointer_to(**this);  }
        inline reference operator[](difference_type diff) { return const_cast<reference>(super::operator[](diff)); }

        inline Iterator& operator+=(difference_type rhs) { super::operator+=(rhs); return *this; }
        inline Iterator& operator++() { super::operator++(); return *this; }  
        inline Iterator operator++(int) { Iterator tmp (*this); super::operator++(); return tmp; }
        inline Iterator& operator-=(difference_type rhs) { super::operator-=(rhs); return *this; }
        inline Iterator& operator--() { super::operator--(); return *this; }  
        inline Iterator operator--(int) { Iterator tmp(*this); super::operator--(); return tmp; }

        using super::operator-;
        inline Iterator operator-(difference_type rhs) const { Iterator tmp (*this); return tmp -= rhs; }
        inline Iterator operator+(difference_type rhs) const { Iterator tmp (*this); return tmp += rhs; }
        friend inline Iterator operator+(difference_type lhs, Iterator<StrType> rhs){ return rhs += lhs; }

        using super::operator==;
        using super::operator!=;
        using super::operator<=;
        using super::operator>=;
        using super::operator<;
        using super::operator>;
    };

    typedef Const_Iterator<SIMDString>        const_iterator;
    typedef Iterator<SIMDString>              iterator;
    typedef std::reverse_iterator<const_iterator>    const_reverse_iterator;
    typedef std::reverse_iterator<iterator>          reverse_iterator;

protected:
    // Throw compile time error if INTERNAL_SIZE is not a multiple of SSO_ALIGNMENT
    static_assert(INTERNAL_SIZE % SSO_ALIGNMENT == 0, "SIMDString Internal Size must be a multiple of 16");

    // Using a union to save space. m_buffer and m_ptr are never used at the same time. 
    union {
        // This is intentionally char, as it is bytes
        char                m_buffer[INTERNAL_SIZE];
        pointer             m_ptr;
    };

    /** Bytes to but not including '\0' */
    size_type   m_length = 0;



        // Uses the empty-base optimization trick from the standard library: http://www.cantrip.org/emptyopt.html,
        // which unfortunately requires slightly obfuscating the code by sticking the
        // m_allocated member into the data property, which we try to minimize the visibility
        // of using the m_allocated macro.

    mutable struct _AllocHider : public Allocator {
        _AllocHider() { }
        _AllocHider(size_t allocated) : m_allocated(allocated) { }
        size_t      m_allocated = INTERNAL_SIZE;
    } m_allocator;

    /** Total size of data() including '\0', or 0 if data() is in a const segment. This is actually
    *   implemented in the m_allocator.m_allocated property in practice.
    *    
    *   There are 3 modes you can be in: 
    *   1. In the constant data segment: m_allocated = 0 (inConst() = true)
    *   2. In the buffer inline data structure: m_allocated = INTERNAL_SIZE (inBuffer() = true)
    *   3. In the heap: m_allocated is the size of the allocated data. This will 
    *                   never be less than INTERNAL_SIZE (inHeap() = true) */
#   define m_allocatedSize m_allocator.m_allocated

    constexpr inline bool inConst() const {
        return !m_allocatedSize;
    }

    constexpr inline bool inHeap() const {
        return m_allocatedSize > INTERNAL_SIZE;
    }

    constexpr inline bool inBuffer() const {
        return !(m_allocatedSize - INTERNAL_SIZE);
    }

    /** Requires 128-bit alignment */
    constexpr inline static void swapBuffer(void* buf1, void* buf2) {
#       if USE_SSE_MEMCPY
            // Can assume that INTERNAL_SIZE % SSO_ALIGNMENT == 0 because of the static assertion on line 201
            u64x2_t* d = reinterpret_cast<u64x2_t*>(buf1);
            u64x2_t* s = reinterpret_cast<u64x2_t*>(buf2);
            u64x2_t tmp;

            unsigned int i = (unsigned int) (INTERNAL_SIZE / SSO_ALIGNMENT);
            assert(i);
            while(i--) {
#               ifdef SSE_x64
                    tmp = _mm_stream_load_si128(d + i);
                    d[i] = _mm_load_si128(s + i);
                    s[i] = _mm_load_si128(&tmp);
#               else
                    tmp = d[i];
                    d[i] = s[i];
                    s[i] = tmp;
#               endif
            }
#       else
            char tmp[INTERNAL_SIZE];
            ::memcpy(tmp, buf1, INTERNAL_SIZE);
            ::memcpy(buf1, buf2, INTERNAL_SIZE);
            ::memcpy(buf2, tmp, INTERNAL_SIZE);
#       endif
    }

    /** Requires 128-bit alignment */
    constexpr inline static void memcpyBuffer(void* dst, const void* src, size_t count = INTERNAL_SIZE) {
#       if USE_SSE_MEMCPY
            // Can assume that INTERNAL_SIZE % SSO_ALIGNMENT == 0 because of the static assertion on line 201
            u64x2_t* d = reinterpret_cast<u64x2_t*>(dst);

            #ifdef SSE_x64
                const u64x2_t* s = reinterpret_cast<const u64x2_t*>(src);
            #else
                const uint64_t* s = reinterpret_cast<const uint64_t*>(src);
            #endif

            size_t i = count / SSO_ALIGNMENT;
            assert(i);
            while (i--) {
#               ifdef SSE_x64
                    d[i] = _mm_stream_load_si128(s + i);
#               else
                    d[i] = vld1q_u64(s + (2 * i));
#               endif 
            }
#       else
            ::memcpy(dst, src, count);
#       endif
    }

    constexpr inline pointer alloc(size_t b) {
        if (b <= INTERNAL_SIZE) {
            return m_buffer;
        } else {
            m_ptr = m_allocator.allocate(b);
            return m_ptr; 
        }
    }

    constexpr void free(void* p, size_t oldSize) const {
        m_allocator.deallocate(static_cast<pointer>(p), oldSize);
    }


    /** Choose the number of bytes to allocate to hold a string of length L 
     *  Note: Calling functions are expected to +1 for the null terminator */
    constexpr inline static size_t chooseAllocationSize(size_t requestedSize) {
        // Avoid allocating more than internal size unless required, but always allocate at least the internal size
        return (requestedSize <= INTERNAL_SIZE) ? INTERNAL_SIZE : std::max((size_t)(2 * requestedSize + 1), (size_t)(2 * INTERNAL_SIZE + 1));
    }

    constexpr pointer prepareToMutate() {
        if (inConst()) {
            pointer const old = m_ptr;
            m_allocatedSize = chooseAllocationSize(m_length + 1);
            // can call alloc and assign directly to m_buffer or m_ptr
            // because we know the old pointer points to const data
            pointer dataPtr = (pointer) alloc(m_allocatedSize);
            ::memcpy(dataPtr, old, m_length + 1);
            return dataPtr; 
        }
        return data();
    }

    /** Ensure enough bytes are allocated to hold a string of length newSize
     *  and copies the old string over 
     *  Note: Calling functions are expected to +1 for the null terminator */
    constexpr pointer ensureAllocation(size_t newSize) {
        if (m_allocatedSize < newSize) {
            const bool wasInHeap = inHeap(); 
            pointer const old = data();
            const size_type oldSize = m_allocatedSize;
            m_allocatedSize = chooseAllocationSize(newSize);
            pointer newPtr = m_buffer; 
            if (inBuffer()){
                ::memcpy(newPtr, old, m_length); 
            } else {
                // do not set m_ptr directly because old data could be in m_buffer
                newPtr = m_allocator.allocate(m_allocatedSize); 
                ::memcpy(newPtr, old, m_length); 
                m_ptr = newPtr;
            }
            if (wasInHeap) free(old, oldSize);
            return newPtr; 
        }
        return data();
    }

    /** Create a gap in the data for insert/replace.
     *  Re-allocates if necessary
     *  Note: Calling functions are expected to +1 for the null terminator */
    constexpr pointer createGap(size_type newSize, size_type count, size_type count2, size_type pos) {
        if (((m_allocatedSize < newSize) && !(inBuffer() && (newSize < INTERNAL_SIZE))) || inConst()) {
            // Allocate a new string and copy over first n values
            const bool wasInHeap = inHeap(); 
            pointer const old = data();
            const size_type oldSize = m_allocatedSize;
            m_allocatedSize = chooseAllocationSize(newSize);
            pointer newPtr = m_buffer;
            if (inBuffer()) {
                // copy [old, old + pos) to [newPtr, newPtr + pos)
                ::memcpy(newPtr, old, pos);
                // copy [old + pos + count, old + m_length) to [newPtr + pos + count2, newPtr + newSize)
                ::memcpy(newPtr + pos + count2, old + pos + count, m_length - pos - count + 1);
            } else {
                newPtr = m_allocator.allocate(m_allocatedSize); 
                // copy [old, old + pos) to [newPtr, newPtr + pos)
                ::memcpy(newPtr, old, pos);
                // copy [old + pos + count, old + m_length) to [newPtr + pos + count2, newPtr + newSize)
                ::memcpy(newPtr + pos + count2, old + pos + count, m_length - pos - count + 1);
                m_ptr = newPtr;
            }
            if (wasInHeap) { free(old, oldSize); }
            return newPtr;
        } else {
            // move [data() + pos + count, data() + m_length) to [data() + pos + count2, data() + newSize)
            pointer const dataPtr = data();
            memmove(dataPtr + pos + count2, dataPtr + pos + count, m_length - pos - count + 1);
            return dataPtr; 
        }
    }

    constexpr inline void maybeDeallocate() {
        if (inHeap()) {
            // Free previously allocated data
            free(m_ptr, m_allocatedSize);
            m_allocatedSize = INTERNAL_SIZE;
        }
    }

    /** Ensure enough bytes are allocated to hold a string of length newSize.
     *  Note: Calling functions are expected to +1 for the null terminator */
    constexpr inline pointer maybeReallocate(size_t newSize) {
        // Don't waste an allocation if the memory already allocated is large enough to hold the new data
        if (m_allocatedSize && m_allocatedSize >= newSize) {
            return data();
        }

        // free the old data, if applicable
        if (inHeap()) {
            // Free previously allocated data
            free(m_ptr, m_allocatedSize);
        }

        // allocate memory
        m_allocatedSize = chooseAllocationSize(newSize);
        return alloc(m_allocatedSize);
    }

    // primary template handles types that have no nested ::iterator_category:
    template<class InputIter, class = void>
    static inline constexpr bool is_iterator = false;

    // specialization recognizes types that have a nested ::iterator_category
    // void_t is part of c++17 and up
#if (__cplusplus < 201703L) && !defined(_MSC_VER)
    template<class InputIter>
    static constexpr bool is_iterator<InputIter, std::__void_t<typename std::iterator_traits<InputIter>::iterator_category>> = true;
#else 
    template<class InputIter>
    static constexpr bool is_iterator<InputIter, std::void_t<typename std::iterator_traits<InputIter>::iterator_category>> = true;
#endif

    #define ITERATOR_TRAITS template<typename InputIter, std::enable_if_t<is_iterator<InputIter>, int> = 0>

    // Construct for input iterators, which do not implement operator-
    ITERATOR_TRAITS
    inline void m_construct(InputIter first, InputIter last, std::input_iterator_tag t) {
        m_length = 0;

        // first allocate to buffer
        m_allocatedSize = INTERNAL_SIZE;
        while (first != last && m_length < m_allocatedSize) {
            m_buffer[m_length++] = *first++;
        }

        // too large for buffer -> allocate to heap
        ensureAllocation(m_length + 1);
        while (first != last){
            // we can use m_ptr here because we know we've maxxed out buffer space
            m_ptr[m_length++] = *first++;
            if (m_length == m_allocatedSize){
                // chooseAllocation will allocate 2x the requested amount
                ensureAllocation(m_length + 1);
            }
        }
        
        data()[m_length] = '\0';
    }

    // Construct for all other iterators (forward, random access, const char*, etc.)
    ITERATOR_TRAITS
    inline void m_construct(InputIter first, InputIter last, std::forward_iterator_tag t) {
        m_length = last - first;
        // Allocate more than needed for fast append
        m_allocatedSize = chooseAllocationSize(m_length + 1);

        if (inBuffer()){
            ::memcpy(m_buffer, &*first, last - first);
            m_buffer[m_length] = '\0';
        } else {
            m_ptr = m_allocator.allocate(m_allocatedSize);
            ::memcpy(m_ptr, &*first, last - first);
            m_ptr[m_length] = '\0';
        }
    }

    
    // Assign for input iterators, which do not implement operator-
    ITERATOR_TRAITS
    constexpr SIMDString& m_assign(InputIter first, InputIter last, std::input_iterator_tag t) {
        m_length = 0;

        // Allocate to buffer first if existing string is inConst
        if (inConst()){
            m_allocatedSize = INTERNAL_SIZE;
        }
        // don't need to allocate because either the string already has heap allocation or 
        // we're allocating to the buffer
        pointer dataPtr = data();
        while (first != last){
            dataPtr[m_length++] = *first++;
            if (m_length == m_allocatedSize){
                // chooseAllocation will allocate 2x the requested amount
                dataPtr = ensureAllocation(m_length + 1);
            }
        }
        
        dataPtr[m_length] = '\0';
        return *this;
    }

    // Assign for all other iterators (forward, random access, const char*, etc.)
    ITERATOR_TRAITS
    constexpr SIMDString& m_assign(InputIter first, InputIter last, std::forward_iterator_tag t){
        m_length = last - first;
        //allocate memory if necessary. 
        pointer dataPtr = maybeReallocate(m_length + 1);

        // Clone the other value, putting it in the internal storage if possible
        ::memcpy(dataPtr, &*first, last - first);
        dataPtr[m_length] = '\0';
        return *this;
    }

public:

    static constexpr size_type npos = size_type(-1);
    
    SIMDString(std::nullptr_t): m_length(0), m_allocator(INTERNAL_SIZE) {
        m_buffer[0] = '\0';
    }

    /** Creates a zero-length string */
    constexpr inline SIMDString(): m_length(0), m_allocator(INTERNAL_SIZE) {
        m_buffer[0] = '\0';
    }

    /** \param count Copy this many characters.  */
    constexpr SIMDString(size_type count, value_type c) : m_length(count) {
        // Allocate more than needed for fast append
        m_allocatedSize = chooseAllocationSize(m_length + 1);
        if (inBuffer()){ 
            ::memset((void*) m_buffer, c, m_length);
            m_buffer[m_length] = '\0';
        } else {
            m_ptr = m_allocator.allocate(m_allocatedSize);
            ::memset((void*) m_ptr, c, m_length);
            m_ptr[m_length] = '\0';
        }
    }

    explicit constexpr inline SIMDString(const value_type c): m_length(1), m_allocator(INTERNAL_SIZE) {
        m_buffer[0] = c;
        m_buffer[1] = '\0';
    }

    constexpr SIMDString(const SIMDString& str, size_type pos = 0) {
        m_length = str.m_length - pos;
        if (str.inConst()) {
            // Share this const_seg value
            m_ptr = str.m_ptr + pos;
            m_allocatedSize = 0;
        } else {
            m_allocatedSize = chooseAllocationSize(m_length + 1);

            // Clone the value, putting it in the internal storage if possible
            // memcpyBuffer assumes SSE so this needs to be aligned to SSO_ALIGNMENT 
            // Since INTERNAL_SIZE is a multiple of 2, the compiler will optimize `% SSO_ALIGNMENT` to `& (SSO_ALIGNMENT - 1)`
            if (inBuffer() && str.inBuffer() && !(pos % SSO_ALIGNMENT)) {
                memcpyBuffer(m_buffer, str.m_buffer + pos, INTERNAL_SIZE - pos);
            } else {
                pointer dataPtr = (pointer) alloc(m_allocatedSize);
                // + 1 is for the '\0'
                ::memcpy(dataPtr, str.data() + pos, m_length + 1);
            }
        }
    }

    constexpr SIMDString(const SIMDString& str, size_type pos, size_type count) {
        // cannot point to const string 
        m_length = (count == npos || pos + count >= str.size()) ? str.size() - pos : count;
        m_allocatedSize = chooseAllocationSize(m_length + 1);
        if (inBuffer() && str.inBuffer() && !(pos % SSO_ALIGNMENT)) {
            memcpyBuffer(m_buffer, str.m_buffer + pos, INTERNAL_SIZE - pos);
            m_buffer[m_length] = '\0';
        } else {
            pointer dataPtr = (pointer) alloc(m_allocatedSize);
            // + 1 is for the '\0'
            ::memcpy(dataPtr, str.data() + pos, m_length + 1);
            dataPtr[m_length] = '\0';
        }
    }

    constexpr SIMDString(const_pointer s) : m_length(::strlen(s)) {
        if (inConstSegment(s)) {
            m_ptr = const_cast<pointer>(s);
            m_allocatedSize = 0;
        } else {
            // Allocate more than needed for fast append
            m_allocatedSize = chooseAllocationSize(m_length + 1);
            pointer dataPtr = (pointer) alloc(m_allocatedSize);
            ::memcpy(dataPtr, s, m_length + 1);
        }
    }

    /** \param count Copy this many characters. The result is always copied because it is unsafe to
        check past the end of s for a null terminator.*/
    constexpr SIMDString(const_pointer s, size_type count) : m_length(count) {
        // Allocate more than needed for fast append
        m_allocatedSize = chooseAllocationSize(m_length + 1);
        
        pointer const dataPtr = (pointer) alloc(m_allocatedSize);
        ::memcpy(dataPtr, s, m_length);
        dataPtr[m_length] = '\0';
    }

    constexpr SIMDString(const_pointer s, size_type pos, size_type count)
        : m_length(count)
    {
        // Allocate more than needed for fast append
        m_allocatedSize = chooseAllocationSize(m_length + 1);
        
        pointer const dataPtr = (pointer) alloc(m_allocatedSize);
        ::memcpy(dataPtr, s + pos, m_length);
        dataPtr[m_length] = '\0';
    }

    constexpr SIMDString(SIMDString&& str) noexcept: m_length(0), m_allocator(INTERNAL_SIZE) {
        swap(str);
    }

    // These aren't passed by reference because this was the signature on basic_string
    // https://en.cppreference.com/w/cpp/string/basic_string/basic_string
    ITERATOR_TRAITS
    constexpr SIMDString(InputIter first, InputIter last)  {
        typedef typename std::iterator_traits<InputIter>::iterator_category tag; 
        m_construct(first, last, tag());
    }

    // explicit to prevent auto casting
    explicit constexpr SIMDString(const std::string& str, size_type pos = 0, size_type count = npos) {
        m_length = (count == npos || pos + count >= str.size()) ? str.size() - pos : count;
        // Allocate more than needed for fast append
        m_allocatedSize = chooseAllocationSize(m_length + 1);
        
        pointer const dataPtr = (pointer) alloc(m_allocatedSize);
        ::memcpy(dataPtr, str.data() + pos, m_length);
        dataPtr[m_length] = '\0';
    }

    constexpr SIMDString(std::initializer_list<value_type> ilist) : m_length(ilist.size()) {
        // The initializer list points to a list of const elements
        // They can't be moved and they're not null terminated
        m_allocatedSize = chooseAllocationSize(m_length + 1);
        pointer const dataPtr = (pointer) alloc(m_allocatedSize);
        ::memcpy(dataPtr, ilist.begin(), m_length);
        dataPtr[m_length] = '\0';
    }

    explicit constexpr SIMDString(const std::string_view& sv, size_type pos = 0) : m_length(sv.size() - pos) {
        if (inConstSegment(sv.data() + pos) && sv.data()[pos + m_length] == '\0') {
            m_ptr = const_cast<pointer>(sv.data() + pos);
            m_allocatedSize = 0;
        } else {
        // Allocate more than needed for fast append
            m_allocatedSize = chooseAllocationSize(m_length + 1);
            pointer const dataPtr = (pointer) alloc(m_allocatedSize);
            ::memcpy(dataPtr, sv.data() + pos, m_length);
            dataPtr[m_length] = '\0';
        }
    }

    constexpr SIMDString(const std::string_view& sv, size_type pos, size_type count) {
        m_length = (count == npos || pos + count >= sv.size()) ? sv.size() - pos : count;
        // Allocate more than needed for fast append
        m_allocatedSize = chooseAllocationSize(m_length + 1);
        pointer const dataPtr = (pointer) alloc(m_allocatedSize);
        ::memcpy(dataPtr, sv.data() + pos, m_length);
        dataPtr[m_length] = '\0';
    }

    ~SIMDString() {
        if (inHeap()) {
            // Note that this calls the method, not ::free 
            free(m_ptr, m_allocatedSize);
        }
    }

    constexpr SIMDString& operator=(const SIMDString& str) {
        if (&str == this) {
            return *this;
        } else if (str.inConst()) { // Constant storage
            maybeDeallocate();
            // Share this const_seg value
            m_ptr = str.m_ptr;
            m_length = str.m_length;
            m_allocatedSize = str.m_allocatedSize;
        } else { // Buffer and heap storage
            m_length = str.m_length;

            // free and/or allocate memory if necessary. 
            pointer dataPtr = maybeReallocate(m_length + 1);

            // Clone the other value, putting it in the internal storage if possible
            if (inBuffer() && str.inBuffer()) {
                memcpyBuffer(dataPtr, str.m_buffer);
            } else {
                ::memcpy(dataPtr, str.data(), m_length + 1);
            }
        }

        return *this;
    }

    constexpr SIMDString& operator=(SIMDString&& str) noexcept {
        swap(str);
        str.clear();
        return *this;
    }

    constexpr SIMDString& operator=(const_pointer s) {
        m_length = ::strlen(s);

        if (inConstSegment(s)) {
            maybeDeallocate();
            // Share this const_seg value
            m_ptr = const_cast<pointer>(s);
            m_allocatedSize = 0;
        } else {
            // free and/or allocate memory if necessary. 
            pointer const dataPtr = maybeReallocate(m_length + 1);
            // Clone the other value, putting it in the internal storage if possible
            ::memcpy(dataPtr, s, m_length + 1);
        }
        return *this;
    }

    constexpr SIMDString& operator=(const std::string& str) {
        m_length = str.length();
        // free and/or allocate memory if necessary. 
        pointer const dataPtr = maybeReallocate(m_length + 1);
        // Clone the other value, putting it in the internal storage if possible
        ::memcpy(dataPtr, str.data(), m_length + 1);

        return *this;
    }

    constexpr SIMDString& operator=(const std::string&& str)
    {
        m_length = str.length();
        // free and/or allocate memory if necessary. 
        pointer const dataPtr = maybeReallocate(m_length + 1);
        // Clone the other value, putting it in the internal storage if possible
        ::memcpy(dataPtr, str.data(), m_length + 1);

        return *this;
    }

    constexpr SIMDString& operator=(const value_type c) {
        maybeDeallocate();
        m_length = 1;
        m_allocatedSize = INTERNAL_SIZE;
        m_buffer[0] = c;
        m_buffer[1] = '\0';
        return *this;
    }

    constexpr SIMDString& operator=(std::initializer_list<value_type> ilist) {
        m_length = ilist.size();

        // free and/or allocate memory if necessary. 
        pointer const dataPtr = maybeReallocate(m_length + 1);
        // Clone the other value, putting it in the internal storage if possible
        ::memcpy(dataPtr, ilist.begin(), m_length);
        dataPtr[m_length] = '\0';
        return *this;
    }

    constexpr SIMDString& operator=(const std::string_view& sv) {
        m_length = sv.size();

        // free and/or allocate memory if necessary. 
        pointer const dataPtr = maybeReallocate(m_length + 1);
        // Clone the other value, putting it in the internal storage if possible
        ::memcpy(dataPtr, sv.data(), m_length);
        dataPtr[m_length] = '\0';
        return *this;
    }

    constexpr SIMDString& assign(const SIMDString& str, size_type pos = 0, size_type count = npos) {
        size_type copy_len = (count == npos || pos + count >= str.size()) ? str.size() - pos : count;
        if (pos == 0 && copy_len == str.size()) {
            return (*this) = str;
        }
        // Constant storage and can use the same null terminator
        if (str.inConst() && pos + copy_len == str.size()) {
            maybeDeallocate();
            // Share this const_seg value
            m_ptr = str.m_ptr + pos;
            m_length = copy_len;
            m_allocatedSize = str.m_allocatedSize;
        } else { // Buffer and heap storage or a substring
            m_length = copy_len;
            // free and/or allocate memory if necessary. 
            pointer const dataPtr = maybeReallocate(m_length + 1);

            // Clone the other value, putting it in the internal storage if possible
            // memcpyBuffer assumes SSE this needs be aligned to SSO_ALIGNMENT 
            // Since INTERNAL_SIZE is a multiple of 2, the compiler will optimize `% SSO_ALIGNMENT` to `& (SSO_ALIGNMENT - 1)`
            if (inBuffer() && str.inBuffer() && !(pos % SSO_ALIGNMENT)) {
                // can copy over entire buffer because the string gets null terminated anyway
                memcpyBuffer(dataPtr, str.m_buffer + pos, INTERNAL_SIZE - pos);
            } else {
                ::memcpy(dataPtr, str.data() + pos, m_length);
            }
            dataPtr[m_length] = '\0';
        }
        return *this;
    }

    constexpr SIMDString& assign(const_pointer s, size_type count) {
        m_length = count;
        // free and/or allocate memory if necessary. 
        pointer const dataPtr = maybeReallocate(m_length + 1);
        // Clone the other value, putting it in the internal storage if possible
        ::memcpy(dataPtr, s, m_length);
        dataPtr[m_length] = '\0';
        return *this;
    }

    constexpr SIMDString& assign(const_pointer s) {
        return (*this = s);
    }

    constexpr SIMDString& assign(size_type count, const value_type c) {
        m_length = count;
        // free and/or allocate memory if necessary. 
        pointer const dataPtr = maybeReallocate(m_length + 1);
        // Clone the other value, putting it in the internal storage if possible
        ::memset(dataPtr, c, m_length);
        dataPtr[m_length] = '\0';
        return *this;
    }

    constexpr SIMDString& assign(const SIMDString&& str) {
        return (*this) = str;
    }

    ITERATOR_TRAITS
    constexpr SIMDString& assign(InputIter first, InputIter last)
    {
        typedef typename std::iterator_traits<InputIter>::iterator_category tag;
        return m_assign(first, last, tag());
    }

    constexpr SIMDString& assign(std::initializer_list<value_type> ilist) {
        return (*this) = ilist;
    }

    constexpr SIMDString& assign(const std::string_view& sv) {
        return (*this) = sv;
    }

    constexpr SIMDString& assign(const std::string_view& sv, size_type pos, size_type count) {
        if (pos == 0 && count == sv.size()) {
            return (*this = sv);
        }
        m_length = (count == npos || pos + count >= sv.size()) ? sv.size() - pos : count;
            
        // free and/or allocate memory if necessary. 
        pointer const dataPtr = maybeReallocate(m_length + 1);
        // Clone the other value, putting it in the internal storage if possible
        ::memcpy(dataPtr, sv.data(), m_length);
        dataPtr[m_length] = '\0';
        return *this;
    }


    constexpr Allocator get_allocator() {
        return m_allocator;
    }

    // access
    constexpr const_pointer c_str() const noexcept{
        return data();
    }

    /**
     * data: https://github.com/llvm-mirror/libcxx/blob/78d6a7767ed57b50122a161b91f59f19c9bd0d19/include/string#L1242
     * __get_pointer: https://github.com/llvm-mirror/libcxx/blob/78d6a7767ed57b50122a161b91f59f19c9bd0d19/include/string#L1513
     */
    constexpr const_pointer data() const noexcept {
        return inBuffer() ?  m_buffer : m_ptr;
    }

    constexpr pointer data() noexcept {
        return inBuffer() ?  m_buffer : m_ptr;
    }

    constexpr const_reference operator[](size_type x) const {
        assert(x < m_length && x >= 0); // "Index out of bounds");
        return data()[x];
    }

    constexpr reference operator[](size_type x) {
        assert(x < m_length && x >= 0); // "Index out of bounds");
        prepareToMutate();
        return data()[x];
    }

    constexpr const_reference at(size_type x) const {
        assert(x < m_length&& x >= 0); // "Index out of bounds");
        return data()[x];
    }

    constexpr reference at(size_type x) {
        assert(x < m_length&& x >= 0); // "Index out of bounds");
        prepareToMutate();
        return data()[x];
    }

    constexpr const_reference front() const {
        const_pointer dataPtr = data(); 
        assert(dataPtr); // "Empty string"
        return dataPtr[0];
    }

    constexpr reference front() {
        assert(data()); // "Empty string"
        prepareToMutate();
        // call data again because it's a mutated string
        return data()[0];
    }

    constexpr const_reference back() const {
        const_pointer dataPtr = data(); 
        assert(dataPtr); // "Empty string"
        return dataPtr[m_length - 1];
    }

    constexpr reference back() {
        assert(data()); // "Empty string"
        prepareToMutate();
        // call data again because it's a mutated string
        return data()[m_length - 1];
    }

    explicit constexpr inline operator std::basic_string_view<value_type>() {
        return std::string_view(data(), m_length);
    }

    // iterators
    constexpr iterator begin() {
        prepareToMutate();
        return iterator(data());
    }

    constexpr iterator end() {
        prepareToMutate();
        return iterator(data() + m_length);
    }

    constexpr const_iterator begin() const {
        return const_iterator(data());
    }

    constexpr const_iterator end() const {
        return const_iterator(data() + m_length);
    }

    constexpr const_iterator cbegin() const {
        return const_iterator(data());
    }

    constexpr const_iterator cend() const {
        return const_iterator(data() + m_length);
    }

    // std::reverse_iterator in cpp2017 or older don't have constexpr constructors
    // thus these functions can't be constexpr for now
    reverse_iterator rbegin() {
        prepareToMutate();
        return reverse_iterator(data() + m_length);
    }

    reverse_iterator rend() {
        prepareToMutate();
        return reverse_iterator(data());
    }

    const_reverse_iterator rbegin() const {
        return const_reverse_iterator(data() + m_length);
    }

    const_reverse_iterator rend() const {
        return const_reverse_iterator(data());
    }

    const_reverse_iterator crbegin() const
    {
        return const_reverse_iterator(data() + m_length);
    }

    const_reverse_iterator crend() const {
        return const_reverse_iterator(data());
    }

    constexpr size_type size() const {
        return m_length;
    }

    constexpr size_type length() const {
        return m_length;
    }

    constexpr size_type capacity() const {
        if (inBuffer()) {
            return INTERNAL_SIZE;
        } else if (inConst()) {
            return m_length;
        } else {
            return m_allocatedSize; // 0 when isConst() is true
        }
    }

    constexpr size_type max_size() const {
        return size_type(-1);
    }

    constexpr bool empty() const {
        return (m_length == 0);
    }

    /* Almost the same as ensureAllocation, except this one 
     * sets allocation to the passed in length exactly while
     * ensureAllocation uses chooseAllocationSize
     */
    constexpr void reserve(size_type newLength = 0) {
        if (newLength > m_allocatedSize) {
            // Does not fit in buffer, reserve more space in Heap
            if (newLength > INTERNAL_SIZE) {
                // Need heap allocation
                const bool wasInHeap = inHeap();
                pointer old = data();
                size_t oldSize = m_allocatedSize;

                m_allocatedSize = newLength + 1;
                pointer newPtr = m_allocator.allocate(m_allocatedSize);
                ::memcpy(newPtr, old, m_length + 1);
                m_ptr = newPtr; 
                if (wasInHeap) free(old, oldSize);
            } else if (inConst()) {
                // copy to the internal buffer.
                ::memcpy(m_buffer, data(), m_length + 1);
                m_allocatedSize = INTERNAL_SIZE;
            } else {
                // Should already have been in the internal buffer and fitting
                assert(false); // "Should not reach this case if the new length is less than the internal buffer"
            }
        }
    }

    constexpr void shrink_to_fit() {
        // only shrink if heap allocation
        if (inHeap() && (m_allocatedSize != m_length + 1)) {
            pointer const old = data();
            const size_type oldSize = m_allocatedSize;
            m_allocatedSize = chooseAllocationSize(m_length + 1);
            if (inBuffer()){
                ::memcpy(m_buffer, old, m_length + 1);
            } else {
                m_ptr = m_allocator.allocate(m_allocatedSize); 
                ::memcpy(m_buffer, old, m_length + 1);
            }
            free(old, oldSize);
        }
    }

    constexpr SIMDString& insert(size_type pos, const SIMDString& str, size_type pos2, size_type count = npos) {
        if (pos == m_length) {
            return append(str, pos2, count);
        }
        return replace(pos, 0, str, pos2, count);
    }

    constexpr SIMDString& insert(size_type pos, const SIMDString& str) {
        if (pos == m_length) {
            return append(str);
        }
        return replace(pos, 0, str);
    }

    constexpr SIMDString& insert(size_type pos, const_pointer s) {
        if (pos == m_length) {
            return append(s);
        }
        return replace(pos, 0, s, ::strlen(s));
    }

    constexpr SIMDString& insert(size_type pos, const_pointer s, size_type count) {
        if (pos == m_length) {
            return append(s, count);
        }
        return replace(pos, 0, s, count);
    }

    constexpr SIMDString& insert(size_type pos, size_type count, value_type c) {
        if (pos == m_length) {
            return append(count, c);
        }
        return replace(pos, 0, count, c);
    }

    constexpr iterator insert(const_iterator pos, value_type c) {
        if (pos == end()) {
            append(1, c);
        } else {
            replace(pos - data(), 0, 1, c);
        }
        return iterator(data() + (pos - begin()));
    }

    constexpr iterator insert(const_iterator pos, size_type count, value_type c) {
        if (pos == end()) {
            return append(count, c);
        } else {
            replace(pos - data(), 0, count, c);
        }
        return iterator(data() + (pos - begin()));
    }

    
    ITERATOR_TRAITS
    constexpr iterator insert(const_iterator pos, InputIter first, InputIter last) {
        if (pos == end()) {
            append(first, last);
        } else {
            replace(pos, pos, first, last);
        }
        return iterator(data() + (pos - begin()));
    }

    constexpr iterator insert(const_iterator pos, std::initializer_list<value_type> ilist) {
        if (pos == end()) {
            return append(ilist.begin());
        } else {
            return replace(pos - data(), 0, ilist.begin(), ilist.size());
        }
        return pos;
    }

    constexpr SIMDString& insert(size_type pos, const std::string_view& sv) {
        if (pos == end()) {
            return append(sv.begin());
        }
        return replace(pos, 0, sv.begin(), sv.size());

    }

    constexpr SIMDString& insert(size_type pos, const std::string_view& sv, size_type pos2, size_type count) {
        if (pos == end()) {
            return append(sv.begin() + pos2, count);
        }
        return replace(pos, 0, sv.begin() + pos2, count);
    }

    constexpr void resize(size_type count, value_type c = '\0') {
        if (count < m_length) {
            m_length = count; 
            data()[m_length] = '\0';
        } else if (count > m_length) {
            append(count - m_length, c);
        }
    }

    constexpr size_type copy(pointer dest, size_type count, size_type pos = 0) const {
        size_type cpyCount = pos + count > m_length ? m_length - pos : count;

        // the resulting string of copy is not null terminated
        ::memcpy(dest, data() + pos, cpyCount);
        return cpyCount;
    }

    constexpr SIMDString& replace(size_type pos, size_type count, const SIMDString& str) {
        if (pos == m_length) {
            return append(str.data(), str.m_length);
        }
        return replace(pos, count, str.data(), str.m_length);
    }

    constexpr SIMDString& replace(const_iterator first, const_iterator last, const SIMDString& str) {
        if (last == end()) {
            return append(str.data(), str.m_length);
        }
        return replace(first - data(), last - first, str.data(), str.m_length);
    }

    constexpr SIMDString& replace(size_type pos, size_type count, const SIMDString& str, size_type pos2, size_type count2 = npos) {
        return replace(pos, count, str.data() + pos2, count2);
    }

    constexpr SIMDString& replace(size_type pos, size_type count, const_pointer s, size_type count2) {
        if (pos == m_length) {
            return append(s, count2);
        }

        // clamp count 
        if (pos + count > m_length) count = m_length - pos; 
        long sizeDiff = (long) (count2 - count);

        assert(pos <= m_length && max_size() >= m_length + sizeDiff); // "Index out of bounds");

        if (sizeDiff > 0) { // count < count2 -> insert
            size_type newSize = m_length + sizeDiff + 1;
            pointer const dataPtr = createGap(newSize, count, count2, pos); 
            ::memcpy(dataPtr + pos, s, count2);
            m_length += sizeDiff;
        } else if (sizeDiff < 0) { // count > count2 
            pointer const dataPtr = prepareToMutate();
            ::memcpy(dataPtr + pos, s, count2);
            memmove(dataPtr + pos + count2, dataPtr + pos + count, m_length - pos - count + 1);
            m_length += sizeDiff;
        } else {
            pointer const dataPtr = prepareToMutate();
            ::memcpy(dataPtr + pos, s, count2);
        }
        return (*this);
    }

    constexpr SIMDString &replace(const_iterator first, const_iterator last, const_pointer s, size_type count2) {
        if (last == end()) {
            return append(s, count2);
        }
        return replace(first - data(), last - first, s, count2);
    }

    ITERATOR_TRAITS
    constexpr SIMDString& replace(const_iterator first, const_iterator last, InputIter first2, InputIter last2) {
        const SIMDString secondString (first2, last2); 
        return replace(first - begin(), last - first, secondString.data(), secondString.size());
    }

    constexpr SIMDString& replace(size_type pos, size_type count, const_pointer s) {
        if (pos == m_length) {
            return append(s, ::strlen(s));
        }
        return replace(pos, count, s, ::strlen(s));
    }

    constexpr SIMDString& replace(const_iterator first, const_iterator last, const_pointer s) {
        if (last == end()) {
            return append(s, ::strlen(s));
        }
        return replace(first - data(), last - first, s, ::strlen(s));
    }

    constexpr SIMDString& replace(size_type pos, size_type count, size_type count2, value_type c) {
        if (pos == m_length) {
            return append(count2, c);
        }

        // clamp count 
        if (pos + count > m_length) count = m_length - pos; 
        long sizeDiff = (long) (count2 - count);

        assert(pos <= m_length && max_size() >= m_length + sizeDiff); // "Index out of bounds");

        // count < count2
        if (sizeDiff > 0) { 
            size_type newSize = m_length + sizeDiff + 1;
            pointer const dataPtr = createGap(newSize, count, count2, pos); 
            ::memset(dataPtr + pos, c, count2);
            m_length += sizeDiff;
        } else if (sizeDiff < 0) { // count > count2 
            pointer const dataPtr = prepareToMutate();
            ::memset(dataPtr + pos, c, count2);
            ::memmove(dataPtr + pos + count2, dataPtr + pos + count, m_length - pos - count + 1);
            m_length += sizeDiff;
        } else {
            pointer const dataPtr = prepareToMutate();
            ::memset(dataPtr + pos, c, count2);
        }
        return (*this);
    }

    constexpr SIMDString& replace(const_iterator first, const_iterator last, size_type count2, value_type c) {
        if (last == end()) {
            return append(count2, c);
        }
        return replace(first - data(), last - first, count2, c);
    }

    constexpr SIMDString& replace(const_iterator first, const_iterator last, std::initializer_list<value_type> ilist) {
        if (last == end()) {
            return append(ilist.begin(), ilist.size());
        }
        return replace(first - data(), first - last, ilist.begin(), ilist.size());
    }

    constexpr SIMDString& replace(const_iterator first, const_iterator last, const std::string_view& sv) {
        if (last == end()) {
            return append(sv.begin(), sv.size());
        }
        return replace(first - data(), first - last, sv.begin(), sv.size());
    }

    constexpr SIMDString& replace(size_type pos, size_type count, const std::string_view& sv) {
        if (pos == m_length) {
            return append(sv.begin(), sv.size());
        }
        return replace(pos, count, sv.data(), sv.size());
    }

    constexpr SIMDString& replace(size_type pos, size_type count, const std::string_view& sv, size_type pos2, size_type count2) {
        if (pos == m_length) {
            return append(sv.begin() + pos2, count2);
        }
        return replace(pos, count, sv.begin() + pos2, count2);
    }

    constexpr void clear() {
        if (inConst()) {
            // switch to inBuffer
            m_allocatedSize = INTERNAL_SIZE;
        }
        m_length = 0;
        *data() = '\0';
    }

    constexpr SIMDString& erase(size_type pos = 0, size_type count = npos) {
        if ((count == npos) || (pos + count > m_length)) {
            count = m_length - pos;
        }

        if ((pos == 0) && (count == m_length)) {
            // Optimize erasing the entire string
            clear();
        } else if (count > 0) {
            if (inConst()) {
                pointer const old = m_ptr;
                m_allocatedSize = chooseAllocationSize(m_length - count + 1);
                pointer const dataPtr = (pointer) alloc(m_allocatedSize);
                // copy over [old, old + pos)
                ::memcpy(dataPtr, old, pos);
                // copy over [old + pos + count, old + m_length] <- includes 0
                ::memcpy(dataPtr + pos, old + pos + count, m_length - (pos + count) + 1);
            } else {
                // move [old + pos + count, old + m_length] up by count
                pointer const dataPtr = data();
                memmove(dataPtr + pos, dataPtr + pos + count, m_length - (pos + count) + 1);
            }
            m_length -= count;
        }

        return *this;
    }

    constexpr iterator erase(iterator pos) {
        const_pointer dataPtr = data(); 
        size_type n = pos - dataPtr;
        erase(n, 1);
        // call data() again because state may have changed
        return iterator(dataPtr + n);
    }

    constexpr iterator erase(iterator first, iterator last) {
        const_pointer dataPtr = data(); 
        size_type n = first - dataPtr;
        size_type count = last - first;
        if (!n && count == m_length) {
            clear();
        } else {
            erase(n, count);
        }
        return iterator(dataPtr + n);
    }

    constexpr inline friend SIMDString operator+(const SIMDString& lhs, const SIMDString& rhs) {
        SIMDString result;
        result.m_length = lhs.m_length + rhs.m_length;
        result.m_allocatedSize = chooseAllocationSize(result.m_length + 1);
        
        if (result.inBuffer() && lhs.inBuffer()) {
            memcpyBuffer(result.m_buffer, lhs.m_buffer);
            ::memcpy(result.m_buffer + lhs.m_length, rhs.data(), rhs.m_length + 1);
        } else {
            pointer const resultData = (pointer) result.alloc(result.m_allocatedSize); 
            ::memcpy(resultData, lhs.data(), lhs.m_length);
            ::memcpy(resultData + lhs.m_length, rhs.data(), rhs.m_length + 1);
        }

        return std::move(result);
    }

    constexpr inline friend SIMDString operator+(const SIMDString& lhs, SIMDString&& rhs) {
        return std::move(rhs.insert(0, lhs));
    }

    constexpr inline friend SIMDString operator+(const SIMDString& lhs, const_pointer rhs) {
        const size_type L(::strlen(rhs));
        SIMDString result;
        result.m_length = lhs.m_length + L;
        result.m_allocatedSize = chooseAllocationSize(result.m_length + 1);

        // Copy this to output string
        if (result.inBuffer() && lhs.inBuffer()) {
            memcpyBuffer(result.m_buffer, lhs.m_buffer);
            ::memcpy(result.m_buffer, lhs.data(), lhs.m_length);
            ::memcpy(result.m_buffer + lhs.m_length, rhs, L + 1);
        } else {
            pointer const resultData = (pointer) result.alloc(result.m_allocatedSize);
            ::memcpy(resultData, lhs.data(), lhs.m_length);
            ::memcpy(resultData + lhs.m_length, rhs, L + 1);
        }

        // Copy s to output string
        return std::move(result);
    }

    constexpr inline friend SIMDString operator+(const SIMDString& lhs, const value_type rhs) {
        SIMDString result;
        result.m_length = lhs.m_length + 1;
        result.m_allocatedSize = chooseAllocationSize(result.m_length + 1);

        if (result.inBuffer() && lhs.inBuffer()) {
            memcpyBuffer(result.m_buffer, lhs.m_buffer);
            result.m_buffer[result.m_length - 1] = rhs;
            result.m_buffer[result.m_length] = '\0';
        } else {
            pointer const resultData = (pointer) result.alloc(result.m_allocatedSize);
            ::memcpy(resultData, lhs.data(), lhs.m_length);
            resultData[result.m_length - 1] = rhs;
            resultData[result.m_length] = '\0';
        }
        return std::move(result);
    }

    constexpr friend SIMDString operator+(const_pointer lhs, const SIMDString& rhs) {
        const size_type L(::strlen(lhs));
        SIMDString result;
        result.m_length = rhs.m_length + L;
        result.m_allocatedSize = chooseAllocationSize(result.m_length + 1);
        pointer const resultData = (pointer) result.alloc(result.m_allocatedSize);

        // Copy s to output string
        ::memcpy(resultData, lhs, L);
        ::memcpy(resultData + L, rhs.data(), rhs.m_length + 1);

        return std::move(result);
    }

    constexpr friend SIMDString operator+(const value_type lhs, const SIMDString& rhs) {
        SIMDString result(rhs.m_length + 1, lhs);
        ::memcpy(result.data() + 1, rhs.data(), rhs.m_length + 1);
        return std::move(result);
    }

    constexpr inline friend SIMDString operator+(const_pointer lhs, SIMDString&& rhs) {
        return std::move(rhs.insert(0, lhs));
    }

    constexpr inline friend SIMDString operator+(const value_type lhs, SIMDString&& rhs) {
        return std::move(rhs.insert(0, 1, lhs));
    }

    constexpr inline friend SIMDString operator+(SIMDString&& lhs, SIMDString&& rhs) {
        return std::move(SIMDString(lhs).append(rhs));
    }

    constexpr inline friend SIMDString operator+(SIMDString&& lhs, const SIMDString& rhs) {
        return std::move(lhs.append(rhs));
    }

    constexpr inline friend SIMDString operator+(SIMDString&& lhs, const_pointer rhs) {
        return std::move(lhs.append(rhs));
    }

    constexpr inline friend SIMDString operator+(SIMDString&& lhs, const value_type rhs) {
        return std::move(lhs.append(1, rhs));
    }

    constexpr SIMDString& operator+=(const SIMDString& str) {
        pointer dataPtr = ensureAllocation(m_length + str.m_length + 1);
        ::memcpy(dataPtr + m_length, str.data(), str.m_length + 1);
        m_length += str.m_length;
        return *this;
    }

    constexpr SIMDString& operator+=(const value_type c) {
        // +1 for c, +1 for null operator
        pointer const dataPtr = ensureAllocation(m_length + 2);
        dataPtr[m_length++] = c;
        dataPtr[m_length] = '\0';
        return *this;
    }

    constexpr SIMDString& operator+=(const_pointer s) {
        const size_type t = ::strlen(s);
        
        pointer dataPtr = ensureAllocation(m_length + t + 1); 
        ::memcpy(dataPtr + m_length, s, t + 1);
        m_length += t;
        return *this;
    }

    constexpr SIMDString& operator+=(std::initializer_list<value_type> ilist) {
        return this->append(ilist.begin(), ilist.size());
    }

    constexpr SIMDString& operator+=(const std::string_view& sv) {
        return this->append(sv.data(), sv.size());
    }

    constexpr void push_back(value_type c) {
        *this += c;
    }

    constexpr void pop_back() {
        data()[--m_length] = '\0';
    }

    constexpr SIMDString& append(const SIMDString& str, size_type pos, size_type count = npos) {
        size_type copy_len = (count == npos || pos + count >= str.size()) ? str.size() - pos : count;
        pointer const dataPtr = ensureAllocation(m_length + copy_len + 1);
        ::memcpy(dataPtr + m_length, str.data() + pos, copy_len);
        dataPtr[m_length += copy_len] = '\0';
        return *this;
    }

    constexpr SIMDString& append(const SIMDString& str) {
        return (*this += str);
    }

    constexpr SIMDString& append(size_type count, value_type c) {
        pointer const dataPtr = ensureAllocation(m_length + count + 1);
        ::memset(dataPtr + m_length, c, count);
        dataPtr[m_length += count] = '\0';
        return *this;
    }

    constexpr SIMDString& append(const_pointer s, size_type t) {
        pointer const dataPtr = ensureAllocation(m_length + t + 1);
        ::memcpy(dataPtr + m_length, s, t);
        dataPtr[m_length += t] = '\0';
        return *this;
    }

    constexpr SIMDString& append(const_pointer s) {
        return (*this) += s;
    }

    ITERATOR_TRAITS
    constexpr SIMDString& append(InputIter first, InputIter last) {
        const SIMDString secondString(first, last);
        return this->append(secondString);
    }

    constexpr SIMDString& append(std::initializer_list<value_type> ilist) {
        return this->append(ilist.begin(), ilist.size());
    }

    constexpr SIMDString& append(const std::string_view& sv) {
        return this->append(sv.begin(), sv.size());
    }

    constexpr SIMDString& append(const std::string_view& sv, size_type pos, size_type count) {
        return this->append(sv.data());
    }

    constexpr void swap(SIMDString& str) {
        std::swap<size_type>(m_allocatedSize, str.m_allocatedSize);
        std::swap<Allocator>(m_allocator, str.m_allocator);
        std::swap<size_type>(m_length, str.m_length);
        swapBuffer(m_buffer, str.m_buffer); 
    }

    constexpr bool starts_with(value_type c) const {
        return m_length > 0 && *data() == c;
    }

    constexpr bool starts_with(const pointer s) const {
        size_type n = ::strlen(s);
        return m_length >= n && memcmp(s, data(), n) == 0;
    }

    constexpr bool starts_with(std::string_view sv) const {
        return m_length >= sv.size() && memcmp(sv.data(), data(), sv.size()) == 0;
    }

    constexpr bool ends_with(value_type c) const {
        return m_length > 0 && *(data() + m_length - 1) == c;
    }

    constexpr bool ends_with(const_pointer s) const {
        size_type n = ::strlen(s);
        return m_length >= n && memcmp(s, data() + m_length - n, n) == 0;
    }

    constexpr bool ends_with(std::string_view sv) const {
        return m_length >= sv.size() && memcmp(sv.data(), data() + m_length - sv.size(), sv.size()) == 0;
    }

    constexpr SIMDString substr(size_type pos, size_type count = npos) const {
        assert(pos <= m_length); // "Index out of bounds");
        const size_type slen = std::min(m_length - pos, count);
        
        if (slen == 0) return SIMDString();
        else return SIMDString(data() + pos, slen);
    }

    constexpr bool contains(std::string_view sv) const {
        return find(sv.begin(), 0) != npos;
    }

    constexpr bool contains(value_type c) const {
        return find(c, 0) != npos;
    }

    constexpr bool contains(const_pointer s) const {
        return find(s, 0, ::strlen(s)) != npos;
    }

    constexpr size_type find(const SIMDString& str, size_type pos = 0) const {
        return find(str.data(), pos, str.m_length);
    }

    constexpr size_type find(const_pointer s, size_type pos = 0) const {
        return find(s, pos, ::strlen(s));
    }

    constexpr size_type find(const_pointer s, size_type pos, size_type count) const
    {
        if (pos + count > m_length) return npos; 

        if (count == 0) return pos;
        
        const_pointer const dataPtr = data();
        const_pointer pFound = static_cast<const_pointer>(memchr(dataPtr + pos, *s, m_length - pos));
        size_type i = static_cast<size_type>(pFound - data());

        while (pFound && (i + count) <= m_length) {
            if (memcmp(pFound, s, count) == 0) {
                return i;
            }
            pFound = static_cast<const_pointer>(memchr(pFound + 1, *s, m_length - i - 1));
            i = static_cast<size_type>(pFound - dataPtr);
        }
        return npos;
    }

    constexpr size_type find(value_type c, size_type pos = 0) const {
        if (pos >= m_length) return npos;

        const_pointer dataPtr = data(); 
        const_pointer pFound = (const_pointer) memchr(dataPtr + pos, c, m_length - pos);
        return pFound? static_cast<size_type>(pFound - dataPtr) : npos; 
    }

    constexpr size_type find(const std::string_view& sv, size_type pos = 0) const {
        return find(sv.begin(), pos, sv.size());
    }

    constexpr size_type rfind(const SIMDString& str, size_type pos = npos) const {
        return rfind(str.data(), pos, str.m_length);
    }

    constexpr size_type rfind(const_pointer s, size_type pos = npos) const {
        return rfind(s, pos, ::strlen(s));
    }

    constexpr size_type rfind(const_pointer s, size_type pos, size_type count) const {
        if (!m_length || count > m_length) return npos; 

        size_type n_1 = count - 1;
        size_type i = std::min(m_length - count, pos);
        const_pointer const dataPtr = data(); 
        const_pointer leftBound = dataPtr + n_1;
        value_type endVal = *(s + n_1);

        if (count == 0) {
            return i;
        }

        do {
            if (*(leftBound + i) == endVal && !memcmp(dataPtr + i, s, count)) {
                return i;
            }
        } while (i--);

        return npos;
    }

    constexpr size_type rfind(value_type c, size_type pos = npos) const {
        if (!m_length) return npos; 

        size_type start = pos >= m_length ? m_length - 1 : pos;
        const_pointer const dataPtr = data(); 
        do {
            if (dataPtr[start] == c) { return start; }
        } while (start--);
        return npos;
    }

    constexpr size_type rfind(const std::string_view& sv, size_type pos = npos) const {
        return rfind(sv.begin(), pos, sv.size());
    }

    constexpr size_type find_first_of(const_pointer s, size_type pos, size_type count) const {
        if (pos >= m_length) return npos;

        size_type i = pos;
        const_pointer const dataPtr = data(); 

        do {
            // search for current letter in the string of letters
            if (memchr(s, *(dataPtr + i), count)) {
                return i;
            }
        } while (++i < m_length);

        return npos;
    }

    constexpr size_type find_first_of(const SIMDString& str, size_type pos = 0) const {
        return find_first_of(str.data(), pos, str.m_length);
    }

    constexpr size_type find_first_of(const_pointer s, size_type pos = 0) const {
        return find_first_of(s, pos, ::strlen(s));
    }

    constexpr size_type find_first_of(value_type c, size_type pos = 0) const {
        return find(c, pos);
    }

    constexpr size_type find_first_of(const std::string_view& sv, size_type pos = 0) const {
        return find_first_of(sv.begin(), pos, sv.size());
    }

    constexpr size_type find_first_not_of(const_pointer s, size_type pos, size_type count) const {
        if (pos >= m_length)  return npos; 

        size_type i = pos;
        const_pointer const dataPtr = data();

        do {
            // search for current letter in the string of letters
            if (!memchr(s, *(dataPtr + i), count)) {
                return i;
            }
        } while (++i < m_length);

        return npos;
    }

    constexpr size_type find_first_not_of(const SIMDString& str, size_type pos = 0) const {
        return find_first_not_of(str.data(), pos, str.m_length);
    }

    constexpr size_type find_first_not_of(const_pointer s, size_type pos = 0) const {
        return find_first_not_of(s, pos, ::strlen(s));
    }

    constexpr size_type find_first_not_of(value_type c, size_type pos = 0) const {
        if (pos >= m_length)  return npos; 

        size_type i = pos;
        const_pointer const dataPtr = data(); 

        do {
            if (c != *(dataPtr + i)) {
                return i;
            }
        } while (++i < m_length); // do not want to run when i=m_length

        return npos;
    }

    constexpr size_type find_first_not_of(const std::string_view& sv, size_type pos = 0) const {
        return find_first_not_of(sv.begin(), pos, sv.size());
    }

    constexpr size_type find_last_of(const_pointer s, size_type pos, size_type count) const {
        if (!m_length || count > m_length) return npos; 
        // search [data(), data() + pos]
        size_type i = std::min(m_length - 1, pos);
        const_pointer const dataPtr = data();

        do {
            if (memchr(s, *(dataPtr + i), count)) {
                return i;
            }
        } while (i--);

        return npos;
    }

    constexpr size_type find_last_of(const SIMDString& str, size_type pos = npos) const {
        return find_last_of(str.data(), pos, str.m_length);
    }

    constexpr size_type find_last_of(const_pointer s, size_type pos = npos) const {
        return find_last_of(s, pos, ::strlen(s));
    }

    constexpr size_type find_last_of(value_type c, size_type pos = npos) const {
        return rfind(c, pos); 
    }

    constexpr size_type find_last_of(const std::string_view& sv, size_type pos = 0) const {
        return find_last_of(sv.begin(), pos, sv.size());
    }

    constexpr size_type find_last_not_of(const_pointer s, size_type pos, size_type count) const {
        if (!m_length || count > m_length) return npos; 
        // search [data(), data() + pos]
        size_type i = std::min(m_length - 1, pos);
        const_pointer const dataPtr = data(); 

        do {
            if (!memchr(s, *(dataPtr + i), count)) {
                return i;
            }
        } while (i--);

        return npos;
    }

    constexpr size_type find_last_not_of(const SIMDString& str, size_type pos = npos) const {
        return find_last_not_of(str.data(), pos, str.m_length);
    }

    constexpr size_type find_last_not_of(const_pointer s, size_type pos = npos) const {
        return find_last_not_of(s, pos, ::strlen(s));
    }

    constexpr size_type find_last_not_of(value_type c, size_type pos = npos) const {
        if (!m_length) return npos; 
        size_type i = std::min(m_length - 1, pos);
        const_pointer const dataPtr = data(); 

        do {
            if (c != *(dataPtr + i)) {
                return i;
            }
        } while (i--);

        return npos;
    }

    constexpr size_type find_last_not_of(const std::string_view& sv, size_type pos = 0) const {
        return find_last_not_of(sv.begin(), pos, sv.size());
    }

private:

    // Does not stop for internal null terminators.  Does not include the null
    // terminators.
    // See http://www.cplusplus.com/reference/string/string/compare/
    constexpr inline int m_compare(const_pointer a, size_type alen, const_pointer b, size_type blen) const noexcept {
        const size_type count = std::min(alen, blen);
        int res = memcmp(a, b, count);
        return res ? res : (int) (alen - blen);
    }

public:

    constexpr int compare(const SIMDString& str) const {
        const_pointer const dataPtr = data(); 
        if (dataPtr == str.data() && m_length == str.m_length) {
            return 0;
        } else {
            return m_compare(dataPtr, m_length, str.data(), str.m_length);
        }
    }

    constexpr int compare(size_type pos, size_type count, const SIMDString& str) const {
        return m_compare(data() + pos, std::min(m_length - pos, count), str.data(), str.m_length);
    }

    constexpr int compare(size_type pos, size_type count1, const SIMDString& str, size_type pos2, size_type count2) const {
        return m_compare(data() + pos, std::min(m_length - pos, count1), str.data() + pos2, std::min(str.m_length - pos2, count2));
    }

    constexpr int compare(const_pointer s) const {
        return m_compare(data(), m_length, s, ::strlen(s));
    }

    constexpr int compare(size_type pos, size_type count, const_pointer s) const {
        return m_compare(data() + pos, std::min(m_length - pos, count), s, ::strlen(s));
    }

    constexpr int compare(size_type pos, size_type count1, const_pointer s, size_type count2) const {
        return m_compare(data() + pos, std::min(m_length - pos, count1), s, count2);
    }

    constexpr int compare(const std::string_view& sv) const noexcept {
        return m_compare(data(), m_length, sv.data(), sv.size());
    }

    constexpr int compare(size_type pos, size_type count, const std::string_view& sv) const {
        return m_compare(data() + pos, count, sv.data(), sv.size());
    }

    constexpr int compare(size_type pos, size_type count1, const std::string_view& sv, size_type pos2, size_type count2) const {
        return m_compare(data() + pos, count1, sv.data() + pos2, count2);
    }

    friend constexpr inline bool operator==(const std::string_view& sv, const SIMDString& str) {
        return ((sv.length() == str.m_length) && (sv.data() == str.data())) || !str.compare(sv);
    }

    friend constexpr inline bool operator==(const_pointer s, const SIMDString& str) {
        return str == s;
    }

    constexpr inline bool operator==(const SIMDString& str) const {
        return ((m_length == str.m_length) && (data() == str.data())) || !m_compare(data(), m_length, str.data(), str.m_length);
    }

    constexpr inline bool operator==(const_pointer s) const {
        return ((m_length == ::strlen(s)) && (data() == s)) || !m_compare(data(), m_length, s, ::strlen(s));
    }

    constexpr inline bool equals(const SIMDString& str) const {
        return ((m_length == str.m_length) && (data() == str.data())) || !m_compare(data(), m_length, str.data(), str.m_length);
    }

    constexpr inline bool operator!=(const SIMDString& s) const {
        return !(*this == s);
    }

    constexpr inline bool operator!=(const_pointer s) const {
        return !(*this == s);
    }

    friend constexpr inline bool operator!=(const_pointer s, const SIMDString& str) {
        return str != s;
    }


    constexpr bool operator>(const SIMDString& s) const {
        return compare(s) > 0;
    }

    constexpr bool operator<(const SIMDString& s) const {
        return compare(s) < 0;
    }

    constexpr bool operator>=(const SIMDString& s) const {
        return compare(s) >= 0;
    }

    constexpr bool operator<=(const SIMDString& s) const {
        return compare(s) <= 0;
    }

    friend constexpr inline bool operator<(const_pointer s, const SIMDString& str) {
        return !(str.compare(s) <= 0);
    }

    friend constexpr inline bool operator<=(const_pointer s, const SIMDString& str) {
        return str.compare(s) > 0;
    }

    friend constexpr inline bool operator>(const_pointer s, const SIMDString& str) {
        return !(str.compare(s) >= 0);
    }

    friend constexpr inline bool operator>=(const_pointer s, const SIMDString& str) {
        return str.compare(s) < 0;
    }


}
#ifdef __APPLE__
__attribute__((__aligned__(SSO_ALIGNMENT)))
#endif
;

TEMPLATE
std::ostream& operator<<(std::ostream& os, const SIMDString<INTERNAL_SIZE, Allocator>& str) {
    std::ostream::sentry sen(os);
    if (sen) {
        try {
            const std::streamsize w = os.width();

            if (w > (std::streamsize) str.size()) {
                const bool left = ((os.flags() & std::ostream::adjustfield) == std::ostream::left);

                if (!left) {    
                    const typename SIMDString<INTERNAL_SIZE, Allocator>::value_type c = os.fill();
                    for (std::streamsize fillN = w - str.size(); fillN > 0; --fillN)
                    {
                        if (os.rdbuf()->sputc(c) == EOF) {
                            os.setstate(std::ostream::badbit);
                            break;
                        }
                    }
                }

                if (os.good() && (os.rdbuf()->sputn(str.data(), str.size()) != str.size())){
                    os.setstate(std::ostream::badbit);
                }
                
                if (left && os.good()){
                    const typename SIMDString<INTERNAL_SIZE, Allocator>::value_type c = os.fill();
                    for (std::streamsize fillN = w - str.size(); fillN > 0; --fillN)
                    {
                        if (os.rdbuf()->sputc(c) == EOF) {
                            os.setstate(std::ostream::badbit);
                            break;
                        }
                    }
                }
		    } else if (os.rdbuf()->sputn(str.data(), str.size()) != str.size()){
                os.setstate(std::ostream::badbit);
            }
	        os.width(0);
	    }
	    catch(...)
	    { 
            os.setstate(std::ostream::badbit); 
        }
	}
    return os;
}

TEMPLATE
std::istream& operator>>(std::istream& is, SIMDString<INTERNAL_SIZE, Allocator>& str) {
    typename SIMDString<INTERNAL_SIZE, Allocator>::size_type numExtracted = 0;
    std::istream::ios_base::iostate err = std::istream::ios_base::goodbit;
    std::istream::sentry sen(is);

    if (sen) {
        try
        {
            str.erase();
            const typename SIMDString<INTERNAL_SIZE, Allocator>::size_type n =
                is.width() > 0 ? static_cast<typename SIMDString<INTERNAL_SIZE, Allocator>::size_type>(is.width())
                    : str.max_size();
            typename SIMDString<INTERNAL_SIZE, Allocator>::value_type c = is.rdbuf()->sgetc();

            while (numExtracted < n && c != EOF && !std::isspace(c, is.getloc())) {
                str += c;
                ++numExtracted;
                c = is.rdbuf()->snextc();
            }

            if (numExtracted < n && c == EOF) {
                err |= std::istream::ios_base::eofbit;
            }
            is.width(0);
        }
        catch (...) {
            is.setstate(std::istream::ios_base::badbit);
            throw;
        }
    }

    if (!numExtracted) {
        err |= std::istream::ios_base::failbit;
    }
    if (err) {
        is.setstate(err);
    }

    return is;
}

TEMPLATE
std::istream& getline(
    std::istream& is, SIMDString<INTERNAL_SIZE, Allocator>& str, typename SIMDString<INTERNAL_SIZE, Allocator>::value_type delim = '\n') {
    typename SIMDString<INTERNAL_SIZE, Allocator>::size_type numExtracted = 0;
    std::istream::ios_base::iostate  err = std::istream::ios_base::goodbit;
    std::istream::sentry sen(is, true);

    if (sen) {
        try
        {
            str.erase();
            const typename SIMDString<INTERNAL_SIZE, Allocator>::size_type n = str.max_size();
            typename SIMDString<INTERNAL_SIZE, Allocator>::value_type c = is.rdbuf()->sgetc();

            while (numExtracted < n && c != EOF && c != delim) {
                str += c;
                ++numExtracted;
                c = is.rdbuf()->snextc();
            }

            if (c == EOF) {
                err |= std::istream::ios_base::eofbit;
            } else if (c == delim) {
                ++numExtracted;
                is.rdbuf()->sbumpc();
            } else {
                err |= std::istream::ios_base::eofbit;
            }
        }
        catch (...) {
            is.setstate(std::istream::ios_base::badbit);
            throw;
        }
    }

    if (!numExtracted) {
        err |= std::istream::ios_base::failbit;
    }
    if (err) {
        is.setstate(err);
    }

    return is;
}


TEMPLATE 
inline int stoi(
    const SIMDString<INTERNAL_SIZE, Allocator> &str, typename SIMDString<INTERNAL_SIZE, Allocator>::size_type* pos = nullptr, int base = 10) {
    typename SIMDString<INTERNAL_SIZE, Allocator>::pointer end;
    int answer = ::strtol(str.data(), &end, base);
    if ( end == str.data() ) {
        throw std::invalid_argument("invalid stof argument");
    }

    if (errno == ERANGE) {
        throw std::out_of_range("stof argument out of range");
    }

    if (pos) {
        *pos = end - str.data();
    }

    return answer;
}

TEMPLATE 
inline long stol(
    const SIMDString<INTERNAL_SIZE, Allocator> &str, typename SIMDString<INTERNAL_SIZE, Allocator>::size_type* pos = nullptr, int base = 10) {
    typename SIMDString<INTERNAL_SIZE, Allocator>::pointer end;
    long answer = ::strtol(str.data(), &end, base);
    if ( end == str.data() ) {
        throw std::invalid_argument("invalid stof argument");
    }

    if (errno == ERANGE) {
        throw std::out_of_range("stof argument out of range");
    }

    if (pos) {
        *pos = end - str.data();
    }

    return answer;
}

TEMPLATE 
inline long long stoll(
    const SIMDString<INTERNAL_SIZE, Allocator> &str, typename SIMDString<INTERNAL_SIZE, Allocator>::size_type* pos = nullptr, int base = 10) {
    typename SIMDString<INTERNAL_SIZE, Allocator>::pointer end;
    long long answer = ::strtoll(str.data(), &end, base);
    if ( end == str.data() ) {
        throw std::invalid_argument("invalid stof argument");
    }

    if (errno == ERANGE) {
        throw std::out_of_range("stof argument out of range");
    }

    if (pos) {
        *pos = end - str.data();
    }

    return answer;
}

TEMPLATE 
inline unsigned long stoul(
    const SIMDString<INTERNAL_SIZE, Allocator> &str, typename SIMDString<INTERNAL_SIZE, Allocator>::size_type* pos = nullptr, int base = 10) {
    typename SIMDString<INTERNAL_SIZE, Allocator>::pointer end;
    unsigned long answer = ::strtoul(str.data(), &end, base);
    if ( end == str.data() ) {
        throw std::invalid_argument("invalid stof argument");
    }

    if (errno == ERANGE) {
        throw std::out_of_range("stof argument out of range");
    }

    if (pos) {
        *pos = end - str.data();
    }

    return answer;
}

TEMPLATE 
inline unsigned long long stoull(
    const SIMDString<INTERNAL_SIZE, Allocator> &str, typename SIMDString<INTERNAL_SIZE, Allocator>::size_type* pos = nullptr, int base = 10) {
    typename SIMDString<INTERNAL_SIZE, Allocator>::pointer end;
    unsigned long long answer = ::strtoull(str.data(), &end, base);
    if ( end == str.data() ) {
        throw std::invalid_argument("invalid stof argument");
    }

    if (errno == ERANGE) {
        throw std::out_of_range("stof argument out of range");
    }

    if (pos) {
        *pos = end - str.data();
    }

    return answer;
}

TEMPLATE 
inline float stof(
    const SIMDString<INTERNAL_SIZE, Allocator> &str, typename SIMDString<INTERNAL_SIZE, Allocator>::size_type* pos = nullptr) {
    typename SIMDString<INTERNAL_SIZE, Allocator>::pointer end;
    float answer = ::strtof(str.data(), &end);
    
    if ( end == str.data() ) {
        throw std::invalid_argument("invalid stof argument");
    }

    if (errno == ERANGE) {
        throw std::out_of_range("stof argument out of range");
    }

    if (pos) {
        *pos = end - str.data();
    }

    return answer;
}

TEMPLATE 
inline double stod(
    const SIMDString<INTERNAL_SIZE, Allocator> &str, typename SIMDString<INTERNAL_SIZE, Allocator>::size_type* pos = nullptr) {
    typename SIMDString<INTERNAL_SIZE, Allocator>::pointer end;
    double answer = ::strtod(str.data(), &end);
    if ( end == str.data() ) {
        throw std::invalid_argument("invalid stof argument");
    }

    if (errno == ERANGE) {
        throw std::out_of_range("stof argument out of range");
    }

    if (pos) {
        *pos = end - str.data();
    }

    return answer;
}

TEMPLATE 
inline long double stold(
    const SIMDString<INTERNAL_SIZE, Allocator> &str, typename SIMDString<INTERNAL_SIZE, Allocator>::size_type* pos = nullptr) {
    typename SIMDString<INTERNAL_SIZE, Allocator>::pointer end;
    long double answer = ::strtold(str.data(), &end);
    if ( end == str.data() ) {
        throw std::invalid_argument("invalid stof argument");
    }

    if (errno == ERANGE) {
        throw std::out_of_range("stof argument out of range");
    }

    if (pos) {
        *pos = end - str.data();
    }

    return answer;
}

template<typename UIntType>
char * uint_to_buffer(char* bufEnd, UIntType value) {

    while (value >= 10)
	{
	  *(--bufEnd) = static_cast<char>('0' + (value % 10));
	  value /= 10;
	}

	*(--bufEnd) = static_cast<char>('0' + value);

    return bufEnd; 
}

#ifdef G3D_System_h
template<size_t INTERNAL_SIZE = 64, class Allocator = G3D::g3d_allocator<char>, typename IntType>
#else
template<size_t INTERNAL_SIZE = 64, class Allocator = ::std::allocator<char>, typename IntType>
#endif
SIMDString<INTERNAL_SIZE, Allocator>  int_to_string(IntType value) {
    const int n = std::numeric_limits<IntType>::digits10 + 3;
    char str[n + 1] = {'\0'};

    if (std::is_unsigned_v<IntType> || value >= 0) {
        return SIMDString<INTERNAL_SIZE, Allocator>(uint_to_buffer(str + n, value));
    } else {
        using UIntType = std::make_unsigned_t<IntType>;
        char* start  = uint_to_buffer(str + n, static_cast<UIntType>(0 - value));
        *(--start) = '-';

        return SIMDString<INTERNAL_SIZE, Allocator>(start);
    }
}

TEMPLATE 
SIMDString<INTERNAL_SIZE, Allocator> to_string(int value) {
    return int_to_string(value);
}

TEMPLATE 
SIMDString<INTERNAL_SIZE, Allocator> to_string(long value) {
    return int_to_string(value);
}

TEMPLATE 
SIMDString<INTERNAL_SIZE, Allocator> to_string(long long value) {
    return int_to_string(value);
}

TEMPLATE 
SIMDString<INTERNAL_SIZE, Allocator> to_string(unsigned int value) {
    return int_to_string(value);
}

TEMPLATE 
SIMDString<INTERNAL_SIZE, Allocator> to_string(unsigned long value) {
    return int_to_string(value);
}

TEMPLATE 
SIMDString<INTERNAL_SIZE, Allocator> to_string(unsigned long long value) {
    return int_to_string(value);
}

TEMPLATE 
SIMDString<INTERNAL_SIZE, Allocator> to_string(float value) {
    // max digits for exponent in base 10 + max digits for mantissa in base 10 + extra buffer for extraneous symbols [+-01.e+-]
    const int n = std::numeric_limits<float>::max_exponent10 + std::numeric_limits<float>::max_digits10 + 10;
    typename SIMDString<INTERNAL_SIZE, Allocator>::value_type str[n];
    snprintf(str, n, "%f", value);
    return SIMDString<INTERNAL_SIZE, Allocator>(str);
}

TEMPLATE 
SIMDString<INTERNAL_SIZE, Allocator> to_string(double value) {
    const int n = std::numeric_limits<double>::max_exponent10 + std::numeric_limits<double>::max_digits10 + 10;
    typename SIMDString<INTERNAL_SIZE, Allocator>::value_type str[n];
    snprintf(str, n, "%f", value); 
    return SIMDString<INTERNAL_SIZE, Allocator>(str);
}

TEMPLATE 
SIMDString<INTERNAL_SIZE, Allocator> to_string(long double value) {
    const int n = std::numeric_limits<long double>::max_exponent10 + std::numeric_limits<long double>::max_digits10 + 10;
    typename SIMDString<INTERNAL_SIZE, Allocator>::value_type str[n];
    snprintf(str, n, "%Lf", value);
    return SIMDString<INTERNAL_SIZE, Allocator>(str);
}

    
template <size_t _Size, class _Alloc1>
struct std::hash<SIMDString<_Size, _Alloc1>>
{ 
    size_t operator()(const SIMDString<_Size, _Alloc1>& str) const noexcept
    { 
        // a recommended way of hashing bytes that is compiler neutral 
        // and does not require implementing our own hash function
        // https://learn.microsoft.com/en-us/cpp/porting/fix-your-dependencies-on-library-internals
        return std::hash<std::string_view>{}(std::string_view(str.data()));
    }
};

TEMPLATE 
typename SIMDString<INTERNAL_SIZE, Allocator>::iterator begin(SIMDString<INTERNAL_SIZE, Allocator>& str) {
    return str.begin();
}

TEMPLATE
typename SIMDString<INTERNAL_SIZE, Allocator>::iterator end(SIMDString<INTERNAL_SIZE, Allocator>& str) {
    return str.end();
}

// undef arguments
#undef USE_SSE_MEMCPY
#undef TEMPLATE
#undef ITERATOR_TRAITS
#undef SSE_x64
#undef m_allocatedSize