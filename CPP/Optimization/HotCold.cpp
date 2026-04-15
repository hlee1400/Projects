

//HotCold.cpp -- Cache - Optimized Hot / Cold Separation

// HotNode - 20 bytes, cache-line friendly for search iteration
//   pNext, pPrev, key, pCold
//
// ColdNode - 300+ bytes, only touched after a key match
//   key, chars, floats, Vect[4], Matrix[5], name[]

// Convert bloated linked list into separate hot/cold contiguous arrays
HotCold::HotCold(const Bloated* const pBloated)
{
    Node* pTemp = pBloated->GetListHead();
    size_t size = 0;
    while (pTemp != nullptr)
    {
        size++;
        pTemp = pTemp->pNext;
    }

    // Bulk-allocate contiguous arrays -- hot and cold data live in separate memory regions
    this->pHotHead = new HotNode[size];
    this->pColdHead = new ColdNode[size];

    HotNode* pTempHot = this->pHotHead;
    ColdNode* pTempCold = this->pColdHead;
    HotNode* pFirst = pTempHot;
    HotNode* pLast = pTempHot + size - 1;
    pTemp = pBloated->GetListHead();

    for (size_t i = 0; i < size; i++)
    {
        pTempHot->key = pTemp->key;       // Copy only the search key to hot node
        pTempHot->pCold = pTempCold;       // Cross-reference to cold payload
        *pTempCold = *pTemp;               // Bulk copy full payload to cold array
        pTempHot->pPrev = pTempHot - 1;   // Pointer arithmetic within contiguous array
        pTempHot->pNext = pTempHot + 1;
        pTempHot++;
        pTempCold++;
        pTemp++;
    }
    pLast->pNext = nullptr;
    pFirst->pPrev = nullptr;
}

HotCold::~HotCold()
{
    delete[] this->pHotHead;
    delete[] this->pColdHead;
}

// Search only touches 20-byte hot nodes in a contiguous array
// Cache lines are fully utilized instead of pulling in 300+ byte bloated nodes
bool HotCold::FindKey(int key, ColdNode*& pFoundColdNode, HotNode*& pFoundHotNode)
{
    HotNode* pTemp = this->GetHotHead();
    bool foundFlag = false;
    while (pTemp != nullptr)
    {
        if (pTemp->key == key)
        {
            pFoundColdNode = pTemp->pCold;  // Cold data only accessed on match
            pFoundHotNode = pTemp;
            foundFlag = true;
            break;
        }
        pTemp = pTemp->pNext;
    }
    return foundFlag;
}

// Performance (10K nodes, Release x86):
//   Bloated find: 8.62 ms
//   HotCold find: 0.55 ms  (15.7x faster)
//   Jedi    find: 0.20 ms  (40.1x faster)

