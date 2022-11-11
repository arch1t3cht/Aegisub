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
#include "include/aegisub/video_provider.h"

#include "options.h"
#include "video_frame.h"

#include <libaegisub/access.h>
#include <libaegisub/format.h>
#include <libaegisub/path.h>
#include <libaegisub/make_unique.h>

#include <mutex>

#include "vapoursynth_wrap.h"
#include "vapoursynth_common.h"
#include "VSScript4.h"
#include "VSHelper4.h"
#include "VSConstants4.h"

namespace {
class VapoursynthVideoProvider: public VideoProvider {
	VapourSynthWrapper vs;
	VSScript *script = nullptr;
	VSNode *node = nullptr;
	const VSVideoInfo *vi = nullptr;

	double dar = 0;
	agi::vfr::Framerate fps;
	std::vector<int> keyframes;
	std::string colorspace;
	std::string real_colorspace;

	const VSFrame *GetVSFrame(int n);
	void SetResizeArg(VSMap *args, const VSMap *props, const char *arg_name, const char *prop_name, int deflt, int unspecified = -1);

public:
	VapoursynthVideoProvider(agi::fs::path const& filename, std::string const& colormatrix);
	~VapoursynthVideoProvider();

	void GetFrame(int n, VideoFrame &frame) override;

	void SetColorSpace(std::string const& matrix) override { }

	int GetFrameCount() const override             { return vi->numFrames; }
	agi::vfr::Framerate GetFPS() const override    { return fps; }
	int GetWidth() const override                  { return vi->width; }
	int GetHeight() const override                 { return vi->height; }
	double GetDAR() const override                 { return dar; }
	std::vector<int> GetKeyFrames() const override { return keyframes; }
	std::string GetColorSpace() const override     { return GetRealColorSpace(); }
	std::string GetRealColorSpace() const override { return colorspace == "Unknown" ? "None" : colorspace; }
	bool HasAudio() const override                 { return false; }
	bool WantsCaching() const override             { return true; }
	std::string GetDecoderName() const override    { return "VapourSynth"; }
	bool ShouldSetVideoProperties() const override { return colorspace != "Unknown"; }
};

std::string colormatrix_description(int colorFamily, int colorRange, int matrix) {
	if (colorFamily != cfYUV) {
		return "None";
	}
	// Assuming TV for unspecified
	std::string str = colorRange == VSC_RANGE_FULL ? "PC" : "TV";

	switch (matrix) {
		case VSC_MATRIX_RGB:
			return "None";
		case VSC_MATRIX_BT709:
			return str + ".709";
		case VSC_MATRIX_FCC:
			return str + ".FCC";
		case VSC_MATRIX_BT470_BG:
		case VSC_MATRIX_ST170_M:
			return str + ".601";
		case VSC_MATRIX_ST240_M:
			return str + ".240M";
		default:
			return "Unknown"; 	// Will return "None" in GetColorSpace
	}
}

// Adds an argument to the rescaler if the corresponding frameprop does not exist or is set as unspecified
void VapoursynthVideoProvider::SetResizeArg(VSMap *args, const VSMap *props, const char *arg_name, const char *prop_name, int deflt, int unspecified) {
	int err;
	int result = vs.GetAPI()->mapGetInt(props, prop_name, 0, &err);
	if (err != 0 || result == unspecified) {
		result = deflt;
		vs.GetAPI()->mapSetInt(args, arg_name, result, maAppend);
	}
}

VapoursynthVideoProvider::VapoursynthVideoProvider(agi::fs::path const& filename, std::string const& colormatrix) try {
	std::lock_guard<std::mutex> lock(vs.GetMutex());

	script = vs.GetScriptAPI()->createScript(nullptr);
	if (script == nullptr) {
		throw VapoursynthError("Error creating script API");
	}
	vs.GetScriptAPI()->evalSetWorkingDir(script, 1);
	if (OpenScriptOrVideo(vs.GetScriptAPI(), script, filename, OPT_GET("Provider/Video/VapourSynth/Default Script")->GetString())) {
		std::string msg = agi::format("Error executing VapourSynth script: %s", vs.GetScriptAPI()->getError(script));
		vs.GetScriptAPI()->freeScript(script);
		throw VapoursynthError(msg);
	}
	node = vs.GetScriptAPI()->getOutputNode(script, 0);
	if (node == nullptr) {
		vs.GetScriptAPI()->freeScript(script);
		throw VapoursynthError("No output node set");
	}
	if (vs.GetAPI()->getNodeType(node) != mtVideo) {
		vs.GetAPI()->freeNode(node);
		vs.GetScriptAPI()->freeScript(script);
		throw VapoursynthError("Output node isn't a video node");
	}
	vi = vs.GetAPI()->getVideoInfo(node);
	if (!vsh::isConstantVideoFormat(vi)) {
		vs.GetAPI()->freeNode(node);
		vs.GetScriptAPI()->freeScript(script);
		throw VapoursynthError("Video doesn't have constant format");
	}

	// Assume constant frame rate, since handling VFR would require going through all frames when loading.
	// Users can load custom timecodes files to deal with VFR.
	// Alternatively (TODO) the provider could read timecodes and keyframes from a second output node.
	fps = agi::vfr::Framerate(vi->fpsNum, vi->fpsDen);

	// Find the first frame to get some info
	const VSFrame *frame;
	try {
		frame = GetVSFrame(0);
	} catch (VapoursynthError const& err) {
		vs.GetAPI()->freeNode(node);
		vs.GetScriptAPI()->freeScript(script);
		throw err;
	}
	int err1, err2;
	const VSMap *props = vs.GetAPI()->getFramePropertiesRO(frame);
	int sarn = vs.GetAPI()->mapGetInt(props, "_SARNum", 0, &err1);
	int sard = vs.GetAPI()->mapGetInt(props, "_SARDen", 0, &err2);
	if (!err1 && !err2) {
		dar = ((double) vi->width * sarn) / (vi->height * sard);
	}

	int range = vs.GetAPI()->mapGetInt(props, "_ColorRange", 0, &err1);
	int matrix = vs.GetAPI()->mapGetInt(props, "_Matrix", 0, &err2);
	colorspace = colormatrix_description(vi->format.colorFamily, err1 == 0 ? range : -1, err2 == 0 ? matrix : -1);

	vs.GetAPI()->freeFrame(frame);

	if (vi->format.colorFamily != cfRGB || vi->format.bitsPerSample != 8) {
		// Convert to RGB24 format
		VSPlugin *resize = vs.GetAPI()->getPluginByID(VSH_RESIZE_PLUGIN_ID, vs.GetScriptAPI()->getCore(script));
		if (resize == nullptr) {
			throw VapoursynthError("Couldn't find resize plugin");
		}
		VSMap *args = vs.GetAPI()->createMap();
		if (args == nullptr) {
			throw VapoursynthError("Failed to create argument map");
		}

		vs.GetAPI()->mapSetNode(args, "clip", node, maAppend);
		vs.GetAPI()->mapSetInt(args, "format", pfRGB24, maAppend);
		if (vi->format.colorFamily != cfGray)
			SetResizeArg(args, props, "matrix_in", "_Matrix", VSC_MATRIX_BT709, VSC_MATRIX_UNSPECIFIED);
		SetResizeArg(args, props, "transfer_in", "_Transfer", VSC_TRANSFER_BT709, VSC_TRANSFER_UNSPECIFIED);
		SetResizeArg(args, props, "primaries_in", "_Primaries", VSC_PRIMARIES_BT709, VSC_PRIMARIES_UNSPECIFIED);
		SetResizeArg(args, props, "range_in", "_ColorRange", VSC_RANGE_LIMITED);
		SetResizeArg(args, props, "chromaloc_in", "_ChromaLocation", VSC_CHROMA_LEFT);

		VSMap *result = vs.GetAPI()->invoke(resize, "Bicubic", args);
		vs.GetAPI()->freeMap(args);
		const char *error = vs.GetAPI()->mapGetError(result);
		if (error) {
			vs.GetAPI()->freeMap(result);
			vs.GetAPI()->freeNode(node);
			vs.GetScriptAPI()->freeScript(script);
			throw VideoProviderError(agi::format("Failed to convert to RGB24: %s", error));
		}
		int err;
		vs.GetAPI()->freeNode(node);
		node = vs.GetAPI()->mapGetNode(result, "clip", 0, &err);
		vs.GetAPI()->freeMap(result);
		if (err) {
			vs.GetScriptAPI()->freeScript(script);
			throw VideoProviderError("Failed to get resize output node");
		}

		// Finally, try to get the first frame again, so if the filter does crash, it happens before loading finishes
		const VSFrame *rgbframe;
		try {
			rgbframe = GetVSFrame(0);
		} catch (VapoursynthError const& err) {
			vs.GetAPI()->freeNode(node);
			vs.GetScriptAPI()->freeScript(script);
			throw err;
		}
		vs.GetAPI()->freeFrame(rgbframe);
	}
}
catch (VapoursynthError const& err) {
	throw VideoProviderError(agi::format("Vapoursynth error: %s", err.GetMessage()));
}

const VSFrame *VapoursynthVideoProvider::GetVSFrame(int n) {
	char errorMsg[1024];
	const VSFrame *frame = vs.GetAPI()->getFrame(n, node, errorMsg, sizeof(errorMsg));
	if (frame == nullptr) {
		throw VapoursynthError(agi::format("Error getting frame: %s", errorMsg));
	}
	return frame;
}

void VapoursynthVideoProvider::GetFrame(int n, VideoFrame &out) {
	std::lock_guard<std::mutex> lock(vs.GetMutex());

	const VSFrame *frame = GetVSFrame(n);

	const VSVideoFormat *format = vs.GetAPI()->getVideoFrameFormat(frame);
	if (format->colorFamily != cfRGB || format->numPlanes != 3 || format->bitsPerSample != 8 || format->subSamplingH != 0 || format->subSamplingW != 0) {
		throw VapoursynthError("Frame not in RGB24 format");
	}

	out.width = vs.GetAPI()->getFrameWidth(frame, 0);
	out.height = vs.GetAPI()->getFrameHeight(frame, 0);
	out.pitch = out.width * 4;
	out.flipped = false;

	out.data.resize(out.pitch * out.height);

	for (int p = 0; p < format->numPlanes; p++) {
		ptrdiff_t stride = vs.GetAPI()->getStride(frame, p);
		const uint8_t *readPtr = vs.GetAPI()->getReadPtr(frame, p);
		uint8_t *writePtr = &out.data[2 - p];
		int rows = vs.GetAPI()->getFrameHeight(frame, p);
		int cols = vs.GetAPI()->getFrameWidth(frame, p);

		for (int row = 0; row < rows; row++) {
			const uint8_t *rowPtr = readPtr;
			uint8_t *rowWritePtr = writePtr;
			for (int col = 0; col < cols; col++) {
				*rowWritePtr = *rowPtr++;
				rowWritePtr += 4;
			}
			readPtr += stride;
			writePtr += out.pitch;
		}
	}

	vs.GetAPI()->freeFrame(frame);
}

VapoursynthVideoProvider::~VapoursynthVideoProvider() {
	if (node != nullptr) {
		vs.GetAPI()->freeNode(node);
	}
	if (script != nullptr) {
		vs.GetScriptAPI()->freeScript(script);
	}
}
}

namespace agi { class BackgroundRunner; }
std::unique_ptr<VideoProvider> CreateVapoursynthVideoProvider(agi::fs::path const& path, std::string const& colormatrix, agi::BackgroundRunner *) {
	agi::acs::CheckFileRead(path);
	return agi::make_unique<VapoursynthVideoProvider>(path, colormatrix);
}
#endif // WITH_VAPOURSYNTH
