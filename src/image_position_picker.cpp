#include "image_position_picker.h"
BEGIN_EVENT_TABLE(ImagePositionPicker, wxPanel)
	// some useful events
	/*
	 EVT_MOTION(ImagePositionPicker::mouseMoved)
	 EVT_LEFT_DOWN(ImagePositionPicker::mouseDown)
	 EVT_LEFT_UP(ImagePositionPicker::mouseReleased)
	 EVT_RIGHT_DOWN(ImagePositionPicker::rightClick)
	 EVT_LEAVE_WINDOW(ImagePositionPicker::mouseLeftWindow)
	 EVT_KEY_DOWN(ImagePositionPicker::keyPressed)
	 EVT_KEY_UP(ImagePositionPicker::keyReleased)
	 EVT_MOUSEWHEEL(ImagePositionPicker::mouseWheelMoved)
	 */

	 // catch paint events
	EVT_PAINT(ImagePositionPicker::paintEvent)
	//Size event
	EVT_SIZE(ImagePositionPicker::OnSize)
	EVT_MOUSE_EVENTS(ImagePositionPicker::OnMouseEvent)
END_EVENT_TABLE()


// some useful events
/*
 void ImagePositionPicker::mouseMoved(wxMouseEvent& event) {}
 void ImagePositionPicker::mouseDown(wxMouseEvent& event) {}
 void ImagePositionPicker::mouseWheelMoved(wxMouseEvent& event) {}
 void ImagePositionPicker::mouseReleased(wxMouseEvent& event) {}
 void ImagePositionPicker::rightClick(wxMouseEvent& event) {}
 void ImagePositionPicker::mouseLeftWindow(wxMouseEvent& event) {}
 void ImagePositionPicker::keyPressed(wxKeyEvent& event) {}
 void ImagePositionPicker::keyReleased(wxKeyEvent& event) {}
 */

ImagePositionPicker::ImagePositionPicker(wxWindow* parent, wxImage i, updator upd) : wxPanel(parent)
{
	image = i;
	prevW = -1;
	prevH = -1;
	w = image.GetWidth();
	h = image.GetHeight();
	update = upd;
}

/*
 * Called by the system of by wxWidgets when the panel needs
 * to be redrawn. You can also trigger this call by
 * calling Refresh()/Update().
 */

void ImagePositionPicker::paintEvent(wxPaintEvent& evt)
{
	// depending on your system you may need to look at double-buffered dcs
	wxPaintDC dc(this);
	render(dc);
}

/*
 * Alternatively, you can use a clientDC to paint on the panel
 * at any time. Using this generally does not free you from
 * catching paint events, since it is possible that e.g. the window
 * manager throws away your drawing when the window comes to the
 * background, and expects you will redraw it when the window comes
 * back (by sending a paint event).
 */
void ImagePositionPicker::paintNow()
{
	// depending on your system you may need to look at double-buffered dcs
	wxClientDC dc(this);
	render(dc);
}

/*
 * Here we do the actual rendering. I put it in a separate
 * method so that it can work no matter what type of DC
 * (e.g. wxPaintDC or wxClientDC) is used.
 */
void ImagePositionPicker::render(wxDC& dc)
{
	int neww, newh;
	dc.GetSize(&neww, &newh);

	if (neww != prevW || newh != prevH)
	{
		// keep the image proportionate
		int ww, hh;
		if (double(neww) / w >= double(newh) / h) // too long
		{
			ww = newh * w / h;
			hh = newh;
		}
		else
		{
			ww = neww;
			hh = neww * h / w;
		}
		resized = wxBitmap(image.Scale(ww, hh /*, wxIMAGE_QUALITY_HIGH*/));
		prevW = ww;
		prevH = hh;
		dc.DrawBitmap(resized, 0, 0, false);
	}
	else {
		dc.DrawBitmap(resized, 0, 0, false);
	}
}

/*
 * Here we call refresh to tell the panel to draw itself again.
 * So when the user resizes the image panel the image should be resized too.
 */
void ImagePositionPicker::OnSize(wxSizeEvent& event) {
	Refresh();
	//skip the event.
	event.Skip();
}

void ImagePositionPicker::OnMouseEvent(wxMouseEvent& evt)
{
	wxPoint pos = evt.GetPosition();
	if (evt.LeftUp())
	{
		int x = pos.x * w / prevW;
		int y = pos.y * h / prevH;
		if (x >= 0 && x < w && y >= 0 && y < h)
			update(x, y, image.GetRed(x, y), image.GetGreen(x, y), image.GetBlue(x, y));
	}
	else if (evt.LeftDClick()) {
		// Propagate the double click event to submit
		evt.ResumePropagation(wxEVENT_PROPAGATE_MAX);
		evt.Skip();
	}
}

void ImagePositionPicker::changeImage(wxImage i)
{
	image = i;
	prevW = -1;
	prevH = -1;
	w = image.GetWidth();
	h = image.GetHeight();
	paintNow();
}
