#ifndef CAS_INCLUDE_CAS_MACROS_H    
#define CAS_INCLUDE_CAS_MACROS_H

#include <cassert>
#include <iostream>
#include <cstdint>

#define CAS_ASSERT(x)\
    if(!(x))[[unlikely]]\
    {\
        std::cerr << __FILE__ << ":" << __LINE__ << " ASSERT FAILED : " << #x << "\nStackTrace: todo \n";\
        assert(x);\
    }

#define CAS_ASSERT2(x, w)\
    if(!(x))[[unlikely]]\
    {\
        std::cerr << __FILE__ << ":" << __LINE__ << " ASSERT FAILED : " << #x << " faild.[" << w << "].\nStackTrace: todo \n";\
        assert(x);\
    }

#define ASSERT_RETVAL(x, val) \
    do \
    { \
       if(x)[[likely]] break; \
       CAS_ASSERT(x); \
       return val; \
    } while (0)
    
#define ASSERT_RETVAL2(x, val, info) \
    do \
    { \
       if(x)[[likely]] break; \
       CAS_ASSERT2(x, info); \
       return val; \
    } while (0)

#define ASSERT_RETNONE(x) \
    do \
    { \
       if(x)[[likely]] break; \
       CAS_ASSERT(x); \
       return; \
    } while (0)

#define ASSERT_RETNONE2(x, info) \
    do \
    { \
       if(x)[[likely]] break; \
       CAS_ASSERT2(x, info); \
       return; \
    } while (0)

#define ASSERT_NOEFFECT(x) \
    do \
    { \
       if(x)[[likely]] break; \
       CAS_ASSERT(x); \
    } while (0)

#define ASSERT_NOEFFECT2(x, info) \
    do \
    { \
       if(x)[[likely]] break; \
       CAS_ASSERT2(x, info); \
    } while (0)

#define ASSERT_RETNONE(x) \
    do \
    { \
       if(x)[[likely]] break; \
       CAS_ASSERT(x); \
       return; \
    } while (0)

#define ASSERT_CONTINUE(x) \
    if (!(x)) [[unlikely]] { \
        CAS_ASSERT(x); \
        continue; \
    }

#define ASSERT_CONTINUE2(x, info) \
    if (!(x)) [[unlikely]] { \
        CAS_ASSERT2(x, info); \
        continue; \
    }

#define ASSERT_BREAK(x) \
    if (!(x)) [[unlikely]] { \
        CAS_ASSERT(x); \
        break; \
    }

#define ASSERT_BREAK2(x, info) \
    if (!(x)) [[unlikely]] { \
        CAS_ASSERT2(x, info); \
        break; \
    }

#define INVALID64 (~0ULL)
#define INVALID32 0xFFFFFFFF
#define INVALID16 0xFFFF
#define INVALID8 0xFF

#define MAX_U8 0xFF
#define MAX_U16 0xFFFF
#define MAX_U32 0xFFFFFFFF
#define MAX_U64 (~0ULL)

using u8 = uint8_t;
using s8 = int8_t;
using u16 = uint16_t;
using s16 = int16_t;
using u32 = uint32_t;
using s32 = int32_t;
using u64 = uint64_t;
using s64 = int64_t;

#endif