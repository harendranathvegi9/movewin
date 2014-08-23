#include "winutils.h"

/* Search windows for match (NULL for all), run function (NULL for none) */
int windowList(
    char *pattern,
    void(*callback)(CFDictionaryRef window, void *callback_data),
    void *callback_data
) {
    CFArrayRef windowList;
    int count, i, layer, titleSize;
    CFDictionaryRef window;
    char *appName, *windowName, *title;

    windowList = CGWindowListCopyWindowInfo(
        (kCGWindowListOptionOnScreenOnly|kCGWindowListExcludeDesktopElements),
        kCGNullWindowID
    );
    count = 0;
    for(i = 0; i < CFArrayGetCount(windowList); i++) {
        window = CFArrayGetValueAtIndex(windowList, i);
        layer = CFDictionaryGetInt(window, kCGWindowLayer);
        if(layer > 0) continue;

        appName = CFDictionaryCopyCString(window, kCGWindowOwnerName);
        windowName = CFDictionaryCopyCString(window, kCGWindowName);
        titleSize = strlen(appName) + strlen(" - ") + strlen(windowName) + 1;
        title = (char *)malloc(titleSize);
        snprintf(title, titleSize, "%s - %s", appName, windowName);

        if(!pattern || fnmatch(pattern, title, 0) == 0) {
            if(callback) (*callback)(window, callback_data);
            count++;
        }

        free(title);
        free(windowName);
        free(appName);
    }

    return count;
}

/* Fetch an integer value from a CFDictionary */
int CFDictionaryGetInt(CFDictionaryRef dict, const void *key) {
    int isSuccess, value;

    isSuccess = CFNumberGetValue(
        CFDictionaryGetValue(dict, key), kCFNumberIntType, &value
    );

    return isSuccess ? value : 0;
}

/* Copy a string value from a CFDictionary into a newly allocated string */
char *CFDictionaryCopyCString(CFDictionaryRef dict, const void *key) {
    const void *dictValue;
    CFIndex length;
    int maxSize, isSuccess;
    char *value;

    dictValue = CFDictionaryGetValue(dict, key);
    if(dictValue == (void *)NULL) return (char *)NULL;

    length = CFStringGetLength(dictValue);
    maxSize = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8);
    if(length == 0 || maxSize == 0) {
        value = (char *)malloc(1);
        *value = '\0';
        return value;
    }

    value = (char *)malloc(maxSize);
    isSuccess = CFStringGetCString(
        dictValue, value, maxSize, kCFStringEncodingUTF8
    );

    return isSuccess ? value : (char *)NULL;
}

/* Given window dictionary from CGWindowList, return position */
CGPoint CGWindowGetPosition(CFDictionaryRef window) {
    CFDictionaryRef bounds = CFDictionaryGetValue(window, kCGWindowBounds);
    int x = CFDictionaryGetInt(bounds, CFSTR("X"));
    int y = CFDictionaryGetInt(bounds, CFSTR("Y"));
    return CGPointMake(x, y);
}

/* Given window dictionary from CGWindowList, return size */
CGSize CGWindowGetSize(CFDictionaryRef window) {
    CFDictionaryRef bounds = CFDictionaryGetValue(window, kCGWindowBounds);
    int width = CFDictionaryGetInt(bounds, CFSTR("Width"));
    int height = CFDictionaryGetInt(bounds, CFSTR("Height"));
    return CGSizeMake(width, height);
}

/* Given window dictionary from CGWindowList, return accessibility object */
AXUIElementRef AXWindowFromCGWindow(CFDictionaryRef window) {
    CFStringRef targetWindowName, actualWindowTitle;
    CGPoint targetPosition, actualPosition;
    CGSize targetSize, actualSize;
    pid_t pid;
    AXUIElementRef app, appWindow, foundAppWindow;
    CFArrayRef appWindowList;
    int i;

    /* Save the window name, position, and size we are looking for */
    targetWindowName = CFDictionaryGetValue(window, kCGWindowName);
    targetPosition = CGWindowGetPosition(window);
    targetSize = CGWindowGetSize(window);

    /* Load accessibility application from window PID */
    pid = CFDictionaryGetInt(window, kCGWindowOwnerPID);
    app = AXUIElementCreateApplication(pid);
    AXUIElementCopyAttributeValue(
        app, kAXWindowsAttribute, (CFTypeRef *)&appWindowList
    );

    /* Search application windows for first matching title, position, size */
    foundAppWindow = NULL;
    for(i = 0; i < CFArrayGetCount(appWindowList); i++) {
        appWindow = CFArrayGetValueAtIndex(appWindowList, i);

        /* Window name must match */
        AXUIElementCopyAttributeValue(
            appWindow, kAXTitleAttribute, (CFTypeRef *)&actualWindowTitle
        );
        if(CFStringCompare(targetWindowName, actualWindowTitle, 0) != 0) continue;

        /* Position and size must match */
        actualPosition = AXWindowGetPosition(appWindow);
        if(!CGPointEqualToPoint(targetPosition, actualPosition)) continue;
        actualSize = AXWindowGetSize(appWindow);
        if(!CGSizeEqualToSize(targetSize, actualSize)) continue;

        /* If we got here, we found the first matching window, save and break */
        foundAppWindow = appWindow;
        break;
    }

    return foundAppWindow;
}

/* Get a value from an accessibility object */
void AXWindowGetValue(
    AXUIElementRef window,
    CFStringRef attrName,
    void *valuePtr
) {
    AXValueRef attrValue;
    AXUIElementCopyAttributeValue(window, attrName, (CFTypeRef *)&attrValue);
    AXValueGetValue(attrValue, AXValueGetType(attrValue), valuePtr);
    CFRelease(attrValue);
}

/* Get position of window via accessibility object */
CGPoint AXWindowGetPosition(AXUIElementRef window) {
    CGPoint position;
    AXWindowGetValue(window, kAXPositionAttribute, &position);
    return position;
}

/* Set position of window via accessibility object */
void AXWindowSetPosition(AXUIElementRef window, CGPoint position) {
    AXValueRef attrValue = AXValueCreate(kAXValueCGPointType, &position);
    AXUIElementSetAttributeValue(window, kAXPositionAttribute, attrValue);
    CFRelease(attrValue);
}

/* Get size of window via accessibility object */
CGSize AXWindowGetSize(AXUIElementRef window) {
    CGSize size;
    AXWindowGetValue(window, kAXSizeAttribute, &size);
    return size;
}

/* Set size of window via accessibility object */
void AXWindowSetSize(AXUIElementRef window, CGSize size) {
    AXValueRef attrValue = AXValueCreate(kAXValueCGSizeType, &size);
    AXUIElementSetAttributeValue(window, kAXSizeAttribute, attrValue);
    CFRelease(attrValue);
}