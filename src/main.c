#include <pebble.h>
#include "autoconfig.h"
#include "gbitmap_color_palette_manipulator.h"

static Window *window = NULL;
static Layer  *layer = NULL;

static GBitmap *bt_disconnected = NULL;

static GFont custom_font;
static GFont small_font;
static GFont medium_font;

static GPath *hour_arrow;
static const GPathInfo LINE_HAND_POINTS =  {4,(GPoint []) {{-3, 0},{-3, -300},{3, -300},{3, 0}}};
static const GPathInfo BIG_LINE_HAND_POINTS =  {4,(GPoint []) {{-5, 0},{-5, -300},{5, -300},{5, 0}}};
static const GPathInfo ARROW_HAND_POINTS = {4,(GPoint []) {{-9, 0},{-2, -175},{2, -175},{9, 0}}};

static const GPathInfo LINE_HAND_24_POINTS =  {4,(GPoint []) {{-3, 0},{-3, -300-150},{3, -300-150},{3, 0}}};
static const GPathInfo BIG_LINE_HAND_24_POINTS =  {4,(GPoint []) {{-5, 0},{-5, -300-150},{5, -300-150},{5, 0}}};
static const GPathInfo ARROW_HAND_24_POINTS = {4,(GPoint []) {{-9*2, 0},{-2, -175-150},{2, -175-150},{9*2, 0}}};

static char* txt[] = {"0","1","2","3","4","5","6","7","8","9","10","11","12","13","14","15","16","17","18","19","20","21","22","23"};
static char info_text[5] = "0000";

static Animation* animation;
static AnimationImplementation animImpl;

static bool isAnimating = false;
static uint8_t percent = 100;

static bool btConnected = false;

static GColor bg_color;
static GColor bg_circle_color;
static GColor text_and_dots_color;
static GColor dots_outline_color;

static GColor hand_color;
static GColor hand_outline_color;

static int hours = 0;
static int minutes = 0;
static int day = 0;

static int layer_update_count = 0;

#define DATE_POSITION_FROM_CENTER 100
#define SECOND_HAND_LENGTH_A 150
#define SECOND_HAND_LENGTH_C 180

#define SMALL_DOT_RADIUS 3
#define BIG_DOT_RADIUS 6
static const GPathInfo SMALL_LINE_MARK_POINTS =  {4,(GPoint []) {{-3, 2},{-3, -6},{3, -6},{3, 2}}};
static const GPathInfo BIG_LINE_MARK_POINTS   =  {4,(GPoint []) {{-3, 4},{-3, -8},{3, -8},{3, 4}}};

static GPath *small_line_mark_path;
static GPath *big_line_mark_path;

#ifdef PBL_ROUND
#define SCREEN_WIDTH 180
#define SCREEN_HEIGHT 180
#else
#define SCREEN_WIDTH 144
#define SCREEN_HEIGHT 168
#endif

static bool containsCircle(GPoint center, int radius){
	return center.x - radius > 0 && center.x + radius < SCREEN_WIDTH && center.y - radius > 0 && center.y + radius < SCREEN_HEIGHT;
}

static void handle_minute_tick(struct tm *tick_time, TimeUnits units_changed) {
	hours = tick_time->tm_hour;
	minutes = tick_time->tm_min;
	day = tick_time->tm_mday;
	if(layer)
		layer_mark_dirty(layer);
}

#ifdef PBL_SDK_3
static void animationUpdate(Animation *animation, const AnimationProgress progress) {
#else
static void animationUpdate(Animation *animation, const uint32_t progress) {
#endif
	percent = progress * 100 / ANIMATION_NORMALIZED_MAX;
	layer_mark_dirty(layer);
}

static void animation_started(Animation *animation, void *data) {
	percent = 0;
	isAnimating = true;
}

static void animation_stopped(Animation *animation, bool finished, void *data) {
	percent = 100;
	isAnimating = false;
	layer_mark_dirty(layer);
#ifndef PBL_SDK_3
	animation_destroy(animation);
	animation = NULL;
#endif
}

static void drawClock(GPoint center, GContext *ctx){
	GPoint segA;
	GPoint segC;

	uint16_t segA_length = getFull_hour_mode() ? SECOND_HAND_LENGTH_A + 150 : SECOND_HAND_LENGTH_A;
	uint16_t segC_length = getFull_hour_mode() ? SECOND_HAND_LENGTH_C + 150 : SECOND_HAND_LENGTH_C;
	
	graphics_context_set_fill_color(ctx, bg_circle_color);
	graphics_fill_circle(ctx, center, segC_length);

	graphics_context_set_text_color(ctx, text_and_dots_color);
	graphics_context_set_fill_color(ctx, text_and_dots_color);
	graphics_context_set_stroke_color(ctx, dots_outline_color);
 
	int minhour = hours + 24 - 2;
	int maxhour = hours + 24 + 3;
	
	for(int i=minhour*4; i<maxhour*4; i++){
		int32_t angle = 
			getFull_hour_mode() ?
				TRIG_MAX_ANGLE * (i % (24*4)) / (24*4):
				TRIG_MAX_ANGLE * (i % (12*4)) / (12*4);
				;
		if(getFull_hour_mode()){
			angle += TRIG_MAX_ANGLE / 2;
			angle = angle % TRIG_MAX_ANGLE;
		}

		if(isAnimating){
			segC.y = (int16_t)((-cos_lookup(angle) * segC_length / TRIG_MAX_RATIO) + center.y - SCREEN_HEIGHT/2) * percent / 100 + SCREEN_HEIGHT/2;
			segC.x = (int16_t)((sin_lookup(angle) * segC_length / TRIG_MAX_RATIO) + center.x - SCREEN_WIDTH/2) * percent / 100 + SCREEN_WIDTH/2;
			segA.x = segA.y = 0;
		}
		else {
			segA.y = (int16_t)(-cos_lookup(angle) * segA_length / TRIG_MAX_RATIO) + center.y;
			segA.x = (int16_t)(sin_lookup(angle) * segA_length / TRIG_MAX_RATIO) + center.x;
			segC.y = (int16_t)(-cos_lookup(angle) * segC_length / TRIG_MAX_RATIO) + center.y;
			segC.x = (int16_t)(sin_lookup(angle) * segC_length / TRIG_MAX_RATIO) + center.x;
		}
		
		bool hour_mark = (i % 4) == 0;
		uint8_t radius = hour_mark ? BIG_DOT_RADIUS : SMALL_DOT_RADIUS ;

		if(getMark_style() == MARK_STYLE_DOTS) {
			graphics_fill_circle(ctx, segC, radius);
			graphics_draw_circle(ctx, segC, radius);
		}
		else {
			GPath* mark_path = hour_mark ? big_line_mark_path : small_line_mark_path;
			gpath_move_to(mark_path, segC);
			gpath_rotate_to(mark_path, angle);
			gpath_draw_filled(ctx, mark_path);
			// don't draw the outline for 'monochrome' themes
			if(getColor_theme() == COLOR_THEME_BLACK_ON_WHITE || getColor_theme() == COLOR_THEME_WHITE_ON_BLACK)
				gpath_draw_outline(ctx, mark_path);
		}
		
		if(!isAnimating && hour_mark) {			
			if(clock_is_24h_style() || getFull_hour_mode()){
				graphics_draw_text(ctx,
					txt[(i % (24*4))/4],
					custom_font,
					GRect(segA.x-25, segA.y-25, 50, 50),
					GTextOverflowModeWordWrap,
					GTextAlignmentCenter,
					NULL);	
			}
			else {
				graphics_draw_text(ctx,
					(i % (12*4))/4 == 0 ? "12" : txt[(i % (12*4))/4],
					custom_font,
					GRect(segA.x-25, segA.y-25, 50, 50),
					GTextOverflowModeWordWrap,
					GTextAlignmentCenter,
					NULL);
			}
		}
	}
}

static void drawInfo(GPoint center, int angle, GContext *ctx, char* text){
	GPoint segA;

	uint16_t outer_radius = 0;
	uint16_t inner_radius = 0;
	uint16_t font_height = 0;
	GFont font = small_font;

	switch(getInfo_text_size()){
		case INFO_TEXT_SIZE_DEFAULT : 
			outer_radius = 17;
			inner_radius = 14;
			font = small_font;
			font_height = 14;
			break;
		case INFO_TEXT_SIZE_LARGER : 
			outer_radius = 23;
			inner_radius = 20;
			font = medium_font;
			font_height = 20;
			break;
	}

#ifdef PBL_ROUND
	uint8_t pos = getFull_hour_mode() ? SECOND_HAND_LENGTH_A + 150 : SECOND_HAND_LENGTH_A;
	pos -= 87;
	pos += outer_radius;
	segA.y = (int16_t)(-cos_lookup(angle) * pos / TRIG_MAX_RATIO) + center.y;
	segA.x = (int16_t)(sin_lookup(angle) * pos / TRIG_MAX_RATIO) + center.x;
#else

	int32_t posFromCenter = getFull_hour_mode() ? DATE_POSITION_FROM_CENTER + 150 : DATE_POSITION_FROM_CENTER;

	do{
		segA.y = (int16_t)(-cos_lookup(angle) * posFromCenter / TRIG_MAX_RATIO) + center.y;
		segA.x = (int16_t)(sin_lookup(angle) * posFromCenter / TRIG_MAX_RATIO) + center.x;
		posFromCenter--;
	}
	while(containsCircle(segA, outer_radius + 1));

#endif

	graphics_context_set_text_color(ctx, text_and_dots_color);
	graphics_context_set_stroke_color(ctx, hand_outline_color);
	graphics_context_set_fill_color(ctx, hand_color);

	graphics_fill_circle(ctx, segA, outer_radius);
	if(!gcolor_equal(hand_outline_color,GColorClear)){
		graphics_draw_circle(ctx, segA, outer_radius);
	}
	
	graphics_context_set_fill_color(ctx, bg_circle_color);

	graphics_fill_circle(ctx, segA, inner_radius);
	if(!gcolor_equal(hand_outline_color,GColorClear)){
		graphics_draw_circle(ctx, segA, inner_radius);
	}
	GRect text_bounds = (GRect){.origin=segA,.size={60,60}};
	GSize text_size = graphics_text_layout_get_content_size(text,font,text_bounds,GTextOverflowModeWordWrap,GTextAlignmentCenter);
	text_bounds.origin.x -= text_size.w/2;
	text_bounds.origin.y -= font_height/2 + (text_size.h - font_height) - 1;
	text_bounds.size = text_size;
	graphics_draw_text(ctx,
					text,
					font,
					text_bounds,
					GTextOverflowModeWordWrap,
					GTextAlignmentCenter,
					NULL);
}

static void drawHand(GPoint center, int angle, GContext *ctx){
	
	graphics_context_set_fill_color(ctx, hand_color);
	
	gpath_move_to(hour_arrow, center);
	gpath_rotate_to(hour_arrow, angle);
	gpath_draw_filled(ctx, hour_arrow);

	if(!gcolor_equal(hand_outline_color,GColorClear)){
		graphics_context_set_stroke_color(ctx, hand_outline_color);
		gpath_draw_outline(ctx, hour_arrow);
	}
}

static void layer_update_proc(Layer *layer, GContext *ctx) {
	GRect bounds = layer_get_bounds(layer);
	GPoint center = grect_center_point(&bounds);

	if(isAnimating){
		if(layer_update_count == 0){
			layer_mark_dirty(layer);
		}
		else if(layer_update_count == 1){
			animation = animation_create();
			animImpl.update = animationUpdate;
			animation_set_handlers(animation, (AnimationHandlers) {
				.started = (AnimationStartedHandler) animation_started,
				.stopped = (AnimationStoppedHandler) animation_stopped,
			}, NULL);
			animation_set_duration(animation, 500);
			animation_set_implementation(animation, &animImpl);
			animation_schedule(animation);
		}
		layer_update_count++;
	}
	
	int32_t angle = 
		getFull_hour_mode() ?
			TRIG_MAX_ANGLE * ((hours % 24) * 60 + minutes) / (24 * 60):
			TRIG_MAX_ANGLE * ((hours % 12) * 60 + minutes) / (12 * 60);
			
	if(getFull_hour_mode()){
		angle += TRIG_MAX_ANGLE / 2;
		angle = angle % TRIG_MAX_ANGLE;
	}

	uint16_t segA_length = getFull_hour_mode() ? SECOND_HAND_LENGTH_A + 150 : SECOND_HAND_LENGTH_A;

	GPoint centerClock;
	centerClock.y = (int16_t)(cos_lookup(angle) * segA_length / TRIG_MAX_RATIO) + center.y;
	centerClock.x = (int16_t)(-sin_lookup(angle) * segA_length / TRIG_MAX_RATIO) + center.x;

	drawClock(centerClock, ctx);

	if(!isAnimating){
		drawHand(centerClock, angle, ctx);

		if(getDisplay_bt_icon() && !btConnected){
#ifdef PBL_COLOR
  			graphics_context_set_compositing_mode(ctx,GCompOpSet);
#else
  			if(gcolor_equal(GColorBlack,bg_color))
  				graphics_context_set_compositing_mode(ctx,GCompOpAssignInverted);
#endif

  			if(getFull_hour_mode()){
				if(hours < 6)
					graphics_draw_bitmap_in_rect(ctx, bt_disconnected, GRect(0,160-32,32,32));
				else if(hours < 12)
					graphics_draw_bitmap_in_rect(ctx, bt_disconnected, GRect(0,0,32,32));
				else if(hours < 18)
					graphics_draw_bitmap_in_rect(ctx, bt_disconnected, GRect(SCREEN_WIDTH-32,0,32,32));
				else 
					graphics_draw_bitmap_in_rect(ctx, bt_disconnected, GRect(SCREEN_WIDTH-32,160-32,32,32));
			}
  			else {
				if(hours % 12 < 3)
					graphics_draw_bitmap_in_rect(ctx, bt_disconnected, GRect(SCREEN_WIDTH-32,0,32,32));
				else if(hours % 12 < 6)
					graphics_draw_bitmap_in_rect(ctx, bt_disconnected, GRect(SCREEN_WIDTH-32,160-32,32,32));
				else if(hours % 12 < 9)
					graphics_draw_bitmap_in_rect(ctx, bt_disconnected, GRect(0,160-32,32,32));
				else 
					graphics_draw_bitmap_in_rect(ctx, bt_disconnected, GRect(0,0,32,32));
  			}

		}
		
		if(getInfo_display() == INFO_DISPLAY_DATE){
			snprintf(info_text, sizeof(info_text), "%d", day); 
			drawInfo(centerClock, angle, ctx, info_text);
		}
		else if(getInfo_display() == INFO_DISPLAY_MINUTE){
			snprintf(info_text, sizeof(info_text), "%d", minutes); 
			drawInfo(centerClock, angle, ctx, info_text);
		}
	}	
}

static void window_load(Window *window) {
	Layer *window_layer = window_get_root_layer(window);
	GRect bounds = layer_get_bounds(window_layer);
	
	layer = layer_create(bounds);
	layer_set_update_proc(layer, layer_update_proc);
	layer_add_child(window_layer, layer);
}

static void window_unload(Window *window) {
	layer_destroy(layer);
}

static void updateSettings(){
	gpath_destroy(hour_arrow);
	switch(getHand()){
		case HAND_LINE : hour_arrow = gpath_create(getFull_hour_mode() ? &LINE_HAND_24_POINTS : &LINE_HAND_POINTS); break;
		case HAND_BIGLINE : hour_arrow = gpath_create(getFull_hour_mode() ? &BIG_LINE_HAND_24_POINTS : &BIG_LINE_HAND_POINTS); break;
		case HAND_ARROW : 
		default : hour_arrow = gpath_create(getFull_hour_mode() ? &ARROW_HAND_24_POINTS : &ARROW_HAND_POINTS); break;
	}

	switch(getColor_theme()){
		case COLOR_THEME_DARK : 
			bg_color = GColorBlack;
			bg_circle_color = GColorBlack;
			text_and_dots_color = GColorWhite;
			dots_outline_color = GColorWhite;
			hand_color = GColorWhite;
			break;
		case COLOR_THEME_CLEAR :
			bg_color = GColorWhite;
			bg_circle_color = GColorWhite;
			text_and_dots_color = GColorBlack;
			dots_outline_color = GColorBlack;
			hand_color = GColorBlack;
			break;
		case COLOR_THEME_BLACK_ON_WHITE :
			bg_color = GColorWhite;
			bg_circle_color = GColorBlack;
			text_and_dots_color = GColorWhite;
			dots_outline_color = GColorBlack;
			hand_color = GColorWhite;
			break;
		case COLOR_THEME_WHITE_ON_BLACK :
			bg_color = GColorBlack;
			bg_circle_color = GColorWhite;
			text_and_dots_color = GColorBlack;
			dots_outline_color = GColorWhite;
			hand_color = GColorBlack;
			break;
	}

#ifdef PBL_COLOR
	hand_color = getHand_color();
#endif
	hand_outline_color = GColorClear;
	if(gcolor_equal(bg_color,hand_color) || gcolor_equal(bg_circle_color,hand_color))
		hand_outline_color = gcolor_equal(hand_color,GColorWhite) ? GColorBlack : GColorWhite;

	replace_gbitmap_color(GColorBlack, text_and_dots_color, bt_disconnected);

	window_set_background_color(window, bg_color);
}

static void in_received_handler(DictionaryIterator *iter, void *context) {
  	// call autoconf_in_received_handler
	autoconfig_in_received_handler(iter, context);

	updateSettings();

	layer_mark_dirty(layer);
}

static void bluetooth_connection_handler(bool connected){
	btConnected = connected;
	if(getVibrate_on_bt_lost()){
		vibes_cancel();
		if(connected){
			vibes_long_pulse();
		}
		else {
			vibes_double_pulse();
		}
	}
	layer_mark_dirty(layer);
}

static void init(void) {
	autoconfig_init(200,0);

	window = window_create();
	window_set_window_handlers(window, (WindowHandlers) {
		.load = window_load,
		.unload = window_unload,
	});

	custom_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_COMFORTAA_40));
	small_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_COMFORTAA_18));
	medium_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_COMFORTAA_26));
	bt_disconnected = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BT_DISCONNECTED);

	small_line_mark_path = gpath_create(&SMALL_LINE_MARK_POINTS);
	big_line_mark_path = gpath_create(&BIG_LINE_MARK_POINTS);

	app_message_register_inbox_received(in_received_handler);

	btConnected = bluetooth_connection_service_peek();
	bluetooth_connection_service_subscribe(bluetooth_connection_handler);

	tick_timer_service_subscribe(MINUTE_UNIT, handle_minute_tick);

	time_t now = time(NULL);
	struct tm *t = localtime(&now);
	handle_minute_tick(t, MINUTE_UNIT);

	updateSettings();

	isAnimating = getAnimated();
	if(isAnimating)
		percent = 0;

	window_stack_push(window, false);
}

static void deinit(void) {
	autoconfig_deinit();
	fonts_unload_custom_font(custom_font);
	fonts_unload_custom_font(small_font);
	fonts_unload_custom_font(medium_font);
	gbitmap_destroy(bt_disconnected);
	gpath_destroy(hour_arrow);
	gpath_destroy(small_line_mark_path);
	gpath_destroy(big_line_mark_path);
	window_destroy(window);
	bluetooth_connection_service_unsubscribe();
}

int main(void) {
	init();
	app_event_loop();
	deinit();
}