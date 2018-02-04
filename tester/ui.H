#include "simple_ili9341.H"
#include <cairomm/cairomm.h>
#include <sstream>

using namespace ::Cairo;
using namespace std;

simple_ili9341 ili9341 {
	.pin_cs = 15,
	.pin_sda = 1,
	.pin_scl = 0,
	.pin_dc = 16
};

constexpr int W=240, H=320;
constexpr int pixels=W*H;
uint16_t fb[pixels];
RefPtr<ImageSurface> imageSurface;
RefPtr<Context> context;
pthread_mutex_t ui_mutex;

void ui_init() {
	pthread_mutex_init(&ui_mutex, NULL);
	ili9341.init();
	ili9341.reset();
	
	imageSurface = ImageSurface::create((unsigned char*)fb, FORMAT_RGB16_565, W, H, W*2);
	context = Context::create(imageSurface);
	auto transform = rotation_matrix(M_PI/2);
	//transform.translate(0, -W);
	transform.scale(1, -1);
	context->set_matrix(transform);
	
	
	context->set_line_width(1);
	context->set_source_rgb(1,1,1);
	
	FontOptions fo;
	fo.set_antialias(ANTIALIAS_NONE);
	fo.set_hint_style(HINT_STYLE_FULL);
	
	context->select_font_face("Liberation Mono", FONT_SLANT_NORMAL, FONT_WEIGHT_NORMAL);
	context->set_antialias(ANTIALIAS_NONE);
	context->set_font_options(fo);
	context->set_font_size(30);
	
	return;
	
	context->move_to(0, 30);
	context->show_text("gsfdghk");
	context->stroke();
	
	context->move_to(0, 30);
	context->line_to(100, 30);
	
	context->move_to(20, 0);
	context->line_to(20, 50);
	context->move_to(40, 0);
	context->line_to(40, 50);
	context->stroke();
	
	ili9341.writeFrame(fb,pixels);
}

void set_rgb(RefPtr<Context> context, uint32_t color) {
	context->set_source_rgb((color>>16)/255.,((color>>8)&0xff)/255.,(color&0xff)/255.);
}

void draw_text(RefPtr<Context> context, string text) {
	stringstream ss(text);
	string line;
    while (getline(ss, line, '\n')) {
		TextExtents ext;
        context->get_text_extents(line, ext);
        context->show_text(line);
        context->rel_move_to(-ext.x_advance, ext.height-ext.y_advance);
    }
}
void ui_show_banner(string text, uint32_t bgcolor=0x000000, uint32_t color=0xffffff) {
	pthread_mutex_lock(&ui_mutex);
	printf("%s\n", text.c_str());
	
	set_rgb(context, bgcolor);
	context->paint();
	set_rgb(context, color);
	
	context->set_font_size(45);
	
	TextExtents ext;
	context->get_text_extents(text, ext);
	context->move_to((H-ext.x_advance)/2, W/2);
	context->show_text(text);
	
	ili9341.writeFrame(fb,pixels);
	pthread_mutex_unlock(&ui_mutex);
}
void ui_show_details(string title, string details, uint32_t bgcolor=0x000000, uint32_t color=0xffffff) {
	pthread_mutex_lock(&ui_mutex);
	printf("%s\n", title.c_str());
	
	set_rgb(context, bgcolor);
	context->paint();
	set_rgb(context, color);
	
	context->set_font_size(35);
	context->move_to(5, 35);
	context->show_text(title);
	
	context->set_font_size(18);
	context->move_to(5, 72);
	draw_text(context, details);
	ili9341.writeFrame(fb,pixels);
	pthread_mutex_unlock(&ui_mutex);
}
