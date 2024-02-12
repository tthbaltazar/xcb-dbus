#include <stdio.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>

#include <xcb/xcb.h>

#include <dbus/dbus.h>

static DBusHandlerResult echo_handle(DBusConnection *connection, DBusMessage *message, void *data)
{
	const char *interface = dbus_message_get_interface(message);
	const char *member = dbus_message_get_member(message);
	printf("%s %s\n", interface, member);

	if (strcmp(interface, "org.freedesktop.DBus.Introspectable") == 0) {
		DBusMessage *reply = dbus_message_new_method_return(message);
		const char *str =
			"<node>"
				"<interface name=\"com.example.Echo\">"
					"<method name=\"Echo\">"
						"<arg name=\"data\" type=\"s\"/>"
					"</method>"
				"</interface>"
			"</node>";
		dbus_message_append_args(reply, DBUS_TYPE_STRING, &str, DBUS_TYPE_INVALID);

		dbus_connection_send(connection, reply, NULL);

		return DBUS_HANDLER_RESULT_HANDLED;
	}

	if (strcmp(interface, "com.example.Echo") == 0) {
		if (strcmp(member, "Echo") == 0) {
			const char *data = NULL;
			dbus_message_get_args(message, NULL, DBUS_TYPE_STRING, &data, DBUS_TYPE_INVALID);

			printf("%s\n", data);

			DBusMessage *reply = dbus_message_new_method_return(message);
			dbus_message_append_args(reply, DBUS_TYPE_STRING, &data, DBUS_TYPE_INVALID);

			dbus_connection_send(connection, reply, NULL);

			return DBUS_HANDLER_RESULT_HANDLED;
		}

		/* TODO: return invalid member error */
	}

	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static const struct DBusObjectPathVTable echo_vtable = {
	.message_function = echo_handle
};

struct watch_list {
	int count;
	DBusWatch **items;
};

static dbus_bool_t add_watch(DBusWatch *watch, void *data)
{
	printf("add_watch\n");

	struct watch_list *list = data;

	for(int i = 0; i < list->count; i++) {
		if(list->items[i] == watch) {
			fprintf(stderr, "WARNING: duplicate dbus watch not added\n");
			return 1;
		}
	}

	list->items = reallocarray(list->items, list->count + 1, sizeof(list->items[0]));
	list->items[list->count] = watch;
	list->count += 1;
	return 1;
}

static void remove_watch(DBusWatch *watch, void *data)
{
	struct watch_list *list = data;

	int i = 0;

	/* find item */
	for(; i < list->count; i++) {
		if (list->items[i] == watch) {
			break;
		}
	}

	/* shift elements after it back */
	for(; i < list->count; i++) {
		list->items[i] = list->items[i + 1];
	}

	/* resize */
	list->count -= 1;
	list->items = reallocarray(list->items, list->count, sizeof(list->items[0]));
}

struct window {
	xcb_connection_t *x_con;
	xcb_window_t win;
	xcb_gcontext_t gc;
};

static void window_set_color(struct window *window, int color)
{
	int gc_value_mask = XCB_GC_FOREGROUND;
	int gc_values[] = {
		color
	};
	xcb_change_gc(window->x_con, window->gc, gc_value_mask, gc_values);

	xcb_get_geometry_cookie_t ggc = xcb_get_geometry(window->x_con, window->win);
	xcb_get_geometry_reply_t *reply = xcb_get_geometry_reply(window->x_con, ggc, NULL);
	if (reply == NULL) {
		fprintf(stderr, "ERROR: failed to get window geometry\n");
	} else {
		xcb_rectangle_t rect = {
			.x = 0,
			.y = 0,
			.width = reply->width,
			.height = reply->height
		};
		xcb_poly_fill_rectangle(window->x_con, window->win, window->gc, 1, &rect);
	}

	xcb_flush(window->x_con);
}

static DBusHandlerResult window_handle_dbus_message(DBusConnection *connection, DBusMessage *message, void *data)
{
	const char *interface = dbus_message_get_interface(message);
	const char *member = dbus_message_get_member(message);
	printf("%s %s\n", interface, member);

	if (strcmp(interface, "org.freedesktop.DBus.Introspectable") == 0) {
		DBusMessage *reply = dbus_message_new_method_return(message);
		const char *str =
			"<node>"
				"<interface name=\"com.example.Window\">"
					"<method name=\"SetColor\">"
						"<arg name=\"color\" type=\"i\"/>"
					"</method>"
				"</interface>"
			"</node>";
		dbus_message_append_args(reply, DBUS_TYPE_STRING, &str, DBUS_TYPE_INVALID);

		dbus_connection_send(connection, reply, NULL);

		return DBUS_HANDLER_RESULT_HANDLED;
	}

	if (strcmp(interface, "com.example.Window") == 0) {
		struct window *window = data;
		if (strcmp(member, "SetColor") == 0) {
			int color;
			dbus_message_get_args(message, NULL, DBUS_TYPE_INT32, &color, DBUS_TYPE_INVALID);

			window_set_color(window, color);

			DBusMessage *reply = dbus_message_new_method_return(message);
			dbus_connection_send(connection, reply, NULL);

			return DBUS_HANDLER_RESULT_HANDLED;
		}

		/* TODO: return invalid member error */
	}

	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static const DBusObjectPathVTable window_dbus_vtable = {
	.message_function = window_handle_dbus_message,
};

struct window *create_window(xcb_connection_t *x_con, DBusConnection *dbus_con)
{
	xcb_window_t root_win = xcb_setup_roots_iterator(xcb_get_setup(x_con)).data->root;

	xcb_window_t win = xcb_generate_id(x_con);

	xcb_cw_t value_mask = XCB_CW_EVENT_MASK;
	int values[1] = {
		XCB_EVENT_MASK_EXPOSURE
	};

	xcb_create_window(
		x_con,
		XCB_COPY_FROM_PARENT,
		win,
		root_win,
		0, 0,
		512, 512,
		0,
		XCB_WINDOW_CLASS_INPUT_OUTPUT,
		XCB_COPY_FROM_PARENT,
		value_mask, values
	);

	xcb_gcontext_t gc = xcb_generate_id(x_con);
	int gc_value_mask = XCB_GC_FOREGROUND;
	int gc_values[] = {
		0xff00ff
	};
	xcb_create_gc(x_con, gc, win, gc_value_mask, gc_values);

	xcb_map_window(x_con, win);

	xcb_flush(x_con);

	struct window *w = calloc(1, sizeof(*w));
	w->x_con = x_con;
	w->win = win;
	w->gc = gc;

	char path[256];
	snprintf(path, 256, "/com/example/windows/%i", win);
	if (!dbus_connection_register_object_path(dbus_con, path, &window_dbus_vtable, w)) {
		fprintf(stderr, "Failed to register window %i object\n", win);
	}

	return w;
}

void window_expose(struct window *window, xcb_expose_event_t *expose_event)
{
	xcb_rectangle_t rect = {
		.x = expose_event->x,
		.y = expose_event->y,
		.width = expose_event->width,
		.height = expose_event->height
	};
	xcb_poly_fill_rectangle(window->x_con, window->win, window->gc, 1, &rect);
	xcb_flush(window->x_con);
}

int main(int argc, char **argv)
{
	xcb_connection_t *x_con = xcb_connect(NULL, 0);
	if (x_con == NULL) {
		fprintf(stderr, "Failed to connect to x server\n");
		return 1;
	}

	DBusConnection *dbus_con = dbus_bus_get(DBUS_BUS_SESSION, NULL);
	if (dbus_con == NULL) {
		fprintf(stderr, "Failed to connect to session bus\n");
		return 1;
	}

	const char *dbus_id = "com.example.echo";
	if (!dbus_bus_request_name(dbus_con, dbus_id, 0, NULL)) {
		fprintf(stderr, "WARNING: failed to claim bus name %s\n", dbus_id);
		dbus_id = dbus_bus_get_unique_name(dbus_con);
	}
	printf("INFO: dbus name is %s\n", dbus_id);

	struct watch_list watch_list;
	memset(&watch_list, 0, sizeof(watch_list));
	dbus_connection_set_watch_functions(
		dbus_con,
		add_watch,
		remove_watch,
		NULL,
		&watch_list,
		NULL);

	if (!dbus_connection_register_object_path(dbus_con, "/com/example/echo", &echo_vtable, NULL)) {
		fprintf(stderr, "Failed to register echo object\n");
		return 1;
	}

	int windows_count = 2;
	struct window **windows = calloc(windows_count, sizeof(windows[0]));
	for (int i = 0; i < windows_count; i++) {
		windows[i] = create_window(x_con, dbus_con);
	}

	struct pollfd *fds = NULL;
	for(;;) {
		fds = reallocarray(fds, 1 + watch_list.count, sizeof(fds[0]));

		fds[0].fd = xcb_get_file_descriptor(x_con);
		fds[0].events = POLLIN;

		for(int i = 0; i < watch_list.count; i++) {
			fds[i + 1].fd = dbus_watch_get_unix_fd(watch_list.items[i]);
			fds[i + 1].events = dbus_watch_get_flags(watch_list.items[i]);
		}

		if (poll(fds, 1 + watch_list.count, -1) < 0) {
			fprintf(stderr, "Failed to poll\n");
			return 1;
		}

		if (fds[0].revents) {
			for(;;) {
				xcb_generic_event_t *event = xcb_poll_for_event(x_con);
				if (event == NULL) {
					if (xcb_connection_has_error(x_con)) {
						fprintf(stderr, "XCB IO error\n");
						return 1;
					}
					break;
				}

				switch(event->response_type) {
					case 0: {
						fprintf(stderr, "X11 error event\n");
						return 1;
					} break;
					case XCB_EXPOSE: {
						printf("expose\n");
						xcb_expose_event_t *expose_event = event;
						for(int i = 0; i < windows_count; i++) {
							if (windows[i]->win == expose_event->window) {
								window_expose(windows[i], expose_event);
							}
						}
					} break;
					default: {
						fprintf(stderr, "WARNING: Unknown X11 event type: %d\n", event->response_type);
					} break;
				}
			}
		}

		for(int i = 0; i < watch_list.count; i++) {
			if(fds[i + 1].revents) {
				dbus_watch_handle(watch_list.items[i], fds[i + 1].revents);
			}
		}

		dbus_connection_dispatch(dbus_con);
	}

	return 0;
}