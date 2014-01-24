/*
 * fg_main_blackberry.c
 *
 * The BlackBerry-specific windows message processing methods.
 *
 * Copyright (c) 1999-2000 Pawel W. Olszta. All Rights Reserved.
 * Written by Pawel W. Olszta, <olszta@sourceforge.net>
 * Copied for Platform code by Evan Felix <karcaw at gmail.com>
 * Copyright (C) 2012  Sylvain Beucler
 * Copyright (C) 2013  Vincent Simonetti
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * PAWEL W. OLSZTA BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <GL/freeglut.h>
#include "fg_internal.h"
#include "egl/fg_window_egl.h"

#include <slog2.h>
#define LOGI(...) ((void)slog2fa(NULL, 1337, SLOG2_INFO, __VA_ARGS__, SLOG2_FA_END))
#define LOGW(...) ((void)slog2fa(NULL, 1337, SLOG2_WARNING, __VA_ARGS__, SLOG2_FA_END))
#include <sys/keycodes.h>
#include <input/screen_helpers.h>
#include <bps/bps.h>
#include <bps/event.h>
#include <bps/screen.h>
#include <bps/navigator.h>

extern void fghOnReshapeNotify(SFG_Window *window, int width, int height, GLboolean forceNotify);
extern void fghOnPositionNotify(SFG_Window *window, int x, int y, GLboolean forceNotify);
extern void fgPlatformFullScreenToggle( SFG_Window *win );
extern void fgPlatformPositionWindow( SFG_Window *window, int x, int y );
extern void fgPlatformReshapeWindow ( SFG_Window *window, int width, int height );
extern void fgPlatformPushWindow( SFG_Window *window );
extern void fgPlatformPopWindow( SFG_Window *window );
extern void fgPlatformHideWindow( SFG_Window *window );
extern void fgPlatformIconifyWindow( SFG_Window *window );
extern void fgPlatformShowWindow( SFG_Window *window );

static struct touchscreen touchscreen;

unsigned int key_special(int qnxKeycode)
{
    switch(qnxKeycode) {
    case KEYCODE_F1:
        return GLUT_KEY_F1;
    case KEYCODE_F2:
        return GLUT_KEY_F2;
    case KEYCODE_F3:
        return GLUT_KEY_F3;
    case KEYCODE_F4:
        return GLUT_KEY_F4;
    case KEYCODE_F5:
        return GLUT_KEY_F5;
    case KEYCODE_F6:
        return GLUT_KEY_F6;
    case KEYCODE_F7:
        return GLUT_KEY_F7;
    case KEYCODE_F8:
        return GLUT_KEY_F8;
    case KEYCODE_F9:
        return GLUT_KEY_F9;
    case KEYCODE_F10:
        return GLUT_KEY_F10;
    case KEYCODE_F11:
        return GLUT_KEY_F11;
    case KEYCODE_F12:
        return GLUT_KEY_F12;
    case KEYCODE_PG_UP:
        return GLUT_KEY_PAGE_UP;
    case KEYCODE_PG_DOWN:
        return GLUT_KEY_PAGE_DOWN;
    case KEYCODE_HOME:
        return GLUT_KEY_HOME;
    case KEYCODE_END:
        return GLUT_KEY_END;
    case KEYCODE_INSERT:
        return GLUT_KEY_INSERT;
    case KEYCODE_UP:
    //case KEYCODE_KP_UP:
        return GLUT_KEY_UP;
    case KEYCODE_DOWN:
    //case KEYCODE_KP_DOWN:
        return GLUT_KEY_DOWN;
    case KEYCODE_LEFT:
    //case KEYCODE_KP_LEFT:
        return GLUT_KEY_LEFT;
    case KEYCODE_RIGHT:
    //case KEYCODE_KP_RIGHT:
        return GLUT_KEY_RIGHT;
    case KEYCODE_NUM_LOCK:
        return GLUT_KEY_NUM_LOCK;
    case KEYCODE_LEFT_ALT:
        return GLUT_KEY_ALT_L;
    case KEYCODE_RIGHT_ALT:
        return GLUT_KEY_ALT_R;
    case KEYCODE_LEFT_SHIFT:
        return GLUT_KEY_SHIFT_L;
    case KEYCODE_RIGHT_SHIFT:
        return GLUT_KEY_SHIFT_R;
    case KEYCODE_LEFT_CTRL:
        return GLUT_KEY_CTRL_L;
    case KEYCODE_RIGHT_CTRL:
        return GLUT_KEY_CTRL_R;
    }
    return 0;
}

unsigned char key_ascii(int qnxKeycode)
{
    if (qnxKeycode >= KEYCODE_PC_KEYS && qnxKeycode <= UNICODE_PRIVATE_USE_AREA_LAST) {
        switch (qnxKeycode) {
        case KEYCODE_BACKSPACE:
            return 0x0008;
        case KEYCODE_TAB:
            return 0x0009;
        case KEYCODE_KP_ENTER:
        case KEYCODE_RETURN:
            return 0x000A;
        case KEYCODE_ESCAPE:
            return 0x001B;
        }
    }
    return qnxKeycode;
}

//From fg_main_x11
fg_time_t fgPlatformSystemTime ( void )
{
#ifdef CLOCK_MONOTONIC
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return now.tv_nsec/1000000 + now.tv_sec*1000;
#elif defined(HAVE_GETTIMEOFDAY)
    struct timeval now;
    gettimeofday( &now, NULL );
    return now.tv_usec/1000 + now.tv_sec*1000;
#endif
}

/*
 * Does the magic required to relinquish the CPU until something interesting
 * happens.
 */
void fgPlatformSleepForEvents( fg_time_t msec )
{
    //XXX: Is this right? Is there a more direct way to access the context?
    if(fgStructure.CurrentWindow && bps_get_event(&fgStructure.CurrentWindow->Window.pContext.event, (int)msec) != BPS_SUCCESS) {
        LOGW("BPS couldn't get event");
    }
}

void handle_left_mouse(int x, int y, int height, int eventType, SFG_Window* window)
{
    bool handled = false;
    /* Virtual arrows PAD */
    /* Don't interfere with existing mouse move event */
    if (!touchscreen.in_mmotion) {
        struct vpad_state prev_vpad = touchscreen.vpad;
        touchscreen.vpad.left = touchscreen.vpad.right = touchscreen.vpad.up = touchscreen.vpad.down = false;

        if (eventType == SCREEN_EVENT_MTOUCH_TOUCH || eventType == SCREEN_EVENT_MTOUCH_MOVE) {
            if ((x > 0 && x < 100) && (y > (height - 100) && y < height))
            {
                touchscreen.vpad.left = true;
            }
            if ((x > 200 && x < 300) && (y > (height - 100) && y < height))
            {
                touchscreen.vpad.right = true;
            }
            if ((x > 100 && x < 200) && (y > (height - 100) && y < height))
            {
                touchscreen.vpad.down = true;
            }
            if ((x > 100 && x < 200) && (y > (height - 200) && y < (height - 100)))
            {
                touchscreen.vpad.up = true;
            }
        }

        if (eventType == SCREEN_EVENT_MTOUCH_TOUCH &&
                (touchscreen.vpad.left || touchscreen.vpad.right || touchscreen.vpad.down || touchscreen.vpad.up)) {
            touchscreen.vpad.on = true;
        }
        if (eventType == SCREEN_EVENT_MTOUCH_RELEASE) {
            touchscreen.vpad.on = false;
        }

        if (prev_vpad.left != touchscreen.vpad.left
                || prev_vpad.right != touchscreen.vpad.right
                || prev_vpad.up != touchscreen.vpad.up
                || prev_vpad.down != touchscreen.vpad.down
                || prev_vpad.on != touchscreen.vpad.on) {
            if (FETCH_WCB(*window, Special)) {
                if (prev_vpad.left == false && touchscreen.vpad.left == true) {
                    INVOKE_WCB(*window, Special, (GLUT_KEY_LEFT, x, y));
                }
                else if (prev_vpad.right == false && touchscreen.vpad.right == true) {
                    INVOKE_WCB(*window, Special, (GLUT_KEY_RIGHT, x, y));
                }
                else if (prev_vpad.up == false && touchscreen.vpad.up == true) {
                    INVOKE_WCB(*window, Special, (GLUT_KEY_UP, x, y));
                }
                else if (prev_vpad.down == false && touchscreen.vpad.down == true) {
                    INVOKE_WCB(*window, Special, (GLUT_KEY_DOWN, x, y));
                }
            }
            if (FETCH_WCB(*window, SpecialUp)) {
                if (prev_vpad.left == true && touchscreen.vpad.left == false) {
                    INVOKE_WCB(*window, SpecialUp, (GLUT_KEY_LEFT, x, y));
                }
                if (prev_vpad.right == true && touchscreen.vpad.right == false) {
                    INVOKE_WCB(*window, SpecialUp, (GLUT_KEY_RIGHT, x, y));
                }
                if (prev_vpad.up == true && touchscreen.vpad.up == false) {
                    INVOKE_WCB(*window, SpecialUp, (GLUT_KEY_UP, x, y));
                }
                if (prev_vpad.down == true && touchscreen.vpad.down == false) {
                    INVOKE_WCB(*window, SpecialUp, (GLUT_KEY_DOWN, x, y));
                }
            }
            handled = true;
        }
    }

    /* Normal mouse events */
    if (!handled && !touchscreen.vpad.on) {
        window->State.MouseX = x;
        window->State.MouseY = y;

        if(eventType == SCREEN_EVENT_MTOUCH_MOVE) {
            INVOKE_WCB(*window, Motion, (x, y));
        } else if(FETCH_WCB(*window, Mouse)) {
            touchscreen.in_mmotion = eventType == SCREEN_EVENT_MTOUCH_TOUCH;
            int glutTouchType = eventType == SCREEN_EVENT_MTOUCH_TOUCH ? GLUT_DOWN : GLUT_UP;
            INVOKE_WCB(*window, Mouse, (GLUT_LEFT_BUTTON, glutTouchType, x, y));
        }
    }
}

void fgPlatformProcessSingleEvent ( void )
{
    int domain;
    bps_event_t** eventPtr = &fgStructure.CurrentWindow->Window.pContext.event; //XXX Is there a more direct way to access the context?
    bps_event_t* event;
    do
    {
        if(*eventPtr != NULL) {
            SFG_Window* window = fgStructure.CurrentWindow;
            if (window != NULL && window->Window.Handle != NULL) {
                int size[2];
                screen_get_window_property_iv(window->Window.Handle, SCREEN_PROPERTY_BUFFER_SIZE, size);
                fghOnReshapeNotify(window,size[0],size[1],GL_FALSE);
            }

            event = *eventPtr;
            domain = bps_event_get_domain(event);
            if (domain == screen_get_domain()) {
                int eventType;
                screen_event_t screenEvent = screen_event_get_event(event);
                screen_get_event_property_iv(screenEvent, SCREEN_PROPERTY_TYPE, &eventType);
                switch (eventType) {

                //Mostly from fg_main_android
                case SCREEN_EVENT_MTOUCH_TOUCH:
                case SCREEN_EVENT_MTOUCH_RELEASE:
                case SCREEN_EVENT_MTOUCH_MOVE:
                {
                    mtouch_event_t touchEvent;
                    screen_get_mtouch_event(screenEvent, &touchEvent, 0);
                    LOGI("fgPlatformProcessSingleEvent: SCREEN_EVENT_MTOUCH_*: Type: 0x%X, X: %d, Y: %d, Contact Id: %d", SLOG2_FA_SIGNED(eventType), SLOG2_FA_SIGNED(touchEvent.x), SLOG2_FA_SIGNED(touchEvent.y), SLOG2_FA_SIGNED(touchEvent.contact_id));
                    if(touchEvent.contact_id == 0) {
                        int size[2];
                        screen_get_window_property_iv(window->Window.Handle, SCREEN_PROPERTY_BUFFER_SIZE, size);
                        handle_left_mouse(touchEvent.x, touchEvent.y, size[1], eventType, window);
                    }

                    //Now handle mutlitouch (adapted from fg_main_windows)
                    if (eventType == SCREEN_EVENT_MTOUCH_TOUCH) {
                        INVOKE_WCB( *window, MultiEntry,  ( touchEvent.contact_id, GLUT_ENTERED ) );
                        INVOKE_WCB( *window, MultiButton, ( touchEvent.contact_id, touchEvent.x, touchEvent.y, 0, GLUT_DOWN ) );
                    } else if (eventType == SCREEN_EVENT_MTOUCH_MOVE) {
                        INVOKE_WCB( *window, MultiMotion, ( touchEvent.contact_id, touchEvent.x, touchEvent.y ) );
                        //XXX No motion is performed without contact, thus MultiPassive is never used
                    } else if (eventType == SCREEN_EVENT_MTOUCH_RELEASE) {
                        INVOKE_WCB( *window, MultiButton, ( touchEvent.contact_id, touchEvent.x, touchEvent.y, 0, GLUT_UP ) );
                        INVOKE_WCB( *window, MultiEntry,  ( touchEvent.contact_id, GLUT_LEFT ) );
                    }
                    break;
                }

                case SCREEN_EVENT_POINTER:
                {
                    //Based off/part taken from GamePlay3d PlatformBlackBerry
                    static int mouse_pressed = 0;
                    int buttons;
                    int position[2];
                    int wheel;
                    // A move event will be fired unless a button state changed.
                    bool move = true;
                    bool left_move = false;
                    // This is a mouse move event, it is applicable to a device with a usb mouse or simulator.
                    screen_get_event_property_iv(screenEvent, SCREEN_PROPERTY_BUTTONS, &buttons);
                    screen_get_event_property_iv(screenEvent, SCREEN_PROPERTY_SOURCE_POSITION, position);
                    screen_get_event_property_iv(screenEvent, SCREEN_PROPERTY_MOUSE_WHEEL, &wheel);
                    int size[2];
                    screen_get_window_property_iv(window->Window.Handle, SCREEN_PROPERTY_BUFFER_SIZE, size);

                    LOGI("fgPlatformProcessSingleEvent: SCREEN_EVENT_POINTER: Buttons: 0x%X, X: %d, Y: %d, Wheel: %d", SLOG2_FA_SIGNED(buttons), SLOG2_FA_SIGNED(position[0]), SLOG2_FA_SIGNED(position[1]), SLOG2_FA_SIGNED(wheel));

                    //XXX Should multitouch be handled?

                    // Handle left mouse. Interpret as touch if the left mouse event is not consumed.
                    if (buttons & SCREEN_LEFT_MOUSE_BUTTON) {
                        if (mouse_pressed & SCREEN_LEFT_MOUSE_BUTTON) {
                            left_move = true;
                        } else {
                            move = false;
                            mouse_pressed |= SCREEN_LEFT_MOUSE_BUTTON;
                            handle_left_mouse(position[0], position[1], size[1], SCREEN_EVENT_MTOUCH_TOUCH, window);
                        }
                    } else if (mouse_pressed & SCREEN_LEFT_MOUSE_BUTTON) {
                        move = false;
                        mouse_pressed &= ~SCREEN_LEFT_MOUSE_BUTTON;
                        handle_left_mouse(position[0], position[1], size[1], SCREEN_EVENT_MTOUCH_RELEASE, window);
                    }

                    // Handle right mouse.
                    if (buttons & SCREEN_RIGHT_MOUSE_BUTTON) {
                        if ((mouse_pressed & SCREEN_RIGHT_MOUSE_BUTTON) == 0) {
                            move = false;
                            mouse_pressed |= SCREEN_RIGHT_MOUSE_BUTTON;
                            INVOKE_WCB(*window, Mouse, (GLUT_RIGHT_BUTTON, GLUT_DOWN, position[0], position[1]));
                        }
                    } else if (mouse_pressed & SCREEN_RIGHT_MOUSE_BUTTON) {
                        move = false;
                        mouse_pressed &= ~SCREEN_RIGHT_MOUSE_BUTTON;
                        INVOKE_WCB(*window, Mouse, (GLUT_RIGHT_BUTTON, GLUT_UP, position[0], position[1]));
                    }

                    // Handle middle mouse.
                    if (buttons & SCREEN_MIDDLE_MOUSE_BUTTON) {
                        if ((mouse_pressed & SCREEN_MIDDLE_MOUSE_BUTTON) == 0) {
                            move = false;
                            mouse_pressed |= SCREEN_MIDDLE_MOUSE_BUTTON;
                            INVOKE_WCB(*window, Mouse, (GLUT_MIDDLE_BUTTON, GLUT_DOWN, position[0], position[1]));
                        }
                    } else if (mouse_pressed & SCREEN_MIDDLE_MOUSE_BUTTON) {
                        move = false;
                        mouse_pressed &= ~SCREEN_MIDDLE_MOUSE_BUTTON;
                        INVOKE_WCB(*window, Mouse, (GLUT_MIDDLE_BUTTON, GLUT_UP, position[0], position[1]));
                    }

                    // Fire a move event if none of the buttons changed.
                    if (left_move || move) {
                        handle_left_mouse(position[0], position[1], size[1], SCREEN_EVENT_MTOUCH_MOVE, window);
                    }

                    if (wheel) {
                        fgState.MouseWheelTicks -= wheel;
                        //TODO: Implement wheel support (based on fg_main_mswin... though it seems excessive)
                    }
                    break;
                }

                //Based off fg_main_android
                case SCREEN_EVENT_KEYBOARD:
                {
                    int flags;
                    int value;
                    screen_get_event_property_iv(screenEvent, SCREEN_PROPERTY_KEY_FLAGS, &flags);
                    screen_get_event_property_iv(screenEvent, SCREEN_PROPERTY_KEY_SYM, &value);
                    LOGI("fgPlatformProcessSingleEvent: SCREEN_EVENT_KEYBOARD. Flags: 0x%X, Sym: 0x%X", SLOG2_FA_SIGNED(flags), SLOG2_FA_SIGNED(value));
                    // Suppress key repeats if desired
                    if ((flags & KEY_REPEAT) == 0 || (fgState.KeyRepeat == GLUT_KEY_REPEAT_ON && !fgStructure.CurrentWindow->State.IgnoreKeyRepeat)) {
                        unsigned int keypress = 0;
                        unsigned char ascii = 0;
                        if ((keypress = key_special(value))) {
                            if(flags & KEY_DOWN) {
                                INVOKE_WCB(*window, Special, (keypress, window->State.MouseX, window->State.MouseY));
                            } else {
                                INVOKE_WCB(*window, SpecialUp, (keypress, window->State.MouseX, window->State.MouseY));
                            }
                        } else if((flags & KEY_SYM_VALID) && (ascii = key_ascii(value))) {
                            if(flags & KEY_DOWN) {
                                INVOKE_WCB(*window, Keyboard, (ascii, window->State.MouseX, window->State.MouseY));
                            } else {
                                INVOKE_WCB(*window, KeyboardUp, (ascii, window->State.MouseX, window->State.MouseY));
                            }
                        }
                    }
                    break;
                }

                case SCREEN_EVENT_PROPERTY:
                case SCREEN_EVENT_IDLE:
                    break;

                default:
                    LOGW("fgPlatformProcessSingleEvent: unknown screen event: 0x%X", SLOG2_FA_SIGNED(eventType));
                    break;
                }
            } else if (domain == navigator_get_domain()) {
                int eventType = bps_event_get_code(event);
                switch (eventType) {

                case NAVIGATOR_WINDOW_STATE:
                {
                    LOGI("fgPlatformProcessSingleEvent: NAVIGATOR_WINDOW_STATE");
                    navigator_window_state_t state = navigator_event_get_window_state(event);
                    switch (state)
                    {
                    case NAVIGATOR_WINDOW_FULLSCREEN:
                        LOGI("fgPlatformProcessSingleEvent: NAVIGATOR_WINDOW_STATE-NAVIGATOR_WINDOW_FULLSCREEN");
                        INVOKE_WCB(*window, AppStatus, (GLUT_APPSTATUS_RESUME));
                        break;
                    case NAVIGATOR_WINDOW_THUMBNAIL:
                    case NAVIGATOR_WINDOW_INVISIBLE:
                        LOGI("fgPlatformProcessSingleEvent: NAVIGATOR_WINDOW_STATE-NAVIGATOR_WINDOW_THUMBNAIL/NAVIGATOR_WINDOW_INVISIBLE");
                        INVOKE_WCB(*window, AppStatus, (GLUT_APPSTATUS_PAUSE));
                        break;
                    default:
                        LOGW("fgPlatformProcessSingleEvent: NAVIGATOR_WINDOW_STATE unknown: 0x%X", SLOG2_FA_SIGNED(state));
                        break;
                    }
                    break;
                }

                case NAVIGATOR_EXIT:
                {
                    LOGI("fgPlatformProcessSingleEvent: NAVIGATOR_EXIT");
                    /* User closed the application for good, let's kill the window */
                    SFG_Window* window = fgStructure.CurrentWindow;
                    if (window != NULL) {
                        fgDestroyWindow(window);
                    } else {
                        LOGW("NAVIGATOR_EXIT: No current window");
                    }
                    break;
                }

                case NAVIGATOR_SWIPE_DOWN:
                case NAVIGATOR_BACK:
                case NAVIGATOR_WINDOW_ACTIVE:
                case NAVIGATOR_WINDOW_INACTIVE:
                case NAVIGATOR_KEYBOARD_STATE:
                case NAVIGATOR_KEYBOARD_POSITION:
                case NAVIGATOR_DEVICE_LOCK_STATE:
                case NAVIGATOR_WINDOW_COVER:
                case NAVIGATOR_WINDOW_COVER_ENTER:
                case NAVIGATOR_WINDOW_COVER_EXIT:
                case NAVIGATOR_APP_STATE:
                    //XXX Should probably do something with these
                    break;

                default:
                    LOGW("fgPlatformProcessSingleEvent: unknown navigator event: 0x%X", SLOG2_FA_SIGNED(eventType));
                    break;
                }
            }
        }
    } while(bps_get_event(eventPtr, 1) == BPS_SUCCESS && *eventPtr != NULL);

    /* Reset event to reduce chances of triggering something */
    *eventPtr = NULL;
}

void fgPlatformMainLoopPreliminaryWork ( void )
{
    LOGI("fgPlatformMainLoopPreliminaryWork");
}


/* deal with work list items */
void fgPlatformInitWork(SFG_Window* window)
{
    /* notify windowStatus/visibility */
    INVOKE_WCB( *window, WindowStatus, ( GLUT_FULLY_RETAINED ) );

    /* Position callback, always at 0,0 */
    fghOnPositionNotify(window, 0, 0, GL_TRUE);

    /* Size gets notified on window creation with size detection in mainloop above
     * XXX CHECK: does this messages happen too early like on windows,
     * so client code cannot have registered a callback yet and the message
     * is thus never received by client?
     */
}

void fgPlatformPosResZordWork(SFG_Window* window, unsigned int workMask)
{
    if (workMask & GLUT_FULL_SCREEN_WORK)
        fgPlatformFullScreenToggle( window );
    if (workMask & GLUT_POSITION_WORK)
        fgPlatformPositionWindow( window, window->State.DesiredXpos, window->State.DesiredYpos );
    if (workMask & GLUT_SIZE_WORK)
        fgPlatformReshapeWindow ( window, window->State.DesiredWidth, window->State.DesiredHeight );
    if (workMask & GLUT_ZORDER_WORK)
    {
        if (window->State.DesiredZOrder < 0)
            fgPlatformPushWindow( window );
        else
            fgPlatformPopWindow( window );
    }
}

void fgPlatformVisibilityWork(SFG_Window* window)
{
    /* Visibility status of window should get updated in the window message handlers
     * For now, none of these functions called below do anything, so don't worry
     * about it
     */
    SFG_Window *win = window;
    switch (window->State.DesiredVisibility)
    {
    case DesireHiddenState:
        fgPlatformHideWindow( window );
        break;
    case DesireIconicState:
        /* Call on top-level window */
        while (win->Parent)
            win = win->Parent;
        fgPlatformIconifyWindow( win );
        break;
    case DesireNormalState:
        fgPlatformShowWindow( window );
        break;
    }
}

