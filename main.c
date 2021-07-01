#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_PROPERTY_LEN 1024
#define HELP "Usage: winsperg [OPTION]...\n" \
    "Options:\n" \
    " -h\tDisplay this menu.\n" \
    " -v\tGive more verbose output.\n"

char* getProperty(Display* display, Window window, Atom propType, char* propName, unsigned long* size);
Window* getClientList(Display* display, unsigned long* size);
char* getWindowTitle(Display* display, Window window);
void randomizeGeometry(Display* display, Window window, int screenWidth, int screenHeight);
char wmSupportsResizing(Display* display);
int doFunny(Display* display);

char verbose = 0;

int main(int argc, char** argv)
{
    Display* display;
    Window window;
    time_t t;

    display = XOpenDisplay(NULL);
    window = DefaultRootWindow(display);
    XSelectInput(display, window, SubstructureNotifyMask);
    srand((unsigned) time(&t));

    int opt;
    while ((opt = getopt(argc, argv, "hv")) != -1)
    {
        switch (opt)
        {
            case 'h':
                printf(HELP);
                return EXIT_SUCCESS;
            case 'v':
                verbose = 1;
                break;
            default:
                return EXIT_FAILURE;
        }
    }

    return doFunny(display);
}

char* getProperty(Display* display, Window window, Atom propType, char* propName, unsigned long* size)
{
    Atom name;
    Atom returnType;
    int returnFormat;
    unsigned long numItems, bytesAfter, tmpSize;
    unsigned char* prop;
    char* ret;

    name = XInternAtom(display, propName, 0);

    if (XGetWindowProperty(display, window, name, 0, MAX_PROPERTY_LEN, 0, propType, &returnType, &returnFormat, &numItems, &bytesAfter, &prop) != Success)
    {
        if (verbose) fprintf(stderr, "Cannot get property: %s\n", propName);
        return NULL;
    }

    if (returnType != propType)
    {
        if (verbose) fprintf(stderr, "Cannot get property %s as its return type and property type are not equal.\n", propName);
        XFree(prop);
        return NULL;
    }

    tmpSize = (returnFormat / (32 / sizeof(long))) * numItems;
    ret = malloc(tmpSize + 1);
    memcpy(ret, prop, tmpSize);
    ret[tmpSize] = '\0';

    if (size) *size = tmpSize;

    XFree(prop);
    return ret;
}

Window* getClientList(Display* display, unsigned long* size)
{
    Window* clientList;

    if ((clientList = (Window*)getProperty(display, DefaultRootWindow(display), XA_WINDOW, "_NET_CLIENT_LIST", size)) == NULL)
    {
        if ((clientList = (Window*)getProperty(display, DefaultRootWindow(display), XA_CARDINAL, "_WIN_CLIENT_LIST", size)) == NULL)
        {
            if (verbose) fprintf(stderr, "Cannot get properties of _NET_CLIENT_LIST or _WIN_CLIENT_LIST.\n");
            return NULL;
        }
    }

    return clientList;
}

char* getWindowTitle(Display* display, Window window)
{
    char* title;
    char* wmName;
    char* netWmName;

    wmName = getProperty(display, window, XA_STRING, "WM_NAME", NULL);
    netWmName = getProperty(display, window, XInternAtom(display, "UTF8_STRING", 0), "_NET_WM_NAME", NULL);
    
    if (netWmName)
    {
        title = malloc(strlen(netWmName) + 1);
        strcpy(title, netWmName);
    }
    else
    {
        if (wmName)
        {
            title = malloc(strlen(wmName) + 1);
            strcpy(title, wmName);
        }
        else
        {
            title = NULL;
        }
    }

    free(wmName);
    free(netWmName);

    return title;
}

void randomizeGeometry(Display* display, Window window, int screenWidth, int screenHeight)
{
    int x, y, x2, y2;
    unsigned int width, height, bw, depth;
    Window rootWnd;

    XGetGeometry(display, window, &rootWnd, &x2, &y2, &width, &height, &bw, &depth);
    XTranslateCoordinates(display, window, rootWnd, x2, y2, &x, &y, &rootWnd);

    if (x >= 1 && y >= 1 && width >= 1 && height >= 1)
    {
        int randX = (rand() % (screenWidth - 1)) + 1;
        int randY = (rand() % (screenHeight - 1)) + 1;
        unsigned int randWidth = (rand() % (screenWidth - 1)) + 1;
        unsigned int randHeight = (rand() % (screenHeight - 1)) + 1;
        int result = XMoveResizeWindow(display, window, randX, randY, randWidth, randHeight);
        if (verbose)
        {
            if (result == BadValue) fprintf(stderr, "Failed to resize a window: Bad Value");
            else if (result == BadWindow) fprintf(stderr, "Failed to resize a window: Bad Window");
        }
    }
}

char wmSupportsResizing(Display* display)
{
    Atom prop = XInternAtom(display, "_NET_MOVERESIZE_WINDOW", 0);
    Atom* list;
    unsigned long size;

    if (!(list = (Atom*)getProperty(display, DefaultRootWindow(display), XA_ATOM, "_NET_SUPPORTED", &size))) return 0;

    for (int i = 0; i < size / sizeof(Atom); i++)
    {
        if (list[i] == prop)
        {
            free(list);
            return 1;
        }
    }

    free(list);
    return 0;
}

int doFunny(Display* display)
{
    Window* clientList;
    unsigned long lSize;

    if (!wmSupportsResizing(display))
    {
        fprintf(stderr, "Your window manager is not supported by this program\n");
        return EXIT_FAILURE;
    }

    if ((clientList = getClientList(display, &lSize)) == NULL)
    {
        fprintf(stderr, "Could not get client list\n");
        return EXIT_FAILURE;
    }

    Screen* screen = DefaultScreenOfDisplay(display);
    int screenWidth = WidthOfScreen(screen);
    int screenHeight = HeightOfScreen(screen);

    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = 16969696; // likely safe sleep time so X doesn't die

    if (verbose)
    {
        printf("Currently open windows:\n");
        for (int i = 0; i < lSize / sizeof(Window); i++)
        {
            char* title = getWindowTitle(display, clientList[i]);
            unsigned long* pid;

            pid = (unsigned long*)getProperty(display, clientList[i], XA_CARDINAL, "_NET_WM_PID", NULL);

            printf("PID: %-6lu", pid ? *pid : 0);
            printf("Title: %s\n", title ? title : "None");
            free(title);
            free(pid);
        }
    }

    while (1)
    {
        for (int i = 0; i < lSize / sizeof(Window); i++)
        {
            randomizeGeometry(display, clientList[i], screenWidth, screenHeight);
            nanosleep(&ts, &ts);
        }
    }

    free(clientList);
    return EXIT_SUCCESS;
}
