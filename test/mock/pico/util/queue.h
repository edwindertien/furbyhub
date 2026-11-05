#pragma once
#include <stdint.h>
#include <deque>
#include <vector>
struct queue_t { unsigned elem; unsigned cap; std::deque<std::vector<uint8_t>>* q; };
inline void queue_init(queue_t* q, unsigned elem, unsigned cap) { q->elem = elem; q->cap = cap; q->q = new std::deque<std::vector<uint8_t>>(); }
inline bool queue_try_add(queue_t* q, const void* d) {
  if (q->q->size() >= q->cap) return false;
  q->q->emplace_back((const uint8_t*)d, (const uint8_t*)d + q->elem); return true;
}
inline bool queue_try_remove(queue_t* q, void* d) {
  if (q->q->empty()) return false;
  memcpy(d, q->q->front().data(), q->elem); q->q->pop_front(); return true;
}
