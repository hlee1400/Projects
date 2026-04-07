# GPU Animation Engine

A real-time 3D animation engine built with **DirectX 11** and **HLSL compute shaders**, designed to offload skeletal animation computations entirely to the GPU. This project demonstrates low-level GPU programming, parallel computation, and performant engine architecture.

## Demo

[![Watch the demo](https://img.youtube.com/vi/fJgJ7EWizRg/maxresdefault.jpg)](https://youtu.be/fJgJ7EWizRg)

## Key Technical Features

### GPU Compute Shader Pipeline
- Skeletal animation blending (bone interpolation) executed entirely on the GPU via HLSL compute shaders
- **Quaternion SLERP** and **vector LERP** implemented in compute shaders for smooth bone interpolation between keyframes
- **World matrix computation** parallelized on the GPU — hierarchy traversal, bone-to-matrix conversion (TRS decomposition), and matrix concatenation all run as GPU dispatches
- Multi-stage compute pipeline: keyframe blending -> hierarchy resolution -> world bone output, with SRV/UAV resource binding between stages

### DirectX 11 Rendering Pipeline
- Full D3D11 device initialization with hardware driver, feature level negotiation (11_1 down to 9_1), and DXGI swap chain setup (double-buffered, `FLIP_DISCARD`, `R8G8B8A8_UNORM`)
- **State pattern** wrapping all pipeline configuration: rasterizer (solid/wireframe, front/none culling), depth-stencil (`D32_FLOAT`, `LESS` comparison), blend (alpha enable/disable with `SRC_ALPHA`/`INV_SRC_ALPHA`), viewport, and render target views
- Configurable VSync via DXGI refresh rate querying through the factory/adapter/output chain
- Debug layer support (`D3D11_CREATE_DEVICE_DEBUG`) enabled in debug builds for GPU validation

### Low-Level GPU Resource Management
- Direct management of GPU buffer resources: Constant Buffer Views (CBV), Shader Resource Views (SRV), Unordered Access Views (UAV), Vertex/Index Buffer Views (VBV/IBV)
- Structured buffer design for efficient GPU read/write of bone data (translation, quaternion rotation, scale)
- **6-slot vertex buffer architecture** — Position, Color, Normal, TexCoord, Weights, and Joints bound to separate input assembler slots via `IASetVertexBuffers()`
- Constant buffers created per shader stage (VS/PS/CS) with per-frame `UpdateSubresource()` transfers and stage-specific binding (`VSSetConstantBuffers`, `PSSetConstantBuffers`, `CSSetConstantBuffers`)
- Compute shader output via UAV-to-SRV `CopyResource()` bridge — compute results written to UAVs are copied into SRVs for vertex shader consumption, since UAVs are not bindable to vertex shader stages
- Compute shader dispatch and resource barrier management for correct execution ordering

### Engine Architecture
- **Manager pattern** for resource lifecycle management (meshes, textures, shaders, clips, skeletons, game objects)
- **State pattern** for DirectX pipeline configuration (device, swap chain, rasterizer, depth stencil, render targets)
- Animation system supporting single-animation playback, two-animation blending, and multi-stage compute blending
- GLTF model loading with Protocol Buffers for serialized asset data
- Scene graph with camera management and hierarchical game object transforms

### Rendering
- Multiple shader pipelines: flat texturing, lit texturing, wireframe, constant color, color-by-vertex, sprite rendering
- Per-vertex diffuse lighting computed in the vertex shader using model-view normal matrix extraction and light direction calculation
- **4-bone weighted skinned mesh rendering** — inverse bind matrices and GPU-computed bone world matrices sampled from structured buffers directly in the vertex shader via SRVs
- All shaders compiled to Shader Model 5.0 (`vs_5_0`, `ps_5_0`, `cs_5_0`) with row-major matrix packing

## Code Highlights

### D3D11 Device & Swap Chain Initialization

The engine bootstraps DirectX through a singleton manager that creates the device, device context, and swap chain in a single call. The swap chain is configured for double-buffered flip-model presentation with optional VSync.

```cpp
DXGI_SWAP_CHAIN_DESC swapChainDesc{0};
swapChainDesc.BufferCount = 2;
swapChainDesc.BufferDesc.Width = clientWidth;
swapChainDesc.BufferDesc.Height = clientHeight;
swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
swapChainDesc.BufferDesc.RefreshRate = StateDirectXMan::QueryRefreshRate(clientWidth, clientHeight, vSync);
swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
swapChainDesc.SampleDesc.Count = 1;

D3D_FEATURE_LEVEL featureLevels[] = {
    D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0,
    D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0,
    D3D_FEATURE_LEVEL_9_3,  D3D_FEATURE_LEVEL_9_2, D3D_FEATURE_LEVEL_9_1
};

D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
    createDeviceFlags, featureLevels, _countof(featureLevels),
    D3D11_SDK_VERSION, &swapChainDesc,
    &poSwapChain, &poDevice, &featureLevel, &poDeviceContext);
```

---

### Pipeline State Configuration — Alpha Blending

Each pipeline state is encapsulated in its own class with `Initialize()` and `Activate()` methods. The blend state demonstrates the pattern — configuring standard alpha blending for transparent rendering:

```cpp
void StateBlend::Initialize(Mode mode)
{
    CD3D11_BLEND_DESC BlendState;
    ZeroMemory(&BlendState, sizeof(CD3D11_BLEND_DESC));

    // Standard alpha blending: dest.rgb = src.rgb * src.a + dest.rgb * (1 - src.a)
    BlendState.RenderTarget[0].BlendEnable = TRUE;
    BlendState.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    BlendState.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    BlendState.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    BlendState.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;
    BlendState.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
    BlendState.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    BlendState.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    StateDirectXMan::GetDevice()->CreateBlendState(&BlendState, &this->poD3DBlendState);
}

void StateBlend::Activate()
{
    float blendFactor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    UINT sampleMask = 0xffffffff;
    StateDirectXMan::GetContext()->OMSetBlendState(this->poD3DBlendState, blendFactor, sampleMask);
}
```

---

### GPU Buffer Management — Vertex & Constant Buffers

Vertex buffers are created with `D3D11_BIND_VERTEX_BUFFER` and bound to named input assembler slots. Constant buffers use `UpdateSubresource()` for per-frame data transfer to the GPU:

```cpp
// Vertex buffer creation
D3D11_BUFFER_DESC vertexBufferDesc{0};
vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
vertexBufferDesc.ByteWidth = numBytes;
vertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;

D3D11_SUBRESOURCE_DATA resourceData{0};
resourceData.pSysMem = pData;
StateDirectXMan::GetDevice()->CreateBuffer(&vertexBufferDesc, &resourceData, &poBufferVBV);

// Vertex buffer activation (per slot: Position, Color, Normal, TexCoord, Weights, Joints)
StateDirectXMan::GetContext()->IASetVertexBuffers((uint32_t)slot, 1,
    &poBufferVBV, &strideSize, &offset);
```

```cpp
// Constant buffer creation
D3D11_BUFFER_DESC BufferDesc{0};
BufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
BufferDesc.ByteWidth = structSize;
BufferDesc.Usage = D3D11_USAGE_DEFAULT;
StateDirectXMan::GetDevice()->CreateBuffer(&BufferDesc, nullptr, &poBufferCBV);

// Per-frame transfer and binding
StateDirectXMan::GetContext()->UpdateSubresource(poBufferCBV, 0, nullptr, pBuff, 0, 0);
StateDirectXMan::GetContext()->VSSetConstantBuffers((uint32_t)slot, 1, &poBufferCBV);
```

---

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

### Skinned Mesh Vertex Shader — 4-Bone Weighted Skinning

The vertex shader samples inverse bind matrices and GPU-computed bone world matrices from structured buffers (SRVs), applies 4-bone weighted blending per vertex, and transforms through the full MVP chain. The bone world matrices arrive directly from the compute shader pipeline with no CPU round-trip.

```hlsl
VertexShaderOutput main(VertData_pos inPos, VertData_tex inTex,
                        VertData_weight inWeight, VertData_joint inJoint)
{
    VertexShaderOutput outValue;

    // 4-bone weighted skinning: InvBind * BoneWorld per joint, weighted sum
    row_major matrix SkinWorld =
        mul(mul(tSkinInvBind[inJoint.j.x].m, tSkinBoneWorld[inJoint.j.x].m), inWeight.w.x) +
        mul(mul(tSkinInvBind[inJoint.j.y].m, tSkinBoneWorld[inJoint.j.y].m), inWeight.w.y) +
        mul(mul(tSkinInvBind[inJoint.j.z].m, tSkinBoneWorld[inJoint.j.z].m), inWeight.w.z) +
        mul(mul(tSkinInvBind[inJoint.j.w].m, tSkinBoneWorld[inJoint.j.w].m), inWeight.w.w);

    // Skin * World * View * Projection
    row_major matrix Mat = mul(mul(mul(SkinWorld, vsWorldMatrix), vsViewMatrix), vsProjectionMatrix);

    outValue.position = mul(float4(inPos.pos, 1.0f), Mat);
    outValue.tex = inTex.tex;

    return outValue;
}
```

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

| Component | Technology |
|-----------|-----------|
| Graphics API | DirectX 11 |
| Shading Language | HLSL (Vertex, Pixel, Compute) — Shader Model 5.0 |
| Language | C++ |
| Asset Format | GLTF / Protocol Buffers |
| Platform | Windows |

## Architecture Overview

```
Engine Architecture
===========================================

         StateDirectXMan (Singleton)
        /          |           \
   ID3D11Device  ID3D11DeviceContext  IDXGISwapChain
       |               |                    |
  Create Buffers   Bind & Draw           Present
  Create States    Dispatch Compute      VSync

Pipeline State Objects
===========================================
StateBlend ──── StateRasterizer ──── StateDepthStencil
     |                |                     |
 OMSetBlendState  RSSetState     OMSetDepthStencilState

GPU Buffer Wrappers
===========================================
BufferVBV_ia ─── BufferIBV_ia ─── BufferCBV_vs/ps/cs
BufferSRV_cs ─── BufferUAV_cs ─── BufferTexture2D

Compute Shader Pipeline (GPU)
===========================================
Keyframe Data (SRV) --> [Mixer Compute Shader] --> Blended Bones (UAV)
                              |
                              v
Hierarchy Table (SRV) --> [World Compute Shader] --> Bone World Matrices (UAV)
                              |
                      CopyResource (UAV -> SRV)
                              |
                              v
                    [Skinned Vertex Shader] --> Final Rendered Frame
```

## Source Code Access

This repository serves as a portfolio showcase. The full source code is available upon request for interview or review purposes.

**Contact:** leehs.codes@gmail.com | [LinkedIn](https://www.linkedin.com/in/leehscodes/)

---

*Built as part of graduate coursework in Game Engine development. Represents collaborative work between student implementation and course framework.*
