# Real-Time 3D Rendering Engine (DirectX 11)

A custom real-time 3D rendering engine built from scratch in C++ using DirectX 11, demonstrating low-level GPU pipeline programming, shader architecture, advanced lighting models, and procedural terrain generation.

---

## Project Overview

This project implements a complete graphics rendering pipeline including:

- **GPU Device & Pipeline Initialization** -- Direct3D 11 device creation, DXGI swap chain configuration, rasterizer state management, depth-stencil buffering, and viewport setup
- **Shader Compilation & Management** -- Runtime HLSL shader compilation with an object-oriented shader hierarchy (color, texture, lighting, texture+lighting)
- **Phong Lighting System** -- Full Phong illumination model supporting directional, point, and spot lights with configurable attenuation, fog, and material properties
- **Heightmap-Based Terrain Generation** -- Procedural terrain mesh construction from TGA heightmap textures with per-vertex normal averaging for smooth shading
- **Skybox Environment Rendering** -- Cross-texture mapped cube environment with epsilon UV offsets to prevent seam artifacts
- **Custom Camera System** -- Right-hand perspective camera with manual projection/view matrix construction and OpenGL-to-DirectX NDC conversion
- **GPU Resource Management** -- Vertex/index buffer creation, constant buffer pooling, COM object lifecycle management, and multi-mesh batching via MeshSeparator

---

## Architecture

```
DXApp (Main Loop: Init -> Update -> Draw)
    |
    |-- ShaderBase (abstract)
    |       |-- ShaderColor          (Position only)
    |       |-- ShaderColorLight     (Position + Normal, Phong lighting)
    |       |-- ShaderTexture        (Position + UV)
    |       |-- ShaderTextureLight   (Position + UV + Normal)
    |
    |-- GraphicObject_Base (abstract)
    |       |-- GraphicObject_Color
    |       |-- GraphicObject_ColorLighting
    |       |-- GraphicObject_TextureLight
    |
    |-- Model (GPU mesh storage, file loading, procedural generation)
    |-- MeshSeparator (multi-mesh index reordering for batch rendering)
    |-- TerrainModel (heightmap -> mesh pipeline)
    |-- SkyBox (6-face environment cube)
    |-- Camera (projection + view matrix math)
    |-- Texture (TGA/DDS loading, mipmap generation, sampler state)
```

**Rendering Data Flow (per frame):**
```
Clear Buffers -> Update Camera Matrices
    -> Shader.SetToContext()           [Bind VS, PS, Input Layout]
    -> Shader.SendCamMatrices()        [UpdateSubresource -> cb0]
    -> Shader.SendLightParameters()    [UpdateSubresource -> cb1]
    -> Shader.SendWorldAndMaterial()   [UpdateSubresource -> cb2]
    -> Model.SetToContext()            [IASetVertexBuffers, IASetIndexBuffer]
    -> Model.DrawIndexed()             [GPU draw call]
    -> SwapChain.Present()
```

---

## Highlighted Code Snippets

### 1. GPU Device Initialization & Pipeline Configuration

Full DirectX 11 device setup including DXGI factory chain traversal, swap chain creation, rasterizer state, and depth-stencil configuration.

```cpp
// DXApp.cpp - InitDirect3D()

// D3D11 device creation with debug layer support
UINT createDeviceFlags = 0;
#ifdef _DEBUG
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
    createDeviceFlags, nullptr, 0, D3D11_SDK_VERSION,
    &md3dDevice, nullptr, &md3dImmediateContext);

// DXGI factory chain: Device -> DXGIDevice -> Adapter -> Factory
IDXGIDevice* dxgiDevice = nullptr;
hr = md3dDevice->QueryInterface(__uuidof(IDXGIDevice),
    reinterpret_cast<void**>(&dxgiDevice));
IDXGIAdapter* adapter = nullptr;
hr = dxgiDevice->GetAdapter(&adapter);
IDXGIFactory1* dxgiFactory1 = nullptr;
hr = adapter->GetParent(__uuidof(IDXGIFactory1),
    reinterpret_cast<void**>(&dxgiFactory1));

// Swap chain with double buffering
DXGI_SWAP_CHAIN_DESC sd;
ZeroMemory(&sd, sizeof(sd));
sd.BufferCount = 2;
sd.BufferDesc = buffdesc;
sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
sd.OutputWindow = mhMainWnd;
sd.SampleDesc = sampDesc;
sd.Windowed = TRUE;
hr = dxgiFactory1->CreateSwapChain(md3dDevice, &sd, &mSwapChain);

// Rasterizer state: front-face winding, culling, depth clipping
D3D11_RASTERIZER_DESC rd;
rd.FillMode = D3D11_FILL_SOLID;
rd.CullMode = D3D11_CULL_BACK;
rd.FrontCounterClockwise = true;  // Right-hand coordinate system
rd.DepthClipEnable = true;
rd.MultisampleEnable = true;

// Depth-stencil buffer (24-bit depth, 8-bit stencil)
D3D11_TEXTURE2D_DESC descDepth;
descDepth.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
descDepth.SampleDesc = sampDesc;
descDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL;
hr = md3dDevice->CreateTexture2D(&descDepth, NULL, &pDepthStencil);

D3D11_DEPTH_STENCIL_VIEW_DESC descDSV;
descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMS;
hr = md3dDevice->CreateDepthStencilView(pDepthStencil, &descDSV, &mpDepthStencilView);

md3dImmediateContext->OMSetRenderTargets(1, &mRenderTargetView, mpDepthStencilView);
```

---

### 2. Shader Compilation Pipeline & GPU State Binding

Runtime HLSL compilation with debug flag support, vertex/pixel shader creation from compiled blobs, and constant buffer binding across shader stages.

```cpp
// ShaderBase.cpp - Shader compilation and GPU upload

void ShaderBase::privBuildShaders(WCHAR* filename, LPCSTR vsModel, LPCSTR psModel)
{
    pVSBlob = nullptr;
    HRESULT hr = CompileShaderFromFile(filename, "VS", vsModel, &pVSBlob);

    hr = mDevice->CreateVertexShader(
        pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(),
        nullptr, &mpVertexShader);

    pPSBlob = nullptr;
    hr = CompileShaderFromFile(filename, "PS", psModel, &pPSBlob);

    hr = mDevice->CreatePixelShader(
        pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(),
        nullptr, &mpPixelShader);
}

HRESULT ShaderBase::CompileShaderFromFile(const WCHAR* szFileName,
    LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut)
{
    DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
    dwShaderFlags |= D3DCOMPILE_DEBUG;
    dwShaderFlags |= D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    ID3DBlob* pErrorBlob = nullptr;
    hr = D3DCompileFromFile(szFileName, nullptr, nullptr,
        szEntryPoint, szShaderModel, dwShaderFlags, 0, ppBlobOut, &pErrorBlob);
    // ... error handling with OutputDebugStringA
}

// ShaderColorLight.cpp - Constant buffer binding to VS and PS stages

void ShaderColorLight::SetToContext(ID3D11DeviceContext* devcon)
{
    ShaderBase::SetToContext_VS_PS_InputLayout();

    // Bind constant buffers to both vertex and pixel shader stages
    devcon->VSSetConstantBuffers(0, 1, &mpBufferCamMatrices);   // cb0: View/Proj
    devcon->VSSetConstantBuffers(1, 1, &mpBufferLightParams);   // cb1: Lights
    devcon->VSSetConstantBuffers(2, 1, &mpBuffWordAndMaterial);  // cb2: World/Material
    devcon->VSSetConstantBuffers(3, 1, &mpBufferFog);            // cb3: Fog

    devcon->PSSetConstantBuffers(0, 1, &mpBufferCamMatrices);
    devcon->PSSetConstantBuffers(1, 1, &mpBufferLightParams);
    devcon->PSSetConstantBuffers(2, 1, &mpBuffWordAndMaterial);
    devcon->PSSetConstantBuffers(3, 1, &mpBufferFog);
}
```

---

### 3. Phong Lighting Model with Multiple Light Types

Complete Phong illumination system with directional, point (with attenuation & range), and spot (with cone exponent) lights, plus distance-based fog.

```cpp
// ShaderColorLight.h - Light and material data structures

struct PhongADS {
    Vect Ambient;
    Vect Diffuse;
    Vect Specular;
};

struct DirectionalLight {
    PhongADS Light;
    Vect Direction;
};

struct PointLight {
    PhongADS Light;
    Vect Position;
    Vect Attenuation;  // (constant, linear, quadratic)
    float Range;
};

struct SpotLight {
    PhongADS Light;
    Vect Position;
    Vect Attenuation;
    Vect Direction;
    float SpotExp;     // Controls beam tightness
    float Range;
};

struct Data_LightParams {
    DirectionalLight DirLight;
    PointLight       PntLight[3];
    SpotLight        SptLight[3];
    Vect             EyePosWorld;
};

// ShaderColorLight.cpp - Marshaling light data to GPU constant buffer

void ShaderColorLight::SendLightParameters(const Vect& eyepos)
{
    Data_LightParams dl;
    dl.DirLight = DirLightData;
    for (int i = 0; i < numLights; i++) {
        dl.PntLight[i] = PointLightData[i];
        dl.SptLight[i] = SpotLightData[i];
    }
    dl.EyePosWorld = eyepos;
    this->GetContext()->UpdateSubresource(mpBufferLightParams, 0, nullptr, &dl, 0, 0);
}

void ShaderColorLight::SendWorldAndMaterial(const Matrix& world,
    const Vect& amb, const Vect& dif, const Vect& sp)
{
    Data_WorldAndMaterial wm;
    wm.World = world;
    wm.WorlInv = world.getInv();  // Inverse for normal transformation
    wm.Mat.Ambient = amb;
    wm.Mat.Diffuse = dif;
    wm.Mat.Specular = sp;
    this->GetContext()->UpdateSubresource(mpBuffWordAndMaterial, 0, nullptr, &wm, 0, 0);
}
```

---

### 4. Heightmap Terrain Generation with Normal Averaging

Procedural terrain mesh from TGA heightmap pixels with per-vertex smooth normals computed by averaging 6 adjacent face normals.

```cpp
// TerrainModel.cpp - Terrain mesh construction

TerrainModel::TerrainModel(ID3D11Device* dev, LPCWSTR heightmapFile,
    float len, float maxheight, float ytrans, int RepeatU, int RepeatV)
{
    DirectX::ScratchImage scrtTex;
    HRESULT hr = LoadFromTGAFile(heightmapFile, nullptr, scrtTex);
    const DirectX::Image* hgtmap = scrtTex.GetImage(0, 0, 0);

    size_t side = hgtmap->height;
    int nverts = side * side;
    int ntri = ((side - 1) * (side - 1)) * 2;
    StandardVertex* pVerts = new StandardVertex[nverts];
    TriangleByIndex* pTriList = new TriangleByIndex[ntri];

    // Generate vertex grid from heightmap pixel data
    for (int row = 0; row < side; row++) {
        for (int col = 0; col < side; col++) {
            float pixelHeight = getPixelHeight(side, row, col, pixel_width, maxheight, hgtmap);
            pVerts[vertIndex].set(xCurrPos, pixelHeight + ytrans, zCurrPos, uCurr, vCurr);
        }
    }

    // Per-vertex normal averaging from 6 surrounding triangles
    for (int i = 1; i < side - 1; i++) {
        for (int j = 1; j < side - 1; j++) {
            Vect& pt_curr = pVerts[(i * side) + j].Pos;

            // Sample 6 neighbors
            Vect& pt_1 = pVerts[((i-1) * side) + j].Pos;
            Vect& pt_2 = pVerts[(i * side) + j - 1].Pos;
            Vect& pt_3 = pVerts[((i+1) * side) + j - 1].Pos;
            Vect& pt_4 = pVerts[((i+1) * side) + j].Pos;
            Vect& pt_5 = pVerts[(i * side) + j + 1].Pos;
            Vect& pt_6 = pVerts[((i-1) * side) + j + 1].Pos;

            // Compute 6 face normals via cross products
            Vect edge1 = pt_1 - pt_curr;
            Vect edge2 = pt_2 - pt_curr;
            Vect norm1 = edge1.cross(edge2);
            // ... (repeat for all 6 surrounding faces)

            // Average and normalize
            Vect Normal = norm1 + norm2 + norm3 + norm4 + norm5 + norm6;
            pVerts[(i * side) + j].normal = (Normal * (1.0f / 6.0f));
            pVerts[(i * side) + j].normal.norm();
        }
    }
    pModTerrain = new Model(dev, pVerts, nverts, pTriList, ntri);
}

// Heightmap pixel sampling
float TerrainModel::getPixelHeight(int side, int row, int col,
    size_t pixel_width, float maxheight, const DirectX::Image* hgtmap)
{
    int index = TexelIndex(side, row, col, pixel_width);
    unsigned char val = hgtmap->pixels[index];
    return (maxheight * val) / 255.0f;
}
```

---

### 5. Custom Perspective Camera with NDC Conversion

Hand-built projection and view matrices with right-hand coordinate system support and OpenGL-to-DirectX normalized device coordinate conversion.

```cpp
// Camera.cpp - Projection matrix with NDC space conversion

void Camera::privUpdateProjectionMatrix(void)
{
    float d = 1 / tanf(fovy / 2);

    // Standard perspective projection
    this->projMatrix[m0] = d / aspectRatio;
    this->projMatrix[m5] = d;
    this->projMatrix[m10] = -(farDist + nearDist) / (farDist - nearDist);
    this->projMatrix[m11] = -1.0f;
    this->projMatrix[m14] = (-2.0f * farDist * nearDist) / (farDist - nearDist);

    // Convert OpenGL NDC [-1,1] to DirectX NDC [0,1]
    Matrix B(TRANS, 0, 0, 1.0f);
    Matrix S(SCALE, 1, 1, 0.5f);
    Matrix NDCConvert = B * S;
    projMatrix = projMatrix * NDCConvert;
}

// Right-hand view matrix from camera basis vectors
void Camera::privUpdateViewMatrix(void)
{
    this->viewMatrix[m0]  = vRight[x];
    this->viewMatrix[m1]  = vUp[x];
    this->viewMatrix[m2]  = vDir[x];
    // ... (3x3 rotation from basis vectors)

    // Translation: R^T * (-Pos) via dot products
    this->viewMatrix[m12] = -vPos.dot(vRight);
    this->viewMatrix[m13] = -vPos.dot(vUp);
    this->viewMatrix[m14] = -vPos.dot(vDir);
    this->viewMatrix[m15] = 1.0f;
}

// Camera orientation from look-at with Gram-Schmidt orthogonalization
void Camera::setOrientAndPosition(const Vect& inUp, const Vect& inLookAt, const Vect& inPos)
{
    this->vDir = -(inLookAt - inPos);  // Right-hand: view direction is flipped
    this->vDir.norm();

    this->vRight = inUp.cross(this->vDir);
    this->vRight.norm();

    this->vUp = this->vDir.cross(this->vRight);
    this->vUp.norm();
}
```

---

### 6. Vertex Buffer Management & Multi-Mesh Batch Rendering

GPU buffer creation, model file parsing (.azul format), and MeshSeparator for reordering triangle indices to enable efficient per-mesh draw calls from a single buffer.

```cpp
// Model.cpp - GPU buffer upload

void Model::privLoadDataToGPU()
{
    meshes = new MeshSeparator(pStdVerts, numVerts, pTriList, numTris);

    D3D11_BUFFER_DESC bd;
    ZeroMemory(&bd, sizeof(bd));
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(StandardVertex) * numVerts;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA InitData;
    InitData.pSysMem = pStdVerts;
    hr = mDevice->CreateBuffer(&bd, &InitData, &mpVertexBuffer);

    bd.ByteWidth = sizeof(TriangleByIndex) * numTris;
    bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    InitData.pSysMem = pTriList;
    hr = mDevice->CreateBuffer(&bd, &InitData, &mpIndexBuffer);
}

void Model::SetToContext(ID3D11DeviceContext* context)
{
    UINT stride = sizeof(StandardVertex);
    UINT offset = 0;
    context->IASetVertexBuffers(0, 1, &mpVertexBuffer, &stride, &offset);
    context->IASetIndexBuffer(mpIndexBuffer, DXGI_FORMAT_R32_UINT, 0);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void Model::RenderMesh(ID3D11DeviceContext* context, int meshnum)
{
    int tricount, trioffset;
    meshes->GetMeshTriCountAndOffset(meshnum, tricount, trioffset);
    context->DrawIndexed(tricount * 3, trioffset * 3, 0);
}

// MeshSeparator.cpp - Triangle index reordering for batch rendering

MeshSeparator::MeshSeparator(StandardVertex* pVerts, int nverts,
    TriangleByIndex*& pTriList, int ntri)
{
    // Sort triangles by mesh ID using std::map
    std::map<int, std::vector<TriangleByIndex>> meshes;
    for (int i = 0; i < ntri; ++i) {
        TriangleByIndex ind = pTriList[i];
        int key = pVerts[ind.v0].meshNum;
        meshes[key].push_back(ind);
    }

    // Rebuild index list ordered by mesh, storing offsets for DrawIndexed()
    TriangleByIndex* templist = new TriangleByIndex[ntri];
    meshdata = new MeshIndexData[meshes.size()];
    int offsetval = 0;
    for (size_t i = 0; i < meshes.size(); ++i) {
        meshdata[i].numTri = meshes[i].size();
        meshdata[i].offset = offsetval;
        for (size_t j = 0; j < meshes[i].size(); ++j)
            templist[offsetval + j] = meshes[i][j];
        offsetval += meshes[i].size();
    }
    delete[] pTriList;
    pTriList = templist;  // Replace with reordered list
}
```

---

## Why These Files? -- Showcased Source Code & Skill Alignment

If reviewing only two source files from this project, **`ShaderColorLight.h/.cpp`** and **`TerrainModel.cpp`** best demonstrate the skills relevant to graphics systems software engineering.

### `ShaderColorLight.h` + `ShaderColorLight.cpp`

| Internship Requirement | How This File Demonstrates It |
|------------------------|-------------------------------|
| **Designing and implementing graphics drivers** | Directly configures the GPU rendering pipeline: creates constant buffers (`D3D11_BUFFER_DESC`), binds them to specific register slots (`VSSetConstantBuffers`, `PSSetConstantBuffers`), and marshals structured data from CPU to GPU via `UpdateSubresource` -- the same patterns used in real driver-level shader state management. |
| **Supporting new hardware features** | The shader architecture is extensible by design -- adding a new light type or material property means defining a new struct, allocating a constant buffer, and binding it to the next register slot. This mirrors how driver teams expose new GPU features through the shader pipeline. |
| **Device Driver Programming** | Manages GPU resource lifecycle (buffer creation, COM `Release` in destructors), handles `HRESULT` error checking, and directly interfaces with the D3D11 device context -- the same API surface that graphics drivers implement beneath. |
| **3D Graphics Theory & Implementation** | Implements the complete Phong illumination model with `PhongADS` (Ambient/Diffuse/Specular) structs, three distinct light types (directional, point with attenuation, spot with cone exponent), material properties, world-inverse matrix for normal transformation, and distance-based fog. |
| **Low-Level Library Development** | Designed as a reusable shader abstraction layer inheriting from `ShaderBase`. The `SetToContext()` / `Send*()` API pattern cleanly separates pipeline state binding from per-object data upload -- a design used in production graphics middleware. |
| **Simulation or Emulation (writing & debugging tests)** | Includes `D3DCOMPILE_DEBUG` and `D3DCOMPILE_SKIP_OPTIMIZATION` flags for shader debugging, enabling step-through debugging of GPU shaders -- directly relevant to conformance test development and hardware validation. |

### `TerrainModel.cpp`

| Internship Requirement | How This File Demonstrates It |
|------------------------|-------------------------------|
| **3D/2D Graphics Theory, Implementation & Optimizations** | Implements heightmap-to-mesh conversion end-to-end: texel sampling from raw pixel data, vertex grid generation with UV tiling, triangle strip construction, cross-product face normals, and per-vertex smooth normal averaging from 6 adjacent faces. This is textbook graphics theory applied to a working implementation. |
| **Computer Architecture** | Demonstrates understanding of memory layout: raw pixel indexing (`pixel_width * (row * side + col)`), contiguous vertex/index array construction for efficient GPU upload, and awareness of data locality when iterating the height grid. |
| **Designing and implementing graphics drivers / platform support** | Bridges CPU texture data to GPU geometry: loads a TGA heightmap via DirectXTex (`LoadFromTGAFile`, `ScratchImage`, `GetImage`), processes it on the CPU, then uploads the result as vertex/index buffers via `D3D11_BUFFER_DESC` and `CreateBuffer` -- the same CPU-to-GPU data pipeline that driver code manages. |
| **Real-Time Systems Development** | The terrain is generated once at load time and rendered every frame via a single `DrawIndexed` call -- demonstrating the real-time constraint of separating expensive setup from per-frame rendering, a key concern in game console and embedded GPU systems. |
| **Simulation or Emulation (writing & debugging tests)** | The heightmap approach itself is a form of simulation: sampling a 2D data source to procedurally reconstruct 3D geometry with correct normals. The parameterized constructor (`len`, `maxheight`, `ytrans`, `RepeatU`, `RepeatV`) makes it easy to create varied test terrains for visual validation. |
| **Low-Level Library Development** | Self-contained module with a clean interface: takes a device pointer and heightmap file, produces a renderable model. No external dependencies beyond DirectXTex and the project's own `Model` class. Demonstrates the kind of encapsulated, reusable component expected in graphics middleware and driver support libraries. |

---

## Skills Demonstrated

| Area | Details |
|------|---------|
| **Graphics Driver / Pipeline Programming** | D3D11 device creation, DXGI adapter enumeration, swap chain management, rasterizer state, depth-stencil configuration |
| **Shader Programming** | HLSL compilation pipeline, vertex/pixel shader creation, input layout descriptors, constant buffer register binding |
| **3D Graphics Theory** | Phong illumination (ADS), perspective projection matrices, view transforms, normal vector computation, NDC conversion |
| **GPU Resource Management** | Vertex/index buffer creation, constant buffer pooling via UpdateSubresource, COM lifecycle with Release patterns |
| **Low-Level Optimization** | Multi-mesh batching (MeshSeparator), 16-byte memory alignment (Align16 base class), trilinear texture filtering, mipmap generation |
| **Procedural Generation** | Heightmap terrain from texture data, UV-sphere generation, smooth normal averaging |
| **Device Driver Concepts** | Hardware abstraction via DXGI, debug layer integration (ID3D11Debug, ReportLiveDeviceObjects), shader debug flags |
| **Computer Architecture** | SIMD-aligned data structures, GPU/CPU memory transfer patterns, double-buffered rendering |

---

## Tech Stack

- **Language:** C++
- **Graphics API:** DirectX 11 (D3D11, DXGI, D3DCompiler)
- **Shader Language:** HLSL
- **Image Loading:** DirectXTex (TGA, DDS with mipmap support)
- **Platform:** Windows (Win32 API)

---

## File Structure

```
src/
  DXApp.cpp/h            -- Main application: D3D init, game loop, scene management
  ShaderBase.cpp/h        -- Abstract shader base: compilation, input layout, context binding
  ShaderColor.cpp/h       -- Simple color shader (position-only input)
  ShaderColorLight.cpp/h  -- Phong lighting shader (position + normal)
  ShaderTexture.cpp/h     -- Texture-mapped shader (position + UV)
  ShaderTextureLight.cpp/h-- Combined texture + Phong lighting shader
  Model.cpp/h             -- GPU mesh: buffer management, file loading, procedural models
  MeshSeparator.cpp/h     -- Triangle index reordering for multi-mesh batch rendering
  TerrainModel.cpp/h      -- Heightmap-based terrain generation with normal averaging
  SkyBox.cpp/h            -- 6-face skybox with cross-texture UV mapping
  Camera.cpp/h            -- Perspective camera: projection/view matrices, movement
  Texture.cpp/h           -- TGA/DDS loading, sampler state, mipmap generation
  GraphicObject_*.cpp/h   -- Rendering strategy objects (color, lit, textured)
  GameTimer.cpp/h         -- High-resolution frame timing (QueryPerformanceCounter)
  d3dUtil.h               -- Color constants, COM release macros
  AzulFileHdr.h           -- Custom 3D model file format specification
```
