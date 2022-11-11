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

/// @file visual_tool_perspective.cpp
/// @brief 3D perspective visual typesetting tool
/// @ingroup visual_ts

#include "visual_tool_perspective.h"

#include "command/command.h"
#include "compat.h"
#include "include/aegisub/context.h"
#include "options.h"
#include "selection_controller.h"
#include "vector3d.h"

#include <libaegisub/format.h>
#include <libaegisub/log.h>

#include <cmath>
#include <wx/colour.h>

static const float pi = 3.1415926536f;
static const float deg2rad = pi / 180.f;
static const float rad2deg = 180.f / pi;
static const float screen_z = 312.5;

static const int BUTTON_ID_BASE = 1400;

void VisualToolPerspective::Solve2x2(float a11, float a12, float a21, float a22, float b1, float b2, float &x1, float &x2) {
	// Simple pivoting
	if (abs(a11) < abs(a21)) {
		std::swap(b1, b2);
		std::swap(a11, a21);
		std::swap(a12, a22);
	}
	// LU decomposition
	// i = 1
	a21 = a21 / a11;
	// i = 2
	a22 = a22 - a21 * a12;
	// forward substitution
	float z1 = b1;
	float z2 = b2 - a21 * z1;
	// backward substitution
	x2 = z2 / a22;
	x1 = (z1 - a12 * x2) / a11;
}

void VisualToolPerspective::UnwrapQuadRel(std::vector<Vector2D> quad, float &x1, float &x2, float &x3, float &x4, float &y1, float &y2, float &y3, float &y4) {
	x1 = quad[0].X();
	x2 = quad[1].X() - x1;
	x3 = quad[2].X() - x1;
	x4 = quad[3].X() - x1;
	y1 = quad[0].Y();
	y2 = quad[1].Y() - y1;
	y3 = quad[2].Y() - y1;
	y4 = quad[3].Y() - y1;
}

Vector2D VisualToolPerspective::XYToUV(std::vector<Vector2D> quad, Vector2D xy) {
	float x1, x2, x3, x4, y1, y2, y3, y4;
	UnwrapQuadRel(quad, x1, x2, x3, x4, y1, y2, y3, y4);
	float x = xy.X() - x1;
	float y = xy.Y() - y1;
	// Dumped from Mathematica 
	float u = -(((x3*y2 - x2*y3)*(x4*y - x*y4)*(x4*(-y2 + y3) + x3*(y2 - y4) + x2*(-y3 + y4)))/(x3*x3*(x4*y2*y2*(-y + y4) + y4*(x*y2*(y2 - y4) + x2*(y - y2)*y4)) + x3*(x4*x4*y2*y2*(y - y3) + 2*x4*(x2*y*y3*(y2 - y4) + x*y2*(-y2 + y3)*y4) + x2*y4*(x2*(-y + y3)*y4 + 2*x*y2*(-y3 + y4))) + y3*(x*x4*x4*y2*(y2 - y3) + x2*x4*x4*(y2*y3 + y*(-2*y2 + y3)) - x2*x2*(x4*y*(y3 - 2*y4) + x4*y3*y4 + x*y4*(-y3 + y4)))));
	float v = ((x2*y - x*y2)*(x4*y3 - x3*y4)*(x4*(y2 - y3) + x2*(y3 - y4) + x3*(-y2 + y4)))/(x3*(x4*x4*y2*y2*(-y + y3) + x2*y4*(2*x*y2*(y3 - y4) + x2*(y - y3)*y4) - 2*x4*(x2*y*y3*(y2 - y4) + x*y2*(-y2 + y3)*y4)) + x3*x3*(x4*y2*y2*(y - y4) + y4*(x2*(-y + y2)*y4 + x*y2*(-y2 + y4))) + y3*(x*x4*x4*y2*(-y2 + y3) + x2*x4*x4*(2*y*y2 - y*y3 - y2*y3) + x2*x2*(x4*y*(y3 - 2*y4) + x4*y3*y4 + x*y4*(-y3 + y4))));
	return Vector2D(u, v);
}

Vector2D VisualToolPerspective::UVToXY(std::vector<Vector2D> quad, Vector2D uv) {
	float x1, x2, x3, x4, y1, y2, y3, y4;
	UnwrapQuadRel(quad, x1, x2, x3, x4, y1, y2, y3, y4);
	float u = uv.X();
	float v = uv.Y();
    // Also dumped from Mathematica
	float d = (x4*((-1 + u + v)*y2 + y3 - v*y3) + x3*(y2 - u*y2 + (-1 + v)*y4) + x2*((-1 + u)*y3 - (-1 + u + v)*y4));
	float x = (v*x4*(x3*y2 - x2*y3) + u*x2*(x4*y3 - x3*y4)) / d;
	float y = (v*y4*(x3*y2 - x2*y3) + u*y2*(x4*y3 - x3*y4)) / d;
	return Vector2D(x + x1, y + y1);
}

void VisualToolPerspective::AddTool(std::string command_name, VisualToolPerspectiveSetting setting) {
	cmd::Command *command;
	try {
		command = cmd::get(command_name);
	}
	catch (cmd::CommandNotFound const&) {
		// Toolbar names are all hardcoded so this should never happen
		throw agi::InternalError("Toolbar named " + command_name + " not found.");
	}

	int icon_size = OPT_GET("App/Toolbar Icon Size")->GetInt();
	toolBar->AddTool(BUTTON_ID_BASE + setting, command->StrDisplay(c), command->Icon(icon_size), command->GetTooltip("Video"), wxITEM_CHECK);
}

VisualToolPerspective::VisualToolPerspective(VideoDisplay *parent, agi::Context *context)
: VisualTool<VisualDraggableFeature>(parent, context)
{
	MakeFeatures();
	old_outer.resize(4);
	old_inner.resize(4);
}

void VisualToolPerspective::SetToolbar(wxToolBar *toolBar) {
	this->toolBar = toolBar;

	toolBar->AddSeparator();

	AddTool("video/tool/perspective/plane", PERSP_PLANE);
	AddTool("video/tool/perspective/grid", PERSP_GRID);

	SetSubTool(0);
	toolBar->Realize();
	toolBar->Show(true);
	toolBar->Bind(wxEVT_TOOL, [=](wxCommandEvent& e) { SetSubTool(GetSubTool() ^ (e.GetId() - BUTTON_ID_BASE)); });
}

void VisualToolPerspective::SetSubTool(int subtool) {
	if (toolBar == nullptr) {
		throw agi::InternalError("Vector clip toolbar hasn't been set yet!");
	}
	// Manually enforce radio behavior as we want one selection in the bar
	// rather than one per group
	for (int i = 1; i < PERSP_LAST; i <<= 1)
		toolBar->ToggleTool(BUTTON_ID_BASE + i, i & subtool);

	settings = subtool;
	MakeFeatures();
}

int VisualToolPerspective::GetSubTool() {
	return settings;
}

bool VisualToolPerspective::HasOuter() {
	return GetSubTool() & PERSP_PLANE;
}

std::vector<Vector2D> VisualToolPerspective::MakeRect(Vector2D a, Vector2D b) {
	return std::vector<Vector2D>({
		Vector2D(a.X(), a.Y()),
		Vector2D(b.X(), a.Y()),
		Vector2D(b.X(), b.Y()),
		Vector2D(a.X(), b.Y()),
	});
}

std::vector<Vector2D> VisualToolPerspective::FeaturePositions(std::vector<Feature *> features) {
	std::vector<Vector2D> result;
	for (int i = 0; i < 4; i++) {
		result.push_back(features[i]->pos);
	}
	return result;
}

void VisualToolPerspective::UpdateInner() {
	std::vector<Vector2D> uv = MakeRect(c1, c2);
	std::vector<Vector2D> quad = FeaturePositions(outer_corners);
	for (int i = 0; i < 4; i++)
		inner_corners[i]->pos = UVToXY(quad, uv[i]);
}

void VisualToolPerspective::UpdateOuter() {
	if (!HasOuter())
		return;
	std::vector<Vector2D> uv = MakeRect(-c1 / (c2 - c1), (1 - c1) / (c2 - c1));
	std::vector<Vector2D> quad = FeaturePositions(inner_corners);
	for (int i = 0; i < 4; i++)
		outer_corners[i]->pos = UVToXY(quad, uv[i]);
}

void VisualToolPerspective::MakeFeatures() {
	sel_features.clear();
	features.clear();
	active_feature = nullptr;

	inner_corners.clear();
	outer_corners.clear();

	orgf = new Feature;
	orgf->type = DRAG_BIG_TRIANGLE;
	features.push_back(*orgf);

	for (int i = 0; i < 4; i++) {
		inner_corners.push_back(new Feature);
		inner_corners.back()->type = DRAG_SMALL_CIRCLE;
		features.push_back(*inner_corners.back());

		if (HasOuter()) {
			outer_corners.push_back(new Feature);
			outer_corners.back()->type = DRAG_SMALL_CIRCLE;
			features.push_back(*outer_corners.back());
		}
	}

	TextToPersp();
}

void VisualToolPerspective::Draw() {
	if (!active_line) return;

	wxColour line_color = to_wx(line_color_primary_opt->GetColor());
	wxColour line_color_secondary = to_wx(line_color_secondary_opt->GetColor());

	// Draw Quad
	gl.SetLineColour(line_color);
	for (int i = 0; i < 4; i++) {
		if (HasOuter()) {
			gl.DrawDashedLine(outer_corners[i]->pos, outer_corners[(i + 1) % 4]->pos, 6);
			gl.DrawLine(inner_corners[i]->pos, inner_corners[(i + 1) % 4]->pos);
		} else {
			gl.DrawDashedLine(inner_corners[i]->pos, inner_corners[(i + 1) % 4]->pos, 6);
		}
	}

	DrawAllFeatures();

	if (GetSubTool() & PERSP_GRID) {
		// Draw Grid - Copied and modified from visual_tool_rotatexy.cpp

		// Number of lines on each side of each axis
		static const int radius = 15;
		// Total number of lines, including center axis line
		static const int line_count = radius * 2 + 1;
		// Distance between each line in pixels
		static const int spacing = 20;
		// Length of each grid line in pixels from axis to one end
		static const int half_line_length = spacing * (radius + 1);
		static const float fade_factor = 0.9f / radius;

		// Transform grid
		gl.SetOrigin(FromScriptCoords(org));
		gl.SetScale(100 * video_res / script_res);
		gl.SetRotation(angle_x, angle_y, angle_z);
		gl.SetScale(fsc);
		gl.SetShear(fax, fay);
		gl.SetScale(100 * textheight * Vector2D(1, 1) / spacing / 4);

		// Draw grid
		gl.SetLineColour(line_color_secondary, 0.5f, 2);
		gl.SetModeLine();
		float r = line_color_secondary.Red() / 255.f;
		float g = line_color_secondary.Green() / 255.f;
		float b = line_color_secondary.Blue() / 255.f;

		std::vector<float> colors(line_count * 8 * 4);
		for (int i = 0; i < line_count * 8; ++i) {
			colors[i * 4 + 0] = r;
			colors[i * 4 + 1] = g;
			colors[i * 4 + 2] = b;
			colors[i * 4 + 3] = (i + 3) % 4 > 1 ? 0 : (1.f - abs(i / 8 - radius) * fade_factor);
		}

		std::vector<float> points(line_count * 8 * 2);
		for (int i = 0; i < line_count; ++i) {
			int pos = spacing * (i - radius);

			points[i * 16 + 0] = pos;
			points[i * 16 + 1] = half_line_length;

			points[i * 16 + 2] = pos;
			points[i * 16 + 3] = 0;

			points[i * 16 + 4] = pos;
			points[i * 16 + 5] = 0;

			points[i * 16 + 6] = pos;
			points[i * 16 + 7] = -half_line_length;

			points[i * 16 + 8] = half_line_length;
			points[i * 16 + 9] = pos;

			points[i * 16 + 10] = 0;
			points[i * 16 + 11] = pos;

			points[i * 16 + 12] = 0;
			points[i * 16 + 13] = pos;

			points[i * 16 + 14] = -half_line_length;
			points[i * 16 + 15] = pos;
		}

		gl.DrawLines(2, points, 4, colors);
	}
}

void VisualToolPerspective::UpdateDrag(Feature *feature) {
	if (feature == orgf) {
		if (HasOuter()) {
			std::vector<Vector2D> quad = FeaturePositions(outer_corners);
			Vector2D olduv = XYToUV(quad, old_orgf);
			Vector2D newuv = XYToUV(quad, orgf->pos);
			c1 = c1 + newuv - olduv;
			c2 = c2 + newuv - olduv;
			UpdateInner();
		} else {
			Vector2D diff = orgf->pos - old_orgf;
			for (int i = 0; i < 4; i++) {
				inner_corners[i]->pos = inner_corners[i]->pos + diff;
			}
		}
	}

	std::vector<Feature *> changed_quad;

	for (int i = 0; i < 4; i++) {
		if (HasOuter() && feature == outer_corners[i]) {
			changed_quad = outer_corners;
			UpdateInner();
		} else if (feature == inner_corners[i]) {
			changed_quad = inner_corners;
		}
	}

	if (!changed_quad.empty()) {
		// Validate: If the quad isn't convex, the intersection of the diagonals will not lie inside it.
		Vector2D diag1 = changed_quad[2]->pos - changed_quad[0]->pos;
		Vector2D diag2 = changed_quad[1]->pos - changed_quad[3]->pos;
		Vector2D b = changed_quad[3]->pos - changed_quad[0]->pos;
		float center_la1, center_la2;
		Solve2x2(diag1.X(), diag2.X(), diag1.Y(), diag2.Y(), b.X(), b.Y(), center_la1, center_la2);
		if (center_la1 < 0 || center_la1 > 1 || -center_la2 < 0 || -center_la2 > 1) {
			ResetFeaturePositions();
			return;
		}
	}

	for (int i = 0; i < 4; i++) {
		if (HasOuter() && feature == inner_corners[i]) {
			Vector2D newuv = XYToUV(FeaturePositions(outer_corners), feature->pos);
			c1 = Vector2D(i == 0 || i == 3 ? newuv.X() : c1.X(), i < 2 ? newuv.Y() : c1.Y());
			c2 = Vector2D(i == 0 || i == 3 ? c2.X() : newuv.X(), i < 2 ? c2.Y() : newuv.Y());
			UpdateInner();
		}
	}

	InnerToText();
	SetFeaturePositions();
}

void VisualToolPerspective::InnerToText() {
	Vector2D q1 = ToScriptCoords(inner_corners[0]->pos);
	Vector2D q2 = ToScriptCoords(inner_corners[1]->pos);
	Vector2D q3 = ToScriptCoords(inner_corners[2]->pos);
	Vector2D q4 = ToScriptCoords(inner_corners[3]->pos);

	Vector2D diag1 = q3 - q1;
	Vector2D diag2 = q2 - q4;
	Vector2D b = q4 - q1;
	float center_la1, center_la2;
	Solve2x2(diag1.X(), diag2.X(), diag1.Y(), diag2.Y(), b.X(), b.Y(), center_la1, center_la2);
	Vector2D center = q1 + center_la1 * diag1;

	// Normalize to center
	q1 = q1 - center;
	q2 = q2 - center;
	q3 = q3 - center;
	q4 = q4 - center;

	// Find a parallelogram projecting to the quad
	float z2, z4;
	Vector2D side1 = q2 - q3;
	Vector2D side2 = q4 - q3;
	Solve2x2(side1.X(), side2.X(), side1.Y(), side2.Y(), -diag1.X(), -diag1.Y(), z2, z4);

	float scalefactor = (z2 + z4) / 2;
	Vector3D r1 = Vector3D(q1, screen_z) / scalefactor;
	Vector3D r2 = z2 * Vector3D(q2, screen_z) / scalefactor;
	Vector3D r3 = (z2 + z4 - 1) * Vector3D(q3, screen_z) / scalefactor;
	Vector3D r4 = z4 * Vector3D(q4, screen_z) / scalefactor;

	// Find the rotations
	Vector3D n = (r2 - r1).Cross(r4 - r1);
	float roty = atan(n.X() / n.Z());
	n = n.RotateY(roty);
	float rotx = atan(n.Y() / n.Z());

	Vector3D ab = (r2 - r1).RotateY(roty).RotateX(rotx);
	float rotz = atan(ab.Y() / ab.X());

	Vector3D ad = (r4 - r1).RotateY(roty).RotateX(rotx).RotateZ(-rotz);
	float rawfax = ad.X() / ad.Y();

	float scalex = ab.Len() / textwidth;
	float scaley = abs(ad.Y()) / textheight;

	org = center;
	pos = org - Vector2D(fax_shift_factor * rawfax * scaley, 0);
	angle_x = rotx * rad2deg;
	angle_y = -roty * rad2deg;
	angle_z = -rotz * rad2deg;
	fsc = 100 * Vector2D(scalex, scaley);
	fax = rawfax * scaley / scalex;
	fay = 0;

	for (auto line : c->selectionController->GetSelectedSet()) {
		SetOverride(line, "\\fax", agi::format("%.6f", fax));
		SetOverride(line, "\\fay", agi::format("%.4f", fay)); 	// TODO just kill the tag
		SetOverride(line, "\\fscx", agi::format("%.2f", fsc.X()));
		SetOverride(line, "\\fscy", agi::format("%.2f", fsc.Y()));
		SetOverride(line, "\\frz", agi::format("%.4f", angle_z));
		SetOverride(line, "\\frx", agi::format("%.4f", angle_x));
		SetOverride(line, "\\fry", agi::format("%.4f", angle_y));
		SetOverride(line, "\\org", org.PStr());
		SetOverride(line, "\\pos", pos.PStr());
	}
}

void VisualToolPerspective::ResetFeaturePositions() {
	for (int i = 0; i < 4; i++) {
		inner_corners[i]->pos = old_inner[i];
		if (HasOuter())
			outer_corners[i]->pos = old_outer[i];
	}
	orgf->pos = old_orgf;
}

void VisualToolPerspective::SaveFeaturePositions() {
	for (int i = 0; i < 4; i++) {
		old_inner[i] = inner_corners[i]->pos;
		if (HasOuter())
			old_outer[i] = outer_corners[i]->pos;
	}
	old_orgf = orgf->pos;
}

void VisualToolPerspective::SetFeaturePositions() {
	Vector2D diag1 = inner_corners[2]->pos - inner_corners[0]->pos;
	Vector2D diag2 = inner_corners[1]->pos - inner_corners[3]->pos;
	Vector2D b = inner_corners[3]->pos - inner_corners[0]->pos;
	float center_la1, center_la2;
	Solve2x2(diag1.X(), diag2.X(), diag1.Y(), diag2.Y(), b.X(), b.Y(), center_la1, center_la2);
	orgf->pos = inner_corners[0]->pos + center_la1 * diag1;

	SaveFeaturePositions();
}

void VisualToolPerspective::TextToPersp() {
	if (!active_line) return;

	org = GetLineOrigin(active_line);
	pos = GetLinePosition(active_line);
	if (!org)
		org = pos;

	GetLineRotation(active_line, angle_x, angle_y, angle_z);
	GetLineShear(active_line, fax, fay);
	GetLineScale(active_line, fsc);

	float fs = GetLineFontSize(active_line);
	align = GetLineAlignment(active_line);

	switch (align) {
		case 1:
		case 2:
		case 3:
			fax_shift_factor = fs;
			break;
		case 4:
		case 5:
		case 6:
			fax_shift_factor = fs / 2;
			break;
		default:
			fax_shift_factor = 0.;
			break;
	}

	double descend, extlead;
	GetLineBaseExtents(active_line, textwidth, textheight, descend, extlead);
	double textleft, texttop = 0.;

	switch ((align - 1) % 3) {
		case 1:
			textleft = -textwidth / 2;
			break;
		case 2:
			textleft = -textwidth;
			break;
		default:
			break;
	}
	switch ((align - 1) / 3) {
		case 0:
			texttop = -textheight;
			break;
		case 1:
			texttop = -textheight / 2;
			break;
		default:
			break;
	}

	std::vector<Vector2D> textrect = MakeRect(Vector2D(0, 0), Vector2D(textwidth, textheight));
	for (int i = 0; i < 4; i++) {
		Vector2D p = textrect[i];
		// Apply \fax and \fay
		p = Vector2D(p.X() + p.Y() * fax, p.X() * fay + p.Y());
		// Translate to alignment point
		p = p + Vector2D(textleft, texttop);
		// Apply scaling
		p = Vector2D(p.X() * fsc.X() / 100., p.Y() * fsc.Y() / 100.);
		// Translate relative to origin
		p = p + pos - org;
		// Rotate ZXY
		Vector3D q(p);
		q = q.RotateZ(-angle_z * deg2rad);
		q = q.RotateX(-angle_x * deg2rad);
		q = q.RotateY(angle_y * deg2rad);
		// Project
		q = (screen_z / (q.Z() + screen_z)) * q;
		// Move to origin
		Vector2D r = q.XY() + org;
		inner_corners[i]->pos = FromScriptCoords(r);
	}
	UpdateOuter();
}

void VisualToolPerspective::DoRefresh() {
	TextToPersp();
	SetFeaturePositions();
}

