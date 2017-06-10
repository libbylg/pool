#ifndef __CPoolT_H_
#define __CPoolT_H_

template<typename ID_POLICY, typename DATA_POLICY, typename LOCK_POLICY, unsigned char GROUP_SIZE>
class CPoolT
{
    typedef ID_POLICY::ID       ID;
    typedef DATA_POLICY::DATA   DATA;
    typedef unsigned char       group_t;
    struct  ITEM
    {
        ITEM*   pNext;
        ITEM*   pPrev;
        ID      tID;
        DATA    tData;
        group_t siGroupID;
        void Reset(ID vID)
        {
            pNext       =   NULL;
            pPrev       =   NULL;
            ID_POLICY::Init(tID, vID);
            DATA_POLICY::Init(tData);
            siGroupID   =   0;
        }
        void Reuse()
        {
            ID_POLICY::Reuse(tID);
            DATA_POLICY::Init(tData);
            siGroupID   =   0;
        }
    };

    struct TQUEUE
    {
        ITEM*   pBgn;
        ITEM*   pEnd;
        size_t  siCount;
        group_t siGroupID;
        void Reset(group_t siGID)
        {
            pBgn        =   NULL;
            pEnd        =   NULL;
            siCount     =   0;
            siGroupID   =   siGID;
        }
        TQUEUE()
        {
            Reset(0);
        }
        void    Merage(ITEM* pItem)
        {
            if (NULL == pEnd)
            {
                pBgn    =   pItem;
                pEnd    =   pItem;
                return;
            }

            pItem->pNext        =   pEnd->pNext;
            pItem->pPrev        =   pEnd;
            pEnd->pNext         =   pItem;
            pItem->siGroupID    =   this->siGroupID
            return;
        }
        void    Split(ITEM* pItem)
        {
            if (NULL != pItem->pPrev)
            {
                pItem->pPrev->pNext =   pItem->pNext;
            }

            if (NULL != pItem->pNext)
            {
                pItem->pNext->pPrev =   pItem->pPrev;
            }

            if (pItem == pBgn)
            {
                pBgn = pItem->pNext;
            }

            if (pItem == pEnd)
            {
                pBgn = pItem->pPrev;
            }

            pItem->pNext    =   NULL;
            pItem->pPrev    =   NULL;
        }
        ITEM*   Pick()
        {
            if (NULL == pItem)
            {
                return (NULL);
            }

            ITEM* pItem = pBgn;
            Split(pItem);
            return (pItem);
        }
    };

    ITEM**  m_vItems;
    size_t  m_siItemCount;
    size_t  m_siInc;

    TQUEUE  m_vGroup[ + GROUP_SIZE];

public:
    CPoolT(DATA_POLICY& tIDPolicy, DATA_POLICY& tDataPolicy);
    ~CPoolT();
    int     Create(size_t siInit, size_t siInc);
    void    Destroy();
    int     Occupy(ID& tID, DATA tData, int iFromGroupID, int iToGroupID);
    DATA    Retrive(ID tID, int& iNewGroupID);
    int     MoveTo(ID tID, int iNewGroupID);
    int     WhereIs(ID tID);
    void    Enum(void* pParam, int iGrop, int (*pCallback)(ID tID, DATA tData, int iNewGroupID))const;
};

template<typename ID_POLICY, typename DATA_POLICY, typename LOCK_POLICY, unsigned char GROUP_SIZE>
CPoolT<ID_POLICY, DATA_POLICY, LOCK_POLICY, GROUP_SIZE>::CPoolT(DATA_POLICY& tIDPolicy, DATA_POLICY& tDataPolicy)
{
    m_vItems        =   NULL;
    m_siItemCount   =   0;
    m_siInc         =   0;

    for (size_t i = 0; i < sizeof(m_vGroup) / sizeof(m_vGroup[0]); i++)
    {
        m_vGroup[i].Reset(i);
    }
}

template<typename ID_POLICY, typename DATA_POLICY, typename LOCK_POLICY, unsigned char GROUP_SIZE>
CPoolT<ID_POLICY, DATA_POLICY, LOCK_POLICY, GROUP_SIZE>::~CPoolT()
{
    Destroy();
}

template<typename ID_POLICY, typename DATA_POLICY, typename LOCK_POLICY, unsigned char GROUP_SIZE>
int     CPoolT<ID_POLICY, DATA_POLICY, LOCK_POLICY, GROUP_SIZE>::Create(size_t siInit, size_t siInc)
{
    if ((siInit > ID_POLICY::GetMaxCount()) || (siInc > ID_POLICY::GetMaxCount()))
    {
        return (-1);
    }

    //siInit,siInc需要检测是否超过最大可能个数
    this->Increase(siInit);
    m_siInc = siInc;

    return (0);
}

template<typename ID_POLICY, typename DATA_POLICY, typename LOCK_POLICY, unsigned char GROUP_SIZE>
void    CPoolT<ID_POLICY, DATA_POLICY, LOCK_POLICY, GROUP_SIZE>::Destroy()
{
    if (NULL != m_vItems)
    {
        for (size_t i = 0; i < m_siItemCount; i++)
        {
            delete m_vItems[i];
        }

        delete[] m_vItems;
        m_vItems = NULL;
        m_siItemCount = 0;
    }

    m_siInc =   0;

    for (size_t i = 0; i < sizeof(m_vGroup) / sizeof(m_vGroup[0]); i++)
    {
        m_vGroup[i].Reset(i);
    }
}

template<typename ID_POLICY, typename DATA_POLICY, typename LOCK_POLICY, unsigned char GROUP_SIZE>
int     CPoolT<ID_POLICY, DATA_POLICY, LOCK_POLICY, GROUP_SIZE>::Occupy(ID& tID, DATA tData, int iToGroupID)
{
    if ((iToGroupID < 0) || (iToGroupID >= GROUP_SIZE))
    {
        return (-1);
    }
    
    //  找到GroupID的内部正确表述
    iToGroupID += 1;

    //  获取一个空闲项,如果获取不到就扩容后再获取
    ITEM* pItem = m_vGroup[0].Pick();
    if (NULL == pItem)
    {
        this->Increase(m_siInc);
        pItem = m_vGroup[0].Pick();
        if (NULL == pItem)
        {
            return (-1);
        }
    }

    pItem->tData = tData;
    tID = pItem->tID;

    m_vGroup[iToGroupID].Merage(pItem);
    return (0);
}

template<typename ID_POLICY, typename DATA_POLICY, typename LOCK_POLICY, unsigned char GROUP_SIZE>
int     CPoolT<ID_POLICY, DATA_POLICY, LOCK_POLICY, GROUP_SIZE>::Retrive(ID tID, int& iGroupID, DATA& tData)const
{
    ID tIndex = ID_POLICY::GetIndex(tID);
    if ((tIndex < 0) || (tIndex >= m_siItemCount))
    {
        return (-1);
    }

    ITEM* pItem = m_vItems[tIndex];
    if (NULL == pItem)
    {
        return (-1);
    }

    if (0 == pItem->siGroupID)  ///<    已经回收到空闲表
    {
        return (-1);
    }

    if (pItem->vID != tID)      ///<    已经被重用,ID值已经变化
    {
        return (-1);
    }

    tData       =   pItem->tData;
    iGroupID    =   pItem->siGroupID;
    return (0);
}

template<typename ID_POLICY, typename DATA_POLICY, typename LOCK_POLICY, unsigned char GROUP_SIZE>
int     CPoolT<ID_POLICY, DATA_POLICY, LOCK_POLICY, GROUP_SIZE>::MoveTo(ID tID, int iNewGroupID)
{
    ID tIndex = ID_POLICY::GetIndex(tID);
    if ((tIndex < 0) || (tIndex >= m_siItemCount))
    {
        return (-1);
    }

    ITEM* pItem = m_vItems[tIndex];
    if (NULL == pItem)
    {
        return (-1);
    }

    if (tID != pItem->tID)
    {
        return (-1);
    }

    if (pItem->siGroupID >= GROUP_SIZE)
    {
        return (-1);
    }

    if (iNewGroupID == pItem->siGroupID)
    {
        return (0);
    }

    if ((iNewGroupID < 0) || (iNewGroupID >= GROUP_SIZE))
    {
        return (-1);
    }

    TQUEUE& tOldQueue = m_vGroup[pItem->siGroupID];
    TQUEUE& tNewQueue = m_vGroup[iNewGroupID];
    tOldQueue.Split(pItem);
    tNewQueue.Merage(pItem);

    return (0);
}

template<typename ID_POLICY, typename DATA_POLICY, typename LOCK_POLICY, unsigned char GROUP_SIZE>
int     CPoolT<ID_POLICY, DATA_POLICY, LOCK_POLICY, GROUP_SIZE>::WhereIs(ID tID)
{
}

template<typename ID_POLICY, typename DATA_POLICY, typename LOCK_POLICY, unsigned char GROUP_SIZE>
void    CPoolT<ID_POLICY, DATA_POLICY, LOCK_POLICY, GROUP_SIZE>::Enum(void* pParam, int iGrop, int (*pCallback)(ID tID, DATA tData, int iGroupID))const
{
}

template<typename ID_POLICY, typename DATA_POLICY, typename LOCK_POLICY, unsigned char GROUP_SIZE>
void    CPoolT<ID_POLICY, DATA_POLICY, LOCK_POLICY, GROUP_SIZE>::Increase(size_t siInc)
{
    if (m_siItemCount + siInc > ID_POLICY::GetMaxCount())
    {
        siInc = ID_POLICY::GetMaxCount() - m_siItemCount;
    }

    if (0 == siInc)
    {
        return;
    }

    ITEM** pNewArray = new (ITEM*)[m_siItemCount + siInc];
    if (NULL != m_vItems)
    {
        memcpy(pNewArray, m_vItems, m_siItemCount);
    }

    for (int i = 0; i < siInc; i++)
    {
        ITEM* pItem = new ITEM;
        if (NULL == pItem)
        {
            pNewArray[m_siItemCount + i] = NULL;
            continue;
        }

        pItem->Reset(m_siItemCount + i)
        pNewArray[m_siItemCount + i] = pItem;
        m_vGroup[0].Merage(pItem);  ///<    添加到空闲队列
    }

    ITEM**  pToDelete = m_pItem;
    m_pItem = pNewArray;
    m_siItemCount += siInc;
    delete[] pToDelete;
}



#endif//__CPoolT_H_

