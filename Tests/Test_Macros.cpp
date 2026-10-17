#include <CAS/Macros.h>
#include <iostream>
#include <limits>
#include <type_traits>

static_assert(sizeof(u8) == 1 && std::is_unsigned_v<u8>);
static_assert(sizeof(s8) == 1 && std::is_signed_v<s8>);

static_assert(sizeof(u16) == 2 && std::is_unsigned_v<u16>);
static_assert(sizeof(s16) == 2 && std::is_signed_v<s16>);

static_assert(sizeof(u32) == 4 && std::is_unsigned_v<u32>);
static_assert(sizeof(s32) == 4 && std::is_signed_v<s32>);

static_assert(sizeof(u64) == 8 && std::is_unsigned_v<u64>);
static_assert(sizeof(s64) == 8 && std::is_signed_v<s64>);

static_assert(INVALID8  == MAX_U8);
static_assert(INVALID16 == MAX_U16);
static_assert(INVALID32 == MAX_U32);
static_assert(INVALID64 == MAX_U64);

static_assert(MAX_U8  == std::numeric_limits<u8>::max());
static_assert(MAX_U16 == std::numeric_limits<u16>::max());
static_assert(MAX_U32 == std::numeric_limits<u32>::max());
static_assert(MAX_U64 == std::numeric_limits<u64>::max());

[[nodiscard]] int test_assert_retval()
{
    ASSERT_RETVAL(false, -1);
    return 0;
}

[[nodiscard]] int test_assert_retval2()
{
    ASSERT_RETVAL2(false, -2, "This should not trigger an assertion.");
    return 0;
}

void test_assert_macros()
{
    ASSERT_NOEFFECT2(true, "This should not trigger an assertion.");

    int never_reached_condition = 5;
    int i = 0;

    // 1. ASSERT_CONTINUE
    for (i = 0; i < 10; ++i) {
        ASSERT_CONTINUE(i != never_reached_condition);
        std::cout << "AAA-------: " << i << std::endl;
    }

    // 2. ASSERT_CONTINUE2
    for (i = 0; i < 10; ++i) {
        ASSERT_CONTINUE2(i != never_reached_condition,
                         "loop index should not be " << i);
        std::cout << "B-------: " << i << std::endl;
    }

    // 3. ASSERT_BREAK
    for (i = 0; i < 10; ++i) {
        ASSERT_BREAK(i != never_reached_condition);
        std::cout << "BBB-------: " << i << std::endl;
    }

    // 4. ASSERT_BREAK2
    for (i = 0; i < 10; ++i) {
        ASSERT_BREAK2(i != never_reached_condition,
                      "loop index should not be " << i);
        std::cout << "D-------: " << i << std::endl;
    }

    // 5. ASSERT_NOEFFECT
    for (i = 0; i < 10; ++i) {
        ASSERT_NOEFFECT(i != never_reached_condition);
        std::cout << "CCC-------: " << i << std::endl;
    }

    // 6. ASSERT_NOEFFECT2
    for (i = 0; i < 10; ++i) {
        ASSERT_NOEFFECT2(i != never_reached_condition,
                         "loop index should not be " << i);
        std::cout << "F: " << std::endl;
    }
}

int main()
{
#ifdef CAS_DEBUG
    std::cout << "This program is compiled in debug mode. "
                 "Assert will terminate the program on failure."
              << std::endl;
#endif

#ifdef NDEBUG
    std::cout << "This program is compiled in release mode. "
                 "Assert will do nothing on failure."
              << std::endl;
#endif

    test_assert_macros();

    auto retval1 = test_assert_retval();
    std::cout << "test_assert_retval() returned: " << retval1 << std::endl;

    auto retval2 = test_assert_retval2();
    std::cout << "test_assert_retval2() returned: " << retval2 << std::endl;

    return EXIT_SUCCESS;
}