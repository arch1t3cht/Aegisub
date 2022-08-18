#include <wx/wx.h>
#include <wx/sizer.h>
#include "gl_wrap.h"

typedef std::function<void(double, double, unsigned char, unsigned char, unsigned char)> updator;

class ImagePositionPicker : public wxPanel
{
	wxImage image;
	wxBitmap resized;
	int prevW, prevH, w, h;

	updator update;

public:
	ImagePositionPicker(wxWindow* parent, wxImage i, updator upd);

	void paintEvent(wxPaintEvent & evt);
	void paintNow();
	void OnSize(wxSizeEvent& event);
	void OnMouseEvent(wxMouseEvent& evt);
	void render(wxDC& dc);
	void changeImage(wxImage i);

	// some useful events
	/*
	 void mouseMoved(wxMouseEvent& event);
	 void mouseDown(wxMouseEvent& event);
	 void mouseWheelMoved(wxMouseEvent& event);
	 void mouseReleased(wxMouseEvent& event);
	 void rightClick(wxMouseEvent& event);
	 void mouseLeftWindow(wxMouseEvent& event);
	 void keyPressed(wxKeyEvent& event);
	 void keyReleased(wxKeyEvent& event);
	 */

	DECLARE_EVENT_TABLE()
};
