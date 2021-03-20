
// This is a generated file. Do not edit!

#ifndef O2G_COMPILER_DETECTION_H
#define O2G_COMPILER_DETECTION_H

#ifdef __cplusplus
# define O2G_COMPILER_IS_ARMCC 0
# define O2G_COMPILER_IS_AppleClang 0
# define O2G_COMPILER_IS_ARMClang 0
# define O2G_COMPILER_IS_Clang 0
# define O2G_COMPILER_IS_GNU 0

#if defined(__ARMCC_VERSION) && !defined(__clang__)
# undef O2G_COMPILER_IS_ARMCC
# define O2G_COMPILER_IS_ARMCC 1

#elif defined(__clang__) && defined(__apple_build_version__)
# undef O2G_COMPILER_IS_AppleClang
# define O2G_COMPILER_IS_AppleClang 1

#elif defined(__clang__) && defined(__ARMCOMPILER_VERSION)
# undef O2G_COMPILER_IS_ARMClang
# define O2G_COMPILER_IS_ARMClang 1

#elif defined(__clang__)
# undef O2G_COMPILER_IS_Clang
# define O2G_COMPILER_IS_Clang 1

#elif defined(__GNUC__) || defined(__GNUG__)
# undef O2G_COMPILER_IS_GNU
# define O2G_COMPILER_IS_GNU 1

#endif

#  if O2G_COMPILER_IS_GNU

# if defined(__GNUC__)
#  define O2G_COMPILER_VERSION_MAJOR (__GNUC__)
# else
#  define O2G_COMPILER_VERSION_MAJOR (__GNUG__)
# endif
# if defined(__GNUC_MINOR__)
#  define O2G_COMPILER_VERSION_MINOR (__GNUC_MINOR__)
# endif
# if defined(__GNUC_PATCHLEVEL__)
#  define O2G_COMPILER_VERSION_PATCH (__GNUC_PATCHLEVEL__)
# endif

#    if (__GNUC__ * 100 + __GNUC_MINOR__) >= 406 && (__cplusplus >= 201103L || (defined(__GXX_EXPERIMENTAL_CXX0X__) && __GXX_EXPERIMENTAL_CXX0X__))
#      define O2G_COMPILER_CXX_NULLPTR 1
#    else
#      define O2G_COMPILER_CXX_NULLPTR 0
#    endif

#    if (__GNUC__ * 100 + __GNUC_MINOR__) >= 407 && __cplusplus >= 201103L
#      define O2G_COMPILER_CXX_OVERRIDE 1
#    else
#      define O2G_COMPILER_CXX_OVERRIDE 0
#    endif

#    if (__GNUC__ * 100 + __GNUC_MINOR__) >= 404 && (__cplusplus >= 201103L || (defined(__GXX_EXPERIMENTAL_CXX0X__) && __GXX_EXPERIMENTAL_CXX0X__))
#      define O2G_COMPILER_CXX_STATIC_ASSERT 1
#    else
#      define O2G_COMPILER_CXX_STATIC_ASSERT 0
#    endif

#    if (__GNUC__ * 100 + __GNUC_MINOR__) >= 404 && (__cplusplus >= 201103L || (defined(__GXX_EXPERIMENTAL_CXX0X__) && __GXX_EXPERIMENTAL_CXX0X__))
#      define O2G_COMPILER_CXX_DELETED_FUNCTIONS 1
#    else
#      define O2G_COMPILER_CXX_DELETED_FUNCTIONS 0
#    endif

#    if (__GNUC__ * 100 + __GNUC_MINOR__) >= 406 && (__cplusplus >= 201103L || (defined(__GXX_EXPERIMENTAL_CXX0X__) && __GXX_EXPERIMENTAL_CXX0X__))
#      define O2G_COMPILER_CXX_NOEXCEPT 1
#    else
#      define O2G_COMPILER_CXX_NOEXCEPT 0
#    endif

#  elif O2G_COMPILER_IS_Clang

# define O2G_COMPILER_VERSION_MAJOR (__clang_major__)
# define O2G_COMPILER_VERSION_MINOR (__clang_minor__)
# define O2G_COMPILER_VERSION_PATCH (__clang_patchlevel__)

#    if ((__clang_major__ * 100) + __clang_minor__) >= 301 && __has_feature(cxx_nullptr)
#      define O2G_COMPILER_CXX_NULLPTR 1
#    else
#      define O2G_COMPILER_CXX_NULLPTR 0
#    endif

#    if ((__clang_major__ * 100) + __clang_minor__) >= 301 && __has_feature(cxx_override_control)
#      define O2G_COMPILER_CXX_OVERRIDE 1
#    else
#      define O2G_COMPILER_CXX_OVERRIDE 0
#    endif

#    if ((__clang_major__ * 100) + __clang_minor__) >= 301 && __has_feature(cxx_static_assert)
#      define O2G_COMPILER_CXX_STATIC_ASSERT 1
#    else
#      define O2G_COMPILER_CXX_STATIC_ASSERT 0
#    endif

#    if ((__clang_major__ * 100) + __clang_minor__) >= 301 && __has_feature(cxx_deleted_functions)
#      define O2G_COMPILER_CXX_DELETED_FUNCTIONS 1
#    else
#      define O2G_COMPILER_CXX_DELETED_FUNCTIONS 0
#    endif

#    if ((__clang_major__ * 100) + __clang_minor__) >= 301 && __has_feature(cxx_noexcept)
#      define O2G_COMPILER_CXX_NOEXCEPT 1
#    else
#      define O2G_COMPILER_CXX_NOEXCEPT 0
#    endif

#  endif

#  if !(defined(O2G_COMPILER_CXX_NULLPTR) && O2G_COMPILER_CXX_NULLPTR)
#    if O2G_COMPILER_IS_GNU
#      define nullptr __null
#    else
#      define nullptr 0
#    endif
#  endif

#  if !(defined(O2G_COMPILER_CXX_OVERRIDE) && O2G_COMPILER_CXX_OVERRIDE)
#    define override
#  endif

#  if !(defined(O2G_COMPILER_CXX_STATIC_ASSERT) && O2G_COMPILER_CXX_STATIC_ASSERT)
#    define O2G_STATIC_ASSERT_JOIN(X, Y) O2G_STATIC_ASSERT_JOIN_IMPL(X, Y)
#    define O2G_STATIC_ASSERT_JOIN_IMPL(X, Y) X##Y
template<bool> struct O2GStaticAssert;
template<> struct O2GStaticAssert<true>{};
#    define O2G_STATIC_ASSERT(X) enum { O2G_STATIC_ASSERT_JOIN(O2GStaticAssertEnum, __LINE__) = sizeof(O2GStaticAssert<X>) }
#    define O2G_STATIC_ASSERT_MSG(X, MSG) enum { O2G_STATIC_ASSERT_JOIN(O2GStaticAssertEnum, __LINE__) = sizeof(O2GStaticAssert<X>) }
#    define static_assert(X, MSG) O2G_STATIC_ASSERT_MSG(X, MSG)
#  endif


#  if defined(O2G_COMPILER_CXX_DELETED_FUNCTIONS) && O2G_COMPILER_CXX_DELETED_FUNCTIONS
#    define O2G_DELETED_FUNCTION = delete
#  else
#    define O2G_DELETED_FUNCTION 
#  endif


#  if !(defined(O2G_COMPILER_CXX_NOEXCEPT) && O2G_COMPILER_CXX_NOEXCEPT)
#    define noexcept 
#  endif

#endif

#if __cplusplus >= 201103L
#  define O2G_OPERATOR_EXPLICIT explicit
#else
#  define O2G_OPERATOR_EXPLICIT
#endif


#endif
