/* Copyright (C) 2022 Storj Labs, Inc.
 * See LICENSE for copying information.
 *
 * Determine build target arch, cpu, OS, and features
 *
 * Based on code from the Botan project:
 *
 * (C) 2016 Jack Lloyd
 *
 * Botan is released under the Simplified BSD License (see LICENSE).
 */

/* This header is included in both C++ and C (via ffi.h) and should only
 * contain macro definitions. Avoid C++ style // comments in this file.
 */

#ifndef INFECTIOUS_BUILD_ENV_H
#define INFECTIOUS_BUILD_ENV_H

#if !defined(INFECTIOUS_BUILD_COMPILER_SET)
# if defined(__ibmxl__)
#  define INFECTIOUS_BUILD_COMPILER_IS_XLC
#  define INFECTIOUS_BUILD_COMPILER "xlc"
# elif defined(_SUNPRO_CC)
#  define INFECTIOUS_BUILD_COMPILER_IS_SUN_STUDIO
#  define INFECTIOUS_BUILD_COMPILER "sunstudio"
# elif defined(__INTEL_COMPILER)
#  define INFECTIOUS_BUILD_COMPILER_IS_INTEL
#  define INFECTIOUS_BUILD_COMPILER "intel"
# elif defined(_MSC_VER)
#  define INFECTIOUS_BUILD_COMPILER_IS_MSVC
#  define INFECTIOUS_BUILD_COMPILER "msvc"
# elif defined(__clang__)
#  define INFECTIOUS_BUILD_COMPILER_IS_CLANG
#  define INFECTIOUS_BUILD_COMPILER "clang"
# elif defined(__GNUC__)
#  define INFECTIOUS_BUILD_COMPILER_IS_GCC
#  define INFECTIOUS_BUILD_COMPILER "gcc"
# else
#  warning "Build compiler is not recognized"
#  define INFECTIOUS_BUILD_COMPILER_IS_UNKNOWN
#  define INFECTIOUS_BUILD_COMPILER "unknown"
# endif
# define INFECTIOUS_BUILD_COMPILER_SET
#endif

#if !defined(INFECTIOUS_TARGET_ARCH_SET)
# if defined(__x86_64__) || defined(_M_AMD64)
#  define INFECTIOUS_TARGET_ARCH_IS_X86_64
#  define INFECTIOUS_TARGET_ARCH "x86_64"
# elif defined(__i386__) || defined(_M_IX86) || defined(_X86_)
#  define INFECTIOUS_TARGET_ARCH_IS_X86_32
#  define INFECTIOUS_TARGET_ARCH "x86_32"
# elif defined(__aarch64__)
#  define INFECTIOUS_TARGET_ARCH_IS_ARM64
#  define INFECTIOUS_TARGET_ARCH "arm64"
# elif defined(__arm__) || defined(_M_ARM)
#  define INFECTIOUS_TARGET_ARCH_IS_ARM32
#  define INFECTIOUS_TARGET_ARCH "arm32"
# elif defined(__ppc64__) || defined(_M_PPC)
#  define INFECTIOUS_TARGET_ARCH_IS_PPC64
#  define INFECTIOUS_TARGET_ARCH "ppc64"
# elif defined(__alpha__) || defined(_M_ALPHA)
#  define INFECTIOUS_TARGET_ARCH_IS_ALPHA
#  define INFECTIOUS_TARGET_ARCH "alpha"
# elif defined(__ia64__) || defined(_M_IA64)
#  define INFECTIOUS_TARGET_ARCH_IS_IA64
#  define INFECTIOUS_TARGET_ARCH "ia64"
# else
#  warning "Target architecture is not recognized"
#  define INFECTIOUS_TARGET_ARCH_IS_UNKNOWN
#  define INFECTIOUS_TARGET_ARCH "unknown"
# endif
# define INFECTIOUS_TARGET_ARCH_SET
#endif

#if defined(INFECTIOUS_TARGET_ARCH_IS_X86_64) || defined(INFECTIOUS_TARGET_ARCH_IS_X86_32)
# define INFECTIOUS_TARGET_CPU_IS_X86_FAMILY
# define INFECTIOUS_TARGET_CPU_IS_LITTLE_ENDIAN
# define INFECTIOUS_HAS_VPERM
#endif

#if defined(INFECTIOUS_TARGET_ARCH_IS_ARM64) || defined(INFECTIOUS_TARGET_ARCH_IS_ARM32)
# define INFECTIOUS_TARGET_CPU_IS_ARM_FAMILY
# define INFECTIOUS_TARGET_CPU_IS_LITTLE_ENDIAN
# define INFECTIOUS_HAS_VPERM
#endif

#if defined(INFECTIOUS_TARGET_ARCH_IS_PPC64)
# define INFECTIOUS_TARGET_CPU_IS_PPC_FAMILY
# define INFECTIOUS_TARGET_CPU_IS_BIG_ENDIAN
# define INFECTIOUS_HAS_VPERM
#endif

#if defined(INFECTIOUS_TARGET_ARCH_IS_ALPHA)
# define INFECTIOUS_TARGET_CPU_IS_LITTLE_ENDIAN
#endif

/* Should we use GCC-style inline assembler? */
#if !defined(INFECTIOUS_USE_GCC_INLINE_ASM) && !defined(INFECTIOUS_NO_GCC_INLINE_ASM)
# if defined(INFECTIOUS_BUILD_COMPILER_IS_GCC) || \
	defined(INFECTIOUS_BUILD_COMPILER_IS_CLANG) || \
	defined(INFECTIOUS_BUILD_COMPILER_IS_XLC) || \
	defined(INFECTIOUS_BUILD_COMPILER_IS_SUN_STUDIO)
#  define INFECTIOUS_USE_GCC_INLINE_ASM
# endif
#endif

#if defined(__GNUC__) || defined(__clang__)
# define INFECTIOUS_FUNC_ISA(isa) __attribute__ ((target(isa)))
#else
# define INFECTIOUS_FUNC_ISA(isa)
#endif

#if !defined(INFECTIOUS_TARGET_OS_SET)
# if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
#  define INFECTIOUS_TARGET_OS_IS_WINDOWS
#  define INFECTIOUS_TARGET_OS "windows"
# elif defined(__APPLE__)
#  include <TargetConditionals.h>
#  if TARGET_IPHONE_SIMULATOR || TARGET_OS_IPHONE
#   define INFECTIOUS_TARGET_OS_IS_IOS
#   define INFECTIOUS_TARGET_OS "ios"
#  else
#   define INFECTIOUS_TARGET_OS_IS_MACOS
#   define INFECTIOUS_TARGET_OS "macos"
#  endif
# elif defined(__ANDROID__)
#  define INFECTIOUS_TARGET_OS_IS_ANDROID
#  define INFECTIOUS_TARGET_OS "android"
# elif defined(__FreeBSD__)
#  define INFECTIOUS_TARGET_OS_IS_FREEBSD
#  define INFECTIOUS_TARGET_OS "freebsd"
# elif defined(__NetBSD__)
#  define INFECTIOUS_TARGET_OS_IS_NETBSD
#  define INFECTIOUS_TARGET_OS "netbsd"
# elif defined(__linux__)
#  define INFECTIOUS_TARGET_OS_IS_LINUX
#  define INFECTIOUS_TARGET_OS "linux"
# else
#  define INFECTIOUS_TARGET_OS_IS_NONE
#  define INFECTIOUS_TARGET_OS "none"
# endif
# define INFECTIOUS_TARGET_OS_SET
#endif
 
#if defined(INFECTIOUS_TARGET_OS_IS_IOS) || \
	defined(INFECTIOUS_TARGET_OS_IS_MACOS) || \
	defined(INFECTIOUS_TARGET_OS_IS_ANDROID) || \
	defined(INFECTIOUS_TARGET_OS_IS_FREEBSD) || \
	defined(INFECTIOUS_TARGET_OS_IS_NETBSD) || \
	defined(INFECTIOUS_TARGET_OS_IS_LINUX)
# define INFECTIOUS_TARGET_OS_HAS_POSIX1
#endif

#if defined(INFECTIOUS_TARGET_OS_IS_ANDROID) || defined(INFECTIOUS_TARGET_OS_IS_LINUX)
# define INFECTIOUS_TARGET_OS_HAS_GETAUXVAL
#endif

#if defined(INFECTIOUS_TARGET_OS_IS_FREEBSD)
# define INFECTIOUS_TARGET_OS_HAS_ELF_AUX_INFO
#endif

#if defined(INFECTIOUS_TARGET_OS_IS_NETBSD)
# define INFECTIOUS_TARGET_OS_HAS_AUXINFO
#endif

#if defined(INFECTIOUS_TARGET_CPU_IS_X86_FAMILY)
# define INFECTIOUS_TARGET_SUPPORTS_SSE2
#endif

#if defined(INFECTIOUS_TARGET_CPU_IS_PPC_FAMILY)
# define INFECTIOUS_TARGET_SUPPORTS_ALTIVEC
#endif

#if defined(INFECTIOUS_TARGET_CPU_IS_ARM_FAMILY)
# define INFECTIOUS_TARGET_SUPPORTS_NEON
#endif

#endif /* ifndef INFECTIOUS_BUILD_ENV_H */
