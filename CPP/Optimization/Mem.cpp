//Mem.cpp -- Custom Heap Allocator

// Secret pointer embedded at footer of each free block for O(1) upward coalescing
struct SecretPtr
{
    Free* pFree;
};

void Mem::initialize()
{
    Heap* pHeap = this->poHeap;
    Heap* pA = pHeap + 1;
    Free* pB = (Free*)pA + 1;
    uint32_t addrB = (uint32_t)pB;
    uint32_t addrC = (uint32_t)pHeap + Mem::TotalSize;
    uint32_t size = addrC - addrB;
    Free* pFree = new(pA) Free(size);

    pHeap->pUsedHead = nullptr;
    pHeap->currNumUsedBlocks = 0;
    pHeap->currUsedMem = 0;

    pHeap->addFreeNode(pFree);
    pHeap->addFreeStats(pFree);
    pHeap->setNextFit(pFree);
    this->writeSecret(pFree);
}

void* Mem::malloc(const uint32_t _size)
{
    Free* pFree = this->poHeap->pNextFit;
    Used* pUsed = nullptr;

    // Next-fit: scan from last allocation point
    while (pFree != nullptr && GET_ALLOC_SIZE(pFree->mData) < _size)
        pFree = pFree->pNext;

    // Wrap around to head if no fit found after pNextFit
    if (pFree == nullptr)
    {
        pFree = this->poHeap->pFreeHead;
        while (pFree != nullptr && GET_ALLOC_SIZE(pFree->mData) < _size)
            pFree = pFree->pNext;
    }

    if (pFree != nullptr)
    {
        // Perfect fit: convert entire free block to used
        if (GET_ALLOC_SIZE(pFree->mData) == _size)
        {
            this->poHeap->removeFreeNode(pFree);
            this->poHeap->removeFreeStats(pFree);
            this->poHeap->setNextFit(this->poHeap->pFreeHead);
        }
        // Oversized: subdivide -- carve requested size, create new free from remainder
        else if (GET_ALLOC_SIZE(pFree->mData) > _size)
        {
            Free* pNextFree = (Free*)((uint32_t)pFree + _size + sizeof(Used));
            uint32_t remainingSize = GET_ALLOC_SIZE(pFree->mData) - _size - sizeof(Used);
            this->poHeap->removeFreeNode(pFree);
            this->poHeap->removeFreeStats(pFree);
            Free* pNewFree = new(pNextFree) Free(remainingSize);
            this->poHeap->addFreeNode(pNewFree);
            this->poHeap->addFreeStats(pNewFree);
            this->poHeap->setNextFit(pNewFree);
            SET_ALLOC_SIZE(pFree->mData, _size);
            this->writeSecret(pNewFree);
        }

        // Convert free block to used via placement new
        pUsed = new(pFree) Used(*pFree);
        this->poHeap->addUsedNode(pUsed);
        this->poHeap->addUsedStats(pUsed);

        // Set above-used flag on the block below
        Used* pNextBlock = (Used*)((uint32_t)pUsed + GET_ALLOC_SIZE(pUsed->mData) + sizeof(Used));
        if (pNextBlock < (Used*)this->privGetTotalSize())
            SET_ABOVE_USED(pNextBlock->mData);

        return pUsed + 1; // Return pointer past the header
    }
    return pUsed;
}

void Mem::free(void* const data)
{
    Used* pUsed = (Used*)data - 1; // Recover header from user pointer
    this->poHeap->removeUsedNode(pUsed);
    this->poHeap->removeUsedStats(pUsed);
    this->privCoalesce(pUsed);

    // Update above-free flag on block below
    Used* pNextBlock = (Used*)((uint32_t)pUsed + GET_ALLOC_SIZE(pUsed->mData) + sizeof(Used));
    if (pNextBlock < (Used*)this->privGetTotalSize())
        SET_ABOVE_FREE(pNextBlock->mData);
}

// Bidirectional coalescing: merge with adjacent free blocks above and below
void Mem::privCoalesce(Used* pUsed)
{
    uint32_t size = GET_ALLOC_SIZE(pUsed->mData);
    Free* pPrevTemp = nullptr;
    Free* pNextTemp = nullptr;
    Free* pBelow = nullptr;
    Used* pBelowTemp = (Used*)((uint32_t)pUsed + GET_ALLOC_SIZE(pUsed->mData) + sizeof(Used));
    Free* pAbove = nullptr;
    Free* pNewFree = nullptr;

    // Case 1: block below is free -- merge downward
    if (IS_FREE(pBelowTemp->mData) && pBelowTemp < (Used*)this->privGetTotalSize())
    {
        pBelow = (Free*)pBelowTemp;
        pPrevTemp = pBelow->pPrev;
        pNextTemp = pBelow->pNext;
        size += sizeof(Free) + GET_ALLOC_SIZE(pBelow->mData);
        this->poHeap->removeFreeNode(pBelow);
        this->poHeap->removeFreeStats(pBelow);
    }

    // Case 2: block above is free -- recover via secret pointer backlink
    if (IS_ABOVE_FREE(pUsed->mData))
    {
        pAbove = ((SecretPtr*)pUsed - 1)->pFree; // O(1) upward neighbor lookup
        pUsed = (Used*)pAbove;
        pPrevTemp = pAbove->pPrev;
        pNextTemp = pAbove->pNext;
        size += sizeof(Free) + GET_ALLOC_SIZE(pAbove->mData);
        this->poHeap->removeFreeNode(pAbove);
        this->poHeap->removeFreeStats(pAbove);
    }

    // Place merged free block and re-insert into free list
    pNewFree = new(pUsed) Free(size);
    this->poHeap->addFreeStats(pNewFree);
    writeSecret(pNewFree);

    if (pBelow == nullptr && pAbove == nullptr)
        this->poHeap->addFreeNode(pNewFree);
    else if (this->poHeap->pFreeHead == nullptr)
    {
        this->poHeap->pFreeHead = pNewFree;
        this->poHeap->setNextFit(pNewFree);
    }
    else if (pNewFree < this->poHeap->pFreeHead)
    {
        this->poHeap->pFreeHead = pNewFree;
        pNewFree->pNext = pNextTemp;
        pNextTemp->pPrev = pNewFree;
        this->poHeap->setNextFit(pNewFree->pNext);
    }
    else
    {
        pPrevTemp->pNext = pNewFree;
        pNewFree->pPrev = pPrevTemp;
        pNewFree->pNext = pNextTemp;
    }
}

// Embed a secret pointer at the footer of a free block
// so the block below can find its upper neighbor in O(1)
void Mem::writeSecret(Free* pFree)
{
    Used* pBelow = (Used*)((uint32_t)pFree + GET_ALLOC_SIZE(pFree->mData) + sizeof(Free));
    if (IS_USED(pBelow->mData))
        SET_ABOVE_FREE(pBelow->mData);
    SecretPtr* pSecret = (SecretPtr*)pBelow - 1;
    pSecret->pFree = pFree;
}
