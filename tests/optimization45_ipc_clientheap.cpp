// SPDX-License-Identifier: MIT
#include "clientheap-production.h"
void heapCheck(bool ok, const char *why) { if (!ok) throw std::runtime_error(why); }
template<typename T> class HeapBuffer : public LockableBuffer<T> {
  using Desc = std::conditional_t<std::is_same_v<T, IDirect3DVertexBuffer9>, D3DVERTEXBUFFER_DESC, D3DINDEXBUFFER_DESC>;
public:
  explicit HeapBuffer(const Desc &d) : LockableBuffer<T>(nullptr, nullptr, d) {}
  using LockableBuffer<T>::lock;
  using LockableBuffer<T>::unlock;
  uint32_t allocation() const { return this->m_bufferId; }
};
template<typename T> void bufferFailures() {
  using Desc = std::conditional_t<std::is_same_v<T, IDirect3DVertexBuffer9>, D3DVERTEXBUFFER_DESC, D3DINDEXBUFFER_DESC>;
  Desc d{}; d.Size = 64; d.Usage = D3DUSAGE_DYNAMIC; d.Pool = D3DPOOL_DEFAULT;
  HeapBuffer<T> b(d); void *p = reinterpret_cast<void *>(1);
  SharedHeap::failAllocate = true;
  heapCheck(FAILED(b.lock(4, 8, &p, 0)) && !p && b.allocation() == SharedHeap::kInvalidId, "first allocation failure");
  SharedHeap::failAllocate = false;
  heapCheck(SUCCEEDED(b.lock(4, 8, &p, 0)) && p, "initial heap lock"); b.unlock();
  const auto old = b.allocation(); const auto count = SharedHeap::buffers.size();
  SharedHeap::failAllocate = true;
  heapCheck(FAILED(b.lock(4, 8, &p, D3DLOCK_DISCARD)) && !p && b.allocation() == old && SharedHeap::buffers.size() == count, "failed discard lost old buffer");
  SharedHeap::failAllocate = false; SharedHeap::failPointer = true;
  heapCheck(FAILED(b.lock(4, 8, &p, 0)) && !p && b.allocation() == old, "null existing buffer pointer arithmetic");
  heapCheck(FAILED(b.lock(4, 8, &p, D3DLOCK_DISCARD)) && !p && b.allocation() == old && SharedHeap::buffers.size() == count, "new null buffer leaked or replaced old");
  SharedHeap::failPointer = false;
  heapCheck(SUCCEEDED(b.lock(4, 8, &p, 0)) && p && b.allocation() == old, "recovery lost old buffer"); b.unlock();
}
void surfaceFailures() {
  HeapSurface s; D3DLOCKED_RECT r{}; RECT rect{1, 1, 3, 3};
  SharedHeap::failAllocate = true;
  heapCheck(!s.lock(r, rect, 0) && !r.pBits && s.m_bufferId == SharedHeap::kInvalidId, "surface first allocation failure");
  SharedHeap::failAllocate = false;
  heapCheck(s.lock(r, rect, 0) && r.pBits, "surface initial lock");
  const auto old = s.m_bufferId; const auto count = SharedHeap::buffers.size(); const auto locks = s.m_lockInfoQueue.size();
  SharedHeap::failAllocate = true;
  heapCheck(!s.lock(r, rect, D3DLOCK_DISCARD) && !r.pBits && s.m_bufferId == old, "surface discard lost old buffer");
  SharedHeap::failAllocate = false; SharedHeap::failPointer = true;
  heapCheck(!s.lock(r, rect, 0) && !r.pBits, "surface null pointer arithmetic");
  heapCheck(!s.lock(r, rect, D3DLOCK_DISCARD) && !r.pBits && s.m_bufferId == old && SharedHeap::buffers.size() == count, "surface failed new allocation leaked");
  heapCheck(s.m_lockInfoQueue.size() == locks, "surface failed lock queued state");
  SharedHeap::failPointer = false;
  heapCheck(s.lock(r, rect, 0) && r.pBits && s.m_bufferId == old, "surface recovery failed");
  SharedHeap::deallocate(old);
}
int main() { try {
  bufferFailures<IDirect3DVertexBuffer9>(); bufferFailures<IDirect3DIndexBuffer9>(); surfaceFailures();
  heapCheck(SharedHeap::buffers.empty(), "heap failure cleanup leaked");
  std::puts("CLIENT_HEAP_PASS VB/IB/Surface first-allocation/discard/null-view/cleanup/recovery"); return 0;
} catch (const std::exception &e) { std::fprintf(stderr, "CLIENT_HEAP_FAIL %s\n", e.what()); return 1; } }
