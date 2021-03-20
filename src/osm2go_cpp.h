
// This is a generated file. Do not edit!

#ifndef O2G_COMPILER_DETECTION_H
#define O2G_COMPILER_DETECTION_H

#ifdef __cplusplus
# define O2G_COMPILER_IS_Comeau 0
# define O2G_COMPILER_IS_Intel 0
# define O2G_COMPILER_IS_PathScale 0
# define O2G_COMPILER_IS_Embarcadero 0
# define O2G_COMPILER_IS_Borland 0
# define O2G_COMPILER_IS_Watcom 0
# define O2G_COMPILER_IS_OpenWatcom 0
# define O2G_COMPILER_IS_SunPro 0
# define O2G_COMPILER_IS_HP 0
# define O2G_COMPILER_IS_Compaq 0
# define O2G_COMPILER_IS_zOS 0
# define O2G_COMPILER_IS_XLClang 0
# define O2G_COMPILER_IS_XL 0
# define O2G_COMPILER_IS_VisualAge 0
# define O2G_COMPILER_IS_PGI 0
# define O2G_COMPILER_IS_Cray 0
# define O2G_COMPILER_IS_TI 0
# define O2G_COMPILER_IS_Fujitsu 0
# define O2G_COMPILER_IS_GHS 0
# define O2G_COMPILER_IS_SCO 0
# define O2G_COMPILER_IS_ARMCC 0
# define O2G_COMPILER_IS_AppleClang 0
# define O2G_COMPILER_IS_ARMClang 0
# define O2G_COMPILER_IS_Clang 0
# define O2G_COMPILER_IS_GNU 0
# define O2G_COMPILER_IS_MSVC 0
# define O2G_COMPILER_IS_ADSP 0
# define O2G_COMPILER_IS_IAR 0
# define O2G_COMPILER_IS_MIPSpro 0

#if defined(__COMO__)
# undef O2G_COMPILER_IS_Comeau
# define O2G_COMPILER_IS_Comeau 1

#elif defined(__INTEL_COMPILER) || defined(__ICC)
# undef O2G_COMPILER_IS_Intel
# define O2G_COMPILER_IS_Intel 1

#elif defined(__PATHCC__)
# undef O2G_COMPILER_IS_PathScale
# define O2G_COMPILER_IS_PathScale 1

#elif defined(__BORLANDC__) && defined(__CODEGEARC_VERSION__)
# undef O2G_COMPILER_IS_Embarcadero
# define O2G_COMPILER_IS_Embarcadero 1

#elif defined(__BORLANDC__)
# undef O2G_COMPILER_IS_Borland
# define O2G_COMPILER_IS_Borland 1

#elif defined(__WATCOMC__) && __WATCOMC__ < 1200
# undef O2G_COMPILER_IS_Watcom
# define O2G_COMPILER_IS_Watcom 1

#elif defined(__WATCOMC__)
# undef O2G_COMPILER_IS_OpenWatcom
# define O2G_COMPILER_IS_OpenWatcom 1

#elif defined(__SUNPRO_CC)
# undef O2G_COMPILER_IS_SunPro
# define O2G_COMPILER_IS_SunPro 1

#elif defined(__HP_aCC)
# undef O2G_COMPILER_IS_HP
# define O2G_COMPILER_IS_HP 1

#elif defined(__DECCXX)
# undef O2G_COMPILER_IS_Compaq
# define O2G_COMPILER_IS_Compaq 1

#elif defined(__IBMCPP__) && defined(__COMPILER_VER__)
# undef O2G_COMPILER_IS_zOS
# define O2G_COMPILER_IS_zOS 1

#elif defined(__ibmxl__) && defined(__clang__)
# undef O2G_COMPILER_IS_XLClang
# define O2G_COMPILER_IS_XLClang 1

#elif defined(__IBMCPP__) && !defined(__COMPILER_VER__) && __IBMCPP__ >= 800
# undef O2G_COMPILER_IS_XL
# define O2G_COMPILER_IS_XL 1

#elif defined(__IBMCPP__) && !defined(__COMPILER_VER__) && __IBMCPP__ < 800
# undef O2G_COMPILER_IS_VisualAge
# define O2G_COMPILER_IS_VisualAge 1

#elif defined(__PGI)
# undef O2G_COMPILER_IS_PGI
# define O2G_COMPILER_IS_PGI 1

#elif defined(_CRAYC)
# undef O2G_COMPILER_IS_Cray
# define O2G_COMPILER_IS_Cray 1

#elif defined(__TI_COMPILER_VERSION__)
# undef O2G_COMPILER_IS_TI
# define O2G_COMPILER_IS_TI 1

#elif defined(__FUJITSU) || defined(__FCC_VERSION) || defined(__fcc_version)
# undef O2G_COMPILER_IS_Fujitsu
# define O2G_COMPILER_IS_Fujitsu 1

#elif defined(__ghs__)
# undef O2G_COMPILER_IS_GHS
# define O2G_COMPILER_IS_GHS 1

#elif defined(__SCO_VERSION__)
# undef O2G_COMPILER_IS_SCO
# define O2G_COMPILER_IS_SCO 1

#elif defined(__ARMCC_VERSION) && !defined(__clang__)
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

#elif defined(_MSC_VER)
# undef O2G_COMPILER_IS_MSVC
# define O2G_COMPILER_IS_MSVC 1

#elif defined(__VISUALDSPVERSION__) || defined(__ADSPBLACKFIN__) || defined(__ADSPTS__) || defined(__ADSP21000__)
# undef O2G_COMPILER_IS_ADSP
# define O2G_COMPILER_IS_ADSP 1

#elif defined(__IAR_SYSTEMS_ICC__) || defined(__IAR_SYSTEMS_ICC)
# undef O2G_COMPILER_IS_IAR
# define O2G_COMPILER_IS_IAR 1


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
# if defined(_MSC_VER)
   /* _MSC_VER = VVRR */
#  define O2G_SIMULATE_VERSION_MAJOR (_MSC_VER / 100)
#  define O2G_SIMULATE_VERSION_MINOR (_MSC_VER % 100)
# endif

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

#  if defined(O2G_COMPILER_CXX_NULLPTR) && O2G_COMPILER_CXX_NULLPTR
#    define O2G_NULLPTR nullptr
#  elif O2G_COMPILER_IS_GNU
#    define O2G_NULLPTR __null
#  else
#    define O2G_NULLPTR 0
#  endif


#  if defined(O2G_COMPILER_CXX_OVERRIDE) && O2G_COMPILER_CXX_OVERRIDE
#    define O2G_OVERRIDE override
#  else
#    define O2G_OVERRIDE 
#  endif

#  if defined(O2G_COMPILER_CXX_STATIC_ASSERT) && O2G_COMPILER_CXX_STATIC_ASSERT
#    define O2G_STATIC_ASSERT(X) static_assert(X, #X)
#    define O2G_STATIC_ASSERT_MSG(X, MSG) static_assert(X, MSG)
#  else
#    define O2G_STATIC_ASSERT_JOIN(X, Y) O2G_STATIC_ASSERT_JOIN_IMPL(X, Y)
#    define O2G_STATIC_ASSERT_JOIN_IMPL(X, Y) X##Y
template<bool> struct O2GStaticAssert;
template<> struct O2GStaticAssert<true>{};
#    define O2G_STATIC_ASSERT(X) enum { O2G_STATIC_ASSERT_JOIN(O2GStaticAssertEnum, __LINE__) = sizeof(O2GStaticAssert<X>) }
#    define O2G_STATIC_ASSERT_MSG(X, MSG) enum { O2G_STATIC_ASSERT_JOIN(O2GStaticAssertEnum, __LINE__) = sizeof(O2GStaticAssert<X>) }
#  endif


#  if defined(O2G_COMPILER_CXX_DELETED_FUNCTIONS) && O2G_COMPILER_CXX_DELETED_FUNCTIONS
#    define O2G_DELETED_FUNCTION = delete
#  else
#    define O2G_DELETED_FUNCTION 
#  endif


#  if defined(O2G_COMPILER_CXX_NOEXCEPT) && O2G_COMPILER_CXX_NOEXCEPT
#    define O2G_NOEXCEPT noexcept
#    define O2G_NOEXCEPT_EXPR(X) noexcept(X)
#  else
#    define O2G_NOEXCEPT
#    define O2G_NOEXCEPT_EXPR(X)
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


#  if !(defined(O2G_COMPILER_CXX_NOEXCEPT) && O2G_COMPILER_CXX_NOEXCEPT)
#    define noexcept 
#  endif

#endif

#if !(defined(O2G_COMPILER_CXX_STATIC_ASSERT) && O2G_COMPILER_CXX_STATIC_ASSERT)
#  define static_assert(X, MSG) O2G_STATIC_ASSERT_MSG(X, MSG)
#endif
#if __cplusplus >= 201103L
#  define O2G_OPERATOR_EXPLICIT explicit
#else
#  define O2G_OPERATOR_EXPLICIT
#endif


#endif
