#pragma once

#ifndef DESCMAN_H
#define DESCMAN_H

#include <unordered_map>
#include <mutex>
#include <stdexcept>
//#include <iostream>

template <typename DescriptorType>
struct DefaultDescriptorStringifier {
    std::string operator()(DescriptorType descriptor) {
        return std::to_string(descriptor);
    }
};

template <typename DescriptorType, typename Allocator, typename Deallocator, typename DescriptorStringifier = DefaultDescriptorStringifier<DescriptorType> >
class DescriptorManager {
private:
    static std::unordered_map<DescriptorType, size_t> refCount;  // Handle -> Reference Count
    static std::mutex mutex;
    static Allocator allocator;
    static Deallocator deallocator;
    static DescriptorStringifier descriptorStringifier;

public:
    template<typename... AllocArgs>
    static DescriptorType acquire(AllocArgs... allocArgs) {
        DescriptorType descriptor = allocator(allocArgs...);
        addRef(descriptor);
        return descriptor;
    }

    static void addRef(DescriptorType descriptor) {
        std::lock_guard<std::mutex> lock(mutex);
        refCount[descriptor]++;
        //std::cout << "[DEBUG] Handle " << descriptor << " now has " << refCount[descriptor] << " references.\n";
    }

    static void removeRef(DescriptorType descriptor) {
        std::lock_guard<std::mutex> lock(mutex);

        auto it = refCount.find(descriptor);
        if (it != refCount.end()) {
            it->second--;
            //std::cout << "[DEBUG] Handle " << descriptor << " now has " << it->second << " references.\n";

            if (it->second == 0) {
                refCount.erase(it);
                deallocator(descriptor);  // Free the resource
            }
        }
        else {
            throw std::runtime_error("[WARNING] Attempted to remove reference from an untracked descriptor: " + descriptorStringifier(descriptor));
        }
    }

    static int getRefCount(DescriptorType descriptor) {
        std::lock_guard<std::mutex> lock(mutex);
        return refCount.count(descriptor) ? refCount[descriptor] : 0;
    }
};

// Initialize static members
template <typename DescriptorType, typename Allocator, typename Deallocator, typename DescriptorStringifier>
std::unordered_map<DescriptorType, size_t> DescriptorManager<DescriptorType, Allocator, Deallocator, DescriptorStringifier>::refCount;
template <typename DescriptorType, typename Allocator, typename Deallocator, typename DescriptorStringifier>
std::mutex DescriptorManager<DescriptorType, Allocator, Deallocator, DescriptorStringifier>::mutex;
template <typename DescriptorType, typename Allocator, typename Deallocator, typename DescriptorStringifier>
Allocator DescriptorManager<DescriptorType, Allocator, Deallocator, DescriptorStringifier>::allocator;
template <typename DescriptorType, typename Allocator, typename Deallocator, typename DescriptorStringifier>
Deallocator DescriptorManager<DescriptorType, Allocator, Deallocator, DescriptorStringifier>::deallocator;
template <typename DescriptorType, typename Allocator, typename Deallocator, typename DescriptorStringifier>
DescriptorStringifier DescriptorManager<DescriptorType, Allocator, Deallocator, DescriptorStringifier>::descriptorStringifier;

#endif // DESCMAN_H
