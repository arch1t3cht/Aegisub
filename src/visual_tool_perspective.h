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

/// @file visual_tool_perspective.h
/// @see visual_tool_perspective.cpp
/// @ingroup visual_ts
///

#include "visual_feature.h"
#include "visual_tool.h"
#include "options.h"

class wxToolBar;

/// Button IDs
enum VisualToolPerspectiveSetting {
	PERSP_OUTER = 1 << 0,
	PERSP_LOCK_OUTER = 1 << 1,
	PERSP_GRID = 1 << 2,
	PERSP_LAST = 1 << 3,    // End of simple toggle-able options
	PERSP_ORGMODE_CENTER = 0 << 4,    // Always puts \org at the center of the quad. Default.
	PERSP_ORGMODE_NOFAX = 1 << 4,     // Picks a position for \org where \fax = 0, when possible
	PERSP_ORGMODE_KEEP = 2 << 4,      // Takes the previous \org position as \org
    PERSP_ORGMODE = PERSP_ORGMODE_CENTER | PERSP_ORGMODE_NOFAX | PERSP_ORGMODE_KEEP,
};

class VisualToolPerspective;

class VisualToolPerspectiveDraggableFeature final : public VisualDraggableFeature {
	VisualToolPerspective *tool;

public:
	int group;
	int index;

	VisualToolPerspectiveDraggableFeature(VisualToolPerspective *tool, int group, int index);
	void UpdateDrag(Vector2D d, bool single_axis);
};

class VisualToolPerspective final : public VisualTool<VisualToolPerspectiveDraggableFeature> {
	wxToolBar *toolBar = nullptr; /// The subtoolbar
	int settings = 0;

	agi::OptionValue* optOuter;
	agi::OptionValue* optOuterLocked;
	agi::OptionValue* optGrid;
	agi::OptionValue* optOrgMode;

	// All current transform coefficients. Used for drawing the grid.
	float angle_x = 0.f;
	float angle_y = 0.f;
	float angle_z = 0.f;

	float fax = 0.f;
	float fay = 0.f;

	int align = 0;

	// Corners of the bounding box of the event without any formatting.
	// The top left corner is the zero vector for text but might not be for drawings.
	std::pair<Vector2D, Vector2D> bbox;

	Vector2D fsc;

	Vector2D org;
	Vector2D pos;

	// Store these here to reduce rounding errors compounding on updates
	Vector2D bord;
	Vector2D shad;

    // Corner coordinates of the transform quad relative to the ambient quad.
    Vector2D c1 = Vector2D(.25, .25);
    Vector2D c2 = Vector2D(.75, .75);

	Feature *centerf;
	Feature *orgf;
	Vector2D old_centerf;

	std::vector<Feature *> inner_corners;
	std::vector<Feature *> outer_corners;

	std::vector<Vector2D> FeaturePositions(std::vector<Feature *> features) const;
    void UpdateInner();
    void UpdateOuter();
    void TextToPersp();
    bool InnerToText();

    void WrapSetOverride(AssDialogue* line, std::string const& tag, float value, int precision, float defaultval=0);

	void OnMouseEvent(wxMouseEvent &event) override;
	void DoRefresh() override;
	void Draw() override;
	void OnDoubleClick() override;
	void UpdateDrag(Feature *feature) override;
	void EndDrag(Feature *feature) override;
    void MakeFeatures();
	void SetFeaturePositions();
	void SaveFeaturePositions();
	void SaveOuterToLines();

	void AddTool(std::string command_name, VisualToolPerspectiveSetting mode);

public:
	bool ctrl_down = false;
	bool shift_down = false;
	bool alt_down = false;

	std::vector<Vector2D> old_inner;
	std::vector<Vector2D> old_outer;

	VisualToolPerspective(VideoDisplay *parent, agi::Context *context);

	bool HasOuter();
	bool OuterLocked();
	int GetOrgMode();
	bool HasOrgf();

	void SetToolbar(wxToolBar *tb) override;
	void OnSubTool(wxCommandEvent &);
	void SetSubTool(int subtool) override;
	int GetSubTool() override;
};
