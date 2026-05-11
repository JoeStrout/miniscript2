// IGCSet.h — non-generic abstract base for typed GCSets.
//
// GCManager holds an array of IGCSet* (one per GCSet) and dispatches
// PrepareForGC/MarkRetained/Sweep across all sets, plus Mark(idx) for
// per-Value mark calls. Mirrors C# IGCSet.

#ifndef IGCSET_H
#define IGCSET_H

#ifdef __cplusplus

namespace MiniScript {

class GCManager;  // forward

class IGCSet {
public:
    virtual ~IGCSet() = default;

    virtual void PrepareForGC() = 0;
    virtual void Mark(int idx, GCManager& gc) = 0;
    virtual void MarkRetained(GCManager& gc) = 0;
    virtual void Sweep(GCManager& gc) = 0;
    virtual int  LiveCount() const = 0;
};

} // namespace MiniScript

#endif // __cplusplus
#endif // IGCSET_H
