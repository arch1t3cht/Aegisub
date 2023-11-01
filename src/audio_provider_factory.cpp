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

#include "audio_provider_factory.h"

#include "compat.h"
#include "factory_manager.h"
#include "gui_wrap.h"
#include "options.h"
#include "utils.h"

#include <libaegisub/audio/provider.h>
#include <libaegisub/format.h>
#include <libaegisub/fs.h>
#include <libaegisub/log.h>
#include <libaegisub/path.h>

#include <boost/range/iterator_range.hpp>

using namespace agi;

std::unique_ptr<AudioProvider> CreateAvisynthAudioProvider(fs::path const& filename, BackgroundRunner *);
std::unique_ptr<AudioProvider> CreateFFmpegSourceAudioProvider(fs::path const& filename, BackgroundRunner *);
std::unique_ptr<AudioProvider> CreateBSAudioProvider(fs::path const& filename, BackgroundRunner *);
std::unique_ptr<AudioProvider> CreateVapourSynthAudioProvider(fs::path const& filename, BackgroundRunner *);

namespace {
struct factory {
	const char *name;
	std::unique_ptr<AudioProvider> (*create)(fs::path const&, BackgroundRunner *);
	bool hidden;
	std::function<bool(agi::fs::path const&)> wants_to_open = [](auto p) { return false; };
};

const factory providers[] = {
	{"Dummy", CreateDummyAudioProvider, true},
	{"PCM", CreatePCMAudioProvider, true},
#ifdef WITH_FFMS2
	{"FFmpegSource", CreateFFmpegSourceAudioProvider, false},
#endif
#ifdef WITH_AVISYNTH
	{"Avisynth", CreateAvisynthAudioProvider, false, [](auto p) { return agi::fs::HasExtension(p, "avs"); }},
#endif
#ifdef WITH_BESTSOURCE
	{"BestSource", CreateBSAudioProvider, false},
#endif
#ifdef WITH_VAPOURSYNTH
	{"VapourSynth", CreateVapourSynthAudioProvider, false, [](auto p) { return agi::fs::HasExtension(p, "py") || agi::fs::HasExtension(p, "vpy"); }},
#endif
};
}

std::vector<std::string> GetAudioProviderNames() {
	return ::GetClasses(boost::make_iterator_range(std::begin(providers), std::end(providers)));
}

std::unique_ptr<agi::AudioProvider> SelectAudioProvider(fs::path const& filename,
                                                        Path const& path_helper,
                                                        BackgroundRunner *br) {
	auto preferred = OPT_GET("Audio/Provider")->GetString();

	if (!std::any_of(std::begin(providers), std::end(providers), [&](factory provider) { return provider.name == preferred; })) {
		preferred = OPT_GET("Audio/Provider")->GetDefaultString();
	}

	auto sorted = GetSorted(boost::make_iterator_range(std::begin(providers), std::end(providers)), preferred);

	RearrangeWithPriority(sorted, filename);

	bool found_file = false;
	std::string errors;

	auto tried_providers = sorted.begin();

	for (; tried_providers < sorted.end(); tried_providers++) {
		auto factory = *tried_providers;
		std::string err;
		try {
			auto provider = factory->create(filename, br);
			if (!provider) {
				err = "Failed to create provider."; 	// Some generic error message here
			} else {
				LOG_I("audio_provider") << "Using audio provider: " << factory->name;
				return provider;
			}
		}
		catch (AudioDataNotFound const& ex) {
			found_file = true;
			err = ex.GetMessage();
		}
		catch (AudioProviderError const& ex) {
			found_file = true;
			err = ex.GetMessage();
		}

		errors += std::string(factory->name) + ": " + err + "\n";
		LOG_D("audio_provider") << factory->name << ": " << err;
		if (factory->name == preferred)
			break;
	}

	std::vector<const factory *> remaining_providers(tried_providers + 1, sorted.end());

	if (!remaining_providers.size()) {
		// No provider could open the file
		LOG_E("audio_provider") << "Could not open " << filename;
		std::string msg = "Could not open " + filename.string() + ":\n" + errors;

		if (!found_file) throw AudioDataNotFound(filename.string());
		throw AudioProviderError(msg);
	}

	std::vector<std::string> names;
	for (auto const& f : remaining_providers)
		names.push_back(f->name);

	int choice = wrapChoiceDialog("audio-provider", agi::format("Could not open %s with the preferred provider:\n\n%s\nPlease choose a different audio provider to try:", filename.string(), errors), _("Error loading audio"), to_wx(names));
	if (choice == -1) {
		throw agi::UserCancelException("audio loading cancelled by user");
	}

	auto factory = remaining_providers[choice];
	auto provider = factory->create(filename, br);
	if (!provider)
		throw AudioProviderError("Audio provider returned null pointer");
	LOG_I("audio_provider") << factory->name << ": opened " << filename;
	return provider;
}

std::unique_ptr<agi::AudioProvider> GetAudioProvider(fs::path const& filename,
                                                     Path const& path_helper,
                                                     BackgroundRunner *br) {
	std::unique_ptr<agi::AudioProvider> provider = SelectAudioProvider(filename, path_helper, br);

	bool needs_cache = provider->NeedsCache();

	// Give it a converter if needed
	if (provider->GetBytesPerSample() != 2 || provider->GetSampleRate() < 32000 || provider->GetChannels() != 1)
		provider = CreateConvertAudioProvider(std::move(provider));

	// Change provider to RAM/HD cache if needed
	int cache = OPT_GET("Audio/Cache/Type")->GetInt();
	if (!cache || !needs_cache)
		return CreateLockAudioProvider(std::move(provider));

	// Convert to RAM
	if (cache == 1) return CreateRAMAudioProvider(std::move(provider));

	// Convert to HD
	if (cache == 2) {
		auto path = OPT_GET("Audio/Cache/HD/Location")->GetString();
		if (path == "default")
			path = "?temp";
		auto cache_dir = path_helper.MakeAbsolute(path_helper.Decode(path), "?temp");
		return CreateHDAudioProvider(std::move(provider), cache_dir);
	}

	throw InternalError("Invalid audio caching method");
}
