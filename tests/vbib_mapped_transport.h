// SPDX-License-Identifier: MIT
// Copyright (c) 2026 yeyunyyds.
#ifdef REMIX_BRIDGE_CLIENT
#include "mapped_buffer_template.h"
#endif
inline void mappedBufferTransfer() {
#ifdef REMIX_BRIDGE_CLIENT
  for (unsigned type = 0; type < 2; ++type) {
    for (unsigned iteration = 0; iteration < 16; ++iteration) {
      auto test = [&](auto& buffer) {
        void* pointer = nullptr;
        require(buffer.lock(0, 0, &pointer, 0) == S_OK, "mapped real IPC first Lock");
        std::memset(pointer, static_cast<uint8_t>(iteration), 262144);
        require(buffer.unlock() == S_OK, "mapped real IPC full upload");
        require(!l4d2_buffer::MappedBufferShadow::stats().mappedCount, "mapped IPC source retained");
        require(buffer.lock(0, 64, &pointer, 0) == S_OK, "mapped real IPC partial Lock");
        static_cast<uint8_t*>(pointer)[12] = 0xa5;
        require(buffer.unlock() == S_OK, "mapped real IPC partial upload");
        void* first = nullptr; void* second = nullptr;
        require(buffer.lock(8, 16, &first, 0) == S_OK && buffer.lock(12, 8, &second, 0) == S_OK,
          "mapped real IPC overlap");
        require(static_cast<uint8_t*>(first) + 4 == second, "mapped real IPC alias");
        static_cast<uint8_t*>(second)[0] = 0xbb;
        require(buffer.unlock() == S_OK && static_cast<uint8_t*>(second)[0] == 0xbb, "mapped real IPC pin");
        require(buffer.unlock() == S_OK, "mapped real IPC final Unlock");
        require(buffer.lock(0, 0, &pointer, D3DLOCK_READONLY) == S_OK, "mapped real IPC READONLY");
        for (unsigned j = 0; j < 262144; ++j) {
          require(static_cast<uint8_t*>(pointer)[j] == (j == 12 ? 0xbb : static_cast<uint8_t>(iteration)),
            "mapped real IPC restored bytes");
        }
        require(buffer.unlock() == S_OK, "mapped real IPC READONLY release");
      };
      if (!type) {
        D3DVERTEXBUFFER_DESC desc {}; desc.Size = 262144; desc.Format = D3DFMT_VERTEXDATA;
        MappedTransportBuffer<IDirect3DVertexBuffer9> buffer(desc); test(buffer);
      } else {
        D3DINDEXBUFFER_DESC desc {}; desc.Size = 262144; desc.Format = D3DFMT_INDEX32;
        MappedTransportBuffer<IDirect3DIndexBuffer9> buffer(desc); test(buffer);
      }
    }
  }
#else
  std::vector<uint8_t> host(262144);
  unsigned uid = 0;
  for (unsigned type = 0; type < 2; ++type) {
    for (unsigned iteration = 0; iteration < 16; ++iteration) {
      for (unsigned stage = 0; stage < 4; ++stage) {
        if (!stage) { Sleep(20); }
        require(Device::waitForCommand(Commands::Bridge_Any, 2000) == Result::Success, "mapped Host wait");
        const auto h = Device::pop_front();
        require(h.command == (type ? Commands::IDirect3DIndexBuffer9_Unlock : Commands::IDirect3DVertexBuffer9_Unlock), "mapped Host opcode");
        require(Device::get_data() == uid++, "mapped Host UID order");
        const auto packet = Device::get_packet<3>(true);
        const unsigned offset = stage < 2 ? 0 : stage == 2 ? 8 : 12;
        const unsigned size = !stage ? 262144 : stage == 1 ? 64 : stage == 2 ? 16 : 8;
        require(packet.fields[0] == offset && packet.fields[1] == size && !packet.fields[2]
          && packet.bytes == size, "mapped Host unchanged wire layout");
        std::memcpy(host.data() + offset, packet.data, size);
        for (unsigned j = 0; j < 262144; ++j) {
          const uint8_t expected = j == 12 && stage ? (stage == 1 ? 0xa5 : 0xbb) : static_cast<uint8_t>(iteration);
          require(host[j] == expected, "mapped Host historical content after Client unmap");
        }
        Device::end_read_data();
      }
    }
  }
#endif
  handshake<Module>();
  std::puts("VBIB_MAPPED_PRODUCTION_IPC_PASS");
}
