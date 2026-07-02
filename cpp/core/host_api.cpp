// host_api.cpp
//
// Backing definitions for the host-facing globals declared in miniscript.h.
//
// These are references onto the CoreIntrinsics fields that the `version`
// intrinsic reads, so host code can set MiniScript::hostVersion (etc.) directly,
// as it did in MiniScript 1.x.  Binding a reference only takes the address of
// the (statically-stored) target, so there is no static-initialization-order
// hazard: the target need not be constructed yet, and reads/writes happen later
// at run time.
//
#include "CoreIntrinsics.g.h"

namespace MiniScript {

String& hostName    = CoreIntrinsics::hostName;
String& hostInfo    = CoreIntrinsics::hostInfo;
String& hostVersion = CoreIntrinsics::hostVersion;

} // namespace MiniScript
