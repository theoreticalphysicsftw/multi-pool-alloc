# multi-pool-alloc

A simple multi-pool allocator, for allocating many different small resources of the same size.

## Building

Simply include `multi_pool_alloc.hpp` and that's it!

## Usage
     // mpa::allocator_t<> is used just like an allocator conforming to std::allocator_traits but it cannot allocate contiguous
     // memory regions. It's stateless allocator and allocations can be deallocated from any instance of this class for the
     // particular type.
     std::map<uint64_t, uint64_t, std::less<uint64_t>, mpa::allocator_t<std::pair<const uint64_t, uint64_t>>> map;

     // mpa::multi_pool_alloc_t<> is the underlying stateful implementation of mpa::allocator_t.
     mpa::multi_pool_alloct<uint64_t> alloc;
     uint64_t* u_ptr = alloc.allocate();
     alloc.deallocate(u_ptr);
     