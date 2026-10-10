#pragma once

#include <cstddef>
#include <cstdint>

constexpr std::uint32_t MALLOC_CAP_DMA = 1;
constexpr std::uint32_t MALLOC_CAP_INTERNAL = 2;

void *heap_caps_malloc(std::size_t bytes, std::uint32_t caps);
void heap_caps_free(void *data);
