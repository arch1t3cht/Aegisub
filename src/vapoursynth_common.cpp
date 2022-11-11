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

#ifdef WITH_VAPOURSYNTH
#include "vapoursynth_common.h"

#include "options.h"
#include <libaegisub/fs.h>

#include <boost/algorithm/string/replace.hpp>

int OpenScriptOrVideo(const VSSCRIPTAPI *api, VSScript *script, agi::fs::path const& filename, std::string default_script) {
	int result;
	if (agi::fs::HasExtension(filename, "py") || agi::fs::HasExtension(filename, "vpy")) {
		result = api->evaluateFile(script, filename.string().c_str());
	} else {
		std::string fname = filename.string();
		boost::replace_all(fname, "\\", "\\\\");
		boost::replace_all(fname, "'", "\\'");
		std::string vscript = "filename = '" + fname + "'\n" + default_script;
		result = api->evaluateBuffer(script, vscript.c_str(), "aegisub");
	}
	return result;
}

#endif // WITH_VAPOURSYNTH
