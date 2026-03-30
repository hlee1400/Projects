# GPU Animation Engine

A real-time 3D animation engine built with **DirectX 11** and **HLSL compute shaders**, designed to offload skeletal animation computations entirely to the GPU. This project demonstrates low-level GPU programming, parallel computation, and performant engine architecture.

## Demo

[Engine Demo](https://youtu.be/fJgJ7EWizRg)


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

### Rendering
- Multiple shader pipelines: flat texturing, lit texturing, wireframe, constant color, color-by-vertex, sprite rendering
- Skinned mesh rendering with GPU-computed bone world matrices passed directly to vertex shaders

## Code Highlights

### GPU Bone Blending — Quaternion SLERP on the GPU

Each bone's rotation is interpolated via spherical linear interpolation (SLERP) directly in a compute shader. This avoids CPU-GPU data transfer bottlenecks by keeping all per-bone interpolation work on the GPU. Each thread processes one bone in parallel.

```hlsl
[numthreads(1, 1, 1)]
void main(uint3 dtID : SV_DispatchThreadID)
{
    uint boneIndex = dtID.x;

    if (dtID.x < csMixerA.numNodes)
    {
        uMixerOutAx[boneIndex] = tKeyAa[boneIndex];

        float4 trans = lerpMe(tKeyAa[boneIndex].t, tKeyAb[boneIndex].t, csMixerA.ts);
        float4 quat  = slerp(tKeyAa[boneIndex].q, tKeyAb[boneIndex].q, csMixerA.ts);
        float4 scale = lerpMe(tKeyAa[boneIndex].s, tKeyAb[boneIndex].s, csMixerA.ts);

        uMixerOutAx[boneIndex].t = trans;
        uMixerOutAx[boneIndex].q = quat;
        uMixerOutAx[boneIndex].s = scale;
    }
}

float4 slerp(float4 src, float4 tar, float t)
{
    float4 result;
    float4 target;
    float QUAT_EPSILON = 0.001f;

    float cosom = src.x * tar.x + src.y * tar.y + src.z * tar.z + src.w * tar.w;

    if (cosom >= 1.0f)
    {
        result = src;
    }
    else
    {
        if (cosom < 0.0f)
        {
            cosom = -cosom;
            target = -tar;
        }
        else
        {
            target = tar;
        }

        float tarFactor = t;
        float srcFactor = 1.0f - t;

        if (cosom < (1.0f - QUAT_EPSILON))
        {
            const float omega = acos(cosom);
            const float sinom = 1.0f / sin(omega);

            srcFactor = sin(srcFactor * omega) * sinom;
            tarFactor = sin(tarFactor * omega) * sinom;
        }

        result.x = src.x * srcFactor + target.x * tarFactor;
        result.y = src.y * srcFactor + target.y * tarFactor;
        result.z = src.z * srcFactor + target.z * tarFactor;
        result.w = src.w * srcFactor + target.w * tarFactor;
    }

    return result;
}
```

**Relevance:** Demonstrates GPU-accelerated numerical computation — the same class of parallel math (interpolation, matrix operations, per-element transforms) used in computational semiconductor methods and CUDA-based acceleration.

---

### GPU World Matrix Computation — Parallel Hierarchy Traversal

Bone hierarchy traversal and world matrix construction run entirely on the GPU. Each thread walks a bone's parent chain using a pre-computed hierarchy table, concatenating TRS matrices to produce final world-space bone transforms.

```hlsl
[numthreads(1, 1, 1)]
void main(uint3 dtID : SV_DispatchThreadID)
{
    uint boneIndex = dtID.x;

    if (dtID.x < csWorld.numBones)
    {
        row_major matrix tmp =
        {
            { 1, 0, 0, 0 },
            { 0, 1, 0, 0 },
            { 0, 0, 1, 0 },
            { 0, 0, 0, 1 }
        };

        for (uint i = 0; i < csWorld.hierarchyDepth; i++)
        {
            int parentIndex = tHierarchyTable[(boneIndex * csWorld.hierarchyDepth) + i];

            if (parentIndex != -1)
            {
                tmp = mul(Bone2Matrix(uMixerOutAx[parentIndex]), tmp);
            }
        }

        uBoneWorldOut[boneIndex].m = tmp;
    }
}

row_major matrix Bone2Matrix(BoneType bone)
{
    row_major matrix mTrans = TransMatrix(bone.t);
    row_major matrix mScale = ScaleMatrix(bone.s);
    row_major matrix mRot   = RotMatrix(bone.q);

    return mul(mul(mScale, mRot), mTrans);
}
```

**Relevance:** Shows algorithm design for GPU-parallel tree traversal with structured buffer access patterns — directly applicable to hierarchical data processing and workload distribution in scalable compute systems.

---

### Multi-Stage Compute Dispatch — GPU Pipeline Orchestration (C++)

The CPU-side orchestration code binds GPU resources (SRV, UAV, CBV) and dispatches multiple compute shader stages in sequence. This demonstrates low-level GPU memory management and execution ordering across a multi-pass compute pipeline.

```cpp
void ComputeBlend_TwoAnim::privMixerExecute()
{
    ShaderObject *pShaderObj = nullptr;
    unsigned int count = 0;

    // --- Stage 1: Blend Animation A keyframes ---
    pShaderObj = ShaderObjectNodeMan::Find(ShaderObject::Name::MixerACompute);
    pShaderObj->ActivateShader();

    poMixerA->pKeyAa->BindComputeToCS(ShaderResourceBufferSlot::KeyAa);     // SRV input
    poMixerA->pKeyAb->BindComputeToCS(ShaderResourceBufferSlot::KeyAb);     // SRV input
    poMixerA->GetMixerResult()->BindCompute(UnorderedAccessBufferSlot::MixerOutAx);  // UAV output
    poMixerA->GetMixerConstBuff()->Transfer(poMixerA->GetRawConstBuffer());
    poMixerA->GetMixerConstBuff()->BindCompute(ConstantCSBufferSlot::csMixerA);      // CBV constants

    count = (unsigned int)ceil((float)poMixerA->GetNumNodes() / 1.0f);
    StateDirectXMan::GetContext()->Dispatch(count, 1, 1);

    // --- Stage 2: Blend Animation B keyframes ---
    pShaderObj = ShaderObjectNodeMan::Find(ShaderObject::Name::MixerBCompute);
    pShaderObj->ActivateShader();

    poMixerB->pKeyBa->BindComputeToCS(ShaderResourceBufferSlot::KeyBa);
    poMixerB->pKeyBb->BindComputeToCS(ShaderResourceBufferSlot::KeyBb);
    poMixerB->GetMixerResult()->BindCompute(UnorderedAccessBufferSlot::MixerOutBx);
    poMixerB->GetMixerConstBuff()->Transfer(poMixerB->GetRawConstBuffer());
    poMixerB->GetMixerConstBuff()->BindCompute(ConstantCSBufferSlot::csMixerB);

    count = (unsigned int)ceil((float)poMixerB->GetNumNodes() / 1.0f);
    StateDirectXMan::GetContext()->Dispatch(count, 1, 1);

    // --- Stage 3: Cross-blend A and B results ---
    pShaderObj = ShaderObjectNodeMan::Find(ShaderObject::Name::MixerCCompute);
    pShaderObj->ActivateShader();

    poMixerC->pKeyCa->BindCompute(UnorderedAccessBufferSlot::MixerOutAx);   // UAV -> UAV chaining
    poMixerC->pKeyCb->BindCompute(UnorderedAccessBufferSlot::MixerOutBx);
    poMixerC->GetMixerResult()->BindCompute(UnorderedAccessBufferSlot::MixerOutCx);
    poMixerC->GetMixerConstBuff()->Transfer(poMixerC->GetRawConstBuffer());
    poMixerC->GetMixerConstBuff()->BindCompute(ConstantCSBufferSlot::csMixerC);

    count = (unsigned int)ceil((float)poMixerC->GetNumNodes() / 1.0f);
    StateDirectXMan::GetContext()->Dispatch(count, 1, 1);
}
```

**Relevance:** Demonstrates direct GPU resource management (SRV/UAV/CBV binding), multi-stage compute dispatch, and memory transfer orchestration — the same patterns used in CUDA kernel pipelines for high-performance computing and semiconductor simulation workloads.



---

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
