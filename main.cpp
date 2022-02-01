#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XTest.h>
#include <stdio.h>
#include <stdlib.h>
#include <mutex>
#include <thread>
#include <unistd.h>

// -lX11 -lXtst
// XTestFakeRelativeMotionEvent(display, 50, 50, 1);

struct State {
	bool up;
	bool down;
	bool left;
	bool right;
	bool paused;
	bool running;
};

std::mutex state_mutex;

void event_thread_fn(State* state, Display* display) {
	XEvent event;
	int kc;
	bool ctrl_pressed;
	bool c_pressed;

	int UP    = XKeysymToKeycode(display, XK_O);
	int LEFT  = XKeysymToKeycode(display, XK_K);
	int DOWN  = XKeysymToKeycode(display, XK_L);
	int RIGHT = XKeysymToKeycode(display, XK_semicolon);
	int PAUSE = XKeysymToKeycode(display, XK_backslash);
	int CTRL  = XKeysymToKeycode(display, XK_Control_L);
	int C     = XKeysymToKeycode(display, XK_C);
	int LMENU = XKeysymToKeycode(display, XK_Alt_L);
	int RMENU = XKeysymToKeycode(display, XK_Alt_R);

	while (true) {
		if (ctrl_pressed && c_pressed) {
			state_mutex.lock();
			XCloseDisplay(display);
			state->running = false;
			state_mutex.unlock();

			return;
		}

		XNextEvent(display, &event);

		switch (event.type) {
			case KeyPress:
				kc = event.xkey.keycode;

				state_mutex.lock();

				if (kc == UP) {
					state->up = true;
				} else if (kc == DOWN) {
					state->down = true;
				} else if (kc == LEFT) {
					state->left = true;
				} else if (kc == RIGHT) {
					state->right = true;
				} else if (kc == PAUSE) {
					state->paused = !state->paused;
				} else if (kc == CTRL) {
					ctrl_pressed = true;
				} else if (kc == C) {
					c_pressed = true;
				} else if (kc == RMENU) {
					XTestFakeButtonEvent(display, 3, true, 0);
					XFlush(display);
				} else if (kc == LMENU) {
					XTestFakeButtonEvent(display, Button1, true, 0);
					XFlush(display);
				}

				state_mutex.unlock();

				break;

			case KeyRelease:
				kc = event.xkey.keycode;

				state_mutex.lock();

				if (kc == UP) {
					state->up = false;
				} else if (kc == DOWN) {
					state->down = false;
				} else if (kc == LEFT) {
					state->left = false;
				} else if (kc == RIGHT) {
					state->right = false;
				} else if (kc == CTRL) {
					ctrl_pressed = false;
				} else if (kc == C) {
					c_pressed = false;
				} else if (kc == RMENU) {
					XTestFakeButtonEvent(display, 3, false, 0);
					XFlush(display);
				} else if (kc == LMENU) {
					XTestFakeButtonEvent(display, Button1, false, 0);
					XFlush(display);
				}

				state_mutex.unlock();

				break;
		}
	}
}

void mouse_thread_fn() {}

int main(int argc, char** argv) {
	if (!argv[1] || !*argv[1]) {
		printf("the first argument must be the XWindow id\n");
		return 1;
	}

	const int STEP = 5;

	Display* display = XOpenDisplay(0);
	Window root_window = strtoul(argv[1], NULL, 0);

	State state = { false, false, false, false, false, true };

	int revert;

	XSelectInput(display, root_window, KeyPressMask | KeyReleaseMask);
	XGetInputFocus(display, &root_window, &revert);

	std::thread event_thread(event_thread_fn, &state, display);
	
	// try making this a race condition in rust by using the value state.running without locking the mutex
	while (true) {
		state_mutex.lock();
		if (!state.running) {
			return 0;
		}

		// + + b r
		if (!state.paused) {
			if (state.up && !state.down) {
				XTestFakeRelativeMotionEvent(display, 0, -STEP, 0);
			}

			if (state.down && !state.up) {
				XTestFakeRelativeMotionEvent(display, 0, STEP, 0);
			}

			if (state.left && !state.right) {
				XTestFakeRelativeMotionEvent(display, -STEP, 0, 0);
			}

			if (state.right && !state.left) {
				XTestFakeRelativeMotionEvent(display, STEP, 0, 0);
			}
		}

		XFlush(display);

		// printf("STATE up: %d down: %d left: %d right: %d paused: %d running: %d\n", state.up, state.down, state.left, state.right, state.paused, state.running);

		state_mutex.unlock();

		usleep(10000);
	}
}
