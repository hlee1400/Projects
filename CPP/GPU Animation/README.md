# GPU Animation Engine

A real-time 3D animation engine built with **DirectX 11** and **HLSL compute shaders**, designed to offload skeletal animation computations entirely to the GPU. This project demonstrates low-level GPU programming, parallel computation, and performant engine architecture.

## Demo

[![Engine Demo](https://youtu.be/fJgJ7EWizRg)]


## Key Technical Features

### GPU Compute Shader Pipeline
- Skeletal animation blending (bone interpolation) executed entirely on the GPU via HLSL compute shaders
- **Quaternion SLERP** and **vector LERP** implemented in compute shaders for smooth bone interpolation between keyframes
- **World matrix computation** parallelized on the GPU — hierarchy traversal, bone-to-matrix conversion (TRS decomposition), and matrix concatenation all run as GPU dispatches
- Multi-stage compute pipeline: keyframe blending -> hierarchy resolution -> world bone output, with SRV/UAV resource binding between stages

### Low-Level GPU Resource Management
- Direct management of GPU buffer resources: Constant Buffer Views (CBV), Shader Resource Views (SRV), Unordered Access Views (UAV), Vertex/Index Buffer Views (VBV/IBV)
- Structured buffer design for efficient GPU read/write of bone data (translation, quaternion rotation, scale)
- Compute shader dispatch and resource barrier management for correct execution ordering

### Engine Architecture
- **Manager pattern** for resource lifecycle management (meshes, textures, shaders, clips, skeletons, game objects)
- **State pattern** for DirectX pipeline configuration (device, swap chain, rasterizer, depth stencil, render targets)
- Animation system supporting single-animation playback, two-animation blending, and multi-stage compute blending
- GLTF model loading with Protocol Buffers for serialized asset data
- Scene graph with camera management and hierarchical game object transforms

### Rendering
- Multiple shader pipelines: flat texturing, lit texturing, wireframe, constant color, color-by-vertex, sprite rendering
- Skinned mesh rendering with GPU-computed bone world matrices passed directly to vertex shaders

## Technical Stack

| Component         | Technology                    |
|-------------------|-------------------------------|
| Graphics API      | DirectX 11                    |
| Shading Language  | HLSL (Vertex, Pixel, Compute) |
| Language          | C++                           |
| Asset Format      | GLTF / Protocol Buffers       |
| Platform          | Windows                       |

## Architecture Overview

```
Compute Shader Pipeline (GPU)
===========================================
Keyframe Data (SRV) --> [Mixer Compute Shader] --> Blended Bones (UAV)
                              |
                              v
Hierarchy Table (SRV) --> [World Compute Shader] --> Bone World Matrices (UAV)
                              |
                              v
                    [Skinned Vertex Shader] --> Final Rendered Frame
```

## Source Code Access

This repository serves as a portfolio showcase. The full source code is available upon request for interview or review purposes.

**Contact:** leehs.codes@gmail.com | https://www.linkedin.com/in/leehscodes/

---

*Built as part of graduate coursework in Game Engine development. Represents collaborative work between student implementation and course framework.*
