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

/// @file vapoursynth_wrap.h
/// @see vapoursynth_wrap.cpp
/// @ingroup video_input audio_input
///

#ifdef WITH_VAPOURSYNTH

#include <libaegisub/exception.h>
#include <mutex>

DEFINE_EXCEPTION(VapoursynthError, agi::Exception);

struct VSAPI;
struct VSSCRIPTAPI;

class VapourSynthWrapper {
	VapourSynthWrapper(VapourSynthWrapper const&);
public:
	std::mutex& GetMutex() const;
	const VSAPI *GetAPI() const;
	const VSSCRIPTAPI *GetScriptAPI() const;

	VapourSynthWrapper();
};

#endif
