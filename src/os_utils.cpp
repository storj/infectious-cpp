// Copyright (C) 2022 Storj Labs, Inc.
// See LICENSE for copying information.
//
// OS and machine specific utility functions
//
// Based on code from the Botan project:
//
// (C) 2015,2016,2017,2018 Jack Lloyd
// (C) 2016 Daniel Neus
//
// Botan is released under the Simplified BSD License (see LICENSE).

#include "infectious/build_env.h"
#include "os_utils.hpp"

#include <functional>
#include <cstdlib>
#include <system_error>

#if defined(INFECTIOUS_TARGET_OS_HAS_POSIX1)
# include <csignal>
# include <csetjmp>
# include <cerrno>
# include <unistd.h>
#endif

#if defined(INFECTIOUS_TARGET_OS_HAS_GETAUXVAL) || defined(INFECTIOUS_TARGET_OS_HAS_ELF_AUX_INFO)
# include <sys/auxv.h>
#endif

#if defined(INFECTIOUS_TARGET_OS_HAS_AUXINFO)
# include <dlfcn.h>
# include <elf.h>
#endif

#if defined(INFECTIOUS_TARGET_OS_IS_ANDROID)
# include <elf.h>
extern "C" char **environ;
#endif

#if defined(INFECTIOUS_TARGET_OS_IS_IOS) || defined(INFECTIOUS_TARGET_OS_IS_MACOS)
# include <sys/types.h>
# include <sys/sysctl.h>
# include <mach/vm_statistics.h>
#endif

namespace infectious::OS {

auto get_auxval(unsigned long id) -> unsigned long {
#if defined(INFECTIOUS_TARGET_OS_HAS_GETAUXVAL)
	return ::getauxval(id);
#elif defined(INFECTIOUS_TARGET_OS_IS_ANDROID) && defined(INFECTIOUS_TARGET_ARCH_IS_ARM32)
	if (id == 0) {
		return 0;
	}

	char **p = environ;
	while (*p++ != nullptr)
		;

	Elf32_auxv_t *e = reinterpret_cast<Elf32_auxv_t*>(p);

	while (e != nullptr) {
		if (e->a_type == id) {
			return e->a_un.a_val;
		}
		e++;
	}

	return 0;
#elif defined(INFECTIOUS_TARGET_OS_HAS_ELF_AUX_INFO)
	unsigned long auxinfo = 0;
	::elf_aux_info(static_cast<int>(id), &auxinfo, sizeof(auxinfo));
	return auxinfo;
#elif defined(INFECTIOUS_TARGET_OS_HAS_AUXINFO)
	for (const AuxInfo *auxinfo = static_cast<AuxInfo *>(::_dlauxinfo()); auxinfo != AT_NULL; ++auxinfo) {
		if (id == auxinfo->a_type)
			return auxinfo->a_v;
		}

	return 0;
#else
	static_cast<void>(id);
	return 0;
#endif
}

#if defined(INFECTIOUS_TARGET_OS_HAS_POSIX1)

namespace {

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
::sigjmp_buf g_sigill_jmp_buf;

void infectious_sigill_handler(int /*unused*/) {
	// this is idiomatic usage. NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
	siglongjmp(g_sigill_jmp_buf, /*non-zero return value*/1);
}

} // namespace

#endif

auto run_cpu_instruction_probe(const std::function<int ()>& probe_fn) -> int {
	volatile int probe_result = -3;

#if defined(INFECTIOUS_TARGET_OS_HAS_POSIX1)
	struct sigaction old_sigaction {};
	struct sigaction sigaction {};

	sigaction.sa_handler = infectious_sigill_handler;
	sigemptyset(&sigaction.sa_mask);
	sigaction.sa_flags = 0;

	int rc = ::sigaction(SIGILL, &sigaction, &old_sigaction);

	if (rc != 0) {
		throw std::system_error(errno, std::generic_category(), "run_cpu_instruction_probe sigaction failed");
	}

	// this is idiomatic usage. NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
	rc = sigsetjmp(g_sigill_jmp_buf, /*save sigs*/1);

	if(rc == 0) {
		// first call to sigsetjmp
		probe_result = probe_fn();
	} else if(rc == 1) {
		// non-local return from siglongjmp in signal handler: return error
		probe_result = -1;
	}

	// Restore old SIGILL handler, if any
	rc = ::sigaction(SIGILL, &old_sigaction, nullptr);
	if (rc != 0) {
		throw std::system_error(errno, std::generic_category(), "run_cpu_instruction_probe sigaction restore failed");
	}

#else
	static_cast<void>(probe_fn);
#endif

	return probe_result;
}

} // namespace infectious::OS
