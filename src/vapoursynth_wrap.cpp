// Copyright (c) 2022, arch1t3cht <arch1t3cht@gmail.com>
//
// Permission to use, copy, modify, and distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
// WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
// ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
// WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
// ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
// OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
//
// Aegisub Project http://www.aegisub.org/

/// @file vapoursynth_wrap.cpp
/// @brief Wrapper-layer for VapourSynth
/// @ingroup video_input audio_input
///

#ifdef WITH_VAPOURSYNTH
#include "vapoursynth_wrap.h"

#include "VSScript4.h"

#include "options.h"

#include <mutex>

#ifndef _WIN32
#include <dlfcn.h>
#endif

#ifdef _WIN32
#define VSSCRIPT_SO "vsscript.dll"
#else
#define VSSCRIPT_SO "libvapoursynth-script.so"
#endif

// Allocate storage for and initialise static members
namespace {
	bool vs_loaded = false;
#ifdef _WIN32
	HINSTANCE hLib = nullptr;
#else
	void* hLib = nullptr;
#endif
	const VSAPI *api = nullptr;
	VSSCRIPTAPI *scriptapi = nullptr;
	std::mutex VapourSynthMutex;
}

typedef VSSCRIPTAPI* VS_CC FUNC(int);

VapourSynthWrapper::VapourSynthWrapper() {
	// VSScript assumes it's only loaded once, so unlike AVS we can't unload it when the refcount reaches zero
	if (!vs_loaded) {
#ifdef _WIN32
#define CONCATENATE(x, y) x ## y
#define _Lstr(x) CONCATENATE(L, x)
		hLib = LoadLibraryW(_Lstr(VSSCRIPT_SO));
#undef _Lstr
#undef CONCATENATE
#else
		hLib = dlopen(VSSCRIPT_SO, RTLD_LAZY | RTLD_GLOBAL | RTLD_DEEPBIND);
#endif

		if (!hLib)
			throw VapourSynthError("Could not load " VSSCRIPT_SO);

#ifdef _WIN32
		FUNC* getVSScriptAPI = (FUNC*)GetProcAddress(hLib, "getVSScriptAPI");
#else
		FUNC* getVSScriptAPI = (FUNC*)dlsym(hLib, "getVSScriptAPI");
#endif
		if (!getVSScriptAPI)
			throw VapourSynthError("Failed to get address of getVSScriptAPI from " VSSCRIPT_SO);

		// Python will set the program's locale to the user's default locale, which will break
		// half of wxwidgets on some operating systems due to locale mismatches. There's not really anything
		// we can do to fix it except for saving it and setting it back to its original value afterwards.
		std::string oldlocale(setlocale(LC_ALL, NULL));
		scriptapi = getVSScriptAPI(VSSCRIPT_API_VERSION);
		setlocale(LC_ALL, oldlocale.c_str());

		if (!scriptapi)
			throw VapourSynthError("Failed to get VapourSynth ScriptAPI");

		api = scriptapi->getVSAPI(VAPOURSYNTH_API_VERSION);

		if (!api)
			throw VapourSynthError("Failed to get VapourSynth API");

		vs_loaded = true;
	}
}

std::mutex& VapourSynthWrapper::GetMutex() const {
	return VapourSynthMutex;
}

const VSAPI *VapourSynthWrapper::GetAPI() const {
	return api;
}

const VSSCRIPTAPI *VapourSynthWrapper::GetScriptAPI() const {
	return scriptapi;
}

#endif
