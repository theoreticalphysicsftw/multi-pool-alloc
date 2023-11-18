// MIT License
// 
// Copyright (c) 2023 Mihail Mladenov
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.


#ifndef MULTI_POOL_ALLOC_HPP_INCLUDED
#define MULTI_POOL_ALLOC_HPP_INCLUDED

#include <cstdint>
#include <cstdlib>
#include <cassert>

#include <new>

#include <concepts>
#include <type_traits>

#include <iterator>

#include <vector>

#include <thread>

#if defined(_MSC_VER)
    #include <intrin.h>
    #pragma intrinsic(_BitScanForward)
    #pragma intrinsic(_BitScanForward64)
#endif


namespace mpa
{
    using u8_t = uint8_t;
    using u32_t = uint32_t;
    using u64_t = uint64_t;
    using bool_t = bool;
    using void_t = void;

    template <typename T>
    using vector_t = std::vector<T>;

    template <typename Mutex>
    using lock_guard_t = std::lock_guard<Mutex>;

    using mutex_t = std::mutex;

    template <typename T>
    concept sub32bit = sizeof(T) < sizeof(u32_t);

    template <typename T>
    concept pointer_type = std::is_pointer<T>::value;
}


namespace mpa::impl
{

    // These ctz functions should not be used with n = 0!
    template <typename T>
    inline auto ctz(T n) -> u32_t = delete;

    template <>
    inline auto ctz<u64_t>(u64_t)->u32_t;

    template <>
    inline auto ctz<u32_t>(u32_t)->u32_t;

    template <typename T>
        requires std::unsigned_integral<T>&& sub32bit<T>
    inline auto ctz(T n) -> u32_t
    {
        return ctz<u32_t>(u32_t(n));
    }

    template <typename WordType>
        requires std::unsigned_integral<WordType>
    struct block_t
    {
        void* ptr;
        WordType unmaxed_pools;
    };


    template <typename T, typename U>
        requires std::unsigned_integral<T>&& std::unsigned_integral<U>
    auto set_bit(T& n, U bit) -> void_t
    {
        n |= T(1) << bit;
    }


    template <typename T, typename U>
        requires std::unsigned_integral<T>&& std::unsigned_integral<U>
    auto clear_bit(T& n, U bit) -> void_t
    {
        n &= ~(T(1) << bit);
    }


    template <typename T, typename U>
        requires std::unsigned_integral<T>&& std::unsigned_integral<U>
    auto test_bit(T n, U bit) -> bool_t
    {
        return n & (T(1) << bit);
    }



#if defined(_MSC_VER)
    template <>
    inline auto ctz<u64_t>(u64_t n) -> u32_t
    {
        unsigned long result;
        _BitScanForward64(&result, n);
        return result;
    }

    template <>
    inline auto ctz<u32_t>(u32_t n) -> u32_t
    {
        unsigned long result;
        _BitScanForward(&result, n);
        return result;
    }
#elif defined(__GNUC__)
    template <>
    inline auto ctz<u64_t>(u64_t n) -> u32_t
    {
        return __builtin_ctzl(n);
    }

    template <>
    inline auto ctz<u32_t>(u32_t n) -> u32_t
    {
        return __builtin_ctz(n);
    }
#else
    template <>
    inline auto ctz<u64_t>(u64_t n) -> u32_t
    {
        // This function is not supposed to be used on 0!
        u32_t bits = 0;
        while (!((u64_t(1) << bits) & n)) bits++;
        return bits;
    }

    template <>
    inline auto ctz<u32_t>(u32_t n) -> u32_t
    {
        // This function is not supposed to be used on 0!
        u32_t bits = 0;
        while (!((u32_t(1) << bits) & n)) bits++;
        return bits;
    }
#endif

}


namespace mpa
{
    template <typename T, typename WordType = u64_t>
        requires std::unsigned_integral<WordType>
    struct pool_t
    {
        using word_type = WordType;
        word_type unused_words;
        static constexpr u32_t word_bits = sizeof(WordType) * 8;
        static constexpr u32_t pool_size = word_bits * word_bits;
        word_type unallocated_slots[word_bits];
        T data[pool_size];

        auto init() -> void_t;
        auto allocate() -> T*;
        auto deallocate(T* ptr) -> void_t;
        auto full() -> bool_t;
    };

    template <typename T>
    class multi_pool_t
    {
    public:
        multi_pool_t();
        ~multi_pool_t();
        auto allocate(size_t n) -> T*;
        auto deallocate(T* ptr, size_t n) -> void_t;

    private:
        static constexpr uint32_t pools_in_block = pool_t<T>::word_bits;
        using word_type = pool_t<T>::word_type;

        auto new_block() -> void_t;
        vector_t<impl::block_t<word_type>> memory_blocks;
    };


    template <typename T>
    class allocator_t
    {
    public:
        using value_type = T;
        using size_type = size_t;
        using difference_type = ptrdiff_t;
        using propagate_on_container_move_assignment = std::false_type;
        using propagate_on_container_copy_assignment = std::false_type;

        template <typename U>
        struct rebind
        {
            using other = allocator_t<U>;
        };

        allocator_t() noexcept;

        template <typename U>
        allocator_t(allocator_t<U>& other) noexcept
        {
            if (!multi_pool)
            {
                multi_pool = new multi_pool_t<T>;
            }
        }

        [[nodiscard]]
        auto allocate(size_t n) -> T*;
        auto deallocate(T* ptr, size_t n) noexcept -> void_t;

    private:
        inline static multi_pool_t<T>* multi_pool = nullptr;
        inline static mutex_t mutex;
    };


    template <typename T, typename WordType>
        requires std::unsigned_integral<WordType>
    auto pool_t<T, WordType>::init() -> void_t
    {
        unused_words = ~WordType(0);

        for (auto i = 0u; i < word_bits; ++i)
        {
            unallocated_slots[i] = ~WordType(0);
        }
    }


    template <typename T, typename WordType>
        requires std::unsigned_integral<WordType>
    auto pool_t<T, WordType>::allocate() -> T*
    {
        auto bucket = impl::ctz(unused_words);
        auto slot = impl::ctz(unallocated_slots[bucket]);

        impl::clear_bit(unallocated_slots[bucket], slot);
        if (!unallocated_slots[bucket])
        {
            impl::clear_bit(unused_words, bucket);
        }

        return &data[bucket * word_bits + slot];
    }


    template <typename T, typename WordType>
        requires std::unsigned_integral<WordType>
    auto pool_t<T, WordType>::deallocate(T* ptr) -> void_t
    {
        auto index = uint32_t(ptr - data);
        auto bucket = index / word_bits;
        auto slot = index % word_bits;

        impl::set_bit(unallocated_slots[bucket], slot);
        if (unallocated_slots[bucket] == ~WordType(0))
        {
            impl::set_bit(unused_words, bucket);
        }
    }


    template <typename T, typename WordType>
        requires std::unsigned_integral<WordType>
    auto pool_t<T, WordType>::full() -> bool_t
    {
        return !unused_words;
    }

    template <typename T>
    multi_pool_t<T>::multi_pool_t()
    {
        new_block();
    }

    template <typename T>
    multi_pool_t<T>::~multi_pool_t()
    {
        for (auto memory_block : memory_blocks)
        {
            free(memory_block.ptr);
        }
    }

    template <typename T>
    auto multi_pool_t<T>::new_block() -> void_t
    {

        impl::block_t<word_type> memory_block = { malloc(sizeof(pool_t<T>) * pools_in_block), ~word_type(0) };
        memory_blocks.push_back(memory_block);
        auto pools = (pool_t<T>*) memory_block.ptr;

        for (auto i = 0; i < pools_in_block; ++i)
        {
            pools[i].init();
        }
    }

    template <typename T>
    auto multi_pool_t<T>::allocate(size_t n) -> T*
    {
        assert(n == 1);

        for (auto i = memory_blocks.size() - 1; i < memory_blocks.size(); --i)
        {
            auto& memory_block = memory_blocks[i];
            if (memory_block.unmaxed_pools)
            {
                auto pool_idx = impl::ctz(memory_block.unmaxed_pools);
                auto pool = (pool_t<T>*) memory_block.ptr + pool_idx;
                auto result = pool->allocate();
                if (pool->full())
                {
                    impl::clear_bit(memory_block.unmaxed_pools, pool_idx);
                }
                return result;
            }
        }

        new_block();
        return ((pool_t<T>*)memory_blocks.back().ptr)->allocate();

    }

    template <typename T>
    auto multi_pool_t<T>::deallocate(T* ptr, size_t n) -> void_t
    {
        for (auto i = memory_blocks.size() - 1; i < memory_blocks.size(); --i)
        {
            auto& memory_block = memory_blocks[i];
            auto pool_size = sizeof(pool_t<T>);
            size_t pool_idx = ((u8_t*)ptr - (u8_t*)memory_block.ptr) / pool_size;

            if (pool_idx < pools_in_block)
            {
                assert((u8_t*)ptr - (u8_t*)((pool_t<T>*) memory_block.ptr + pool_idx) < pool_size);
                auto pool = (pool_t<T>*) memory_block.ptr + pool_idx;
                impl::set_bit(memory_block.unmaxed_pools, pool_idx);
                pool->deallocate(ptr);
                break;
            }
        }
    }

    template <typename T>
    allocator_t<T>::allocator_t() noexcept
    {
        if (!multi_pool)
        {
            multi_pool = new multi_pool_t<T>;
        }
    }

    template <typename T>
    [[nodiscard]]
    auto allocator_t<T>::allocate(size_t n) -> T*
    {
        using namespace impl;
        lock_guard_t<mutex_t> lg(mutex);
        return multi_pool->allocate(n);
    }

    template <typename T>
    auto allocator_t<T>::deallocate(T* ptr, size_t n) noexcept -> void_t
    {
        using namespace impl;
        lock_guard_t<mutex_t> lg(mutex);
        multi_pool->deallocate(ptr, n);
    }
}


#endif
