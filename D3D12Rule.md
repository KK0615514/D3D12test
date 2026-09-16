
# D3D12 Advanced VRAM & Streaming Rules

  

- **Anti-Fragmentation Architecture**: When implementing dynamic VRAM allocators, focus on strategies that minimize memory fragmentation. Always prefer `ID3D12Device::CreatePlacedResource` over frequent `CreateCommittedResource` for sub-allocating from large memory heaps (`ID3D12Heap`).

- **Asynchronous Data Upload**: Dynamic VRAM management for games requires background loading. All disk-to-VRAM transfers must utilize non-blocking architectures:

- Use a dedicated Copy Command Queue separate from the Direct (Render) Queue.

- Explain the synchronization via Fences between the Copy Queue and Render Queue.

- **VRAM Budget & Eviction**: Address hardware constraints. Code and architecture patterns should account for the VRAM budget (using `IDXGIAdapter3::QueryVideoMemoryInfo`) and handle memory eviction (`ID3D12Device::Evict`) safely when approaching the budget limit.

- **Industry Standard Alignment**: When proposing complex VRAM management solutions, cross-reference or align the logic with industry standards, specifically the **D3D12 Memory Allocator (D3D12MA)** library by AMD/Microsoft, explaining how to achieve similar robust pooling.
