#include <pebble.h>

#define KEY_UPDATEMODE 0
#define KEY_RED_NAME 1
#define KEY_RED_SCORE 2
#define KEY_BLUE_NAME 3
#define KEY_BLUE_SCORE 4
#define KEY_GREEN_NAME 5
#define KEY_GREEN_SCORE 6
#define KEY_THIS_MATCH 7
#define KEY_MATCHES 8

#define REALM_NAME_SIZE 32
#define MIN_REALM_LABEL_SIZE 72
#define REALM_LABEL_SIZE(p) (MIN_REALM_LABEL_SIZE + (int)((144.0f - (float)MIN_REALM_LABEL_SIZE) * ((float)p / 100.0f)))

#define HAND_MARGIN  10
#define FINAL_RADIUS 75

#define ANIMATION_DURATION 500
#define ANIMATION_DELAY    600

typedef struct {
	int hours;
	int minutes;
} Time;

typedef struct {
	char red_name[REALM_NAME_SIZE];
	char blue_name[REALM_NAME_SIZE];
	char green_name[REALM_NAME_SIZE];
	int red_score;
	int blue_score;
	int green_score;
} Match;

static Window * s_main_window;
static TextLayer * s_time_layer;
static GFont s_time_font;
static BitmapLayer * s_background_layer;
static GBitmap * s_background_bitmap;

static GFont s_realm_font;
static TextLayer * s_red_layer;
static TextLayer * s_blue_layer;
static TextLayer * s_green_layer;
static TextLayer * s_matches_layer;

static Layer * s_canvas_layer;
static GPoint s_center;
static Time s_last_time, s_anim_time;
static int s_radius = 0, s_anim_hours_60 = 0;
static bool s_animating = false;

static Layer * s_heart_layer;
static GBitmap * s_heart_bitmap;
static GBitmap * s_charging_bitmap;
static int s_charge_percent = 100;

/*************************** AnimationImplementation **************************/

static void animation_started(Animation *anim, void *context) {
	s_animating = true;
}

static void animation_stopped(Animation *anim, bool stopped, void *context) {
	s_animating = false;
}

static void animate(int duration, int delay, AnimationImplementation * implementation, bool handlers) {
	Animation * anim = animation_create();
	animation_set_duration(anim, duration);
	animation_set_delay(anim, delay);
	animation_set_curve(anim, AnimationCurveEaseInOut);
	animation_set_implementation(anim, implementation);
	if (handlers) {
		animation_set_handlers(anim, (AnimationHandlers) {
			.started = animation_started,
			.stopped = animation_stopped
		}, NULL);
	}
	animation_schedule(anim);
}


static void update_time(struct tm * tick_time) {	
	// Create a long-lived buffer
	static char buffer[] = "00:00";
	
	// Write the current hours and minutes into the buffer
	if (clock_is_24h_style() == true) {
		// Use 24 hour format
		strftime(buffer, sizeof("00:00"), "%H:%M", tick_time);
	} else {
		// Use 12 hour format
		strftime(buffer, sizeof("00:00"), "%I:%M", tick_time);
	}
	
	// Display this time on the TextLayer
	text_layer_set_text(s_time_layer, buffer);
}

static void update_hands(struct tm * tick_time) {
	// Store time
	s_last_time.hours = tick_time->tm_hour;
	s_last_time.hours -= (s_last_time.hours > 12) ? 12 : 0;
	s_last_time.minutes = tick_time->tm_min;
		
	// Redraw
	if (s_canvas_layer) {
		layer_mark_dirty(s_canvas_layer);
	}
}

static int hours_to_minutes(int hours_out_of_12) {
	return (int)(float)(((float)hours_out_of_12 / 12.0F) * 60.0F);
}

static void tick_handler(struct tm * tick_time, TimeUnits units_changed) {
	update_time(tick_time);
	update_hands(tick_time);
	
	// Update data every 30 minutes)
	if (tick_time->tm_min % 30 == 0) {
		DictionaryIterator * iter;
		app_message_outbox_begin(&iter);
		
		// Add a key-value pair
		dict_write_uint8(iter, KEY_UPDATEMODE, 0);
		
		// Send the message!
		app_message_outbox_send();
	}
	else // Get a different match every minute
	{
		DictionaryIterator * iter;
		app_message_outbox_begin(&iter);
			
		// Add a key-value pair
		dict_write_uint8(iter, KEY_UPDATEMODE, 1);
			
		// Send the message!
		app_message_outbox_send();
	}
}

static void battery_handler(BatteryChargeState charge_state) {
	if (charge_state.is_charging) {
		APP_LOG(APP_LOG_LEVEL_INFO, "battery: charging");
		s_charge_percent = -1;
	} else {
		APP_LOG(APP_LOG_LEVEL_INFO, "battery: %d%% charged", charge_state.charge_percent);
		s_charge_percent = charge_state.charge_percent;
	}
	
	layer_mark_dirty(s_heart_layer);
}

static void update_proc(Layer *layer, GContext *ctx) {	
	graphics_context_set_stroke_color(ctx, GColorSunsetOrange);
	graphics_context_set_stroke_width(ctx, 4);
	
	graphics_context_set_antialiased(ctx, true);
		
	// Don't use current time while animating
	Time mode_time = (s_animating) ? s_anim_time : s_last_time;
	
	// Adjust for minutes through the hour
	float minute_angle = TRIG_MAX_ANGLE * mode_time.minutes / 60;
	float hour_angle;
	if(s_animating) {
		// Hours out of 60 for smoothness
		hour_angle = TRIG_MAX_ANGLE * mode_time.hours / 60;
	} else {
		hour_angle = TRIG_MAX_ANGLE * mode_time.hours / 12;
	}
	hour_angle += (minute_angle / TRIG_MAX_ANGLE) * (TRIG_MAX_ANGLE / 12);
	
	// Plot hands
	GPoint minute_hand = (GPoint) {
		.x = (int16_t)(sin_lookup(TRIG_MAX_ANGLE * mode_time.minutes / 60) * (int32_t)(s_radius - HAND_MARGIN) / TRIG_MAX_RATIO) + s_center.x,
		.y = (int16_t)(-cos_lookup(TRIG_MAX_ANGLE * mode_time.minutes / 60) * (int32_t)(s_radius - HAND_MARGIN) / TRIG_MAX_RATIO) + s_center.y,
	};
	GPoint hour_hand = (GPoint) {
		.x = (int16_t)(sin_lookup(hour_angle) * (int32_t)(s_radius - (2 * HAND_MARGIN)) / TRIG_MAX_RATIO) + s_center.x,
		.y = (int16_t)(-cos_lookup(hour_angle) * (int32_t)(s_radius - (2 * HAND_MARGIN)) / TRIG_MAX_RATIO) + s_center.y,
	};
	
	// Draw hands with positive length only
	if(s_radius > 2 * HAND_MARGIN) {
		graphics_draw_line(ctx, s_center, hour_hand);
	} 
	if(s_radius > HAND_MARGIN) {
		graphics_draw_line(ctx, s_center, minute_hand);
	}
}

static void update_proc_battery(Layer * layer, GContext * ctx) {
	static GBitmap * s_heart_clipped = 0;
	
	GRect heart_bounds = gbitmap_get_bounds(s_heart_bitmap);
	int hh = heart_bounds.size.h;
	if (s_charge_percent >= 0) {
		// charge percent
		hh = (hh - 12) / 2 + (int)((float)12 * ((float)s_charge_percent / 100.0f)); // actual "fillable" part is about 12 pixels
	}
	if (s_heart_clipped) {
		gbitmap_destroy(s_heart_clipped);
	}
	s_heart_clipped = gbitmap_create_as_sub_bitmap(s_heart_bitmap, GRect(0, heart_bounds.size.h - hh, heart_bounds.size.w, hh));
	
	graphics_context_set_compositing_mode(ctx, GCompOpSet);
	graphics_draw_bitmap_in_rect(ctx, s_heart_clipped, GRect(heart_bounds.origin.x, heart_bounds.origin.y  + heart_bounds.size.h - hh, heart_bounds.size.w, hh));
	if (s_charge_percent < 0) {
		// currently charging
		GRect charging_bounds = gbitmap_get_bounds(s_charging_bitmap);
		graphics_draw_bitmap_in_rect(ctx, s_charging_bitmap, GRect(charging_bounds.origin.x + 2, charging_bounds.origin.y + 1, charging_bounds.size.w, charging_bounds.size.h));
	}
}

static void set_realm_text_sizes(int red, int blue, int green) {
	int red_w = REALM_LABEL_SIZE(red);
	int blue_w = REALM_LABEL_SIZE(blue);
	int green_w = REALM_LABEL_SIZE(green);
	
	/*APP_LOG(APP_LOG_LEVEL_INFO, "red_w: %d", red_w);
	APP_LOG(APP_LOG_LEVEL_INFO, "blue_w: %d", blue_w);
	APP_LOG(APP_LOG_LEVEL_INFO, "green_w: %d", green_w);*/
	
	// this would be nicer, but doesn't work for some reason?
	/*layer_set_bounds(text_layer_get_layer(s_red_layer), GRect(0, 123, red_w, 15));
	layer_set_bounds(text_layer_get_layer(s_blue_layer), GRect(72 - (blue_w / 2), 138, blue_w, 15));
	layer_set_bounds(text_layer_get_layer(s_green_layer), GRect(144 - green_w, 153, green_w, 15));*/
	
	text_layer_set_size(s_red_layer, GSize(red_w, 14));
	text_layer_set_size(s_blue_layer, GSize(blue_w, 14));
	text_layer_set_size(s_green_layer, GSize(green_w, 14));
}

static void main_window_load(Window * window) {
	Layer * window_layer = window_get_root_layer(window);
	GRect canvas_rect = GRect(0, 0, 144, 168);
	GRect window_bounds = layer_get_bounds(window_layer);
	
	// Create GBitmap, then set to created BitmapLayer
	s_background_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BACKGROUND);
	s_background_layer = bitmap_layer_create(canvas_rect);
	bitmap_layer_set_bitmap(s_background_layer, s_background_bitmap);
	layer_add_child(window_layer, bitmap_layer_get_layer(s_background_layer));
	
	// Create GFont
	s_time_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_CRONOSPRO_16));
	s_realm_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_CRONOSPRO_12));

	// Create time TextLayer
	GSize time_size = graphics_text_layout_get_content_size("00:00", s_time_font, canvas_rect, GTextOverflowModeFill, GTextAlignmentCenter);
	s_time_layer = text_layer_create(GRect(142 - time_size.w, 0, time_size.w, time_size.h));
	text_layer_set_background_color(s_time_layer, GColorClear);
	text_layer_set_text_color(s_time_layer, GColorWhite);
	text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);
	text_layer_set_font(s_time_layer, s_time_font);
	
	// Creat heart (battery) layer
	s_heart_layer = layer_create(GRect(2, 0, 20, 20));
	layer_set_update_proc(s_heart_layer, update_proc_battery);
	layer_add_child(window_layer, s_heart_layer);
	
	// Create heart bitmap
	s_heart_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_HEART);
	s_charging_bitmap =  gbitmap_create_with_resource(RESOURCE_ID_IMAGE_CHARGING);
	
	// Add it as a child layer to the Window's root layer
	layer_add_child(window_layer, text_layer_get_layer(s_time_layer));
		
	// WvW scores
	s_red_layer = text_layer_create(GRect(0, 126, MIN_REALM_LABEL_SIZE, 14));
	text_layer_set_background_color(s_red_layer, GColorRed);
	text_layer_set_text_color(s_red_layer, GColorWhite);
	text_layer_set_font(s_red_layer, s_realm_font);
	text_layer_set_overflow_mode(s_red_layer, GTextOverflowModeWordWrap);
	text_layer_set_text(s_red_layer, " Loading ...");
	layer_add_child(window_layer, text_layer_get_layer(s_red_layer));
	
	s_blue_layer = text_layer_create(GRect(0, 140, MIN_REALM_LABEL_SIZE, 14));
	text_layer_set_background_color(s_blue_layer, GColorBlue);
	text_layer_set_text_color(s_blue_layer, GColorWhite);
	text_layer_set_font(s_blue_layer, s_realm_font);
	text_layer_set_overflow_mode(s_blue_layer, GTextOverflowModeWordWrap);
	layer_add_child(window_layer, text_layer_get_layer(s_blue_layer));
	
	s_green_layer = text_layer_create(GRect(0, 154, MIN_REALM_LABEL_SIZE, 14));
	text_layer_set_background_color(s_green_layer, GColorDarkGreen);
	text_layer_set_text_color(s_green_layer, GColorWhite);
	text_layer_set_font(s_green_layer, s_realm_font);
	text_layer_set_overflow_mode(s_green_layer, GTextOverflowModeWordWrap);
	layer_add_child(window_layer, text_layer_get_layer(s_green_layer));
	
	GSize matches_size = graphics_text_layout_get_content_size("00/00", s_realm_font, canvas_rect, GTextOverflowModeFill, GTextAlignmentRight);
	s_matches_layer = text_layer_create(GRect(window_bounds.size.w - matches_size.w - 2, 154, matches_size.w, 14));
	text_layer_set_background_color(s_matches_layer, GColorClear);
	text_layer_set_text_color(s_matches_layer, GColorWhite);
	text_layer_set_font(s_matches_layer, s_realm_font);
	text_layer_set_text_alignment(s_matches_layer, GTextAlignmentRight);
	text_layer_set_overflow_mode(s_matches_layer, GTextOverflowModeFill);
	layer_add_child(window_layer, text_layer_get_layer(s_matches_layer));
	
	//set_realm_text_sizes(33, 33, 33);
	
	// Clock-face drawing
	s_center = (GPoint) {
		.x = window_bounds.size.w / 2,
		.y = 75
	};
	
	s_canvas_layer = layer_create(window_bounds);
	layer_set_update_proc(s_canvas_layer, update_proc);
	layer_add_child(window_layer, s_canvas_layer);
	
	// Register with TickTimerService
	tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
	
	// Register battery handler
	battery_state_service_subscribe(battery_handler);
	// Prime the battery state
	battery_handler(battery_state_service_peek());
}

static void main_window_unload(Window * window) {
	// Destroy TextLayer
	text_layer_destroy(s_time_layer);
	text_layer_destroy(s_red_layer);
	text_layer_destroy(s_blue_layer);
	text_layer_destroy(s_green_layer);
	
	// Destroy custom layers
	layer_destroy(s_canvas_layer);
	layer_destroy(s_heart_layer);
	
	// Destroy GBitmap
	gbitmap_destroy(s_background_bitmap);
	gbitmap_destroy(s_heart_bitmap);
	gbitmap_destroy(s_charging_bitmap);
	
	// Destroy BitmapLayer
	bitmap_layer_destroy(s_background_layer);
	
	// Unload GFont
	fonts_unload_custom_font(s_time_font);
	fonts_unload_custom_font(s_realm_font);
	
}

static void inbox_received_callback(DictionaryIterator * iterator, void * context) {
	// Store incoming information
	static Match current_match;
	static char matches_buffer[16];

	// Read first item
	Tuple * t = dict_read_first(iterator);
	int this_match = 0, match_count = 0;
	
	// For all items
	while(t != NULL) {
		// Which key was received?
		switch(t->key) {
			case KEY_RED_NAME:
				current_match.red_name[0] = ' ';
				strncpy(current_match.red_name + 1, t->value->cstring, REALM_NAME_SIZE - 1);
				break;
			case KEY_BLUE_NAME:
				current_match.blue_name[0] = ' ';
				strncpy(current_match.blue_name + 1, t->value->cstring, REALM_NAME_SIZE - 1);
				break;
			case KEY_GREEN_NAME:
				current_match.green_name[0] = ' ';
				strncpy(current_match.green_name + 1, t->value->cstring, REALM_NAME_SIZE - 1);
				break;
			case KEY_RED_SCORE:
				current_match.red_score = t->value->int32;
				break;
			case KEY_BLUE_SCORE:
				current_match.blue_score = t->value->int32;
				break;
			case KEY_GREEN_SCORE:
				current_match.green_score = t->value->int32;
				break;
			case KEY_THIS_MATCH:
				this_match = t->value->int32;
				break;
			case KEY_MATCHES:
				match_count = t->value->int32;
				break;
			default:
				APP_LOG(APP_LOG_LEVEL_ERROR, "Key %d not recognized!", (int)t->key);
				break;
		}
		
		// Look for next item
		t = dict_read_next(iterator);
	}
	
	// Assemble full string and display
	set_realm_text_sizes(current_match.red_score, current_match.blue_score, current_match.green_score);
	text_layer_set_text(s_red_layer, current_match.red_name);
	text_layer_set_text(s_blue_layer, current_match.blue_name);
	text_layer_set_text(s_green_layer, current_match.green_name);
	snprintf(matches_buffer, 16, "%.2d/%.2d", this_match, match_count);
	text_layer_set_text(s_matches_layer, matches_buffer);
}

static void inbox_dropped_callback(AppMessageResult reason, void *context) {
	APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped!");
}

static void outbox_failed_callback(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
	APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed!");
}

static void outbox_sent_callback(DictionaryIterator *iterator, void *context) {
	APP_LOG(APP_LOG_LEVEL_INFO, "Outbox send success!");
}

static int anim_percentage(AnimationProgress dist_normalized, int max) {
	return (int)(float)(((float)dist_normalized / (float)ANIMATION_NORMALIZED_MAX) * (float)max);
}

static void radius_update(Animation * anim, AnimationProgress dist_normalized) {
	s_radius = anim_percentage(dist_normalized, FINAL_RADIUS);
	
	layer_mark_dirty(s_canvas_layer);
}

static void hands_update(Animation * anim, AnimationProgress dist_normalized) {
	s_anim_time.hours = anim_percentage(dist_normalized, hours_to_minutes(s_last_time.hours));
	s_anim_time.minutes = anim_percentage(dist_normalized, s_last_time.minutes);
	
	layer_mark_dirty(s_canvas_layer);
}

static void init(void) {
	// Create main Window element and assign to pointer
	s_main_window = window_create();
	
	// Set handlers to manage the elements inside the Window
	window_set_window_handlers(s_main_window, (WindowHandlers) {
		.load = main_window_load,
		.unload = main_window_unload
	});
	
	// Show the Window on the watch, with animated=true
	window_stack_push(s_main_window, true);
	
	// Get a tm structure
	time_t temp = time(NULL); 
	struct tm * tick_time = localtime(&temp);
	
	// Make sure the time is displayed from the start
	update_time(tick_time);
	
	// Prepare animations
	AnimationImplementation radius_impl = {
		.update = radius_update
	};
	animate(ANIMATION_DURATION, ANIMATION_DELAY, &radius_impl, false);
	
	AnimationImplementation hands_impl = {
		.update = hands_update
	};
	animate(2 * ANIMATION_DURATION, ANIMATION_DELAY, &hands_impl, true);
	
	// Register callbacks
	app_message_register_inbox_received(inbox_received_callback);
	app_message_register_inbox_dropped(inbox_dropped_callback);
	app_message_register_outbox_failed(outbox_failed_callback);
	app_message_register_outbox_sent(outbox_sent_callback);
	
	// Open AppMessage
	app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());
}

static void deinit(void) {
	// Destroy Window
	window_destroy(s_main_window);
}

int main(void) {
	init();
	app_event_loop();
	deinit();
}
