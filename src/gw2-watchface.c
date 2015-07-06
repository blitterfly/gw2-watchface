#include <pebble.h>

#define KEY_TWEET 0

#define HAND_MARGIN  10
#define FINAL_RADIUS 75

#define ANIMATION_DURATION 500
#define ANIMATION_DELAY    600

typedef struct {
	int hours;
	int minutes;
} Time;

static Window * s_main_window;
static TextLayer * s_time_layer;
static GFont s_time_font;
static BitmapLayer * s_background_layer;
static GBitmap * s_background_bitmap;

static Layer *s_canvas_layer;

static GPoint s_center;
static Time s_last_time, s_anim_time;
static int s_radius = 0, s_anim_hours_60 = 0;
static bool s_animating = false;

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
	
	// Get latest tweet update every 30 minutes
	/*if(tick_time->tm_min % 30 == 0) {
		// Begin dictionary
		DictionaryIterator * iter;
		app_message_outbox_begin(&iter);
		
		// Add a key-value pair
		dict_write_uint8(iter, 0, 0);
		
		// Send the message!
		app_message_outbox_send();
	}*/
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


static void main_window_load(Window * window) {
	Layer * window_layer = window_get_root_layer(window);
	GRect canvas_rect = GRect(0, 0, 144, 168);
	
	// Create GBitmap, then set to created BitmapLayer
	s_background_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BACKGROUND);
	s_background_layer = bitmap_layer_create(canvas_rect);
	bitmap_layer_set_bitmap(s_background_layer, s_background_bitmap);
	layer_add_child(window_layer, bitmap_layer_get_layer(s_background_layer));
	
	// Create GFont
	s_time_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_CRONOSPRO_16));

	// Create time TextLayer
	GSize time_size = graphics_text_layout_get_content_size("00:00", s_time_font, canvas_rect, GTextOverflowModeFill, GTextAlignmentCenter);
	s_time_layer = text_layer_create(GRect(142 - time_size.w, 0, time_size.w, time_size.h));
	text_layer_set_background_color(s_time_layer, GColorClear);
	text_layer_set_text_color(s_time_layer, GColorWhite);
	text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);
	text_layer_set_font(s_time_layer, s_time_font);
	
	// Add it as a child layer to the Window's root layer
	layer_add_child(window_layer, text_layer_get_layer(s_time_layer));
	
	// Clock-face drawing
	GRect window_bounds = layer_get_bounds(window_layer);
	
	s_center = (GPoint) {
		.x = window_bounds.size.w / 2,
		.y = 75
	};
	
	s_canvas_layer = layer_create(window_bounds);
	layer_set_update_proc(s_canvas_layer, update_proc);
	layer_add_child(window_layer, s_canvas_layer);
	
	// Register with TickTimerService
	tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
	
}

static void main_window_unload(Window * window) {
	// Destroy TextLayer
	text_layer_destroy(s_time_layer);
	
	// Destroy clock face layer
	layer_destroy(s_canvas_layer);
	
	// Destroy GBitmap
	gbitmap_destroy(s_background_bitmap);
	
	// Destroy BitmapLayer
	bitmap_layer_destroy(s_background_layer);
	
	// Unload GFont
	fonts_unload_custom_font(s_time_font);
	
}

static void inbox_received_callback(DictionaryIterator * iterator, void * context) {
	// Store incoming information
	/*static char tweet_buffer[80];
	
	// Read first item
	Tuple * t = dict_read_first(iterator);
	
	// For all items
	while(t != NULL) {
		// Which key was received?
		switch(t->key) {
			case KEY_TWEET:
				snprintf(tweet_buffer, sizeof(tweet_buffer), "%.76s...", t->value->cstring);
				break;
			default:
				APP_LOG(APP_LOG_LEVEL_ERROR, "Key %d not recognized!", (int)t->key);
				break;
		}
		
		// Look for next item
		t = dict_read_next(iterator);
	}
	
	// Assemble full string and display
	text_layer_set_text(s_tweet_layer, tweet_buffer);*/
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
	/*app_message_register_inbox_received(inbox_received_callback);
	app_message_register_inbox_dropped(inbox_dropped_callback);
	app_message_register_outbox_failed(outbox_failed_callback);
	app_message_register_outbox_sent(outbox_sent_callback);
	
	// Open AppMessage
	app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());*/
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
