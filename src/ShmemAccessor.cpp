#include "ShmemAccessor.h"

// ShmemAccessor constructors
ShmemAccessor::ShmemAccessor(ShmemHeap *heapPtr) : heapPtr(heapPtr), path({}) {}

ShmemAccessor::ShmemAccessor(ShmemHeap *heapPtr, std::vector<KeyType> path) : heapPtr(heapPtr), path(path) {}

inline ShmemObj *ShmemAccessor::entrance() const
{
    return reinterpret_cast<ShmemObj *>(this->heapPtr->entrance());
}

void ShmemAccessor::setEntrance(size_t offset)
{
    this->heapPtr->entranceOffset() = offset;
}

void ShmemAccessor::setEntrance(ShmemObj *obj)
{
    this->heapPtr->entranceOffset() = reinterpret_cast<Byte *>(obj) - this->heapPtr->heapHead();
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

std::string ShmemAccessor::toString(int maxElements) const
{
    ShmemObj *obj, *prev;
    int resolvedDepth;
    resolvePath(prev, obj, resolvedDepth);
    if (resolvedDepth != path.size())
    {
        throw std::runtime_error("Cannot index <remaining index> on object" + ShmemObj::toString(obj));
    }
    return ShmemObj::toString(obj);
}