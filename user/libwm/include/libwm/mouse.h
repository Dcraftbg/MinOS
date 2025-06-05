#pragma once
#define WM_GETMOUSE_X(event) (event)->arg0
#define WM_GETMOUSE_Y(event) (event)->arg1

#define WM_SETMOUSE_X(event, x) ((event)->arg0 = (x))
#define WM_SETMOUSE_Y(event, y) ((event)->arg1 = (y))
