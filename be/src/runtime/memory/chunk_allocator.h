// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace doris {

struct Chunk;
class ChunkArena;
class MetricEntity;
class MemTracker;
class Status;

// Used to allocate memory with power-of-two length.
// This Allocator allocate memory from system and cache free chunks for
// later use.
//
// ChunkAllocator has one ChunkArena for each CPU core, it will try to allocate
// memory from current core arena firstly. In this way, there will be no lock contention
// between concurrently-running threads. If this fails, ChunkAllocator will try to allocate
// memory from other core's arena.
//
// Memory Reservation
// ChunkAllocator has a limit about how much free chunk bytes it can reserve, above which
// chunk will released to system memory. For the worst case, when the limits is 0, it will
// act as allocating directly from system.
//
// ChunkArena will keep a separate free list for each chunk size. In common case, chunk will
// be allocated from current core arena. In this case, there is no lock contention.
//
// Must call CpuInfo::init() and DorisMetrics::instance()->initialize() to achieve good performance
// before first object is created. And call init_instance() before use instance is called.
class ChunkAllocator {
public:
    static void init_instance(size_t reserve_limit);

#ifdef BE_TEST
    static ChunkAllocator* instance();
#else
    static ChunkAllocator* instance() { return _s_instance; }
#endif

    ChunkAllocator(size_t reserve_limit);

    // Allocate a Chunk with a power-of-two length "size".
    // Return true if success and allocated chunk is saved in "chunk".
    // Otherwise return false.
    Status allocate(size_t size, Chunk* chunk, MemTracker* tracker = nullptr,
                    bool check_limits = false);

    Status allocate_align(size_t size, Chunk* chunk, MemTracker* tracker = nullptr,
                          bool check_limits = false);

    // Free chunk allocated from this allocator
    void free(const Chunk& chunk, MemTracker* tracker = nullptr);

    // Transfer the memory ownership to the chunk allocator.
    // If the chunk allocator is full, then free to the system.
    // Note: make sure that the length of 'data' is equal to size,
    // otherwise the capacity of chunk allocator will be wrong.
    void free(uint8_t* data, size_t size, MemTracker* tracker = nullptr);

private:
    static ChunkAllocator* _s_instance;

    size_t _reserve_bytes_limit;
    // When the reserved chunk memory size is greater than the limit,
    // it is allowed to steal the chunks of other arenas.
    size_t _steal_arena_limit;
    std::atomic<int64_t> _reserved_bytes;
    // each core has a ChunkArena
    std::vector<std::unique_ptr<ChunkArena>> _arenas;

    std::shared_ptr<MetricEntity> _chunk_allocator_metric_entity;

    std::shared_ptr<MemTracker> _mem_tracker;
};

} // namespace doris
