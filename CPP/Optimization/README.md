# Optimized C++ -- Performance Engineering & Systems Programming (CSC 461)

A comprehensive collection of C++ performance engineering assignments demonstrating low-level memory management, cache optimization, SIMD vectorization, custom heap allocators, expression templates, and algorithm optimization -- built with a focus on measurable speedups and hardware-aware programming.

## Project Overview

This coursework implements progressively advanced C++ optimization techniques across 20+ assignments, including:

- **Custom Heap Allocator** -- First-fit/next-fit memory allocation with block subdivision, coalescing (above/below merging), secret pointer backlinks, and bit-packed block headers encoding type, above-free flag, and size in a single `uint32_t`
- **Cache-Optimized Data Structures** -- Hot/cold data separation achieving **40x speedup** over naive linked list traversal by isolating search keys into cache-line-friendly contiguous arrays while deferring cold payload access
- **SIMD Intrinsics (SSE)** -- Hand-written `__m128` implementations of 4x4 matrix multiplication, vector-matrix products, and LERP using `_mm_set1_ps`, `_mm_mul_ps`, `_mm_add_ps` broadcast-and-accumulate patterns
- **Expression Templates & Proxy Objects** -- Compile-time elimination of temporary objects in vector addition chains (`A + B + C + D`) via proxy structs with implicit conversion operators, achieving **2.3x speedup**
- **Return Value Optimization (RVO)** -- Restructured operator overloads to enable compiler copy elision, measured at **2.0x speedup** over naive implementations
- **Implicit Conversion Prevention** -- Private template constructor/setter overloads to generate compile errors on implicit type narrowing without runtime overhead
- **Hybrid Sorting Algorithms** -- Merge sort on linked lists with insertion sort cutoff, achieving **381x speedup** over pure insertion sort with an additional **1.26x gain** from the hybrid threshold
- **Struct Alignment Analysis** -- Runtime padding byte detection via `0xAA` sentinel fill, visualizing compiler-inserted padding and computing waste ratios
- **4-Directional Grid Navigation** -- Linked node grid with North/South/East/West pointers supporting boustrophedonic traversal and arbitrary node deletion with full pointer fixup

## Architecture

```
Basics Series (Foundational C++)
    |
    |-- Basics1-2    (Linked lists, deep copy, Big Three)
    |-- Basics3      (Operator overloading, Big Four, bitfield Nyble class)
    |-- Basics5      (C-string algorithms without STL)
    |-- Basics6      (Template programming, 10 progressive problems)
    |-- Basics7      (Virtual functions, vtable behavior analysis)
    |-- Basics8      (STL containers: vector, list, set)
    |-- Basics9-10   (Advanced templates, inheritance hierarchies, leak detection)
    |-- Basics11     (Variadic functions, file format parsing)

Performance Assessments (Optimization & Systems)
    |
    |-- PA2          (Bitfield condiment flags, composite object ownership)
    |-- PA3          (Cache optimization: hot/cold separation, 9-40x speedup)
    |-- PA4-PA5      (Custom heap allocator: malloc/free/coalesce)
    |-- PA6          (RVO, expression templates, implicit conversion guards)
    |-- PA7          (SIMD intrinsics: matrix ops, LERP, layout optimization)
    |-- PA8          (Sorting: insertion, merge, hybrid combo sort)
    |-- PA9          (4-directional linked grid, boustrophedonic traversal)
    |-- PA10         (Expression template puzzle: Binary/AND/XOR/COMBO proxies)

Final Project
    |
    |-- GameParticles (Real-time particle system optimization with OpenGL)
```

## Memory Allocator Data Flow (PA4-PA5)

```
Mem::initialize()
    -> Placement-new Free block over raw heap memory
    -> Set Heap stats (used=0, free=1)
    -> Write secret pointer at block footer

Mem::malloc(size)
    -> Next-fit scan from pNextFit (wrap to pFreeHead if needed)
    -> If exact match: remove Free, convert to Used via placement-new
    -> If oversized: subdivide -> new Free block at (pFree + size + sizeof(Used))
    -> Set ABOVE_USED flag on block below via bit-packed mData
    -> Return (pUsed + 1) -- pointer past the header

Mem::free(data)
    -> Recover header: Used* = (Used*)data - 1
    -> Remove from Used list, update stats
    -> Coalesce: check below (IS_FREE) and above (IS_ABOVE_FREE via secret pointer)
    -> Merge adjacent Free blocks, fix linked list pointers
    -> Write secret pointer at new block's footer for future upward coalescing
```

## Highlighted Code Snippets

### 1. Cache-Optimized Hot/Cold Data Separation (PA3)

Splits a "bloated" linked list into separate hot (search key + pointers) and cold (payload) arrays. During key lookup, only 20-byte hot nodes touch the cache line instead of 300+ byte bloated nodes -- achieving a **40x speedup** in Release builds.

```cpp
// HotNode.h - Minimal cache-friendly search node (20 bytes)
class HotNode {
public:
    HotNode  *pNext;
    HotNode  *pPrev;
    int       key;       // The only field accessed during search
    ColdNode *pCold;     // Deferred access to full payload
};

// ColdNode.h - Full payload, only accessed after key match
class ColdNode {
public:
    int    key;
    char   aa, bb, cc, dd;
    float  x, y, z, w;
    Vect   A, B, C, D;
    Matrix MA, MB, MC, MD, ME;
    char   name[NAME_SIZE];   // ~300+ bytes total
};

// HotCold.cpp - Array-based allocation for contiguous memory layout
HotCold::HotCold(const Bloated* const pBloated)
{
    // Count nodes, then bulk-allocate contiguous arrays
    this->pHotHead = new HotNode[size];   // All hot data in one allocation
    this->pColdHead = new ColdNode[size]; // Cold data separate

    for (size_t i = 0; i < size; i++) {
        pTempHot->key = pTemp->key;
        pTempHot->pCold = pTempCold;      // Cross-reference to cold payload
        *pTempCold = *pTemp;
        pTempHot->pPrev = pTempHot - 1;   // Pointer arithmetic for linked list
        pTempHot->pNext = pTempHot + 1;
        pTempHot++; pTempCold++; pTemp++;
    }
}

// FindKey: only touches HotNode array -- cache-line efficient
bool HotCold::FindKey(int key, ColdNode *&pFoundColdNode, HotNode *&pFoundHotNode)
{
    HotNode *pTemp = this->GetHotHead();
    while (pTemp != nullptr) {
        if (pTemp->key == key) {          // Only cold data accessed on match
            pFoundColdNode = pTemp->pCold;
            pFoundHotNode = pTemp;
            return true;
        }
        pTemp = pTemp->pNext;
    }
    return false;
}
```

**Performance (10K nodes, Release x86):**
| Method | Find Time | Speedup |
|--------|-----------|---------|
| Bloated (naive) | 8.62 ms | 1.0x |
| HotCold | 0.55 ms | 15.7x |
| Jedi HotCold | 0.20 ms | **40.1x** |

### 2. Custom Heap Allocator with Block Coalescing (PA4-PA5)

A complete `malloc`/`free` implementation using next-fit allocation, block subdivision, and bidirectional coalescing via secret pointers and bit-packed headers.

```cpp
// Type.h - Bit-packed block header (framework-provided)
//  32-bit mData: bit(31)=Type, bit(30)=AboveFree, bits(29-0)=size (up to 1GB)
#define SET_FREE(x)       x |= 0x80000000;
#define IS_FREE(x)        (x & 0x80000000)
#define SET_ABOVE_FREE(x) x |= 0x40000000;
#define IS_ABOVE_FREE(x)  (x & 0x40000000)
#define GET_ALLOC_SIZE(x)  (x & 0x3FFFFFFF)

// Secret pointer embedded at the end of each free block's data region
struct SecretPtr { Free *pFree; };

// Mem.cpp - Next-fit allocation with block subdivision
void* Mem::malloc(const uint32_t _size)
{
    Free* pFree = this->poHeap->pNextFit;

    // Next-fit: scan from last allocation point, wrap if needed
    while (pFree != nullptr && GET_ALLOC_SIZE(pFree->mData) < _size)
        pFree = pFree->pNext;
    if (pFree == nullptr) {
        pFree = this->poHeap->pFreeHead;
        while (pFree != nullptr && GET_ALLOC_SIZE(pFree->mData) < _size)
            pFree = pFree->pNext;
    }

    if (pFree != nullptr) {
        if (GET_ALLOC_SIZE(pFree->mData) > _size) {
            // Subdivide: carve out requested size, create new Free block from remainder
            Free* pNextFree = (Free*)((uint32_t)pFree + _size + sizeof(Used));
            uint32_t remainingSize = GET_ALLOC_SIZE(pFree->mData) - _size - sizeof(Used);
            // ... remove old free, create new free, update stats
        }
        // Convert Free -> Used via placement new
        pUsed = new(pFree) Used(*pFree);
        // Set ABOVE_USED flag on the block below
        SET_ABOVE_USED(pNextBlock->mData);
        return pUsed + 1;  // Return pointer past header
    }
    return nullptr;
}

// Bidirectional coalescing: merge with adjacent free blocks
void Mem::privCoalesce(Used* pUsed)
{
    // Case 1: block below is free -- merge downward
    if (IS_FREE(pBelowTemp->mData) && pBelowTemp < (Used*)this->privGetTotalSize()) {
        size += sizeof(Free) + GET_ALLOC_SIZE(pBelow->mData);
        this->poHeap->removeFreeNode(pBelow);
    }
    // Case 2: block above is free -- recover via secret pointer, merge upward
    if (IS_ABOVE_FREE(pUsed->mData)) {
        pAbove = ((SecretPtr*)pUsed - 1)->pFree;  // Secret pointer lookup
        pUsed = (Used*)pAbove;
        size += sizeof(Free) + GET_ALLOC_SIZE(pAbove->mData);
        this->poHeap->removeFreeNode(pAbove);
    }
    // Placement-new merged Free block
    pNewFree = new(pUsed) Free(size);
    writeSecret(pNewFree);  // Embed secret pointer at footer for future coalescing
}
```

### 3. SIMD-Optimized Matrix Multiplication & LERP (PA7)

4x4 matrix multiply and vector LERP using SSE intrinsics with broadcast-and-accumulate pattern, processing 4 floats per instruction.

```cpp
// Vect_LERP_SIMD.h - Union for seamless scalar/SIMD access
class Vect_LERP_SIMD {
public:
    union {
        __m128 _m;                // SSE 128-bit register
        struct { float x, y, z, w; };  // Scalar access
    };
    static Vect_LERP_SIMD Lerp(const Vect_LERP_SIMD &a,
                                const Vect_LERP_SIMD &b, const float t);
};

// Vect_LERP_SIMD.cpp - LERP: a + (b - a) * t in 3 SIMD instructions
Vect_LERP_SIMD Vect_LERP_SIMD::Lerp(const Vect_LERP_SIMD &a,
                                      const Vect_LERP_SIMD &b, const float t)
{
    Vect_LERP_SIMD A, B;
    B._m = _mm_set1_ps(t);            // Broadcast t to all 4 lanes
    A._m = _mm_sub_ps(b._m, a._m);    // (b - a) for all 4 components
    A._m = _mm_mul_ps(A._m, B._m);    // * t
    A._m = _mm_add_ps(a._m, A._m);    // + a
    return A;
}

// Matrix_M_SIMD.cpp - 4x4 matrix multiply via broadcast-accumulate
Matrix_M_SIMD Matrix_M_SIMD::operator * (const Matrix_M_SIMD &mb) const
{
    Matrix_M_SIMD M;
    Vect_M_SIMD A, B, C, D;

    // Column 0: broadcast each row's x component, multiply by mb.row0
    A._m = _mm_set1_ps(this->v0.x);   // [x0, x0, x0, x0]
    B._m = _mm_set1_ps(this->v1.x);
    C._m = _mm_set1_ps(this->v2.x);
    D._m = _mm_set1_ps(this->v3.x);
    M.v0._m = _mm_mul_ps(A._m, mb.v0._m);
    M.v1._m = _mm_mul_ps(B._m, mb.v0._m);
    M.v2._m = _mm_mul_ps(C._m, mb.v0._m);
    M.v3._m = _mm_mul_ps(D._m, mb.v0._m);

    // Column 1: broadcast y, multiply-accumulate with mb.row1
    A._m = _mm_set1_ps(this->v0.y);
    // ... (repeat for y, z, w components)
    M.v0._m = _mm_add_ps(M.v0._m, A._m);  // Accumulate
    // ...

    return M;  // 4 rows x 4 columns = 16 broadcast-mul-add sequences
}

// Vect_vM_SIMD.cpp - Vector * Matrix with 4-lane parallel accumulation
Vect_vM_SIMD Vect_vM_SIMD::operator * (const Matrix_vM_SIMD &ma) const
{
    Vect_vM_SIMD A, B, C, D;
    A._m = _mm_set1_ps(this->x);       // Broadcast x across all lanes
    B._m = _mm_set1_ps(this->y);
    C._m = _mm_set1_ps(this->z);
    D._m = _mm_set1_ps(this->w);

    A._m = _mm_mul_ps(A._m, ma.v0._m); // x * row0
    B._m = _mm_mul_ps(B._m, ma.v1._m); // y * row1
    C._m = _mm_mul_ps(C._m, ma.v2._m); // z * row2
    D._m = _mm_mul_ps(D._m, ma.v3._m); // w * row3

    A._m = _mm_add_ps(A._m, B._m);     // Pairwise reduction
    C._m = _mm_add_ps(C._m, D._m);
    A._m = _mm_add_ps(A._m, C._m);     // Final sum
    return A;
}
```

### 4. Expression Template Proxy Objects (PA6)

Eliminates intermediate `Vect2D` temporaries in chained additions (`A + B + C + D`) by building a compile-time expression tree that evaluates in a single pass.

```cpp
// Proxy.h - Progressive proxy chain eliminates temporary allocations

struct VaddV {
    const Vect2D &v1;
    const Vect2D &v2;
    VaddV(const Vect2D &t1, const Vect2D &t2) : v1(t1), v2(t2) {};

    operator Vect2D() {  // Implicit conversion -- only materializes on assignment
        return Vect2D(v1.x + v2.x, v1.y + v2.y);
    }
};

inline VaddV operator + (const Vect2D &a1, const Vect2D &a2) {
    return VaddV(a1, a2);  // Returns proxy, not a Vect2D
};

struct VaddVaddV {
    const Vect2D &v1, &v2, &v3;
    VaddVaddV(const VaddV &t1, const Vect2D &t2)
        : v1(t1.v1), v2(t1.v2), v3(t2) {};

    operator Vect2D() {  // 3-way addition in one shot
        return Vect2D(v1.x + v2.x + v3.x, v1.y + v2.y + v3.y);
    }
};

struct VaddVaddVaddV {
    const Vect2D &v1, &v2, &v3, &v4;
    // ... chains up to 5 operands, each adding one reference
    operator Vect2D() {  // 4-way addition, zero intermediates
        return Vect2D(v1.x + v2.x + v3.x + v4.x,
                      v1.y + v2.y + v3.y + v4.y);
    }
};

// Usage: A + B + C + D
// Without proxy: 3 temporary Vect2D objects created and destroyed
// With proxy:    returns VaddVaddVaddV holding 4 references,
//                materializes one Vect2D on assignment -- 2.3x faster
```

### 5. Hybrid Merge-Insertion Sort on Linked Lists (PA8)

Recursive merge sort on doubly-linked lists with a configurable cutoff where small sublists fall through to insertion sort -- combining O(n log n) asymptotic behavior with insertion sort's low overhead on nearly-sorted small sequences.

```cpp
// SearchList.cpp - Merge sort with insertion sort cutoff

Node* SortedMerge(Node *first, Node *second)
{
    if (first == nullptr) return second;
    if (second == nullptr) return first;

    if (first->key < second->key) {
        first->pNext = SortedMerge(first->pNext, second);
        first->pNext->pPrev = first;
        first->pPrev = nullptr;
        return first;
    } else {
        second->pNext = SortedMerge(first, second->pNext);
        second->pNext->pPrev = second;
        second->pPrev = nullptr;
        return second;
    }
}

Node *SplitList(Node *source)
{
    Node *slow = source, *fast = source;
    while (fast->pNext && fast->pNext->pNext) {
        fast = fast->pNext->pNext;  // Tortoise-hare to find midpoint
        slow = slow->pNext;
    }
    Node *temp = slow->pNext;
    slow->pNext = nullptr;
    return temp;
}

// Hybrid: merge sort for large sublists, insertion sort below cutoff
Node* SearchList::MergeComboSort(Node *newHead, int start, int end, int CutoffLength)
{
    int size = end - start;
    if (size < CutoffLength) {
        InsertionSort(newHead);    // O(n^2) but fast for small n
        return newHead;
    }
    Node *second = SplitList(newHead);
    int mid = (start + end) / 2;
    newHead = MergeComboSort(newHead, start, mid, CutoffLength);
    second  = MergeComboSort(second, mid, end, CutoffLength);
    return SortedMerge(newHead, second);
}
```

**Performance (linked list sort):**
| Algorithm | Time | vs. Insertion Sort |
|-----------|------|-------------------|
| Insertion Sort | 1,202 ms | 1.0x |
| Merge Sort | 3.16 ms | 381x faster |
| Combo Sort (cutoff=96) | 2.74 ms | **438x faster** |

### 6. Implicit Conversion Prevention via Template Poisoning (PA6)

Private template overloads act as catch-all traps that generate compile errors when non-`float` arguments are passed, preventing silent implicit conversions (e.g., `int` to `float`) with zero runtime cost.

```cpp
// Implicit.h - Compile-time type safety without runtime overhead

class Vect {
public:
    Vect(const float inX, const float inY, const float inZ);  // Only valid ctor

    void setX(const float inX);
    void set(const float inX, const float inY, const float inZ);

private:
    float x, y, z;

    // Private template overloads -- poison pill for non-float arguments
    template <typename T> void setX(T);                        // Catches int, double, etc.
    template <typename T> void setY(T);
    template <typename T> void setZ(T);

    // Cover all mixed-type permutations of set()
    template <typename T> void set(T, T, T);
    template <typename T, typename U> void set(T, U, U);
    template <typename T, typename U> void set(T, T, U);
    template <typename T, typename U> void set(T, U, T);
    template <typename T, typename U, typename V> void set(T, U, V);

    // Same pattern for constructors
    template<typename T> Vect(T, T, T);
    template<typename T, typename U> Vect(T, T, U);
    template<typename T, typename U> Vect(T, U, U);
    template<typename T, typename U> Vect(T, U, T);
    template<typename T, typename U, typename V> Vect(T, U, V);
};

// Result: Vect(1.0f, 2.0f, 3.0f)  -- compiles
//         Vect(1, 2, 3)            -- error: private template ctor
//         v.setX(5)                -- error: private template setter
```

## Showcased Source Files & Skill Alignment

### HotCold.cpp + HotNode.h + ColdNode.h (PA3)

| Skill Area | How This File Demonstrates It |
|-----------|------------------------------|
| Cache Architecture | Separates hot data (20-byte search nodes) from cold data (300+ byte payloads) into contiguous arrays, ensuring search-path iterations touch minimal cache lines -- the same data-oriented design pattern used in game engines and database query executors. |
| Memory Layout | Array-based allocation (`new HotNode[size]`) guarantees contiguous memory for sequential access, with pointer arithmetic (`pTempHot + 1`) for O(1) next-node traversal within the array. |
| Performance Engineering | Measurable 9-40x speedup across Debug/Release builds, demonstrating that algorithmic complexity alone doesn't determine performance -- memory access patterns dominate. |

### Mem.cpp + Free.h + Used.h (PA4-PA5)

| Skill Area | How This File Demonstrates It |
|-----------|------------------------------|
| Systems Programming | Implements `malloc`/`free` from scratch: raw memory partitioning, placement new for type-punning between Free/Used headers, and pointer arithmetic for block traversal. |
| Bit Manipulation | Single `uint32_t` encodes block type (bit 31), above-free flag (bit 30), and allocation size (bits 0-29) via bitwise masks -- the same technique used in real allocators like jemalloc and dlmalloc. |
| Fragmentation Prevention | Bidirectional coalescing merges adjacent free blocks using a secret pointer embedded at each free block's footer, enabling O(1) upward neighbor discovery without scanning. |

### Matrix_M_SIMD.cpp + Vect_LERP_SIMD.cpp (PA7)

| Skill Area | How This File Demonstrates It |
|-----------|------------------------------|
| SIMD / Vectorization | Direct use of SSE intrinsics (`_mm_set1_ps`, `_mm_mul_ps`, `_mm_add_ps`) to process 4 floats per instruction, with broadcast-and-accumulate patterns for matrix-vector products. |
| Data Layout Awareness | Anonymous union overlays `__m128` with scalar `{x,y,z,w}` struct, allowing seamless transition between SIMD operations and element-wise access without type-punning UB. |
| Numerical Computing | LERP, matrix multiply, and vector-matrix multiply are foundational operations in graphics, physics simulation, and animation -- implemented at the intrinsic level rather than relying on auto-vectorization. |

### Proxy.h (PA6)

| Skill Area | How This File Demonstrates It |
|-----------|------------------------------|
| Template Metaprogramming | Expression template chain (`VaddV` -> `VaddVaddV` -> `VaddVaddVaddV`) builds a compile-time type tree that collapses N additions into a single materialization on assignment. |
| Copy Elision Awareness | Proxy objects hold `const Vect2D&` references -- no copies until the implicit `operator Vect2D()` fires at the assignment boundary, leveraging RVO for the final return. |
| Zero-Overhead Abstraction | The proxy pattern adds zero runtime cost -- the compiler inlines the conversion operator, producing the same assembly as hand-written `Vect2D(a.x+b.x+c.x+d.x, a.y+b.y+c.y+d.y)`. |

## Skills Demonstrated

| Area | Details |
|------|---------|
| Memory Management | Custom heap allocator (malloc/free/coalesce), placement new, block subdivision, secret pointer technique, bit-packed headers, next-fit allocation strategy |
| Cache Optimization | Hot/cold data separation, contiguous array allocation for cache-line efficiency, data-oriented design, 9-40x measured speedups |
| SIMD / Vectorization | SSE intrinsics (`__m128`), broadcast-accumulate matrix multiply, vector LERP, row-major vs. column-major layout optimization |
| Template Metaprogramming | Expression templates, proxy objects for lazy evaluation, template poisoning for implicit conversion prevention, generic algorithm design |
| Algorithm Optimization | Merge sort on linked lists, hybrid combo sort with insertion sort cutoff, tortoise-hare list splitting, 381x measured speedup |
| Low-Level C++ | Operator overloading, Big Four (ctor/copy/assign/dtor), deep copy semantics, virtual functions/vtables, RAII, COM-style lifecycle patterns |
| Bit Manipulation | Bit-packed allocation headers (type/flag/size in 32 bits), bitfield condiment flags, bitwise operators (AND, XOR) with proxy evaluation |
| Data Structures | Singly/doubly linked lists, 4-directional grid nodes, custom heap free/used lists, boustrophedonic traversal, composite object graphs |
| Struct Layout | Alignment analysis with padding visualization, struct packing optimization, anonymous unions for SIMD/scalar overlay |
| Performance Profiling | Debug vs. Release benchmarking, ratio-based grading thresholds, stress testing with timing validation |

## Performance Summary

| Assignment | Technique | Measured Speedup |
|-----------|-----------|-----------------|
| PA3 | Hot/cold data separation | **9-40x** (Debug-Release) |
| PA6 | Return Value Optimization | **2.0x** |
| PA6 | Expression template proxies | **2.3x** |
| PA7 | SIMD matrix multiply | SSE intrinsic-level implementation |
| PA7 | SIMD vector LERP | SSE intrinsic-level implementation |
| PA8 | Merge vs. insertion sort | **381x** |
| PA8 | Hybrid combo sort vs. merge | **1.26x** additional gain |
| PA5 | Heap allocator stress test | 305ms (target: <450ms) |

## Tech Stack

- **Language:** C++ (C++11/14)
- **Compiler:** MSVC 19.37 (Visual Studio 2022)
- **Platform:** Windows (x86 Debug and Release)
- **SIMD:** SSE / SSE4.1 (`xmmintrin.h`, `smmintrin.h`)
- **Graphics (GameParticles):** OpenGL
- **Testing:** Custom unit test framework with memory leak tracking
- **Build:** Visual Studio solution (.sln) with per-assignment project configurations

## File Structure

```
Basics1/          -- Linked list fundamentals (AddToFront, Remove, AddToEnd)
Basics2/          -- Deep copy semantics (copy ctor, assignment operator)
Basics3/          -- Operator overloading, Big Four, Nyble bitfield class
Basics5/          -- C-string algorithms (compare, copy, sort without STL)
Basics6/          -- Template programming (10 progressive problems A-J)
Basics7/          -- Virtual functions and vtable behavior analysis
Basics8/          -- STL containers (vector, list, set, Vect3D)
Basics9/          -- Advanced template specialization
Basics10/         -- Inheritance hierarchies, memory leak detection
Basics11/         -- Variadic functions (va_list), file format parsing

PA2/              -- Composite objects: HotDog/Order/Stand with bitfield condiments
PA3/              -- Cache optimization: hot/cold separation (9-40x speedup)
PA4/              -- Custom heap allocator: malloc, free, block management
PA5/              -- Advanced allocator: coalescing, secret pointers, stress test
PA6/
  RVO/            -- Return Value Optimization (2.0x speedup)
  Proxy/          -- Expression template proxy objects (2.3x speedup)
  Implicit/       -- Template poisoning for implicit conversion prevention
PA7/              -- SIMD intrinsics: matrix multiply, vect*matrix, LERP
PA8/              -- Sorting algorithms: insertion, merge, hybrid combo sort
PA9/              -- 4-directional grid: boustrophedonic traversal, node deletion
PA10/             -- Expression template puzzle: Binary/AND/XOR/COMBO proxies
PA11/             -- Programming assessment (4 standalone problems)

GameParticles/    -- Real-time particle system optimization (OpenGL)
```
