#ifndef PACK_H
#define PACK_H

#if defined(_MSC_VER)
    #define PACK_PUSH    __pragma(pack(push, 1))
    #define PACK_POP     __pragma(pack(pop))
#else
    #define PACK_PUSH    _Pragma("pack(push, 1)")
    #define PACK_POP     _Pragma("pack(pop)")
#endif

#endif
