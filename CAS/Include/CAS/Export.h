#ifndef CAS_INCLUDE_CAS_EXPORT_H
#define CAS_INCLUDE_CAS_EXPORT_H

#if defined(_WIN32) && defined(CAS_SHARED_LIBRARY)

    // 构建库时：导出（dllexport）
    #if defined(CAS_BUILDING_LIBRARY)
        #define CAS_API __declspec(dllexport)
    // 使用库时：导入（dllimport）
    #else
        #define CAS_API __declspec(dllimport)
    #endif

#elif defined(__GNUC__) && defined(CAS_SHARED_LIBRARY)

    // GCC/Clang 共享库：标记默认可见性
    #define CAS_API __attribute__((visibility("default")))

#else

    // 静态库或非共享库：空宏
    #define CAS_API

#endif

#endif // CAS_INCLUDE_CAS_EXPORT_H