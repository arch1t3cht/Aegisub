// Copyright (c) 2014, Thomas Goyne <plorkyeran@aegisub.org>
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

#include "video_provider_manager.h"

#include "compat.h"
#include "factory_manager.h"
#include "gui_wrap.h"
#include "include/aegisub/video_provider.h"
#include "options.h"

#include <libaegisub/log.h>
#include <libaegisub/format.h>

#include <boost/range/iterator_range.hpp>

#include <wx/choicdlg.h>

std::unique_ptr<VideoProvider> CreateDummyVideoProvider(agi::fs::path const&, std::string const&, agi::BackgroundRunner *);
std::unique_ptr<VideoProvider> CreateYUV4MPEGVideoProvider(agi::fs::path const&, std::string const&, agi::BackgroundRunner *);
std::unique_ptr<VideoProvider> CreateFFmpegSourceVideoProvider(agi::fs::path const&, std::string const&, agi::BackgroundRunner *);
std::unique_ptr<VideoProvider> CreateAvisynthVideoProvider(agi::fs::path const&, std::string const&, agi::BackgroundRunner *);
std::unique_ptr<VideoProvider> CreateBSVideoProvider(agi::fs::path const&, std::string const&, agi::BackgroundRunner *);
std::unique_ptr<VideoProvider> CreateVapourSynthVideoProvider(agi::fs::path const&, std::string const&, agi::BackgroundRunner *);

std::unique_ptr<VideoProvider> CreateCacheVideoProvider(std::unique_ptr<VideoProvider>);

namespace {
	struct factory {
		const char *name;
		std::unique_ptr<VideoProvider> (*create)(agi::fs::path const&, std::string const&, agi::BackgroundRunner *);
		bool hidden;
		std::function<bool(agi::fs::path const&)> wants_to_open = [](auto p) { return false; };
	};

	const factory providers[] = {
		{"Dummy", CreateDummyVideoProvider, true},
		{"YUV4MPEG", CreateYUV4MPEGVideoProvider, true},
#ifdef WITH_FFMS2
		{"FFmpegSource", CreateFFmpegSourceVideoProvider, false},
#endif
#ifdef WITH_AVISYNTH
		{"Avisynth", CreateAvisynthVideoProvider, false, [](auto p) { return agi::fs::HasExtension(p, "avs"); }},
#endif
#ifdef WITH_BESTSOURCE
		{"BestSource (SLOW)", CreateBSVideoProvider, false},
#endif
#ifdef WITH_VAPOURSYNTH
		{"VapourSynth", CreateVapourSynthVideoProvider, false, [](auto p) { return agi::fs::HasExtension(p, "py") || agi::fs::HasExtension(p, "vpy"); }},
#endif
	};
}

std::vector<std::string> VideoProviderFactory::GetClasses() {
	return ::GetClasses(boost::make_iterator_range(std::begin(providers), std::end(providers)));
}

std::unique_ptr<VideoProvider> VideoProviderFactory::GetProvider(agi::fs::path const& filename, std::string const& colormatrix, agi::BackgroundRunner *br) {
	auto preferred = OPT_GET("Video/Provider")->GetString();

	if (!std::any_of(std::begin(providers), std::end(providers), [&](factory provider) { return provider.name == preferred; })) {
		preferred = OPT_GET("Audio/Provider")->GetDefaultString();
	}

	auto sorted = GetSorted(boost::make_iterator_range(std::begin(providers), std::end(providers)), preferred);

	RearrangeWithPriority(sorted, filename);

	bool found = false;
	bool supported = false;
	std::string errors;
	errors.reserve(1024);

	auto tried_providers = sorted.begin();

	auto finalize_provider = [&](std::unique_ptr<VideoProvider> provider) {
		return provider->WantsCaching() ? CreateCacheVideoProvider(std::move(provider)) : std::move(provider);
	};

	for (; tried_providers < sorted.end(); tried_providers++) {
		auto factory = *tried_providers;
		std::string err;
		try {
			auto provider = factory->create(filename, colormatrix, br);
			if (!provider) {
				err = "Failed to create provider."; 	// Some generic error message here
			} else {
				LOG_I("manager/video/provider") << factory->name << ": opened " << filename;
				return finalize_provider(std::move(provider));
			}
		}
		catch (VideoNotSupported const& ex) {
			found = true;
			err = "video is not in a supported format: " + ex.GetMessage();
		}
		catch (VideoOpenError const& ex) {
			supported = true;
			err = ex.GetMessage();
		}
		catch (agi::vfr::Error const& ex) {
			supported = true;
			err = ex.GetMessage();
		}

		errors += std::string(factory->name) + ": " + err + "\n";
		LOG_D("manager/video/provider") << factory->name << ": " << err;
		if (factory->name == preferred)
			break;
	}

	// We failed to load the video using the preferred video provider (and the ones before it).
	// The user might want to know about this, so show a dialog and let them choose what other provider to try.
	std::vector<const factory *> remaining_providers(tried_providers + 1, sorted.end());

	if (!remaining_providers.size()) {
		// No provider could open the file
		LOG_E("manager/video/provider") << "Could not open " << filename;
		std::string msg = "Could not open " + filename.string() + ":\n" + errors;

		if (!found) throw agi::fs::FileNotFound(filename.string());
		if (!supported) throw VideoNotSupported(msg);
		throw VideoOpenError(msg);
	}

	std::vector<std::string> names;
	for (auto const& f : remaining_providers)
		names.push_back(f->name);

	int choice = wrapChoiceDialog("video-provider", agi::format("Could not open %s with the preferred provider:\n\n%s\nPlease choose a different video provider to try:", filename.string(), errors), _("Error loading video"), to_wx(names));
	if (choice == -1) {
		throw agi::UserCancelException("video loading cancelled by user");
	}

	try {
		auto factory = remaining_providers[choice];
		auto provider = factory->create(filename, colormatrix, br);
		if (!provider)
			throw VideoNotSupported("Video provider returned null pointer");
		LOG_I("manager/video/provider") << factory->name << ": opened " << filename;
		return finalize_provider(std::move(provider));
	} catch (agi::vfr::Error const& ex) {
		throw VideoOpenError(ex.GetMessage());
	}
}
