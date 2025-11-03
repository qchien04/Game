#pragma once
#include <iostream>
#include <thread>
#include <atomic>
#include <vector>
#include <memory>
#include <chrono>

template<typename T>
class MemoryPool {
private:
    std::vector<std::unique_ptr<T[]>> blocks;
    std::atomic<T*> free_list;
    size_t block_size;
    std::atomic<size_t> allocated_count;
    std::atomic<size_t> total_allocated;
    
public:
    MemoryPool(size_t block_size = 2048) : block_size(block_size), allocated_count(0), total_allocated(0) {
        allocate_block();
    }
    
    void allocate_block() {
        auto block = std::make_unique<T[]>(block_size);
        T* block_ptr = block.get();
        
        // Link all objects in the block
        for (size_t i = 0; i < block_size - 1; ++i) {
            *reinterpret_cast<T**>(&block_ptr[i]) = &block_ptr[i + 1];
        }
        *reinterpret_cast<T**>(&block_ptr[block_size - 1]) = free_list.load();
        
        free_list.store(block_ptr);
        blocks.push_back(std::move(block));
        total_allocated.fetch_add(block_size);
    }
    
    T* acquire() {
        T* obj = free_list.load();
        while (obj && !free_list.compare_exchange_weak(obj, *reinterpret_cast<T**>(obj))) {
            // Retry with exponential backoff
            std::this_thread::yield();
        }
        
        if (!obj) {
            allocate_block();
            return acquire();
        }
        
        allocated_count.fetch_add(1);
        new(obj) T(); // Placement new
        return obj;
    }
    
    void release(T* obj) {
        if (!obj) return;
        
        obj->~T(); // Explicit destructor call
        T* old_head = free_list.load();
        do {
            *reinterpret_cast<T**>(obj) = old_head;
        } while (!free_list.compare_exchange_weak(old_head, obj));
        
        allocated_count.fetch_sub(1);
    }
    
    // Get pool statistics
    size_t get_allocated_count() const { return allocated_count.load(); }
    size_t get_total_capacity() const { return total_allocated.load(); }
};
