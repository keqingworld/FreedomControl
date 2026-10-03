// Intentionally repeatable: do NOT add pragma once or an include guard.
// Include after Windows SDK headers. Use GetObjectA/GetObjectW explicitly
// for GDI; the encoding-neutral macro must not rename RE::GetObject calls.
#ifdef GetObject
#    undef GetObject
#endif
