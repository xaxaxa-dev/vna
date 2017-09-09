/*
 * polarView.C
 * 
 * Copyright 2012  <xaxaxa@xaxaxa-mac>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 * 
 * 
 */
#include <gtkmm.h>
#include <gtkmm/drawingarea.h>
#include <gdk/gdk.h>
#include <iostream>
#include <math.h>
#include <complex>

using namespace std;
using namespace Gdk;
using namespace Gtk;
//using namespace ::Cairo;
namespace xaxaxa
{
	class PolarView: public Gtk::DrawingArea
	{
	public:
		vector<complex<double> > points;
		double scale=1.;
		double margin=10;	// pixels
		
		double w,h;
		double r,centerX,centerY;
		int selectedPoint=0;
		
		double ptX(double x) {
			return centerX+x/scale*r;
		}
		double ptY(double y) {
			return centerY-y/scale*r;
		}
		void do_draw(const ::Cairo::RefPtr< ::Cairo::Context>& gc)
		{
			w=get_allocation().get_width();
			h=get_allocation().get_height();
			
			// calculate position and radius of smith chart
			centerX=w/2;
			centerY=h/2;
			r=w/2;
			if(h/2<r) r=h/2;
			r -= margin;
			
			// draw outer circle
			gc->set_line_width(2.0);
			gc->arc(centerX, centerY, r, 0.0, 2 * M_PI);
			gc->stroke();
			
			gc->set_line_width(1.0);
			gc->set_source_rgb(.2,.2,.2);
			
			// draw constant resistance circles
			for(int i=1;i<4;i++) {
				gc->arc(centerX+r-r/4*i, centerY, r/4*i, 0.0, 2 * M_PI);
				gc->stroke();
			}
			
			// draw constant reactance circles
			gc->save();
			gc->arc(centerX, centerY, r, 0.0, 2 * M_PI);
			gc->clip();
			int n=1;
			for(int i=1;i<4;i++) {
				gc->arc(centerX+r, centerY+r/2*n, r/2*n, 0.0, 2 * M_PI);
				gc->stroke();
				gc->arc(centerX+r, centerY-r/2*n, r/2*n, 0.0, 2 * M_PI);
				gc->stroke();
				n*=2;
			}
			gc->restore();
			
			// draw center line
			gc->move_to(centerX-r,centerY);
			gc->line_to(centerX+r,centerY);
			gc->stroke();
			
			if(points.size()==0) return;
			gc->set_line_width(2.0);
			gc->set_source_rgb(0,0,1);
			for(int i=1;i<(int)points.size();i++) {
				if(isnan(points[i-1].real()) || isnan(points[i].real())) continue;
				gc->move_to(ptX(points[i-1].real()), ptY(points[i-1].imag()));
				gc->line_to(ptX(points[i].real()), ptY(points[i].imag()));
				gc->stroke();
			}
			
			gc->set_source_rgb(1,1,0);
			draw_point(gc,points[selectedPoint].real(),points[selectedPoint].imag(),3);
		}
		void draw_point(const ::Cairo::RefPtr< ::Cairo::Context>& gc, double re, double im, double size) {
			double pointX=centerX+re/scale*r;
			double pointY=centerY-im/scale*r;
			gc->arc(pointX, pointY, size, 0.0, 2 * M_PI);
			gc->stroke();
		}
		virtual bool on_motion_notify_event(GdkEventMotion* event)
		{
			Gtk::DrawingArea::on_motion_notify_event(event);
			
			return false;
		}
		virtual bool on_button_press_event(GdkEventButton* event)
		{
			Gtk::DrawingArea::on_button_press_event(event);
			return false;
		}
		virtual bool on_button_release_event(GdkEventButton* event)
		{
			Gtk::DrawingArea::on_button_release_event(event);
			return false;
		}
		void do_draw(GdkEventExpose* evt=NULL)
		{
			Glib::RefPtr<Gdk::Window> window = get_window();
			if(window)
			{
				::Cairo::RefPtr< ::Cairo::Context> ctx = window->create_cairo_context();
				if(evt)
				{
					ctx->rectangle(evt->area.x, evt->area.y, evt->area.width, evt->area.height);
					ctx->clip();
				}
				do_draw(ctx);
			}
		}
		virtual bool on_expose_event(GdkEventExpose* evt)
		{
			printf("on_expose_event\n");
			do_draw(evt);
			return false;
		}
		virtual bool on_draw(const ::Cairo::RefPtr<::Cairo::Context>& cr) {
			do_draw(cr);
			return false;
		}
		PolarView(): Gtk::DrawingArea()
		{
			set_app_paintable(true);
			set_double_buffered(true);
			set_redraw_on_allocate(true);
			set_events(get_events()|EXPOSURE_MASK|POINTER_MOTION_MASK|BUTTON_MOTION_MASK|BUTTON_PRESS_MASK|BUTTON_RELEASE_MASK);
		}
	};
}

