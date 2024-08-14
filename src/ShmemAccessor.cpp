#include "ShmemObj.h"

// ShmemAccessor constructors
ShmemAccessor::ShmemAccessor(ShmemHeap *heapPtr) : heapPtr(heapPtr), path({}) {}

ShmemAccessor::ShmemAccessor(ShmemHeap *heapPtr, std::vector<KeyType> path) : heapPtr(heapPtr), path(path) {}

inline ShmemAccessor::ShmemObj *ShmemAccessor::entrance() const
{
    size_t offset = entranceOffset();
    if (offset == NPtr)
    {
        return nullptr;
    }
    return reinterpret_cast<ShmemObj *>(heapPtr->heapHead() + offset);
}

inline void ShmemAccessor::setEntrance(ShmemObj *obj)
{
    if (obj == nullptr)
        entranceOffset() = NPtr;
    else
        entranceOffset() = reinterpret_cast<Byte *>(obj) - heapPtr->heapHead();
}
// ShmemAccessor ShmemAccessor::operator[](KeyType index) const
// {
//     std::vector<KeyType> newPath(this->path);
//     newPath.push_back(index);
//     return ShmemAccessor(this->heapPtr, newPath);
// }

// protected methods
inline size_t &ShmemAccessor::entranceOffset()
{
    return reinterpret_cast<size_t *>(this->heapPtr->staticSpaceHead())[3];
}

inline size_t ShmemAccessor::entranceOffset() const
{
    return reinterpret_cast<size_t *>(this->heapPtr->staticSpaceHead())[3];
}

void ShmemAccessor::resolvePath(ShmemObj *&prevObj, ShmemObj *&obj, int &resolvedDepth) const
{
    ShmemObj *current = this->entrance();
    ShmemObj *prev = nullptr;
    int resolveDepth = 0;
    for (resolveDepth = 0; resolveDepth < path.size(); resolveDepth++)
    {
        KeyType pathElement = path[resolveDepth];
        try
        {
            if (isPrimitive(current->type))
            { // This mean there's an additional index on a primitive
                break;
            }
            else if (current->type == Dict)
            {
                ShmemDict *currentDict = static_cast<ShmemDict *>(current);

                prev = current;
                current = currentDict->get(pathElement);
            }
            else if (current->type == List)
            {
                ShmemList *currentList = static_cast<ShmemList *>(current);
                if (std::holds_alternative<std::string>(pathElement))
                {
                    throw std::runtime_error("Cannot index <string> on List");
                }

                prev = current;
                current = currentList->at(std::get<int>(pathElement));
            }
        }
        catch (IndexError &e)
        {
            break;
        }
    }
    prevObj = prev;
    obj = current;
    resolvedDepth = resolveDepth;
    return;
}
