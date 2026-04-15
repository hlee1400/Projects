# Overview

  This repository showcases systems-focused and performance-oriented work across **C++ engine architecture, GPU programming, and applied deep learning**.

  The projects emphasize:
  - **Low-level system design**
  - **Parallel computation**
  - **Scalable data pipelines**
  - **Real-time performance and efficient resource management**

  ---

  ## Projects

  ### [GPU Animation Engine](./CPP)
  **C++ · DirectX 11 · HLSL**

  A real-time 3D animation engine that offloads skeletal animation and hierarchy computation entirely to the GPU using compute shaders.

  **Key Highlights:**
  - GPU-based skeletal animation (no CPU bottleneck)
  - Multi-stage compute shader pipeline (blend → hierarchy → world matrices)
  - Explicit GPU resource management (SRV / UAV / CBV)
  - Engine architecture using manager + state patterns

  ---

  ### [Optimized C++ -- Performance Engineering](./CPP)
  **C++ · SSE Intrinsics · MSVC**

  A collection of performance engineering assignments demonstrating low-level memory management, cache optimization, SIMD vectorization, custom heap allocators, and expression templates -- with measured speedups across all implementations.

  **Key Highlights:**
  - Custom heap allocator with next-fit allocation, block subdivision, and bidirectional coalescing via secret pointers and bit-packed headers
  - Cache-optimized hot/cold data separation achieving 40x speedup over naive linked list traversal
  - SIMD intrinsics (SSE) for 4x4 matrix multiplication, vector-matrix products, and LERP
  - Expression template proxy objects eliminating intermediate temporaries for 2.3x speedup
  - Hybrid merge/insertion sort on linked lists (381x faster than pure insertion sort)
  - Implicit conversion prevention via private template constructor poisoning

  ---

  ### [Image Captioning System](./AI_ML)
  **Python · TensorFlow**

  A multimodal deep learning pipeline combining CNN encoders and attention-based sequence decoders to generate image captions.

  **Key Highlights:**
  - CNN + RNN (LSTM) with attention mechanism
  - End-to-end training and evaluation pipeline
  - Techniques for improving generalization and stability
  - Handling vanishing gradients and overfitting

  ---

  ### [Real-Time 3D Rendering Engine](./Graphics)
  **C++ · DirectX 11 · HLSL**

  A custom real-time 3D rendering engine built from scratch, demonstrating low-level GPU
  pipeline programming, shader architecture, advanced lighting models, and procedural terrain
  generation.

  **Key Highlights:**
  - Full D3D11 pipeline initialization (device, swap chain, rasterizer state, depth-stencil)
  - Shader compilation and management with an object-oriented hierarchy
  - Phong lighting system with directional, point, and spot lights, fog, and materials
  - Heightmap-based terrain generation with per-vertex smooth normal averaging
  - Custom perspective camera with OpenGL-to-DirectX NDC conversion
  - GPU resource lifecycle management (constant buffers, vertex/index buffers, COM cleanup)

  ---

  ## Core Competencies Demonstrated

  - **Systems Engineering**
    Modular architecture, memory/resource management, pipeline orchestration

  - **High-Performance Computing**
    GPU compute shaders, multistage dispatch, data-parallel workloads, SIMD vectorization

  - **Graphics & Rendering**
    Shader programming, 3D lighting models, GPU pipeline configuration, procedural geometry

  - **Performance Engineering**
    Cache optimization, custom allocators, expression templates, algorithm tuning with measured results

  - **Machine Learning**
    Deep learning architectures, training workflows, model optimization

  ---

  ## Relevant Coursework

  ### Core Computer Science
  - CSC 400 — Discrete Structures for CS
  - CSC 402 / 403 — Data Structures I & II
  - CSC 406 / 407 — Systems I & II

  ### Systems & Performance
  - CSC 461 — Optimized C++
  - CSC 562 — Optimized C++ Multithreading
  - CSC 588 — Real-Time Multithreaded Architecture
  - SE 456 — Architecture of Real-Time Systems
  - CSC 486 — Real-Time Networking (In Progress)

  ### Graphics & Game Engine Development
  - GAM 425 — Applied 3D Geometry
  - GAM 470 — Rendering / Graphics Programming
  - GAM 475 / 575 — Real-Time Software Development I, II, & III
  - GAM 576 — GPU Architecture

  ### Machine Learning & Parallel Computing
  - CSC 483 — Applied Deep Learning
  - CSC 467 — CUDA Development (In Progress)

  ### Independent Study
  - Game Physics Project
