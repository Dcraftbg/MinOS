#pragma once
#define WM_GETKEY(event)     (event)->arg0
#define WM_GETKEYCODE(event) (event)->arg1

#define WM_SETKEY(event, key)      ((event)->arg0 = (key))
#define WM_SETKEYCODE(event, code) ((event)->arg1 = (code))
