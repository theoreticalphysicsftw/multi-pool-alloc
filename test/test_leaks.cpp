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


#include <chrono>
#include <iostream>
#include <random>
#include <cstdint>
#include <map>

#include "../multi_pool_alloc.hpp"


int main()
{
    static constexpr uint64_t cycles = 1 << 16;
    static constexpr uint64_t total_allocated_resources = uint64_t(1) << 16;
    static constexpr uint64_t max_index = ~uint64_t(0);
    std::random_device rd;
    std::mt19937 mt(rd());
    std::uniform_int_distribution<uint64_t> dist(0, max_index);

    std::map<uint64_t, uint64_t, std::less<uint64_t>, mpa::allocator_t<std::pair<const uint64_t, uint64_t>>> map;

    for (auto i = 0; i < cycles; ++i)
    {
        for (auto j = 0; j < total_allocated_resources; ++j)
        {
            map.emplace(dist(mt), dist(mt));
        }
        map.clear();
    }
}
