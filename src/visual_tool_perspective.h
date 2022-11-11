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

class wxToolBar;

/// Button IDs
enum VisualToolPerspectiveSetting {
	PERSP_PLANE = 1 << 0,
	PERSP_GRID = 2 << 1,
	PERSP_LAST = 2 << 2,
};

class VisualToolPerspective final : public VisualTool<VisualDraggableFeature> {
	wxToolBar *toolBar = nullptr; /// The subtoolbar
	int settings = 0; 

	// All current transform coefficients. Used for drawing the grid.
	float angle_x = 0.f;
	float angle_y = 0.f;
	float angle_z = 0.f;

	float fax = 0.f;
	float fay = 0.f;

	int align = 0;
	float fax_shift_factor = 0.f;

	double textwidth = 0.f;
	double textheight = 0.f;

	Vector2D fsc;

	Vector2D org;
	Vector2D pos;

    // Corner coordinates of the transform quad relative to the ambient quad.
    Vector2D c1 = Vector2D(.25, .25);
    Vector2D c2 = Vector2D(.75, .75);

	Feature *orgf;
	Vector2D old_orgf;

	std::vector<Vector2D> old_inner;
	std::vector<Vector2D> old_outer;

	std::vector<Feature *> inner_corners;
	std::vector<Feature *> outer_corners;

	void Solve2x2Proper(float a11, float a12, float a21, float a22, float b1, float b2, float &x1, float &x2);
	void Solve2x2(float a11, float a12, float a21, float a22, float b1, float b2, float &x1, float &x2);
    void UnwrapQuadRel(std::vector<Vector2D> quad, float &x1, float &x2, float &x3, float &x4, float &y1, float &y2, float &y3, float &y4);
    Vector2D XYToUV(std::vector<Vector2D> quad, Vector2D xy);
    Vector2D UVToXY(std::vector<Vector2D> quad, Vector2D uv);

	std::vector<Vector2D> FeaturePositions(std::vector<Feature *> features);
    std::vector<Vector2D> MakeRect(Vector2D a, Vector2D b);
    void UpdateInner();
    void UpdateOuter();
    void TextToPersp();
    void InnerToText();

	void DoRefresh() override;
	void Draw() override;
	void UpdateDrag(Feature *feature) override;
    void MakeFeatures();
	void SetFeaturePositions();
	void ResetFeaturePositions();
	void SaveFeaturePositions();

	void AddTool(std::string command_name, VisualToolPerspectiveSetting mode);
	bool HasOuter();

public:
	VisualToolPerspective(VideoDisplay *parent, agi::Context *context);

	void SetToolbar(wxToolBar *tb) override;
	void SetSubTool(int subtool) override;
	int GetSubTool() override;
};
