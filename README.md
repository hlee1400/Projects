# Projects

This repository showcases systems-focused and performance-oriented work across **C++ engine architecture, GPU programming, and applied deep learning**.

The projects emphasize:
- **Low-level system design**
- **Parallel computation**
- **Scalable data pipelines**
- **Real-time performance and efficient resource management**

---

## Projects

### GPU Animation Engine  
**C++ · DirectX 11 · HLSL**

A real-time 3D animation engine that offloads skeletal animation and hierarchy computation entirely to the GPU using compute shaders.

**Key Highlights:**
- GPU-based skeletal animation (no CPU bottleneck)
- Multi-stage compute shader pipeline (blend → hierarchy → world matrices)
- Explicit GPU resource management (SRV / UAV / CBV)
- Engine architecture using manager + state patterns

---

### Image Captioning System  
**Python · TensorFlow**

A multimodal deep learning pipeline combining CNN encoders and attention-based sequence decoders to generate image captions.

**Key Highlights:**
- CNN + RNN (LSTM) with attention mechanism
- End-to-end training and evaluation pipeline
- Techniques for improving generalization and stability
- Handling vanishing gradients and overfitting

---

## Core Competencies Demonstrated

- **Systems Engineering**  
  Modular architecture, memory/resource management, pipeline orchestration  

- **High-Performance Computing**  
  GPU compute shaders, multistage dispatch, data-parallel workloads  

- **Machine Learning**  
  Deep learning architectures, training workflows, model optimization  

---

## Relevant Coursework

### Core Computer Science
- CSC 400 — Discrete Structures for CS  
- CSC 401 — Introduction to Programming  
- CSC 402 / 403 — Data Structures I & II  
- CSC 406 / 407 — Systems I & II  

### Systems & Performance
- CSC 461 — Optimized C++  
- CSC 562 — Optimized C++ Multithreading  
- CSC 588 — Real-Time Multithreaded Architecture  
- SE 456 — Architecture of Real-Time Systems  
- CSC 486 — Real-Time Networking  

### Graphics & Game Development
- GAM 425 — Applied 3D Geometry  
- GAM 470 — Rendering / Graphics Programming  
- GAM 475 / 575 — Real-Time Software Development I & II  
- GAM 576 — GPU Architecture  

### Machine Learning & Parallel Computing
- CSC 483 — Applied Deep Learning  
- CSC 467 — CUDA Development  

### Independent Study
- Game Physics Project