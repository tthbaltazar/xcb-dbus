#include <stdio.h>

#include <xcb/xcb.h>

int main(int argc, char **argv)
{
	xcb_connection_t *x_con = xcb_connect(NULL, 0);
	if (x_con == NULL) {
		fprintf(stderr, "Failed to connect to x server\n");
		return 1;
	}

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

	xcb_map_window(x_con, win);

	xcb_flush(x_con);

	for(;;) {
		xcb_generic_event_t *event = xcb_wait_for_event(x_con);
		if (event == NULL) {
			fprintf(stderr, "XCB IO error\n");
			return 1;
		}

		switch(event->response_type) {
			case 0: {
				fprintf(stderr, "X11 error event\n");
				return 1;
			} break;
			case XCB_EXPOSE: {
				printf("expose\n");
				// TODO: fill window with color
			} break;
			default: {
				fprintf(stderr, "WARNING: Unknown X11 event type: %d\n", event->response_type);
			} break;
		}
	}

	return 0;
}