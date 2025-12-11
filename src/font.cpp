// Copyright (c) 2025, arch1t3cht <arch1t3cht@gmail.com>
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

/// @file font.cpp
/// @brief font face name provider
///


#include "font.h"

#include <wx/fontenum.h>

#ifdef _WIN32
#include <libaegisub/exception.h>
#include <libaegisub/scoped_ptr.h>

#include <Windows.h>
#include <windowsx.h>
#endif


wxArrayString GetFaceNames() {
#ifdef _WIN32

	wxArrayString truncated_face_name_list = wxFontEnumerator::GetFacenames();
	wxArrayString face_name_list = wxArrayString();
	face_name_list.Alloc(truncated_face_name_list.GetCount());

	for (const wxString& face_name: truncated_face_name_list) {
		wxFont font = wxFont(
			10, // Any value would be good
			wxFONTFAMILY_DEFAULT,
			wxFONTSTYLE_NORMAL,
			wxFONTWEIGHT_NORMAL,
			false,
			face_name
		);

		face_name_list.Add(GetFaceName(font));

	}

	face_name_list.Sort();
	return face_name_list;

#else

	wxArrayString font_list = wxFontEnumerator::GetFacenames();
	font_list.Sort();
	return font_list;

#endif

}

wxString GetFaceName(const wxFont& font) {
#ifdef _WIN32

	HDC dc = CreateCompatibleDC(nullptr);
	if (dc == nullptr)
		throw agi::EnvironmentError("Failed to initialize the HDC");
	agi::scoped_holder<HDC> dc_sh(dc, [](HDC dc) { DeleteDC(dc); });
;
	WXHFONT hfont = font.GetHFONT();
	SelectFont(dc_sh, hfont);

	UINT otm_size = GetOutlineTextMetricsW(dc_sh, 0, nullptr);
	if (!otm_size)
		throw agi::EnvironmentError("Failed to initialize the otm_size");

	OUTLINETEXTMETRICW* otm = reinterpret_cast<OUTLINETEXTMETRICW*>(malloc(otm_size));
	agi::scoped_holder<OUTLINETEXTMETRICW*> otm_sh(otm, [](OUTLINETEXTMETRICW* otm) { free(otm); });

	otm->otmSize = otm_size;
	if (!GetOutlineTextMetricsW(dc_sh, otm_size, otm_sh))
		throw agi::EnvironmentError("Failed to initialize the otm");

	return reinterpret_cast<wxChar*>(otm) + wxPtrToUInt(otm->otmpFamilyName)/sizeof(wxChar);
#else

	return font.GetFaceName();

#endif
}
