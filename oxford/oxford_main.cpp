// Hardware:
// Behringer FCB1010 Midi Out       == connected to ==   MidiSport port A In
// Roland XV5080 Midi In 1          == connected to ==   MidiSport port A Out
// Thomann Master Keyboard Midi Out == connected to ==   MidiSport port B In
// Arturia Master Keyboard Midi Out == connected to ==   MidiSport port C In
// MidiSport USB                    == connected to ==   Raspberry Pi USB

// Assuming that ALSA is used throughout.
// Make sure you have installed libasound2-dev, to get the headers
// Make sure you also have ncurses: libncurses5-dev libncursesw5-dev
// Make sure you have libcdk version 5: libcdk5-dev
//
// The USB midiman MidiSport 4x4 Anniversary Edition hardware
// requires midisport-firmware: install it with:
// apt-get install midisport-firmware
//
// Note: you can use amidi -l to list MIDI hardware devices, to verify ALSA & MidiSport
// firmware is installed properly (after it's connected to USB).
// You can use pmidi -l to list MIDI devices for pmidi, which is for MIDI file playback
// (but this program does not use the ALSA MIDI sequencer anymore)
//
// Don't forget to link with asound, pthread, cdk, panel
//                           ------  -------  ---  -----

//
// More notes:
// run ldconfig as root

// If running into issues with missing libraries, 3 options:
// 1/ make sure the library is in a "common" place with all other system libraries
// 2/ /lib/ld-linux.so.2 --library-path PATH EXECUTABLE
// 3/ export LD_LIBRARY_PATH=/usr/local/lib

// The synth branch is a major departure from the legacy Oxford requirements and focuses
// solely on controlling synth and focusing on MIDI record and replay.

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <alsa/asoundlib.h>
#include <signal.h>
#include <pthread.h>
#include <linux/input.h>
#include <termios.h>
#include <math.h>
#include <sys/time.h>
#include <ncurses.h>
#include <list>
#include <string>
#include <cstring>
#include <algorithm>
#include <cdk/cdk.h>
#include <panel.h>
#include <array>
#include <thread>
#include <mutex>
#include <vector>
#include <map>
#include <locale>
#include "subrange.h"
#include <queue>
#include <condition_variable>
#include <chrono>
#include <ctime>
#include <atomic>
#include <utility>
#include <map>
#include <algorithm>
#include "mosquitto.h"
#include "pthread.h"


static int const MASTER_KBD_PART_INDEX = 3; // Master Keybard talks to parts 4 and up on XV5080

// Midi Channel, on XV5080, that receives MIDI traffic from the master keyboard
// Normally set to 2. This program forwards all master keyboard traffic to channel 2
// on the XV5080. On the XV5080 side, the parts that are link to the master keyboard
// all listen to channel 2.
static int const MIDI_CHANNEL_MASTER_KBD_XV5080 = 2;

static int const MIDI_CHANNEL_ARTURIA = 3;

// Global variable which tells if the ESC key was pressed
bool ESC_Key_Pressed_Flag = false;

int stop = 0;

// String name of the hardware device for the first midi port (IN1/OUT1)
// obtained with amidi -l
// Will probably evaluate to something like "hw:1,0,0" later on
std::string name_midi_hw_MIDISPORT_A = ""; 

// String name of the hardware device for the first midi port (IN2/OUT2)
// obtained with amidi -l
std::string name_midi_hw_MIDISPORT_B = "";

// String name of the hardware device for the first midi port (IN3/OUT3)
// obtained with amidi -l
std::string name_midi_hw_MIDISPORT_C = "";

// String name of the hardware device for the first midi port (IN4/OUT4)
// obtained with amidi -l
std::string name_midi_hw_MIDISPORT_D = "";

// Mutex used to prevent ncurses refresh routines from being called from
// concurrent threads.
std::mutex ncurses_mutex;

// Start threads with top priority
#define REALTIME


// Test XV5080 class
#undef TEST_XV5080

#undef MIDI_KEYBOARD_CONTROLS_ON_KEYS



// Subrange types defined here
typedef subrange::subrange<subrange::ordinal_range<int, 0, 127>, subrange::saturated_arithmetic> TInt_0_127;
typedef subrange::subrange<subrange::ordinal_range<int, 1, 16>, subrange::saturated_arithmetic> TInt_1_16;
typedef subrange::subrange<subrange::ordinal_range<int, 1, 128>, subrange::saturated_arithmetic> TInt_1_128;
typedef subrange::subrange<subrange::ordinal_range<int, 0, 16383>, subrange::saturated_arithmetic> TInt_14bits;
typedef subrange::subrange<subrange::ordinal_range<int, 1, 4>, subrange::saturated_arithmetic> TInt_1_4;



/** This is a ncurses cdk panel, with a boxed window on it, that can't
be touched, and a "free text" window inside the boxed window.
It makes it easy to manage showing (and hiding, thanks panel), of
text, including automatically scrolling it, _inside_ a nice framed (boxed)
window.
To get a reference to the inner window (on which to display text), for instance
with wprinw, use ::GetRef(). E.g.: wprintw(MyBoxedWindow.GetRef(), "Hello World!\n");
Of course, call ::Init(...) beforehand. */
class TBoxedWindow
{
private:
    PANEL * Panel = 0; ///< Panel is the cdk object that allows showing/hiding parts of the screen.
    WINDOW * BoxedWindow = 0; ///< outer window, shown with a box, prevent writing on that
    WINDOW * SubWindow = 0; ///< inner space of the window, usable to display text
public:
    /// Constructor does nothing - call ::Init() manually prior to using members.
    TBoxedWindow(void) {};

    /// Initialization of a boxed window
    void Init(char const * name, int height, int width, int starty, int startx)
    {
        std::lock_guard<std::mutex> lock(ncurses_mutex);
        BoxedWindow = newwin(height, width, starty, startx);
        Panel = new_panel(BoxedWindow);
        box(BoxedWindow, 0, 0);
        mvwprintw(BoxedWindow, 0, 0, name);
        SubWindow = subwin(BoxedWindow, height-2, width-2, starty+1, startx+1);
        scrollok(SubWindow, TRUE);
        idlok(SubWindow, TRUE);
    }

    /// Show the boxed window. That does not guarantees to display on the foreground, use ::PutOnTop() for that.
    void Show(void)
    {
        std::lock_guard<std::mutex> lock(ncurses_mutex);
        show_panel(Panel);
        update_panels();
        doupdate();
    }

    /// Hide the boxed window.
    void Hide(void)
    {
        std::lock_guard<std::mutex> lock(ncurses_mutex);
        hide_panel(Panel);
        update_panels();
        doupdate();
    }

    /**
     * Get a ncurses reference to the window on which text can be displayed.
     *For use with regular ncurses functions that work with a WINDOW * parameter.
     */
    WINDOW * GetRef(void)
    {
        std::lock_guard<std::mutex> lock(ncurses_mutex);
        return SubWindow;
    }

    /// Refresh boxed window
    void Refresh(void)
    {
        std::lock_guard<std::mutex> lock(ncurses_mutex);
        if (!panel_hidden(Panel))
        {
            touchwin(BoxedWindow);
            update_panels();
            doupdate();
        }
    }

    /// Display the boxed window on top of all other windows.
    void PutOnTop(void)
    {
        std::lock_guard<std::mutex> lock(ncurses_mutex);
        top_panel(Panel);
    }

    /// Erase the contents of the boxed window. That keeps the frame decoration and window name though.
    void Erase(void)
    {
        std::lock_guard<std::mutex> lock(ncurses_mutex);
        wclear(SubWindow);
    }
};


// Create various boxed windows to construct the display
TBoxedWindow win_midi_in; ///< Window that contains MIDI IN events
TBoxedWindow win_midi_out; ///< Window that contains MIDI OUT events
TBoxedWindow win_debug_messages; ///< Window that contains debugging messages
TBoxedWindow win_context_prev; ///< Window that contains the name of the previous context in playlist order
TBoxedWindow win_context_current; ///< Window that contains the name of the current context in playlist (current song)
TBoxedWindow win_context_next; ///< Window that contains the name of the next context in playlist order
TBoxedWindow win_context_usage; ///< Window that contains messages as to how to use the pedalboard, keyboards, etc. in the current context
TBoxedWindow win_context_user_specific; ///< Window that displays user-specific information
TBoxedWindow win_context_select_menu;
TBoxedWindow win_big_message; ///< Window that displays a scrolling banner with a message made with large characters


/**
 * Create a way to change a thread scheduling policy as well as priority
 */
void setScheduling_RealTime_TopPriority(pthread_t target_thread)
{
    struct sched_param param;
    param.__sched_priority = sched_get_priority_max(SCHED_RR);
    int s = pthread_setschedparam(target_thread, SCHED_RR, &param);
    if (s != 0)
    {
        wprintw(win_debug_messages.GetRef(), "Failed to set thread scheduling : %s", std::strerror(errno));
        getchar();
    }
}



/// Pause thread execution for "milliseconds" milliseconds, with a fairly high resolution.
void waitMilliseconds(int milliseconds)
{
    long int nanoseconds = milliseconds * 1000000;
    long int seconds = round(nanoseconds / 1000000000);
    nanoseconds = nanoseconds % 1000000000;
    struct timespec tsp =
    { seconds, nanoseconds };
    struct timespec rem;
start_wait:
    if (nanosleep(&tsp, &rem) != 0)
    {
        tsp = rem;
        goto start_wait;
    }
}


/// Intermediate structure used to communicate information to a spawned thread.
typedef struct
{
    void (*pFunction)(void *);
    unsigned long int Delay_ms;
    void * pFuncParam;
} TExecuteAfterTimeoutStruct;


/// Thread used to support the functionality of function ExecuteAfterTimeout.
void ExecuteAfterTimeout_Thread(TExecuteAfterTimeoutStruct Message)
{
    // Delay execution
    waitMilliseconds(Message.Delay_ms);
    // Call "pFunction(pFuncParam)"
    (Message.pFunction)( Message.pFuncParam );
}


/**
 * This function, "ExecuteAfterTimeout", returns to the caller right away, but it spawns a thread,
 * that waits Timeout_ms milliseconds, and then makes a call to function pFunc, which must have the prototype: void MyFunction(void * myParam);
 * Then the thread disappears.
 */
void ExecuteAfterTimeout(void (*pFunc)(void *), unsigned long int Timeout_ms, void * pFuncParam)
{
    TExecuteAfterTimeoutStruct ExecuteAfterTimeoutStruct;
    ExecuteAfterTimeoutStruct.Delay_ms = Timeout_ms;
    ExecuteAfterTimeoutStruct.pFunction = pFunc;
    ExecuteAfterTimeoutStruct.pFuncParam = pFuncParam;

    std::thread Thread(ExecuteAfterTimeout_Thread, ExecuteAfterTimeoutStruct);
    Thread.detach();
}


//       |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |
char raster[] =
    "         #    # #  # #   ###  #   #   ##     #    ##   ##   # # #   #                         #  ###     #   ###  ####     #  #####   ##  #####  ###   ###                  #       #      ###   ###   ###  ####   #### ####  ##### #####  #### #   #  ####     # #   # #     #   # #   #  ###  ####   ###  ####   #### ##### #   # #   # #   # #   # #   # ##### "\
    "         #    # # ##### # #      #   #       #   #       #   ###    #                        #  #   #   ##  #   #     #   ##  #      #        # #   # #   #    #     #    ##  #####  ##       # # ### #   # #   # #     #   # #     #     #     #   #   #       # #  #  #     ## ## ##  # #   # #   # #   # #   # #       #   #   # #   # #   #  # #   # #     #  "\
    "         #         # #   ###    #     # #        #       #  ##### #####        ###          #   # # #    #     #   ###   # #  ####  ####   ###   ###   ####             ##            ##    ##  # # # ##### ####  #     #   # ###   ###   #  ## #####   #   #   # ###   #     # # # # # # #   # ####  # # # ####   ###    #   #   # #   # # # #   #     #     #   "\
    "                  #####   # #  #     # #         #       #   ###    #                      #    #   #    #    #       # #####     # #   #   #   #   #     #    #     #    ##  #####  ##         # ### #   # #   # #     #   # #     #     #   # #   #   #   #   # #  #  #     #   # #  ## #   # #     #  #  #   #     #   #   #   #  # #  #####  # #    #    #    "\
    "         #         # #   ###  #   #   # #         ##   ##   # # #   #      #          #   #      ###    ### ##### ####     #  ####   ###   #     ###   ###          #       #       #       #    ###  #   # #####  #### ####  ##### #      ###  #   #  ####  ###  #   # ##### #   # #   #  ###  #      ## # #   # ####    #    ###    #    # #  #   #   #   ##### "\
    "                                                                          #                                                                                                                                                                                                                                                                                       ";
//   000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000011111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111112222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222222333333333333333333333333333333333333333333333333333333333333
//   000000000011111111112222222222333333333344444444445555555555666666666677777777778888888888999999999900000000001111111111222222222233333333334444444444555555555566666666667777777777888888888899999999990000000000111111111122222222223333333333444444444455555555556666666666777777777788888888889999999999000000000011111111112222222222333333333344444444445555555555
//   012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789


/// Everything needed to print Big Character banners
class TBanner
{
public:
    TBanner(void) {};
    void Init(int columns, int pos_y, int pos_x, int display_time_ms_param)
    {
        pBoxedWindow = new(TBoxedWindow);
        pBoxedWindow->Init("", char_height +1 +1, columns, pos_y, pos_x);
        // -1 for the right side window, -1 for the left side window, and
        // we're just "flush".
        canvas_width = columns -2;
        canvas = new char[canvas_width*canvas_height];
        char_displayed_on_canvas = canvas_width / char_width;
        display_time_ms = display_time_ms_param;
        std::thread t(BannerThread, this);
        t.detach();
        pBoxedWindow->Hide();
        initialized = true;
    };

    // Is this banner displaying something at this moment?
    bool IsDisplayingSomething(void)
    {
        return !msg_queue.empty();
    }

    void SetMessage(std::string message_param)
    {
        if (initialized == true)
        {
            // Avoid concurrency issues
            std::lock_guard<std::mutex> lock(m);
            // Change whole string to UPPER characters
            msg_queue.push(StringToUpper(message_param));
            //pBoxedWindow->Show();
        }
    }

private:
    bool initialized = false;
    std::mutex m;
    TBoxedWindow * pBoxedWindow;
    static int const char_width = 6;
    static int const char_height = 6;
    int canvas_width;
    static int const canvas_height = 6;
    char * canvas;
    static int const raster_chars = 59;
    static int const raster_width = char_width * raster_chars;
    static int const raster_height = char_height;
    static int const raster_sizeof = raster_height * raster_width;
    int char_displayed_on_canvas;
    std::thread::id BannerThreadID;
    std::queue<std::string> msg_queue;

    int display_time_ms;
    bool shownow = false;

    std::string StringToUpper(std::string strToConvert)
    {
        std::transform(strToConvert.begin(), strToConvert.end(), strToConvert.begin(), ::toupper);

        return strToConvert;
    }

    void BannerProcess(void)
    {
        
    
        while (!msg_queue.empty())
        {
            print_big(msg_queue.front());
            msg_queue.pop();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

    }

    static void BannerThread(TBanner * pBanner)
    {
        while(1)
        {
            pBanner->BannerProcess();
        }
    }

    void print_big(std::string message_param)
    {
        std::string message = message_param;
        // Prepend and append spaces
        message.insert(0, "   ");
        message.insert(message.end(), 3, ' ');
        int message_size = message.size();
        int offset_max = (message_size - char_displayed_on_canvas) * char_width;
        if (offset_max <= 0)
        {
            offset_max = 1;
        }
        int offset = 0;
        pBoxedWindow->PutOnTop();
        while(offset <= offset_max)
        {
            init_pair(7, COLOR_WHITE, COLOR_BLUE);
            wattron(pBoxedWindow->GetRef(), COLOR_PAIR(7));

            mvwprintw(pBoxedWindow->GetRef(), 0, 0, "");
            for (int lin = 0; lin < canvas_height; lin++)
            {
                for (int col = 0+offset; col < canvas_width+offset; col++)
                {
                    int teststring_index = col / char_width;
                    teststring_index %= message_size;
                    int character_to_print_ascii = message[teststring_index];
                    int character_to_print_rasterindex = character_to_print_ascii - 32;
                    character_to_print_rasterindex %= raster_chars;
                    int dot_to_print_index = character_to_print_rasterindex * char_width + col%char_width + lin * raster_width;
                    if (dot_to_print_index < 0) dot_to_print_index = 0;
                    if (dot_to_print_index >= raster_sizeof -1 ) dot_to_print_index = raster_sizeof -1;
                    int dot_to_print = raster[dot_to_print_index];

                    if (dot_to_print == ' ')
                    {
                        wattroff(pBoxedWindow->GetRef(), A_REVERSE);
                    }
                    else
                    {
                        wattron(pBoxedWindow->GetRef(), A_REVERSE);
                    }
                    // Do not put the last character in the bottom right corner, to prevent
                    // scrolling up and losing the first line.
                    if(col != canvas_width+offset-1 || lin != canvas_height-1)
                    {
                        wprintw(pBoxedWindow->GetRef(), " ");
                    }
                }
                // In theory, a Carriage Return could be done here for each end of line, except the last line.
                // But in our case, the usable window width is exactly matching the raster width.
                // So going to the next line is done automatically as part of the "line is full => next character
                // goes to next line" by ncurses.
            }
            pBoxedWindow->Refresh();
            std::this_thread::sleep_for(std::chrono::milliseconds(40));
            offset++;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(display_time_ms));
        pBoxedWindow->Hide();

    }
};


/**
 * This is the main banner object. Call its member function "SetMessage" to
 * temporarily display a message in very large characters on the screen.
 */
 TBanner Banner;


// I try to avoid the use of function prototypes and headers,
// but these are required due to circular references in the program.
void ContextPreviousPress(void);
void ContextPreviousRelease(void);
void ContextNextPress(void);
void ContextNextRelease(void);



// There is an ugly hack, whereby we need some #defines located in two different headers,
// but some of them are redefined differently in each header...
// The key mapping we need is contained in input-event-codes.h. Any other "source" of keymaps
// is not what we look for.
#include <linux/input.h>
#undef _INPUT_EVENT_CODES_H
#include <linux/input-event-codes.h>
namespace ComputerKeyboard
{

bool DoPerformCallbacks = true;

void DisableCallbacks(void)
{
    DoPerformCallbacks = false;
}

void EnableCallbacks(void)
{
    DoPerformCallbacks = true;
}

static const char *const evval[3] =
{
    "RELEASED",
    "PRESSED ",
    "REPEATED"
};

static struct input_event ev;
static ssize_t n;
static int fd;
typedef void (*TKeyboardCallbackFunction)(void *);
typedef struct
{
    TKeyboardCallbackFunction pf;
    void * arg;
} T_pf_arg;

/**
Map a certain set of keypresses to a set of function pointer along with an argument.
That will be called when the key is pressed.
*/
static std::map<int,T_pf_arg> map_pressed;

/**
Same when a key is released.
*/
static std::map<int,T_pf_arg> map_released;

/**
Maps all the key presses to a boolean, can be true (key is pressed)
or false (key is released).
*/
static std::map<int,bool> map_keys;


/**
 * This is the Linux device that gets raw keyboard information.
 * Default is "/dev/input/event0" but it may change depending on Linux distribution
 * This program does not sense key presses through the terminal, it gets them straight
 * from the keyboard.
 */
const char *dev = "/dev/input/event0";

void KeyboardThread(void)
{
    while (1)
    {
        n = read(fd, &ev, sizeof ev);

        if (DoPerformCallbacks == false)
        {
            continue;
            // Do not execute the code below.
        }

        if (n == (ssize_t)-1)
        {
            if (errno == EINTR)
                continue;
            else
            {
                printf("Keyboard code can't be interpreted\n");
                continue;
            }
        }
        else if (n != sizeof ev)
        {
            printf("Keyboard code can't be interpreted\n");
            continue;
            break;
        }

        if (ev.type == EV_KEY && ev.value >= 0 && ev.value <= 2)
        {
            //printf("%s 0x%04x (%d)\n", evval[ev.value], (int)ev.code, (int)ev.code);

            switch (ev.value)
            {
            case 0: // Key released
                map_keys[ev.code] = false;
                if (map_released.find(ev.code) !=  map_released.end())
                {
                    map_released[ev.code].pf(map_released[ev.code].arg);
                }
                break;

            case 1: // Key pressed
                map_keys[ev.code] = true;
                if (map_pressed.find(ev.code) !=  map_pressed.end())
                {
                    map_pressed[ev.code].pf(map_pressed[ev.code].arg);
                }
                break;

            case 2: // Autorepeat

                // do nothing
                break;
            }
        }
    }
}


void Initialize(void)
{
    fd = open(dev, O_RDONLY);
    if (fd == -1)
    {
        fprintf(stdout, "Cannot open %s: %s.\n", dev, strerror(errno));
        fprintf(stdout, "Computer keyboard (e.g. AZERTY...) is not connected?\n");
        return;
    }

    // create separate thread for the keyboard scan
    std::thread t(KeyboardThread);
    setScheduling_RealTime_TopPriority(t.native_handle());
    t.detach();
    // ok now KeyboardThread is running as a separate thread
}

/**
Register a function to be called when a certain key is pressed.
The function is passed as a pointer to function. The function prototype
of the function F should be void F(void *)
*/
void RegisterEventCallbackPressed(int key_value_param, TKeyboardCallbackFunction pfun_param, void * arg_param)
{
    T_pf_arg pf_arg;
    pf_arg.arg = arg_param;
    pf_arg.pf = pfun_param;
    map_pressed[key_value_param] = pf_arg;
}
void RegisterEventCallbackReleased(int key_value_param, TKeyboardCallbackFunction pfun_param, void * arg_param)
{
    T_pf_arg pf_arg;
    pf_arg.arg = arg_param;
    pf_arg.pf = pfun_param;
    map_released[key_value_param] = pf_arg;
}


}

/**
 * A pedal, from the pedalboard (Behringer FCB1010), with analog action
 * (i.e. can take a range of positions between two bounds).
 * Such a pedal must be programmed (from the FCB1010) to send Midi Control Change
 * events on a particular "Controller Number".
 * When this program receives CC events from ControllerNumber, the OnChange callback
 * function is called.
 * These callback functions must be set up properly during the initialization of the
 * TPedalAnalog object.
 * As of today, only CC on Midi channel 1 are recognized.
 */
class TPedalAnalog
{
private:
    void (* OnChange)(int) = NULL;
    std::string Comment = "DEFAULT";

public:
    TPedalAnalog() {}

    TPedalAnalog(void (*Function1_param)(int), const std::string Comment_param)
    {
        OnChange = Function1_param;
        Comment = Comment_param;
    }

    void Change(int param)
    {
        if(OnChange != NULL)
        {
            OnChange(param);
        }
    }

    std::string GetComment(void)
    {
        return Comment;
    }
};



/**
 * A pedal, from the pedalboard (Behringer FCB1010), with digital action
 * (i.e. can take two positions: pressed, or released.)
 * Such a pedal must be programmed (from the FCS1010) to send Midi Note ON events,
 * Each Digital Pedal is associated with a particular Note Number.
 * When this program receives Note ON event for Note Number "Number", the corresponding
 * OnPress and OnRelease callback functions are called. These callback functions must be
 * set up properly during the initialization of the TPedalDigital object.
 *
 * As of today, only Midi events sent on Midi Channel 2 are recognized.
 */
class TPedalDigital
{
private:
    void (* OnPress)(void) = NULL;
    void (* OnRelease)(void) = NULL;
    std::string Comment = "DEFAULT";

public:
    TPedalDigital() {}

    TPedalDigital(void (*Function1_param)(void), void (*Function2_param)(void), std::string Comment_param)
    {
        OnPress = Function1_param;
        OnRelease = Function2_param;
        Comment = Comment_param;
    }

    void Press(void)
    {
        if (OnPress != NULL)
        {
            OnPress();
        }
    }

    void Release(void)
    {
        if (OnRelease != NULL)
        {
            OnRelease();
        }
    }

    std::string GetComment(void)
    {
        return Comment;
    }
};


namespace MIDI_Sequencer
{
    void StartStop_Pressed(void){};
    void StartStop_Released(void){};
    void RegisterPartVolume(int){};
    void RegisterModulation(int){};
}

/**
 * A pedalboard is a collection of digital and analog pedals.
 * In the case of the Behringer FCB1010, there are 10 digital pedals
 * and 2 analog pedals.
 */
class TPedalboard
{
public:
    std::map<int, TPedalDigital> PedalsDigital;
    std::map<int, TPedalAnalog> PedalsAnalog;
    TPedalboard(void)
    {
        // By default: we reserve Digital pedals associated to Midi Note On 6 and 7 (so, if mapped one-to-one
        // with the FCB1010 numbers written on the pedals, the two pedals at the left of the top row), to
        // the context switch actions, i.e. move from one context to the following context, as listed in object
        // PlaylistData. In short, Pedal 6 goes back one song, Pedal 7 goes to the next song.      
        PedalsDigital[6] = TPedalDigital(ContextPreviousPress, ContextPreviousRelease, "Playlist: previous song");
        PedalsDigital[7] = TPedalDigital(ContextNextPress, ContextNextRelease, "Playlist: next song");

        // Let's also reserve more pedals to control the MIDI sequencer:
        // Digital Pedal 1 to start/stop recording (note there are other ways to start/stop recording)
        // Analog Pedal 1 for part volume value
        // Analog Pedal 2 for modulation value
        // => but *all* these settings can be overriden on a per-context basis.
        PedalsDigital[1] = TPedalDigital(MIDI_Sequencer::StartStop_Pressed, MIDI_Sequencer::StartStop_Released, "Seq. Start/Stop");
        PedalsAnalog[1] = TPedalAnalog(MIDI_Sequencer::RegisterPartVolume, "Part volume");
        PedalsAnalog[2] = TPedalAnalog(MIDI_Sequencer::RegisterModulation, "Modulation");
    }
};


void ResetXV5080Performance(void);

/**
 * A Context is a collection of features that belong to a particular performance. In other
 * words, a context gathers everything that is needed to play a song:
 * Song name and Author are used to search through all the available Contexts,
 * Then the base tempo, arrangement of the pedalboards (i.e. which pedal does what), and
 * initialization (with the Init function) that is performed when that particular
 * context is activated.
 *
 * Only one context can be active at a point in time. Two contexts cannot be active at the same time.
 */
class TContext
{
private:
    void (*InitFunc)(void) = NULL;
    void (*ResetMinisynthFunc)(void) = NULL;

public:
    std::string Author; // used to search by Author
    std::string SongName; // used to search by Song Name
    std::string Comments; // used to display various info, cheat sheet, and so on.
    float BaseTempo; // base tempo of the song. The metronome always pulse at the tempo of the active context.
    TPedalboard Pedalboard;
 
    void Init(void)
    {
        // Reset XV5080 sounds
        ResetXV5080Performance();

        if(InitFunc != NULL)
        {
            // Then call the user-defined initialization function
            InitFunc();
        }

        ResetMinisynth();

        // Tell the world which song we're playing now.
        Banner.SetMessage(SongName);
    }

    // Set the function that initializes the whole TContext
    void SetInitFunc( void (*InitFunc_param)(void))
    {
        InitFunc = InitFunc_param;
    }

    bool ResetMinisynth()
    {
        if(ResetMinisynthFunc != NULL)
        {
            // Call the minisynth reset function
            ResetMinisynthFunc();
            return true;
        }
        else
        {
            return false;
        }        
    }

    // Set the function that calls a minisynth reset upon pressing the ESC key
    // Some songs require the minisynth to be set up at specific midi channel and 
    // specific transpose, etc. Other don't use the minisynth and don't need
    // to set anything here.
    void SetResetMinisynthFunc (void (*ResetMinisynthFunc_param)(void))
    {
        ResetMinisynthFunc = ResetMinisynthFunc_param;
    }
};

/**
 * Very important variable:
 * Gather all the data about the playlist, sorted by arbitrary "playlist" order.
 * It is that variables that represents a playlist.
 * If is oksy to define several playlists, but then assign one to PlaylistData.
 * Reads: PlaylistData is a list of TContext pointers. Please note that PlaylistData
 * keeps track of pointer to contexts, not actual contexts. This is useful to generate
 * lists with different orders, yet keeping the pool of context objects the same, and only
 * REFERENCED by these lists (not copied into them).
 */
std::list<TContext *> PlaylistData;

/// Take the raw information of PlaylistData, and sort alphabetically by Author
std::list<TContext *> PlaylistData_ByAuthor;

/// Take the raw information of PlaylistData, and sort alphabetically by Song Name
std::list<TContext *> PlaylistData_BySongName;

/**
 * Very important global variable: points to the current context in the playlist.
 * E.g: TContext MyContext;
 *      MyContext = *PlaylistPosition;
 *      printf("%s", MyContext.SongName.c_str());
 */
TContext * PlaylistPosition;
std::mutex PlaylistPosition_mtx;

 


/**
 * Here, the actual TContext objects are instantiated. One for each context, which
 * more or less represents one for each *song* in the playlist, in the band/musical sense.
 */
TContext cFirstContext;
TContext cRigUp;
TContext cSynth;


/**
 * This function is needed to sort lists of elements.
 * Sorting a list requires that a comparison function be provided.
 * But how can we say "that context comes before that one"? It depends on
 * how we want to sort, namely by Author
 * \see CompareTContextBySongName
 */
bool CompareTContextByAuthor(const TContext* first, const TContext* second)
{
    std::string first_nocase = first->Author;
    std::string second_nocase = second->Author;
    std::transform(first_nocase.begin(), first_nocase.end(), first_nocase.begin(), ::tolower);
    std::transform(second_nocase.begin(), second_nocase.end(), second_nocase.begin(), ::tolower);

    return first_nocase < second_nocase;
}

/**
 * This function is needed to sort lists of elements.
 * Sorting a list requires that a comparison function be provided.
 * But how can we say "that context comes before that one"? It depends on
 * how we want to sort, namely by Song Name
 * \see CompareTContextByAuthor
 */
bool CompareTContextBySongName(const TContext* first, const TContext* second)
{
    std::string first_nocase = first->SongName;
    std::string second_nocase = second->SongName;
    std::transform(first_nocase.begin(), first_nocase.end(), first_nocase.begin(), ::tolower);
    std::transform(second_nocase.begin(), second_nocase.end(), second_nocase.begin(), ::tolower);

    return first_nocase < second_nocase;
}

/**
 * This function is called each time one wants to go to the previous context.
 * Since a FCB1010 pedal is allocated to that (normally pedal 6), two events are generated:
 * That one when you <b>press on</b> the pedal
 */
void ContextPreviousPress(void)
{
    // Protect PlaylistPosition from concurrent access
    std::lock_guard<std::mutex> lock(PlaylistPosition_mtx);
    if (* PlaylistData.begin() == PlaylistPosition )
    {
        // Already at the beginning of the list
        // Do nothing        
    }
    else
    {
        auto it = std::find(PlaylistData.begin(), PlaylistData.end(), PlaylistPosition);
        std::advance(it, -1);
        PlaylistPosition = * it;
        PlaylistPosition->Init();
    }
}


/**
 * This function is called each time one wants to go to the previous context.
 * Since a FCB1010 pedal is allocated to that (normally pedal 6), two events are generated:
 * That one when you <b>release</b> the pedal
 */
void ContextPreviousRelease(void)
{
    // And in that case, do nothing.
}

/// Sames goes with Pedal 7, go to go the next context (you'll say the *next song*...).
void ContextNextPress(void)
{
    // Protect PlaylistPosition from concurrent access
    std::lock_guard<std::mutex> lock(PlaylistPosition_mtx);
    auto it = PlaylistData.end();
    std::advance(it, -1);
    if (*it == PlaylistPosition )
    {
        // Why -1?
        // PlaylistData.end() is already out of bounds
        // PlaylistData.end() '-1' is the last element
        // => Already at the end of the list
        // Do nothing        
    }
    else
    {
        auto it = std::find(PlaylistData.begin(), PlaylistData.end(), PlaylistPosition);
        std::advance(it, 1);
        PlaylistPosition = * it;
        PlaylistPosition->Init();
    }
}

/// What happens when the ContextNext pedal is released
void ContextNextRelease(void)
{
    // In that case, do nothing.
}


// TODO: handle events passed to this program (i.e. CTRL+C, kill, etc.)
void sighandler(int dum)
{
    stop = 1;
}


typedef enum
{
    ceOn, ceOff
} TChordEvent;


typedef enum
{
    nnDo = 1,
    nnC = 1,
    nnDoDiese = 2,
    nnCSharp = 2,
    nnReBemol = 2,
    nnDFlat = 2,
    nnRe = 3,
    nnD = 3,
    nnReDiese = 4,
    nnDSharp = 4,
    nnMiBemol = 4
} TNoteName;

/**
 * Helper structure used to gather some information required to merely play a note.
 * Including how long it should last.
 */
typedef struct
{
    unsigned char NoteNumber;
    unsigned char Velocity;
    unsigned char Channel;
    int DurationMS;
} TPlayNoteMsg;


// Provision for name_midi_hw_MIDISPORT_A, name_midi_hw_MIDISPORT_B, name_midi_hw_MIDISPORT_C, name_midi_hw_MIDISPORT_D
void ProvisionNextMIDI_Port(const std::string StringValue)
{
    if (name_midi_hw_MIDISPORT_A.empty())
    {
        name_midi_hw_MIDISPORT_A = StringValue;
        wprintw(win_debug_messages.GetRef(), "name_midi_hw_MIDISPORT_A=%s\n", StringValue.c_str());
        return;
    }

    if (name_midi_hw_MIDISPORT_B.empty())
    {
        name_midi_hw_MIDISPORT_B = StringValue;
        wprintw(win_debug_messages.GetRef(), "name_midi_hw_MIDISPORT_B=%s\n", StringValue.c_str());
        return;
    }

    if (name_midi_hw_MIDISPORT_C.empty())
    {
        name_midi_hw_MIDISPORT_C = StringValue;
        wprintw(win_debug_messages.GetRef(), "name_midi_hw_MIDISPORT_C=%s\n", StringValue.c_str());
        return;
    }

    if (name_midi_hw_MIDISPORT_D.empty())
    {
        name_midi_hw_MIDISPORT_D = StringValue;
        wprintw(win_debug_messages.GetRef(), "name_midi_hw_MIDISPORT_D=%s\n", StringValue.c_str());
        return;
    }
}

/// Straight from amidi.c
/// E.g. https://github.com/alsa-project/alsa-utils/blob/master/amidi/amidi.c
static void list_device(snd_ctl_t *ctl, int card, int device)
{
	snd_rawmidi_info_t *info;
	const char *name;
	const char *sub_name;
	int subs, subs_in, subs_out;
	int sub;
	int err;

	snd_rawmidi_info_alloca(&info);
	snd_rawmidi_info_set_device(info, device);

    // Count RAW MIDI INputs
	snd_rawmidi_info_set_stream(info, SND_RAWMIDI_STREAM_INPUT);
	err = snd_ctl_rawmidi_info(ctl, info);
	if (err >= 0)
		subs_in = snd_rawmidi_info_get_subdevices_count(info);
	else
		subs_in = 0;

    // Count RAW MIDI OUTputs
	snd_rawmidi_info_set_stream(info, SND_RAWMIDI_STREAM_OUTPUT);
	err = snd_ctl_rawmidi_info(ctl, info);
	if (err >= 0)
		subs_out = snd_rawmidi_info_get_subdevices_count(info);
	else
		subs_out = 0;

	subs = std::max(subs_in, subs_out);
	if (!subs)
    {
        // Neither MIDI INs nor MIDI OUTs - nothing to list within that specific combination of card/device
		return;
    }

	for (sub = 0; sub < subs; ++sub) {
		snd_rawmidi_info_set_stream(info, sub < subs_in ?
					    SND_RAWMIDI_STREAM_INPUT :
					    SND_RAWMIDI_STREAM_OUTPUT);
		snd_rawmidi_info_set_subdevice(info, sub);
		err = snd_ctl_rawmidi_info(ctl, info);
		if (err < 0) {
            wprintw(win_debug_messages.GetRef(), "cannot get rawmidi information %d:%d:%d: %s\n", card, device, sub, snd_strerror(err));
			return;
		}
		name = snd_rawmidi_info_get_name(info);
		sub_name = snd_rawmidi_info_get_subdevice_name(info);
		if (sub == 0 && sub_name[0] == '\0')
        {
            // Card, Device, Sub-devices, one generic Name which applies to all sub-devices
			wprintw(win_debug_messages.GetRef(), "MIDI devices: %c%c  hw:%d,%d    %s",
			       sub < subs_in ? 'I' : ' ',
			       sub < subs_out ? 'O' : ' ',
			       card, device, name);
            ProvisionNextMIDI_Port("hw:" + std::to_string(card) + "," + std::to_string(device));
			if (subs > 1)
            {
                // Let user know the number of implied sub-devices
				wprintw(win_debug_messages.GetRef(), " (%d subdevices)", subs);
            }
			wprintw(win_debug_messages.GetRef(), "\n");
			break;
		}
        else
        {
            // Card, Device, Sub-device, and Names that specifically designates each Sub-device
			wprintw(win_debug_messages.GetRef(), "%c%c  hw:%d,%d,%d  %s\n",
			       sub < subs_in ? 'I' : ' ',
			       sub < subs_out ? 'O' : ' ',
			       card, device, sub, sub_name);
            ProvisionNextMIDI_Port("hw:" + std::to_string(card) + "," + std::to_string(device) + "," + std::to_string(sub));
		}
	}
}


static void list_card_devices(int card)
{
	snd_ctl_t *ctl;
	std::string name;
	int device;
	int err;

    name = "hw:" + std::to_string(card);
	if ((err = snd_ctl_open(&ctl, name.c_str(), 0)) < 0)
    {
		wprintw(win_debug_messages.GetRef(), "cannot open control for card %d: %s", card, snd_strerror(err));
		return;
	}
	device = -1;
	while (1)
    {
		if ((err = snd_ctl_rawmidi_next_device(ctl, &device)) < 0) {
			wprintw(win_debug_messages.GetRef(), "cannot determine device number: %s", snd_strerror(err));
			break;
		}
		if (device < 0)
        {
			break;
        }
		list_device(ctl, card, device);
	}
	snd_ctl_close(ctl);
}

static void device_list(void)
{
	int card, err;

	card = -1;
    wprintw(win_debug_messages.GetRef(), "Listing MIDI devices...\n");
	if ((err = snd_card_next(&card)) < 0)
    {
		wprintw(win_debug_messages.GetRef(), "Cannot determine card number: %s", snd_strerror(err));
		return;
	}
	if (card < 0)
    {
		wprintw(win_debug_messages.GetRef(), "No sound card found");
		return;
	}
	wprintw(win_debug_messages.GetRef(), "Dir Device    Name\n");
	do
    {
		list_card_devices(card);
		if ((err = snd_card_next(&card)) < 0) {
			wprintw(win_debug_messages.GetRef(), "cannot determine card number: %s", snd_strerror(err));
			break;
		}
	} while (card >= 0);
}




/**
 * This class runs a state machine capable of processing MIDI input.
 * It instantiates a thread on its own (that runs the state machine).
 * A defined MIDI IN port must be assigned to it upon initialization.
 * Callback functions must be set up to process:
 * - Note events (from outside into this program)
 * - Controller Change events (from outside into this program)
 * It also implements the functions to send to MIDI OUT (from this program to outside world):
 * - Note events
 * - CC (Controller Change)
 * - PC (Program Change)
 */
class TMIDI_Port
{

public:
    /// Constructor does nothing. Please call ::Init() prior to using member functions.
    TMIDI_Port(void) {};

    /// Hook function called whenever a Note ON event is received
    typedef void (*THookProcessNoteONEvent)(TInt_1_16 rxChannel, TInt_0_127 rxNote_param, TInt_0_127 rxVolume_param);
    /// Hook function called whenever a Note OFF event is received
    typedef void (*THookProcessNoteOFFEvent)(TInt_1_16 rxChannel, TInt_0_127 rxNote_param, TInt_0_127 rxVolume_param);
    /// Hook function called whenever a controller change event is received
    typedef void (*THookProcessControllerChangeEvent)(TInt_1_16 rxChannel, TInt_0_127 rxControllerNumber_param, TInt_0_127 rxControllerValue_param);
    /// Hook function called whenever a pitch bend event is received
    typedef void (*THookProcessPitchBendChangeEvent)(TInt_1_16 rxChannel, TInt_14bits rxPitchBendChangeValue_param);

    /// Initialize this object
    void Init(std::string name_midi_hw_param, THookProcessNoteONEvent HookProcessNoteONEvent_param, THookProcessNoteOFFEvent HookProcessNoteOFFEvent_param, THookProcessControllerChangeEvent HookProcessControllerChangeEvent_param, THookProcessPitchBendChangeEvent HookProcessPitchBendChangeEvent_param)
    {
        name_midi_hw = name_midi_hw_param;
        HookProcessNoteONEvent = HookProcessNoteONEvent_param;
        HookProcessNoteOFFEvent = HookProcessNoteOFFEvent_param;
        HookProcessControllerChangeEvent = HookProcessControllerChangeEvent_param;
        HookProcessPitchBendChangeEvent = HookProcessPitchBendChangeEvent_param;
        // Spawn new thread that takes care of processing MIDI packets
        if (ThreadNativeHandle == 0)
        {
            // Pass "this" object as an argument to the thread StateMachineThread.
            std::thread tmp_thread(StateMachineThread, this);
            // Please note that access to the so-called "native handle" is possible only
            // **BEFORE** we detach the thread. There are other ways to identify a Linux thread,
            // but the native handle is the only one that will work for us here. This is why it
            // is "captured" in the member variable ThreadNativeHandle, for further use later on.
            ThreadNativeHandle = tmp_thread.native_handle();
            setScheduling_RealTime_TopPriority(ThreadNativeHandle);
            tmp_thread.detach();
            // tmp_thread object is thrown away when going out of scope, but StateMachineThread() will carry on,
            // spinning as a separate thread.
        }
        else
        {
            // That thread needs to be spawned only once. If we come here, it probably
            // had been initialized by a previous call to Init(). Check your initialization code.
            wprintw(win_debug_messages.GetRef(), "TProcessMidiInput::Init() called too often\n");
        }
    }

    // Send Note On Event on midi channel Channel (1-16), note number NoteNumber (0-127),
    // velocity Velocity (0-127).
    void SendNoteOnEvent(unsigned int Channel, unsigned int NoteNumber,
                         unsigned int Velocity)
    {
        std::lock_guard<std::recursive_mutex> lock(MIDI_Port_Mutex);
        // This function sends a Note ON event
        // Example: SendNoteOnEvent(2, 60, 100);
        unsigned char NoteOnField = 9; // See MIDI specifications
        unsigned char charArray[3];

        if (Channel < 1)
        {
            Channel = 1;
        }
        if (Channel > 16)
        {
            Channel = 16;
        }

        charArray[0] = (NoteOnField << 4) + ((Channel - 1) & 0x0F);
        charArray[1] = NoteNumber & 0x7F;
        charArray[2] = Velocity;
        wprintw(win_debug_messages.GetRef(), "Note ON num=%i,vel=%i\n", (int) charArray[1], (int) charArray[2]);

        wprintw(win_midi_out.GetRef(), "%i\n%i\n%i\n", (int) charArray[0], (int) charArray[1], (int) charArray[2]);
        if (handle_midi_hw_out != 0)
        {
            snd_rawmidi_write_protected(handle_midi_hw_out, &charArray, sizeof(charArray));
        }
    }


    void SendNoteOnEvent(TPlayNoteMsg * PlayNoteMsg)
    {
        std::lock_guard<std::recursive_mutex> lock(MIDI_Port_Mutex);
        SendNoteOnEvent(PlayNoteMsg->Channel, PlayNoteMsg->NoteNumber, PlayNoteMsg->Velocity);
    }


    /*
     Remark from the MIDI specifications:
     ------------------------------------
    MIDI provides two roughly equivalent means of turning off a note (voice).
    A note may be turned off either by sending a Note-Off message for the same note
    number and channel, or by sending a Note-On message for that note and channel
    with a velocity value of zero. The advantage to using "Note-On at zero velocity"
    is that it can avoid sending additional status bytes when Running Status is
    employed.

    Due to this efficiency, sending Note-On messages with velocity values of zero is
    the most commonly used method.

    */

// Same for Note Off MIDI event.
// If you want to send out some MIDI notes,
// you're better off using PlayNote(), rather than drilling down to the Note ON / OFF events...
    void SendNoteOffEvent(unsigned int Channel, unsigned int NoteNumber, unsigned int Velocity)
    {
        std::lock_guard<std::recursive_mutex> lock(MIDI_Port_Mutex);
        // This function sends a Note OFF event
        // Example: SendNoteOffEvent(2, 60, 100);
        unsigned char NoteOffField = 8; // See MIDI specifications
        unsigned char charArray[3];

        if (Channel < 1)
        {
            Channel = 1;
        }
        if (Channel > 16)
        {
            Channel = 16;
        }

        charArray[0] = (NoteOffField << 4) + ((Channel - 1) & 0x0F);
        charArray[1] = NoteNumber & 0x7F;
        charArray[2] = Velocity;
        wprintw(win_debug_messages.GetRef(), "Note OFF number: %i\n", (int) charArray[1]);

        wprintw(win_midi_out.GetRef(), "%i\n%i\n%i\n", (int) charArray[0], (int) charArray[1], (int) charArray[2]);
        if (handle_midi_hw_out != 0)
        {
            snd_rawmidi_write_protected(handle_midi_hw_out, &charArray, sizeof(charArray));
        }
    }

    void SendNoteOffEvent(TPlayNoteMsg * PlayNoteMsg)
    {
        std::lock_guard<std::recursive_mutex> lock(MIDI_Port_Mutex);
        SendNoteOffEvent(PlayNoteMsg->Channel, PlayNoteMsg->NoteNumber, PlayNoteMsg->Velocity);
    }

    void SendProgramChange(unsigned char Channel, unsigned char Program)
    {
        std::lock_guard<std::recursive_mutex> lock(MIDI_Port_Mutex);
        unsigned char MidiFunctionID = 0xC; // See MIDI specifications
        unsigned char charArray[2];
        if (Channel < 1)
        {
            Channel = 1;
        }
        if (Channel > 16)
        {
            Channel = 16;
        }
        charArray[0] = (MidiFunctionID << 4) + ((Channel - 1) & 0x0F);
        charArray[1] = (Program - 1) & 0x7F;
        wprintw(win_debug_messages.GetRef(), "Program Change: Channel %i; Program %i\n", (int) Channel, (int) Program);
        wprintw(win_midi_out.GetRef(), "%02x\n%02x\n", (int) charArray[0], (int) charArray[1]);
        if(handle_midi_hw_out != 0)
        {
            snd_rawmidi_write_protected(handle_midi_hw_out, &charArray, sizeof(charArray));
        }
    }

    // Send out a CC (Control Change) MIDI event.
    void SendControlChange(unsigned char Channel, unsigned char ControlNumber,
                           unsigned char ControllerValue)
    {
        std::lock_guard<std::recursive_mutex> lock(MIDI_Port_Mutex);
        unsigned char MidiFunctionID = 0xB;
        unsigned char charArray[3];

        if (Channel < 1)
        {
            Channel = 1;

        }
        if (Channel > 16)
        {
            Channel = 16;
        }

        charArray[0] = (MidiFunctionID << 4) + ((Channel - 1) & 0x0F);
        charArray[1] = (ControlNumber) & 0x7F;
        charArray[2] = (ControllerValue);
        wprintw(win_debug_messages.GetRef(), "Control Change - Controller Number: %i; Controller Value: %i\n", (int) charArray[1], (int) charArray[2]);
        if (handle_midi_hw_out != 0)
        {
            wprintw(win_midi_out.GetRef(), "%02x\n%02x\n%02x\n", (int) charArray[0], (int) charArray[1], (int) charArray[2]);
            snd_rawmidi_write_protected(handle_midi_hw_out, &charArray, sizeof(charArray));
        }
    }


    // Send out a PB (Pitch Bend Change) MIDI event.
    void SendPitchBendChange(unsigned char Channel, TInt_14bits PitchBendChangeValue)
    {
        std::lock_guard<std::recursive_mutex> lock(MIDI_Port_Mutex);
        unsigned char MidiFunctionID = 0xE;
        unsigned char charArray[3];

        if (Channel < 1)
        {
            Channel = 1;
        }
        if (Channel > 16)
        {
            Channel = 16;
        }

        charArray[0] = (MidiFunctionID << 4) + ((Channel - 1) & 0x0F);
        charArray[1] = (PitchBendChangeValue) & 0x7F;
        charArray[2] = (char)(((int)PitchBendChangeValue)>>7);
        wprintw(win_debug_messages.GetRef(), "Pitch Bend Change - PB Value: %i\n", (int) PitchBendChangeValue);
        if (handle_midi_hw_out != 0)
        {
            wprintw(win_midi_out.GetRef(), "%02x\n%02x\n%02x\n", (int) charArray[0], (int) charArray[1], (int) charArray[2]);
            snd_rawmidi_write_protected(handle_midi_hw_out, &charArray, sizeof(charArray));
        }
    }

    // Send out raw midi data
    void SendRawData(std::vector<unsigned char> data)
    {
        std::lock_guard<std::recursive_mutex> lock(MIDI_Port_Mutex);
        for (auto x : data)
        {
            wprintw(win_midi_out.GetRef(), "%02x\n", (int) x);
        }

        if (handle_midi_hw_out != 0)
        {
            snd_rawmidi_write_protected(handle_midi_hw_out, &data[0], data.size());
        }
    }

    // Talk to ALSA and open a midi port in RAW mode, for data going OUT.
    // (from this program, out of the computer, to the expander, etc.)
    void StartRawMidiOut(void)
    {
        std::lock_guard<std::recursive_mutex> lock(MIDI_Port_Mutex);
        if (handle_midi_hw_out == 0)
        {
            int err = snd_rawmidi_open(NULL, &handle_midi_hw_out, name_midi_hw.c_str(), SND_RAWMIDI_SYNC);
            if (err)
            {
                wprintw(win_debug_messages.GetRef(), "snd_rawmidi_open %s failed: %d\n", name_midi_hw.c_str(), err);
            }
        }
    }

    // Close the RAW midi OUT port, for port "portnum" (0 or 1)
    void StopRawMidiOut(void)
    {
        std::lock_guard<std::recursive_mutex> lock(MIDI_Port_Mutex);
        if(handle_midi_hw_out != 0)
        {
            snd_rawmidi_drain(handle_midi_hw_out);
            snd_rawmidi_close(handle_midi_hw_out);
            handle_midi_hw_out = 0;
        }
    }

private:
    std::recursive_mutex MIDI_Port_Mutex;
    std::string name_midi_hw;
    std::thread::native_handle_type ThreadNativeHandle = 0;
    int err;
    unsigned char ch;
    TInt_1_16 rxChannel;
    TInt_0_127 rxVolume;
    TInt_0_127 rxControllerNumber;
    TInt_0_127 rxNote;
    TInt_0_127 rxControllerValue;
    TInt_14bits rxPitchBendValue;

    enum
    {
        smInit,
        smWaitMidiChar1,
        smWaitMidiControllerChangeChar2,
        smWaitMidiControllerChangeChar3,
        smProcessControllerChange,
        smWaitMidiNoteChar2,
        smWaitMidiNoteOffChar2,
        smWaitMidiNoteChar3,
        smWaitMidiNoteOffChar3,
        smProcessNoteEvent,
        smProcessNoteOffEvent,
        smWaitPitchBendChar2,
        smWaitPitchBendChar3,
        smProcessPitchBendChangeEvent
    } stateMachine = smInit;

    // Keep track of MIDI IN ALSA Handle
    // Initialize with zero = invalid pointers.
    snd_rawmidi_t * handle_midi_hw_in = 0;
    // Same for the midisport midi output
    snd_rawmidi_t * handle_midi_hw_out = 0;

    // Hook function called whenever a note ON event is received
    THookProcessNoteONEvent HookProcessNoteONEvent = 0;
    // Hook function called whenever a note OFF event is received
    THookProcessNoteOFFEvent HookProcessNoteOFFEvent = 0;
    // Hook function called whenever a controller change event is received
    THookProcessControllerChangeEvent HookProcessControllerChangeEvent = 0;
    // Hook function called whenever a pitch bend change event is received
    THookProcessPitchBendChangeEvent HookProcessPitchBendChangeEvent = 0;

    // Same as snd_rawmidi_write, protected by a mutex
    // Within this program, only use the protected version.
    void snd_rawmidi_write_protected(snd_rawmidi_t * rmidi, const void *buffer, size_t size)
    {
        std::lock_guard<std::recursive_mutex> lock(MIDI_Port_Mutex);
        snd_rawmidi_write(rmidi, buffer, size);
//        snd_rawmidi_drain(rmidi); Not needed if port is opened, for writing, with flag SND_RAWMIDI_SYNC 
    }        

    // Talk to ALSA and open a midi port in RAW mode, for data going IN.
    // (into the computer, into this program, from the pedalboard, from the keyboard...)
    void StartRawMidiIn(void)
    {
        if (handle_midi_hw_in == 0)
        {
            int err = snd_rawmidi_open(&handle_midi_hw_in, NULL, name_midi_hw.c_str(), 0);
            if (err)
            {
                wprintw(win_debug_messages.GetRef(), "snd_rawmidi_open %s failed: %d\n", name_midi_hw.c_str(), err);
            }
        }
    }

    // Close the RAW midi IN port, for port "portnum" (0 or 1)
    void StopRawMidiIn(void)
    {
        if (handle_midi_hw_in != 0)
        {
            snd_rawmidi_drain(handle_midi_hw_in);
            snd_rawmidi_close(handle_midi_hw_in);
            handle_midi_hw_in = 0;
        }
    }


    // This thread runs the main MIDI IN state machine
    static void StateMachineThread(TMIDI_Port * pSelf)
    {
        pSelf->StateMachineFunction();
    }

    void StateMachineFunction(void)
    {
        // Change priority to top-most realtime
#ifdef REALTIME
        setScheduling_RealTime_TopPriority(pthread_self());
#endif

        wprintw(win_debug_messages.GetRef(), "MIDI A: device %s\n", name_midi_hw.c_str());

        StartRawMidiIn();
        StartRawMidiOut();
//    signal(SIGINT,sighandler);

        if (handle_midi_hw_in)
        {
            while (1)
            {
                switch (stateMachine)
                {
                case smInit:
                    stateMachine = smWaitMidiChar1;
                    break;

                case smWaitMidiChar1:
                    snd_rawmidi_read(handle_midi_hw_in, &ch, 1);
                    wprintw(win_midi_in.GetRef(), "%02x\n", ch);
                    if ( ( (ch) & 0xF0 ) == 0x90 && HookProcessNoteONEvent)
                    {
                        // It's a note ON event. Get the least significant nibble, that's the channel
                        rxChannel = (ch & 0x0F) +1;
                        stateMachine = smWaitMidiNoteChar2;
                    }
                    if ( ( (ch) & 0xF0 ) == 0x80 && HookProcessNoteOFFEvent)
                    {
                        // It's a note OFF event. Get the least significant nibble, that's the channel
                        rxChannel = (ch & 0x0F) +1;
                        stateMachine = smWaitMidiNoteOffChar2;
                    }
                    if ( ( (ch) & 0xF0 ) == 0xb0 && HookProcessControllerChangeEvent)
                    {
                        // It's a Controller Change event.
                        // The channel is encoded offset by one. Bring back the value in the 0-16 range (+1 below)
                        rxChannel = (ch & 0x0F) +1;
                        stateMachine = smWaitMidiControllerChangeChar2;
                    }
                    if ( ( (ch) & 0xF0 ) == 0xe0 && HookProcessPitchBendChangeEvent)
                    {
                        // It's a Pitch Bend Change event.
                        rxChannel = (ch & 0x0F) +1;
                        stateMachine = smWaitPitchBendChar2;
                    }
                    break;


                case smWaitPitchBendChar2:
                    snd_rawmidi_read(handle_midi_hw_in, &ch, 1);
                    wprintw(win_midi_in.GetRef(), "%02x\n", ch);
                    if (ch >= 0 && ch <= 127)
                    {
                        stateMachine = smWaitPitchBendChar3;
                        rxPitchBendValue = ch;
                    }
                    else
                    {
                        // Value out of bounds
                        stateMachine = smWaitMidiChar1;
                    }
                    break;

                case smWaitPitchBendChar3:
                    snd_rawmidi_read(handle_midi_hw_in, &ch, 1);
                    wprintw(win_midi_in.GetRef(), "%02x\n", ch);
                    if (ch >= 0 && ch <= 127)
                    {
                        stateMachine = smProcessPitchBendChangeEvent;
                        rxPitchBendValue = rxPitchBendValue + ((int)ch * 128);
                    }
                    else
                    {
                        // Value out of bounds
                        stateMachine = smWaitMidiChar1;
                    }
                    break;

                case smProcessPitchBendChangeEvent:
                    HookProcessPitchBendChangeEvent(rxChannel, rxPitchBendValue);
                    stateMachine = smWaitMidiChar1;
                    break;

                case smWaitMidiNoteOffChar2:
                    snd_rawmidi_read(handle_midi_hw_in, &ch, 1);
                    wprintw(win_midi_in.GetRef(), "%02x\n", ch);
                    if (ch >= 0 && ch <= 127)
                    {
                        stateMachine = smWaitMidiNoteOffChar3;
                        rxNote = ch;
                    }
                    else
                    {
                        // Value out of bounds
                        stateMachine = smWaitMidiChar1;
                    }
                    break;

                    case smWaitMidiNoteChar2:
                    snd_rawmidi_read(handle_midi_hw_in, &ch, 1);
                    wprintw(win_midi_in.GetRef(), "%02x\n", ch);
                    if (ch >= 0 && ch <= 127)
                    {
                        stateMachine = smWaitMidiNoteChar3;
                        rxNote = ch;
                    }
                    else
                    {
                        // Value out of bounds
                        stateMachine = smWaitMidiChar1;
                    }
                    break;

                case smWaitMidiNoteOffChar3:
                    snd_rawmidi_read(handle_midi_hw_in, &ch, 1);
                    wprintw(win_midi_in.GetRef(), "%02x\n", ch);
                    if (ch >= 0 && ch <= 127)
                    {
                        rxVolume = ch;
                        stateMachine = smProcessNoteOffEvent;
                    }
                    else
                    {
                        // Value out of bounds
                        stateMachine = smWaitMidiChar1;
                    }
                    break;


                case smWaitMidiNoteChar3:
                    snd_rawmidi_read(handle_midi_hw_in, &ch, 1);
                    wprintw(win_midi_in.GetRef(), "%02x\n", ch);
                    if (ch >= 0 && ch <= 127)
                    {
                        rxVolume = ch;
                        stateMachine = smProcessNoteEvent;
                    }
                    else
                    {
                        // Value out of bounds
                        stateMachine = smWaitMidiChar1;
                    }
                    break;

                case smWaitMidiControllerChangeChar2:
                    snd_rawmidi_read(handle_midi_hw_in, &ch, 1);
                    wprintw(win_midi_in.GetRef(), "%02x\n", ch);
                    if (ch >= 0 && ch <= 127)
                    {
                        stateMachine = smWaitMidiControllerChangeChar3;
                        rxControllerNumber = ch;
                    }
                    else
                    {
                        // Value out of bounds
                        stateMachine = smWaitMidiChar1;
                    }

                    break;

                case smWaitMidiControllerChangeChar3:
                    snd_rawmidi_read(handle_midi_hw_in, &ch, 1);
                    wprintw(win_midi_in.GetRef(), "%02x\n", ch);
                    stateMachine = smProcessControllerChange;
                    rxControllerValue = ch;
                    wprintw(win_debug_messages.GetRef(), "Controller Change: Control Number %i; Control Value %i\n", rxControllerNumber, rxControllerValue);
                    break;

                case smProcessNoteEvent:
                {
                    if (HookProcessNoteONEvent != 0)
                    {
                        HookProcessNoteONEvent(rxChannel, rxNote, rxVolume);
                    }
                }
                stateMachine = smWaitMidiChar1;
                break;

                case smProcessNoteOffEvent:
                {
                    if (HookProcessNoteOFFEvent != 0)
                    {
                        HookProcessNoteOFFEvent(rxChannel, rxNote, rxVolume);
                    }
                }
                stateMachine = smWaitMidiChar1;
                break;

                case smProcessControllerChange:
                {
                    if (HookProcessControllerChangeEvent != 0)
                    {
                        HookProcessControllerChangeEvent(rxChannel, rxControllerNumber, rxControllerValue);
                    }
                }
                stateMachine = smWaitMidiChar1;

                break;

                default:
                    stateMachine = smInit;

                }
            }
        }

        wprintw(win_debug_messages.GetRef(), "Closing\n");

        StopRawMidiIn();
        StopRawMidiOut();
    }
};


// MIDI port A
TMIDI_Port MIDI_A;

// MIDI port B
TMIDI_Port MIDI_B;

// MIDI port C
TMIDI_Port MIDI_C;


/**
XV-5080 driver

Handles communication with the XV-5080

*/
class TXV5080
{
public:

    // Constructor
    // Tell this object which MIDI port it should use to transmit MIDI information
    TXV5080(TMIDI_Port * pMIDI_Port_param)
    {
        pMIDI_Port = pMIDI_Port_param;
        pTXV5080 = this;
    }

    enum class PatchGroup {USER, PR_A, PR_B, PR_C, PR_D, PR_E, PR_F, PR_G, PR_H,
                        CD_A, CD_B, CD_C, CD_D, CD_E, CD_F, CD_G, CD_H};
    // Note we should also add to this enum the expansion boards, but selecting them is a bit
    // complicated because it depends on the type of expansion card used, plus I made the choice
    // to no use the expansion boards directly because it would be unlikely that I have the same
    // boards installed on two XV-5080 (one plus backup), so if needed I would copy the patches
    // of an expansion board to the memory card or to user preset, but not directly access patches
    // located on an extension board.
    // But in a nutshell, we should also have in this enum:  XP_A, XP_B, XP_C, XP_D, XP_E, XP_F, XP_G, XP_H,
    // and handle them properly in the corresponding switch/case. See Owner's Manual page 21.

    enum class RhythmSetGroup {USER, PR_A, PR_B, PR_C, PR_D, PR_E, PR_F, PR_G,
                        CD_A, CD_B, CD_C, CD_D, CD_E, CD_F, CD_G, CD_H};
    // Same as for patches, I don't include the Rhythm Sets of the expansion boards. See OM page 22.

    enum class PerformanceGroup {USER, PR_A, PR_B,
                        CD_A, CD_B, CD_C, CD_D, CD_E, CD_F, CD_G, CD_H};


    /**
     * Switch to performance mode and jump to a specific performance
     */
    void PerformanceSelect(TXV5080::PerformanceGroup PerformanceGroup_param, TInt_1_128 const PatchNumber_param)
    {
        System.SystemCommon.SoundMode.Perform();
        switch (PerformanceGroup_param)
        {
        case PerformanceGroup::USER:
            // From user's manual page 21
            System.SystemCommon.PerformanceBankSelectMSB.Set(85);
            System.SystemCommon.PerformanceBankSelectLSB.Set(0);
            System.SystemCommon.PerformanceProgramNumber.Set(PatchNumber_param);
            break;

        case PerformanceGroup::PR_A:
            System.SystemCommon.PerformanceBankSelectMSB.Set(85);
            System.SystemCommon.PerformanceBankSelectLSB.Set(64);
            System.SystemCommon.PerformanceProgramNumber.Set(PatchNumber_param);
            break;

        case PerformanceGroup::PR_B:
            System.SystemCommon.PerformanceBankSelectMSB.Set(85);
            System.SystemCommon.PerformanceBankSelectLSB.Set(65);
            System.SystemCommon.PerformanceProgramNumber.Set(PatchNumber_param);
            break;

        case PerformanceGroup::CD_A:
            System.SystemCommon.PerformanceBankSelectMSB.Set(85);
            System.SystemCommon.PerformanceBankSelectLSB.Set(32);
            System.SystemCommon.PerformanceProgramNumber.Set(PatchNumber_param);
            break;

        case PerformanceGroup::CD_B:
            System.SystemCommon.PerformanceBankSelectMSB.Set(85);
            System.SystemCommon.PerformanceBankSelectLSB.Set(33);
            System.SystemCommon.PerformanceProgramNumber.Set(PatchNumber_param);
            break;

        case PerformanceGroup::CD_C:
            System.SystemCommon.PerformanceBankSelectMSB.Set(85);
            System.SystemCommon.PerformanceBankSelectLSB.Set(34);
            System.SystemCommon.PerformanceProgramNumber.Set(PatchNumber_param);
            break;

        case PerformanceGroup::CD_D:
            System.SystemCommon.PerformanceBankSelectMSB.Set(85);
            System.SystemCommon.PerformanceBankSelectLSB.Set(35);
            System.SystemCommon.PerformanceProgramNumber.Set(PatchNumber_param);
            break;

        case PerformanceGroup::CD_E:
            System.SystemCommon.PerformanceBankSelectMSB.Set(85);
            System.SystemCommon.PerformanceBankSelectLSB.Set(36);
            System.SystemCommon.PerformanceProgramNumber.Set(PatchNumber_param);
            break;

        case PerformanceGroup::CD_F:
            System.SystemCommon.PerformanceBankSelectMSB.Set(85);
            System.SystemCommon.PerformanceBankSelectLSB.Set(37);
            System.SystemCommon.PerformanceProgramNumber.Set(PatchNumber_param);
            break;

        case PerformanceGroup::CD_G:
            System.SystemCommon.PerformanceBankSelectMSB.Set(85);
            System.SystemCommon.PerformanceBankSelectLSB.Set(38);
            System.SystemCommon.PerformanceProgramNumber.Set(PatchNumber_param);
            break;

        case PerformanceGroup::CD_H:
            System.SystemCommon.PerformanceBankSelectMSB.Set(85);
            System.SystemCommon.PerformanceBankSelectLSB.Set(39);
            System.SystemCommon.PerformanceProgramNumber.Set(PatchNumber_param);
            break;

        default:

            break;

       }

    }



    class TParameter
    {
    public:
        TParameter(void) {};
        TParameter(unsigned long int OffsetAddress_param, int MinValue_param, int MaxValue_param, std::string str)
        {
            OffsetAddress = OffsetAddress_param;

            for (int i = str.size(); i--;)
            {
                if (str[i] == '0')
                {
                    Mask.push_back(false);
                }
                else if (str[i] == ' ')
                {
                    continue;
                }
                else
                {
                    Mask.push_back(true);
                }
            }
            printf("Mask size: %i\n", (int) Mask.size());
            if (Mask.size() % 8 != 0)
            {
                printf("ERROR: TValue Mask not a multiple of 8 bits\n");
                exit(0);
            }
            // e.g.:
            // str = "00aa aaaa"
            //  v
            //  v
            // Mask[0] = true
            // Mask[1] = true
            // Mask[2] = true
            // Mask[3] = true
            // Mask[4] = true
            // Mask[5] = true
            // Mask[6] = false
            // Mask[7] = false
        }

    protected:

        int MinValue;
        int MaxValue;

        unsigned long int OffsetAddress;
        std::vector<bool> Mask;
        std::vector<unsigned char> GetDataBytes(int Value)
        {
            std::vector<unsigned char> Bytes;
            unsigned long int tmpByte = 0;
            int i = 0;
            for (auto x : Mask)
            {
                if(x)
                {
                    // Copy this bit
                    unsigned long int bit = Value & 1;
                    // LSB consumed => shift Value to the right to get next bit
                    // (for next cycle)
                    Value = Value>>1;
                    // Construct final byte
                    tmpByte = tmpByte | (bit<< (i%8));
                    i++;
                }
                else
                {
                    i++;
                }
                if (i%8 == 0)
                {
                    // Full byte constructed: commit final value in vector
                    Bytes.push_back(tmpByte);
                    // Clear temporary buffer
                    tmpByte = 0;
                }
            }
            // Bytes hold the correct bytes to be sent, but in reverse order.
            // So return the "reverted" version of it:
            std::reverse(Bytes.begin(),Bytes.end());
            // Now Bytes order is correct. Return result.
            return Bytes;
        }
    };

    class TParameterSection
    {
    public:
        TParameterSection(int OffsetAddress_param)
        {
            OffsetAddress = OffsetAddress_param;
        }
    protected:
        int OffsetAddress;
    };

    class TSystem : TParameterSection
    {
    public:
        TSystem(void) : TParameterSection(0x00000000) {};
        class TSystemCommon : TParameterSection
        {
        public:
            TSystemCommon(int val) : TParameterSection(val) {};
            class TSoundMode : TParameter
            {
            public:
                TSoundMode(int val) : TParameter(val + 0x00000000, 0, 4, "0000 0aaa") {};
                void Perform(void)
                {
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(0));
                }

                /**
                 * Merely switch to the patch mode
                 */
                void Patch(void)
                {
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(1));
                }

                void GM1(void)
                {
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(2));
                }
                void GM2(void)
                {
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(3));
                }
                void GS(void)
                {
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(4));
                }
            } SoundMode{OffsetAddress};

            class TMasterTune : TParameter
            {
            public:
                TMasterTune(int val) : TParameter(val + 0x00000001, 24, 2024, "0000 aaaa 0000 bbbb 0000 cccc 0000 dddd") {};
                /** -100.0 to +100.0 cent */
                void Set(float Value_param)
                {
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(Value_param * 100 + 24 + 100));
                }
            } MasterTune{OffsetAddress};


            class TMasterKeyShift : TParameter
            {
            public:
                TMasterKeyShift(int val) : TParameter(val + 0x0005, 40, 88, "00aa aaaa") {};
                /** -24 to +24 */
                void Set(int Value_param)
                {
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(Value_param -40 -24));
                }
            } MasterKeyShift{OffsetAddress};

            class TMasterLevel : TParameter
            {
            public:
                TMasterLevel(int val) : TParameter(val + 0x0006, 0, 127, "0aaa aaaa") {};
                /** 0 to 127 */
                void Set(int Value_param)
                {
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(Value_param));
                }
            } MasterLevel{OffsetAddress};

            class TScaleTuneSwitch : TParameter
            {
            public:
                TScaleTuneSwitch(int val) : TParameter(val + 0x0007, 0, 1, "0000 000a") {};
                /** 0 = OFF, 1 = ON */
                void Set(int Value_param)
                {
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(Value_param));
                }
            } ScaleTuneSwitch{OffsetAddress};

            class TPatchRemain : TParameter
            {
            public:
                TPatchRemain(int val) : TParameter(val + 0x0008, 0, 1, "0000 000a") {};
                /** 0 = OFF, 1 = ON */
                void Set(int Value_param)
                {
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(Value_param));
                }
            } PatchRemain{OffsetAddress};

            class TMixParallel : TParameter
            {
            public:
                TMixParallel(int val) : TParameter(val + 0x0009, 0, 1, "0000 000a") {};
                /** 0 = Mix, 1 = Parallel */
                void Set(int Value_param)
                {
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(Value_param));
                }
            } MixParallel{OffsetAddress};

            class TMFXSwitch : TParameter
            {
            public:
                TMFXSwitch(int val) : TParameter(val + 0x000A, 0, 1, "0000 000a") {};
                /** 0 = BYPASS, 1 = ON */
                void Set(int Value_param)
                {
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(Value_param));
                }
            } MFXSwitch{OffsetAddress};

            class TChorusSwitch : TParameter
            {
            public:
                TChorusSwitch(int val) : TParameter(val + 0x000B, 0, 1, "0000 000a") {};
                /** 0 = OFF, 1 = ON */
                void Set(int Value_param)
                {
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(Value_param));
                }
            } ChorusSwitch{OffsetAddress};

            class TReverbSwitch : TParameter
            {
            public:
                TReverbSwitch(int val) : TParameter(val + 0x000C, 0, 1, "0000 000a") {};
                /** 0 = OFF, 1 = ON */
                void Set(int Value_param)
                {
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(Value_param));
                }
            } ReverbSwitch{OffsetAddress};

            class TPerformanceControlChannel : TParameter
            {
            public:
                TPerformanceControlChannel(int val) : TParameter(val + 0x000D, 0, 16, "000a aaaa") {};
                /** 1-16=Channels 1-16, 0=OFF */
                void Set(int Value_param)
                {
                    if (Value_param >= 1 && Value_param <=16)
                    {
                        pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(Value_param -1));
                    }
                    else
                    {
                        pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(16)); // Corresponds to OFF
                    }
                }
            } PerformanceControlChannel{OffsetAddress};

            class TPerformanceBankSelectMSB : TParameter
            {
            public:
                TPerformanceBankSelectMSB(int val) : TParameter(val + 0x000E, 0, 127, "0aaa aaaa") {};
                /** 0-127 */
                void Set(int Value_param)
                {
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(Value_param));
                }
            } PerformanceBankSelectMSB{OffsetAddress};

            class TPerformanceBankSelectLSB : TParameter
            {
            public:
                TPerformanceBankSelectLSB(int val) : TParameter(val + 0x000F, 0, 127, "0aaa aaaa") {};
                /** 0-127 */
                void Set(int Value_param)
                {
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(Value_param));
                }
            } PerformanceBankSelectLSB{OffsetAddress};

            class TPerformanceProgramNumber : TParameter
            {
            public:
                TPerformanceProgramNumber(int val) : TParameter(val + 0x0010, 0, 127, "0aaa aaaa") {};
                /** 1-128 */
                void Set(int Value_param)
                {
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(Value_param -1));
                }
            } PerformanceProgramNumber{OffsetAddress};

            class TSystemTempo : TParameter
            {
            public:
                TSystemTempo(int val) : TParameter(val + 0x0016, 20, 250, "0000 aaaa 0000 bbbb") {};
                /** 20-250 */
                void Set(int Value_param)
                {
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(Value_param));
                }
            } SystemTempo{OffsetAddress};
         
            class TSystemControl1Source : TParameter
            {
                public:
                TSystemControl1Source(int val) : TParameter(val + 0x0018, 0, 97, "0aaa aaaa") {};
                /** 0-97 == OFF, CC01-CC31, CC33-CC95, BEND, AFT */
                void Set(int Value_param_0_97)
                {
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(Value_param_0_97));
                }
            } SystemControl1Source{OffsetAddress};

            class TSystemControl2Source : TParameter
            {
                public:
                TSystemControl2Source(int val) : TParameter(val + 0x0019, 0, 97, "0aaa aaaa") {};
                /** 0-97 == OFF, CC01-CC31, CC33-CC95, BEND, AFT */
                void Set(int Value_param_0_97)
                {
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(Value_param_0_97));
                }
            } SystemControl2Source{OffsetAddress};

            class TSystemControl3Source : TParameter
            {
                public:
                TSystemControl3Source(int val) : TParameter(val + 0x001A, 0, 97, "0aaa aaaa") {};
                /** 0-97 == OFF, CC01-CC31, CC33-CC95, BEND, AFT */
                void Set(int Value_param_0_97)
                {
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(Value_param_0_97));
                }
            } SystemControl3Source{OffsetAddress};

            class TSystemControl4Source : TParameter
            {
                public:
                TSystemControl4Source(int val) : TParameter(val + 0x001B, 0, 97, "0aaa aaaa") {};
                /** 0-97 == OFF, CC01-CC31, CC33-CC95, BEND, AFT */
                void Set(int Value_param_0_97)
                {
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(Value_param_0_97));
                }
            } SystemControl4Source{OffsetAddress};


         } SystemCommon{OffsetAddress + 0x000000};
    } System;


    class TPerformance : TParameterSection
    {
    public:
        TPerformance(unsigned long int Address_param) : TParameterSection(Address_param) {};

        class TPerformanceCommon : TParameterSection
        {
        public:
            TPerformanceCommon(int val) : TParameterSection(val + 0x0000) {};
            class TPerformanceName : TParameter
            {
            public:
                TPerformanceName(int val) : TParameter(val + 0x0000, 32, 127, "0aaa aaaa") {};
                void Set(std::string Name)
                {
                    for (unsigned int i = 0; i<12 ; i++)
                    {
                        if (i < Name.length() )
                        {
                            pTXV5080->ExclusiveMsgSetParameter(OffsetAddress +i, GetDataBytes(Name[i]));
                        }
                        else
                        {
                            break;
                        }
                    }
                }
            } PerformanceName{OffsetAddress};

            class TSoloPartSelect : TParameter
            {
            public:
                TSoloPartSelect(int val) : TParameter(val + 0x000C, 0, 32, "00aa aaaa") {};
                /** 0 = OFF, 1-32 */
                void Set(int Value_param)
                {
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(Value_param));
                }
            } SoloPartSelect{OffsetAddress};



        } PerformanceCommon{OffsetAddress};

        class TPerformanceCommonReverb_ : TParameterSection
        {
            public:
            TPerformanceCommonReverb_(int val) : TParameterSection(val + 0x0600) {};


            class TReverbType_ : TParameter
            {
                public:
                TReverbType_(int val) : TParameter(val + 0x0000, 0, 4, "0000 aaaa") {};
                /** Value between 0 and 4 */
                void Set(int Value_param)
                {
                    if (Value_param >= 0 && Value_param <=4)
                    {
                       pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(Value_param));
                    }
                }
            } ReverbType_{OffsetAddress};

            class TReverbLevel_ : TParameter
            {
                public:
                TReverbLevel_(int val) : TParameter(val + 0x0001, 0, 127, "0aaa aaaa") {};
                /** Value between 0 and 127 */
                void Set(int Value_param)
                {
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(Value_param));
                }
            } ReverbLevel_{OffsetAddress};

            class TReverbOutputAssign_ : TParameter
            {
                public:
                TReverbOutputAssign_(int val) : TParameter(val + 0x0002, 0, 3, "0000 00aa") {};
                /** Value between 0 and 3. 0=A, 1=B, 2=C, 3=D */
                void Set(int Value_param)
                {
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(Value_param));
                }
            } ReverbOutputAssign_{OffsetAddress};

            class TReverbParameter_ : TParameter
            {
                public:
                TReverbParameter_(int val) : TParameter(val, 12768, 52768, "0000 aaaa 0000 bbbb 0000 cccc 0000 dddd") {};
                /** Value between -20000 and +20000 */
                void Set(int Value_param)
                {
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(Value_param + 32768));
                }
            };


            std::array<TReverbParameter_, 20> ReverbParameter_ = {OffsetAddress + 0x0003, OffsetAddress + 0x0007, OffsetAddress + 0x000B, OffsetAddress + 0x000F, OffsetAddress + 0x0013, OffsetAddress + 0x0017, OffsetAddress + 0x001B, OffsetAddress + 0x001F,
                                                                OffsetAddress + 0x0023, OffsetAddress + 0x0027, OffsetAddress + 0x002B, OffsetAddress + 0x002F, OffsetAddress + 0x0033, OffsetAddress + 0x0037, OffsetAddress + 0x003B, OffsetAddress + 0x003F,
                                                                OffsetAddress + 0x0043, OffsetAddress + 0x0047, OffsetAddress + 0x004B, OffsetAddress + 0x004F};



        } PerformanceCommonReverb_{OffsetAddress};



        class TPerformanceMIDI : TParameterSection
        {
        public:
            TPerformanceMIDI(int val) : TParameterSection(val) {};
// receive program change, receive bank selsect, receive bender, receive polyphonic key pressure, receive channel pressure, receive modulation, receive volume, receive pan, receive expression, receive hold-1, phoase lock velocity curve type
            class TReceiveProgramChange : TParameter
            {
                public:
                TReceiveProgramChange(int val) : TParameter(val + 0x0000, 0, 1, "0000 000a") {};
                /** 0 = OFF, 1 = ON */
                void Set(int Value_param)
                {
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(Value_param));
                }
            } ReceiveProgramChange {OffsetAddress};

            class TReceiveBankSelect : TParameter
            {
                public:
                TReceiveBankSelect(int val) : TParameter(val + 0x0001, 0, 1, "0000 000a") {};
                /** 0 = OFF, 1 = ON */
                void Set(int Value_param)
                {
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(Value_param));
                }
            } ReceiveBankSelect{OffsetAddress};

            class TReceiveBender : TParameter
            {
                public:
                TReceiveBender(int val) : TParameter(val + 0x0002, 0, 1, "0000 000a") {};
                /** 0 = OFF, 1 = ON */
                void Set(int Value_param)
                {
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(Value_param));
                }
            } ReceiveBender {OffsetAddress};

            class TReceivePolyphonicKeyPressure : TParameter
            {
                public:
                TReceivePolyphonicKeyPressure(int val) : TParameter(val + 0x0003, 0, 1, "0000 000a") {};
                /** 0 = OFF, 1 = ON */
                void Set(int Value_param)
                {
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(Value_param));
                }
            } ReceivePolyphonicKeyPressure{OffsetAddress};

            class TReceiveChannelPressure : TParameter
            {
            public:
                TReceiveChannelPressure(int val) : TParameter(val + 0x0004, 0, 1, "0000 000a") {};
                /** 0 = OFF, 1 = ON */
                void Set(int Value_param)
                {
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(Value_param));
                }
            } ReceiveChannelPressure {OffsetAddress};

            class TReceiveModulation : TParameter
            {
            public:
                TReceiveModulation(int val) : TParameter(val + 0x0005, 0, 1, "0000 000a") {};
                /** 0 = OFF, 1 = ON */
                void Set(int Value_param)
                {
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(Value_param));
                }
            } ReceiveModulation{OffsetAddress};

            class TReceiveVolume : TParameter
            {
            public:
                TReceiveVolume(int val) : TParameter(val + 0x0006, 0, 1, "0000 000a") {};
                /** 0 = OFF, 1 = ON */
                void Set(int Value_param)
                {
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(Value_param));
                }
            } ReceiveVolume{OffsetAddress};

            class TReceivePan : TParameter
            {

            public:
                TReceivePan(int val) : TParameter(val + 0x0007, 0, 1, "0000 000a") {};
                /** 0 = OFF, 1 = ON */
                void Set(int Value_param)
                {
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(Value_param));
                }
            } ReceivePan{OffsetAddress};

            class TReceiveExpression : TParameter
            {
            public:
                TReceiveExpression(int val) : TParameter(val + 0x0008, 0, 1, "0000 000a") {};
                /** 0 = OFF, 1 = ON */
                void Set(int Value_param)
                {
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(Value_param));
                }
            } ReceiveExpression{OffsetAddress};

            class TReceiveHold_1 : TParameter
            {
            public:
                TReceiveHold_1(int val) : TParameter(val + 0x0009, 0, 1, "0000 000a") {};
                /** 0 = OFF, 1 = ON */
                void Set(int Value_param)
                {
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(Value_param));
                }
            } ReceiveHold_1{OffsetAddress};

            class TPhaseLock : TParameter
            {
            public:
                TPhaseLock(int val) : TParameter(val + 0x000A, 0, 1, "0000 000a") {};
                /** 0 = OFF, 1 = ON */
                void Set(int Value_param)
                {
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(Value_param));
                }
            } PhaseLock{OffsetAddress};

            class TVelocityCurveType : TParameter
            {
            public:
                TVelocityCurveType(int val) : TParameter(val + 0x000B, 0, 4, "0000 0aaa") {};
                /** 0 = OFF, 1 = type 1, 2 = type 2, 3 = type 3, 4 = type 4 */
                void Set(int Value_param)
                {
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(Value_param));
                }
            } VelocityCurveType{OffsetAddress};

        };

        std::array<TPerformanceMIDI, 16> PerformanceMidi = {OffsetAddress + 0x1000, OffsetAddress + 0x1100, OffsetAddress + 0x1200, OffsetAddress + 0x1300, OffsetAddress + 0x1400, OffsetAddress + 0x1500, OffsetAddress + 0x1600, OffsetAddress + 0x1700,
                                                            OffsetAddress + 0x1800, OffsetAddress + 0x1900, OffsetAddress + 0x1A00, OffsetAddress + 0x1B00, OffsetAddress + 0x1C00, OffsetAddress + 0x1D00, OffsetAddress + 0x1E00, OffsetAddress + 0x1F00};


        class TPerformancePart : TParameterSection
        {
        public:
            TPerformancePart(int val) : TParameterSection(val) {};

            class TReceiveChannel : TParameter
            {
            public:
                TReceiveChannel(int val) : TParameter(val + 0x0000, 0, 15, "0000 aaaa") {};
                /** Set channel number. Value between 1 and 16. */
                void Set_1_16(int Value_param)
                {
                    // Value_param is between 1 and 16 but the binary value is 0-15 hence -1
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(Value_param -1));
                }
            } ReceiveChannel{OffsetAddress};

            class TReceiveSwitch : TParameter
            {
            public:
                TReceiveSwitch(int val) : TParameter(val + 0x0001, 0, 1, "0000 000a") {};
                /** 0=OFF or 1=ON */
                void Set(int Value_param)
                {
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(Value_param));
                }
            } ReceiveSwitch{OffsetAddress};

            class TReceiveMIDI1 : TParameter
            {
            public:
                TReceiveMIDI1(int val) : TParameter(val + 0x0002, 0, 1, "0000 000a") {};
                /** 0=OFF or 1=ON */
                void Set(int Value_param)
                {
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(Value_param));
                }
            } ReceiveMIDI1{OffsetAddress};

            class TReceiveMIDI2 : TParameter
            {
            public:
                TReceiveMIDI2(int val) : TParameter(val + 0x0003, 0, 1, "0000 000a") {};
                /** 0=OFF or 1=ON */
                void Set(int Value_param)
                {
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(Value_param));
                }
            } ReceiveMIDI2{OffsetAddress};


            class TPartLevel : TParameter
            {
            public:
                TPartLevel(int val) : TParameter(val + 0x0007, 0, 127, "0aaa aaaa") {};
                /** Set Part Level (0-127) */
                void Set(int Value_param)
                {
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(Value_param));
                }
            } PartLevel{OffsetAddress};

            class TPartPan : TParameter
            {
            public:
                TPartPan(int val) : TParameter(val + 0x0008, 0, 127, "0aaa aaaa") {};
                /** Set Part Pan (0=L64, 64=CENTERED, 127=R63) */
                void Set(int Value_param)
                {
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(Value_param));
                }
            } PartPan{OffsetAddress};

            class TPartCoarseTune : TParameter
            {
            public:
                TPartCoarseTune(int val) : TParameter(val + 0x0009, 16, 112, "0aaa aaaa") {};
                /** Set Coarse Tune for this Part (-48 .. +48) */
                void Set(int Value_param)
                {
                    // 16 == -48
                    // 112 == +48
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(Value_param + 64));
                }
            } PartCoarseTune{OffsetAddress};

            class TPartFineTune : TParameter
            {
            public:
                TPartFineTune(int val) : TParameter(val + 0x000A, 14, 114, "0aaa aaaa") {};
                /** Set Fine Tune for this Part (-50 .. +50) */
                void Set(int Value_param)
                {
                    // 14 == -50
                    // 114 == +50
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(Value_param + 64));
                }
            } PartFineTune{OffsetAddress};

            class TPartMonoPoly : TParameter
            {
            public:
                TPartMonoPoly(int val) : TParameter(val + 0x000B, 0, 2, "0000 00aa") {};
                /** 0=MONO, 1=POLY, 2=PATCH */
                void Set(int Value_param)
                {
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(Value_param));
                }
            } PartMonoPoly{OffsetAddress};

            class TPartOctaveShift : TParameter
            {
            public:
                TPartOctaveShift(int val) : TParameter(val + 0x0015, 61, 67, "0aaa aaaa") {};
                /** -3 .. +3 */
                void Set(int Value_param)
                {
                    // 61 = -3
                    // 67 = +3
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(Value_param +64));
                }
            } PartOctaveShift{OffsetAddress};

            class TPartVelocitySensOffset : TParameter
            {
            public:
                TPartVelocitySensOffset(int val) : TParameter(val + 0x0016, 1, 127, "0aaa aaaa") {};
                /** -63 .. +63 */
                void Set(int Value_param)
                {
                    // 1 = -63
                    // 127 = +63
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(Value_param +64));
                }
            } PartVelocitySensOffset{OffsetAddress};

            class TKeyboardRangeLower : TParameter
            {
            public:
                TKeyboardRangeLower(int val) : TParameter(val + 0x0017, 0, 127, "0aaa aaaa") {};
                /** C-1 - UPPER */
                void Set(int Value_param)
                {
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(Value_param));
                }
            } KeyboardRangeLower{OffsetAddress};

            class TKeyboardRangeUpper : TParameter
            {
            public:
                TKeyboardRangeUpper(int val) : TParameter(val + 0x0018, 0, 127, "0aaa aaaa") {};
                /** LOWER - G9 */
                void Set(int Value_param)
                {
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(Value_param));
                }
            } KeyboardRangeUpper{OffsetAddress};


            class TKeyboardFadeWithLower : TParameter
            {
            public:
                TKeyboardFadeWithLower(int val) : TParameter(val + 0x0019, 0, 127, "0aaa aaaa") {};
                /** 0-127 */
                void Set(int Value_param)
                {
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(Value_param));
                }
            } KeyboardFadeWithLower{OffsetAddress};

            class TKeyboardFadeWithUpper : TParameter
            {
            public:
                TKeyboardFadeWithUpper(int val) : TParameter(val + 0x0020, 0, 127, "0aaa aaaa") {};
                /** 0-127 */
                void Set(int Value_param)
                {
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(Value_param));
                }
            } KeyboardFadeWithUpper{OffsetAddress};


            void SelectPatch(TXV5080::PatchGroup PatchGroup_param, int PatchNumber)
            {
                SelectPatch(PatchGroup_param, TInt_1_128(PatchNumber));
            }

            void SelectPatch(TXV5080::PatchGroup PatchGroup_param, TInt_1_128 const PatchNumber_param)
            {
                switch(PatchGroup_param)
                {
                    case PatchGroup::USER:
                    PatchBankSelectMSB.Set(87);
                    PatchBankSelectLSB.Set(0);
                    PatchProgramNumber.Set(PatchNumber_param);
                    break;

                    case PatchGroup::PR_A:
                    PatchBankSelectMSB.Set(87);
                    PatchBankSelectLSB.Set(64);
                    PatchProgramNumber.Set(PatchNumber_param);
                    break;

                    case PatchGroup::PR_B:
                    PatchBankSelectMSB.Set(87);
                    PatchBankSelectLSB.Set(65);
                    PatchProgramNumber.Set(PatchNumber_param);
                    break;

                    case PatchGroup::PR_C:
                    PatchBankSelectMSB.Set(87);
                    PatchBankSelectLSB.Set(66);
                    PatchProgramNumber.Set(PatchNumber_param);
                    break;

                    case PatchGroup::PR_D:
                    PatchBankSelectMSB.Set(87);
                    PatchBankSelectLSB.Set(67);
                    PatchProgramNumber.Set(PatchNumber_param);
                    break;

                    case PatchGroup::PR_E:
                    PatchBankSelectMSB.Set(87);
                    PatchBankSelectLSB.Set(68);
                    PatchProgramNumber.Set(PatchNumber_param);
                    break;

                    case PatchGroup::PR_F:
                    PatchBankSelectMSB.Set(87);
                    PatchBankSelectLSB.Set(69);
                    PatchProgramNumber.Set(PatchNumber_param);
                    break;

                    case PatchGroup::PR_G:
                    PatchBankSelectMSB.Set(87);
                    PatchBankSelectLSB.Set(70);
                    PatchProgramNumber.Set(PatchNumber_param);
                    break;

                    case PatchGroup::PR_H:
                    PatchBankSelectMSB.Set(88);
                    if (PatchNumber_param <= 128)
                    {
                        PatchBankSelectLSB.Set(71);
                        PatchProgramNumber.Set(PatchNumber_param);
                    }
                    else
                    {
                        PatchBankSelectLSB.Set(72);
                        PatchProgramNumber.Set(PatchNumber_param-128);
                    }
                    break;

                    case PatchGroup::CD_A:
                    PatchBankSelectMSB.Set(87);
                    PatchBankSelectLSB.Set(32);
                    PatchProgramNumber.Set(PatchNumber_param);
                    break;

                    case PatchGroup::CD_B:
                    PatchBankSelectMSB.Set(87);
                    PatchBankSelectLSB.Set(33);
                    PatchProgramNumber.Set(PatchNumber_param);
                    break;

                    case PatchGroup::CD_C:
                    PatchBankSelectMSB.Set(87);
                    PatchBankSelectLSB.Set(34);
                    PatchProgramNumber.Set(PatchNumber_param);
                    break;

                    case PatchGroup::CD_D:
                    PatchBankSelectMSB.Set(87);
                    PatchBankSelectLSB.Set(35);
                    PatchProgramNumber.Set(PatchNumber_param);
                    break;

                    case PatchGroup::CD_E:
                    PatchBankSelectMSB.Set(87);
                    PatchBankSelectLSB.Set(36);
                    PatchProgramNumber.Set(PatchNumber_param);
                    break;

                    case PatchGroup::CD_F:
                    PatchBankSelectMSB.Set(87);
                    PatchBankSelectLSB.Set(37);
                    PatchProgramNumber.Set(PatchNumber_param);
                    break;

                    case PatchGroup::CD_G:
                    PatchBankSelectMSB.Set(87);
                    PatchBankSelectLSB.Set(38);
                    PatchProgramNumber.Set(PatchNumber_param);
                    break;

                    case PatchGroup::CD_H:
                    PatchBankSelectMSB.Set(87);
                    PatchBankSelectLSB.Set(39);
                    PatchProgramNumber.Set(PatchNumber_param);
                    break;
                }
            }

            void SelectRhythmSet(TXV5080::RhythmSetGroup RhythmSetGroup_param, int PatchNumber)
            {
                SelectRhythmSet(RhythmSetGroup_param, TInt_1_4(PatchNumber));
            }

            void SelectRhythmSet(TXV5080::RhythmSetGroup RhythmSetGroup_param, TInt_1_4 const RhythmSetNumber_param)
            {
                switch(RhythmSetGroup_param)
                {
                    // See XV-5080 Owner's Manual, page 22
                    case RhythmSetGroup::USER:
                    PatchBankSelectMSB.Set(86);
                    PatchBankSelectLSB.Set(0);
                    PatchProgramNumber.Set(RhythmSetNumber_param);
                    break;


                    case RhythmSetGroup::PR_A:
                    PatchBankSelectMSB.Set(86);
                    PatchBankSelectLSB.Set(64);
                    PatchProgramNumber.Set(RhythmSetNumber_param);
                    break;

                    case RhythmSetGroup::PR_B:
                    PatchBankSelectMSB.Set(86);
                    PatchBankSelectLSB.Set(65);
                    PatchProgramNumber.Set(RhythmSetNumber_param);
                    break;

                    case RhythmSetGroup::PR_C:
                    PatchBankSelectMSB.Set(86);
                    PatchBankSelectLSB.Set(66);
                    PatchProgramNumber.Set(RhythmSetNumber_param);
                    break;

                    case RhythmSetGroup::PR_D:
                    PatchBankSelectMSB.Set(86);
                    PatchBankSelectLSB.Set(67);
                    PatchProgramNumber.Set(RhythmSetNumber_param);
                    break;

                    case RhythmSetGroup::PR_E:
                    PatchBankSelectMSB.Set(86);
                    PatchBankSelectLSB.Set(68);
                    PatchProgramNumber.Set(RhythmSetNumber_param);
                    break;

                    case RhythmSetGroup::PR_F:
                    PatchBankSelectMSB.Set(86);
                    PatchBankSelectLSB.Set(69);
                    PatchProgramNumber.Set(RhythmSetNumber_param);
                    break;

                    case RhythmSetGroup::PR_G:
                    PatchBankSelectMSB.Set(86);
                    PatchBankSelectLSB.Set(70);
                    PatchProgramNumber.Set(RhythmSetNumber_param);
                    break;

                    case RhythmSetGroup::CD_A:
                    PatchBankSelectMSB.Set(86);
                    PatchBankSelectLSB.Set(32);
                    PatchProgramNumber.Set(RhythmSetNumber_param);
                    break;

                    case RhythmSetGroup::CD_B:
                    PatchBankSelectMSB.Set(86);
                    PatchBankSelectLSB.Set(33);
                    PatchProgramNumber.Set(RhythmSetNumber_param);
                    break;

                    case RhythmSetGroup::CD_C:
                    PatchBankSelectMSB.Set(86);
                    PatchBankSelectLSB.Set(34);
                    PatchProgramNumber.Set(RhythmSetNumber_param);
                    break;

                    case RhythmSetGroup::CD_D:
                    PatchBankSelectMSB.Set(86);
                    PatchBankSelectLSB.Set(35);
                    PatchProgramNumber.Set(RhythmSetNumber_param);
                    break;

                    case RhythmSetGroup::CD_E:
                    PatchBankSelectMSB.Set(86);
                    PatchBankSelectLSB.Set(36);
                    PatchProgramNumber.Set(RhythmSetNumber_param);
                    break;

                    case RhythmSetGroup::CD_F:
                    PatchBankSelectMSB.Set(86);
                    PatchBankSelectLSB.Set(37);
                    PatchProgramNumber.Set(RhythmSetNumber_param);
                    break;

                    case RhythmSetGroup::CD_G:
                    PatchBankSelectMSB.Set(86);
                    PatchBankSelectLSB.Set(38);
                    PatchProgramNumber.Set(RhythmSetNumber_param);
                    break;

                    case RhythmSetGroup::CD_H:
                    PatchBankSelectMSB.Set(86);
                    PatchBankSelectLSB.Set(39);
                    PatchProgramNumber.Set(RhythmSetNumber_param);
                    break;
                }
            }


            class TPatchBankSelectMSB : TParameter
            {
            public:
                TPatchBankSelectMSB(int val) : TParameter(val + 0x0004, 0, 127, "0aaa aaaa") {};
                /** 0-127 */
                void Set(int Value_param)
                {
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(Value_param));
                }
            } PatchBankSelectMSB{OffsetAddress};

            class TPatchBankSelectLSB : TParameter
            {
            public:
                TPatchBankSelectLSB(int val) : TParameter(val + 0x0005, 0, 127, "0aaa aaaa") {};
                /** 0-127 */
                void Set(int Value_param)
                {
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(Value_param));
                }
            } PatchBankSelectLSB{OffsetAddress};

            class TPatchProgramNumber : TParameter
            {
            public:
                TPatchProgramNumber(int val) : TParameter(val + 0x0006, 0, 127, "0aaa aaaa") {};
                /** 1-128 */
                void Set(int Value_param)
                {
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(Value_param -1));
                }
            } PatchProgramNumber{OffsetAddress};

            class TPartReverbSendLevel : TParameter
            {
                public:
                TPartReverbSendLevel(int val) : TParameter(val + 0x001E, 0, 127, "0aaa aaaa") {};
                /** 0-127 */
                void Set(int Value_param)
                {
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(Value_param));
                }
            } PartReverbSendLevel{OffsetAddress};

            class TPartOutputAssign : TParameter
            {
                public:
                TPartOutputAssign(int val) : TParameter(val + 0x001F, 0, 13, "0000 aaaa") {};
                /** 0=MFX, 1=(A), 2=(B), .. 3=(D), 4=(1), 5=(2), 11=(8), 12, C, D, 4=(1), (2), (3)

                 */
                void ToMFX(void)
                {
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(0));
                }

                void ToOutputA(void)
                {
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(1));
                }

                void ToOutputB(void)
                {
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(2));
                }

                void ToOutputC(void)
                {
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(3));
                }

                void ToOutputD(void)
                {
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(4));
                }

                void ToOutput1(void)
                {
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(5));
                }

                void ToOutput2(void)
                {
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(6));
                }

                void ToOutput3(void)
                {
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(7));
                }

                void ToOutput4(void)
                {
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(8));
                }

                void ToOutput5(void)
                {
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(9));
                }

                void ToOutput6(void)
                {
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(10));
                }

                void ToOutput7(void)
                {
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(11));
                }

                void ToOutput8(void)
                {
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(12));
                }

                void ToPatch(void)
                {
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(13));
                }
            } PartOutputAssign {OffsetAddress};
        };
        std::array<TPerformancePart, 32> PerformancePart = {OffsetAddress + 0x2000, OffsetAddress + 0x2100, OffsetAddress + 0x2200, OffsetAddress + 0x2300, OffsetAddress + 0x2400,
                                                            OffsetAddress + 0x2500, OffsetAddress + 0x2600, OffsetAddress + 0x2700, OffsetAddress + 0x2800, OffsetAddress + 0x2900,
                                                            OffsetAddress + 0x2A00, OffsetAddress + 0x2B00, OffsetAddress + 0x2C00, OffsetAddress + 0x2D00, OffsetAddress + 0x2E00,
                                                            OffsetAddress + 0x2F00, OffsetAddress + 0x3000, OffsetAddress + 0x3100, OffsetAddress + 0x3200, OffsetAddress + 0x3300,
                                                            OffsetAddress + 0x3400, OffsetAddress + 0x3500, OffsetAddress + 0x3600, OffsetAddress + 0x3700, OffsetAddress + 0x3800,
                                                            OffsetAddress + 0x3900, OffsetAddress + 0x3A00, OffsetAddress + 0x3B00, OffsetAddress + 0x3C00, OffsetAddress + 0x3D00,
                                                            OffsetAddress + 0x3E00, OffsetAddress + 0x3F00};
    };

    class TPatch : TParameterSection
    {
    public:
        TPatch(int val) : TParameterSection(val + 0x0000) {};

        class TPatchCommon : TParameterSection
        {
        public:
            TPatchCommon(int Offset_param) : TParameterSection(Offset_param + 0x0000) {};
            class TPatchName : TParameter
            {
            public:
                TPatchName(int val) : TParameter(val, 32, 127, "0aaa aaaa") {};
                void Set(std::string Name)
                {
                    for (unsigned int i = 0; i<12 ; i++)
                    {
                        if (i < Name.length() )
                        {
                            pTXV5080->ExclusiveMsgSetParameter(OffsetAddress +i, GetDataBytes(Name[i]));
                        }
                        else
                        {
                            break;
                        }
                    }
                }
            } PatchName{OffsetAddress};

            class TPatchCategory : TParameter
            {
            public:
                TPatchCategory(int val) : TParameter(val + 0x000C, 0, 127, "0aaa aaaa") {};
            } PatchCategory{OffsetAddress};

            class TToneType : TParameter
            {
            public:
                TToneType(int val) : TParameter(val + 0x000D,0, 1, "0000 000a") {};
                void Set(bool Value_param)
                {
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(Value_param));
                }

            } ToneType{OffsetAddress};

            class TPatchLevel : TParameter
            {
            public:
                TPatchLevel(int val) : TParameter(val + 0x000E, 0, 127, "0aaa aaaa") {};
                void Set(int Value_param)
                {
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(Value_param));
                }
            } PatchLevel{OffsetAddress};

            class TPatchPan : TParameter
            {
            public:
                TPatchPan(int val) : TParameter(val + 0x000F, 0, 127, "0aaa aaaa") {};
                void Set(int Value_param)
                {
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(Value_param));
                }
            } PatchPan{OffsetAddress};

            class TPatchCoarseTune : TParameter
            {
            public:
                TPatchCoarseTune(int val) : TParameter(val + 0x0011, 16, 112, "0aaa aaaa") {};
                /** Patch Coarse Tune: -48 semitones (16) to +48 semitones (112) */
                void Set(int Value_param)
                {
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(Value_param));
                }
            } PatchCoarseTune{OffsetAddress};

            class TAnalogFeel : TParameter
            {
            public:
                TAnalogFeel(int val) : TParameter(val + 0x0015, 0, 127, "0aaa aaaa") {};
                /** Set Analog Feel: 0-127 */
                void Set(int Value_param)
                {
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(Value_param));
                }
            } AnalogFeel{OffsetAddress};

            class TMonoPoly : TParameter
            {
                public:
                TMonoPoly(int val) : TParameter(val + 0x0016, 0, 1, "0000 000a") {};
                /** Set patch to MONOPHONIC (0) or POLYPHONIC (1) */
                void Set(int Value_param)
                {
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(Value_param));
                }
            } MonoPoly{OffsetAddress};

            class TLegatoSwitch : TParameter
            {
                public:
                TLegatoSwitch(int val): TParameter(val + 0x0017, 0, 1, "0000 000a") {};
                /** Legato OFF (0) or ON (1) */
                void Set(int Value_param)
                {
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(Value_param));
                }
            } LegatoSwitch{OffsetAddress};
            
            class TLegatoRetrigger : TParameter
            {
                public:
                TLegatoRetrigger(int val): TParameter(val + 0x0018, 0, 1, "0000 000a") {};
                /** Legato Retrigger OFF (0) or ON (1) */
                void Set(int Value_param)
                {
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(Value_param));
                }
            } LegatoRetrigger{OffsetAddress};
            
            class TPortamentoSwitch : TParameter
            {
                public:
                TPortamentoSwitch(int val): TParameter(val + 0x0019, 0, 1, "0000 000a") {};
                /** Portamento switch OFF (0) or ON (1) */
                void Set(int Value_param)
                {
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(Value_param));
                }
            } PortamentoSwitch{OffsetAddress};

            class TPortamentoMode : TParameter
            {
                public:
                TPortamentoMode(int val): TParameter(val + 0x001A, 0, 1, "0000 000a") {};
                /** Portamento Mode NORMAL (0) or LEGATO (1) */
                void Set(int Value_param)
                {
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(Value_param));
                }
            } PortamentoMode{OffsetAddress};

            class TPortamentoType : TParameter
            {
                public:
                TPortamentoType(int val): TParameter(val + 0x001B, 0, 1, "0000 000a") {};
                /** Portamento Type RATE (0) or TIME (1) */
                void Set(int Value_param)
                {
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(Value_param));
                }
            } PortamentoType{OffsetAddress};

            class TPortamentoStart : TParameter
            {
                public:
                TPortamentoStart(int val): TParameter(val + 0x001C, 0, 1, "0000 000a") {};
                /** Portamento Start: PITCH (0) or NOTE (1) */
                void Set(int Value_param)
                {
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(Value_param));
                }
            } PortamentoStart{OffsetAddress};

            class TPortamentoTime : TParameter
            {
                public:
                TPortamentoTime(int val): TParameter(val + 0x001D, 0, 1, "0aaa aaaa") {};
                /** Portamento Time (0-127) */
                void Set(int Value_param)
                {
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(Value_param));
                }
            } PortamentoTime{OffsetAddress};

            class TPatchClockSource : TParameter
            {
                public:
                TPatchClockSource(int val): TParameter(val + 0x001E, 0, 1, "0000 000a") {};
                /** Patch Clock Source: tempo from PATCH (0) or tempo from SYSTEM (1) */
                void Set(int Value_param)
                {
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(Value_param));
                }
            } PatchClockSource{OffsetAddress};

            class TPatchTempo : TParameter
            {
                public:
                TPatchTempo(int val): TParameter(val + 0x001F, 20, 250, "0000 aaaa 0000 bbbb") {};
                /** Patch tempo (20-250) */
                void Set(int Value_param)
                {
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(Value_param));
                }
            } PatchTempo{OffsetAddress};
        } PatchCommon{OffsetAddress};

        class TPatchCommonReverb : TParameterSection
        {
            public:
            TPatchCommonReverb(int val) : TParameterSection(val + 0x0600) {};
            class TReverbType : TParameter
            {
                public:
                TReverbType(int val) : TParameter(val + 0x0000, 0, 4, "0000 aaaa") {};
                /** Value between 0 and 4 */
                void Set(int Value_param)
                {
                    if (Value_param >= 0 && Value_param <=4)
                    {
                       pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(Value_param));
                    }
                }
            } ReverbType{OffsetAddress};

            class TReverbLevel : TParameter
            {
                public:
                TReverbLevel(int val) : TParameter(val + 0x0001, 0, 127, "0aaa aaaa") {};
                /** Value between 0 and 127 */
                void Set(int Value_param)
                {
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(Value_param));
                }
            } ReverbLevel{OffsetAddress};

            class TReverbOutputAssign : TParameter
            {
                public:
                TReverbOutputAssign(int val) : TParameter(val + 0x0002, 0, 3, "0000 00aa") {};
                /** Value between 0 and 3. 0=A, 1=B, 2=C, 3=D */
                void Set(int Value_param)
                {
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(Value_param));
                }
            } ReverbOutputAssign{OffsetAddress};
        } PatchCommonReverb{OffsetAddress};

        class TPatchTone : TParameterSection
        {
            public:
            TPatchTone(int val) : TParameterSection(val + 0x0000) {};
            class TToneLevel : TParameter
            {
                public:
                TToneLevel(int val) : TParameter(val + 0x0000, 0, 127, "0aaa aaaa") {};
                /** Value between 0 and 127 */
                void Set(int Value_param)
                {
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(Value_param));
                }
            } ToneLevel{OffsetAddress};

            class TToneCoarseTune : TParameter
            {
                public:
                TToneCoarseTune(int val) : TParameter(val + 0x0001, 16, 112, "0aaa aaaa") {};
                /** Value between -48 and +48 */
                void Set(int Value_param)
                {
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(Value_param + 64));
                }
            } ToneCoarseTune{OffsetAddress};

            class TToneFineTune : TParameter
            {
                public:
                TToneFineTune(int val) : TParameter(val + 0x0002, 14, 115, "0aaa aaaa") {};
                /** Value between -50 and +50 */
                void Set(int Value_param)
                {
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(Value_param + 64));
                }
            } ToneFineTune{OffsetAddress};

            class TTonePan : TParameter
            {
                public:
                TTonePan(int val) : TParameter(val + 0x0004, 0, 127, "0aaa aaaa") {};
                /** Value between -64 (Full Left) and +63 (Full Right) */
                void Set(int Value_param)
                {
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(Value_param + 64));
                }
            } TonePan{OffsetAddress};

            class TTonePanKeyfollow : TParameter
            {
                public:
                TTonePanKeyfollow(int val) : TParameter(val + 0x0005, 54, 74, "000a aaaa") {};
                /** Value between -100 and +100, by increments of 10 */
                void Set(int Value_param)
                {
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes((Value_param / 10) + 64 ));
                }
            } TonePanKeyfollow{OffsetAddress};

            class TToneRandomPanDepth : TParameter
            {
                public:
                TToneRandomPanDepth(int val) : TParameter(val + 0x0006, 0, 63, "00aa aaaa") {};
                /** Value between 0 and 63 */
                void Set(int Value_param)
                {
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(Value_param));
                }
            } ToneRandomPanDepth{OffsetAddress};

            class TToneAlternatePanDepth : TParameter
            {
                public:
                TToneAlternatePanDepth(int val) : TParameter(val + 0x0007, 1, 127, "0aaa aaaa") {};
                /** Value between -63 and +63 */
                void Set(int Value_param)
                {
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(Value_param));
                }
            } ToneAlternatePanDepth{OffsetAddress};

            class TWaveNumberL_Mono : TParameter
            {
                public:
                TWaveNumberL_Mono(int val) : TParameter(val + 0x002C, 0, 16384, "0000 aaaa 0000 bbbb 0000 cccc 0000 dddd") {};
                /** Value between 0 (OFF), 1 (1) and 16384 (16384) */
                void Set(int Value_param)
                {
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(Value_param));
                }
            } WaveNumberL_Mono{OffsetAddress};

            class TWaveNumberR : TParameter
            {
                public:
                TWaveNumberR(int val) : TParameter(val + 0x0030, 0, 16384, "0000 aaaa 0000 bbbb 0000 cccc 0000 dddd") {};
                /** Value between 0 (OFF), 1 (1) and 16384 (16384) */
                void Set(int Value_param)
                {
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(Value_param));
                }
            } WaveNumberR{OffsetAddress};

            class TTVAEnvTime2 : TParameter
            {
                public:
                TTVAEnvTime2(int val) : TParameter(val + 0x0067, 0, 127, "0aaa aaaa") {};
                /** TVA Envelope Time #2, value between 0 and 127 */
                void Set(int Value_param)
                {
                    pTXV5080->ExclusiveMsgSetParameter(OffsetAddress, GetDataBytes(Value_param));
                }
            } TVAEnvTime2{OffsetAddress};
        };
        std::array<TPatchTone, 4> PatchTone = { OffsetAddress + 0x002000, 
                                                OffsetAddress + 0x002200,
                                                OffsetAddress + 0x002400,
                                                OffsetAddress + 0x002600};
    };

    class TRhythm : TParameterSection
    {
    public:
        TRhythm(int val) : TParameterSection(val + 0x0000) {};
        class TRhythmCommon : TParameterSection
        {
        public:
            TRhythmCommon(int val) : TParameterSection(val + 0x0000) {};
            void NotImplemented(void) {};
        } RhythmCommon{OffsetAddress};
    };

    class TTemporaryPatchRhythm : TParameterSection
    {
    public:
        TTemporaryPatchRhythm(int val) : TParameterSection(val + 0x0000) {};
        TPatch TemporaryPatch{OffsetAddress + 0x000000};
        TRhythm TemporaryRhythm{OffsetAddress + 0x100000};
    } TemporaryPatchRhythm_InPatchMode{0x1F000000};

    TPerformance TemporaryPerformance = TPerformance(0x10000000);
    // Note the start address does not agree with XV5080 manual - it says it ends at 18600000 - I can't see how.
    std::array<TTemporaryPatchRhythm, 32> TemporaryPatchRhythm_InPerformanceMode = {
        0x11000000, 0x11200000, 0x11400000, 0x11600000, 0x11800000, 0x11A00000, 0x11C00000, 0x11E00000,
        0x12000000, 0x12200000, 0x12400000, 0x12600000, 0x12800000, 0x12A00000, 0x12C00000, 0x12E00000,
        0x13000000, 0x13200000, 0x13400000, 0x13600000, 0x13800000, 0x13A00000, 0x13C00000, 0x13E00000,
        0x14000000, 0x14200000, 0x14400000, 0x14600000, 0x14800000, 0x14A00000, 0x14C00000, 0x14E00000};

    #ifdef TXV5080_USER_PERFORMANCES
    // 64 user performances
    std::array<TPerformance, 64> UserPerformances = {0x20000000, 0x20010000, 0x20020000, 0x20030000, 0x20040000, 0x20050000, 0x20060000, 0x20070000,
                                                     0x20080000, 0x20090000, 0x200A0000, 0x200B0000, 0x200C0000, 0x200D0000, 0x200E0000, 0x200F0000,
                                                     0x20100000, 0x20110000, 0x20120000, 0x20130000, 0x20140000, 0x20150000, 0x20160000, 0x20170000,
                                                     0x20180000, 0x20190000, 0x201A0000, 0x201B0000, 0x201C0000, 0x201D0000, 0x201E0000, 0x201F0000,
                                                     0x20200000, 0x20210000, 0x20220000, 0x20230000, 0x20240000, 0x20250000, 0x20260000, 0x20270000,
                                                     0x20280000, 0x20290000, 0x202A0000, 0x202B0000, 0x202C0000, 0x202D0000, 0x202E0000, 0x202F0000,
                                                     0x20300000, 0x20310000, 0x20320000, 0x20330000, 0x20340000, 0x20350000, 0x20360000, 0x20370000,
                                                     0x20380000, 0x20390000, 0x203A0000, 0x203B0000, 0x203C0000, 0x203D0000, 0x203E0000, 0x203F0000};

    #endif // TXV5080_USER_PERFORMANCES

private:

    // Record which MIDI port should be used for communication
    TMIDI_Port * pMIDI_Port;
    static TXV5080 * pTXV5080;

    char ExclusiveMsgChecksum(std::vector<unsigned char> data)
    {
        unsigned long int checksum_value = 0;
        for (auto x : data)
        {
            checksum_value += x;
        }
        return ((128 - (checksum_value % 128)) & 0x7F);
    }

private:

    void ExclusiveMsgSetParameter(unsigned long int addr, std::vector<unsigned char> value)
    {
        // From Roland XV-5080 manual
        std::vector<unsigned char> v;
        v.push_back(0xF0); // F0H Exclusive status
        v.push_back(0x41); // 41H ID number (Roland)
        v.push_back(0x10); // dev Device ID (dev: 00H - 1FH, Initial value is 10H)
        v.push_back(0x00); // 00H Model ID #1 (XV-5080)
        v.push_back(0x10); // 10H Model ID #2 (XV-5080)
        v.push_back(0x12); // 12H Command ID (DT1)
        v.push_back((addr & 0xFF000000) >> 24); // aaH Address MSB: upper byte of the starting address of the data to be sent
        v.push_back((addr & 0x00FF0000) >> 16); // bbH Address: upper middle byte of the starting address of the data to be sent
        v.push_back((addr & 0x0000FF00) >> 8); // ccH Address: lower middle byte of the starting address of the data to be sent
        v.push_back((addr & 0x000000FF)); // ddH Address LSB: lower byte of the starting address of the data to be sent.
        v.insert(v.end(), value.begin(), value.end()); // eeH Data: the actual data to be sent. Multiple bytes of data are transmitted in order starting from the address.

        std::vector<unsigned char> chksum_data;
        chksum_data.insert(chksum_data.end(), v.begin() +6, v.end());

        v.push_back(ExclusiveMsgChecksum(chksum_data)); // ??H Data sum Checksum
        v.push_back(0xF7);
        v.push_back(0xE0); // F7H EOX (End Of Exclusive)

        pMIDI_Port->SendRawData(v);
    }
};

// Definition of static member in TXV5080 must be done outside the class
TXV5080 * TXV5080::pTXV5080 = 0;
// Instance of the XV-5080 communication driver
TXV5080 XV5080(&MIDI_A);

void test_XV5080(void)
{

    XV5080.System.SystemCommon.SoundMode.Patch();
    XV5080.System.SystemCommon.SoundMode.Perform();
    XV5080.System.SystemCommon.MasterTune.Set(6.5);
    XV5080.System.SystemCommon.MasterKeyShift.Set(1);
    XV5080.System.SystemCommon.MasterLevel.Set(99);
    XV5080.System.SystemCommon.ScaleTuneSwitch.Set(1);
    XV5080.System.SystemCommon.PatchRemain.Set(1);
    XV5080.System.SystemCommon.MixParallel.Set(1);
    XV5080.System.SystemCommon.MFXSwitch.Set(1);
    XV5080.System.SystemCommon.ChorusSwitch.Set(1);
    XV5080.System.SystemCommon.ReverbSwitch.Set(1);
    /*XV5080.System.SystemCommon.PerformanceControlChannel.Set(1);
    XV5080.System.SystemCommon.PerformanceBankSelectMSB.Set(1);
    XV5080.System.SystemCommon.PerformanceBankSelectLSB.Set(3);
    XV5080.System.SystemCommon.PerformanceProgramNumber.Set(5);*/
    XV5080.System.SystemCommon.SystemTempo.Set(130);

    XV5080.PerformanceSelect(TXV5080::PerformanceGroup::USER, TInt_1_128(43));

    XV5080.TemporaryPerformance.PerformanceCommon.PerformanceName.Set("Blah");
    XV5080.TemporaryPerformance.PerformancePart[2].SelectPatch(TXV5080::PatchGroup::CD_A, 43);

    //XV5080.UserPerformances
//    XV5080.PatchSelect(XV5080::PatchGroup::USER, 43);
/*
    XV5080.System.SystemCommon.PerformanceSelect(XV5080::PerformanceGroup::USER, 43);
    XV5080.System.SystemCommon.PerformanceSelect(XV5080::PerformanceGroup::PR_A, 32);
    XV5080.System.SystemCommon.PerformanceSelect(XV5080::PerformanceGroup::PR_B, 12);
    XV5080.System.SystemCommon.PerformanceSelect(XV5080::PerformanceGroup::CD_A, 12);
    XV5080.System.SystemCommon.PerformanceSelect(XV5080::PerformanceGroup::CD_B, 12);
*/
    XV5080.System.SystemCommon.SystemTempo.Set(130);
    XV5080.TemporaryPerformance.PerformanceCommon.PerformanceName.Set("This is a test");
    XV5080.TemporaryPerformance.PerformancePart[0].SelectPatch(TXV5080::PatchGroup::CD_C, 87);
    XV5080.TemporaryPerformance.PerformancePart[0].ReceiveChannel.Set_1_16(1);

    //XV5080.TemporaryPatchRhythm_InPatchMode.TemporaryPatch.PatchCommon.ToneType.Set(true);
//    unsigned long int addr = XV5080.UserPerformances[1].TEST_GetOffsetAddress();

    //  TXV5080::System::SystemCommon::SoundMode::Write(0);

}


// Thread used by PlayNote() to keep track of time, for each note.
void playNoteThread(TPlayNoteMsg msg)
{
    MIDI_A.SendNoteOnEvent(&msg);
    waitMilliseconds(msg.DurationMS);
    wprintw(win_debug_messages.GetRef(), "playNoteThread\n");
    msg.Velocity = 0;
    MIDI_A.SendNoteOffEvent(&msg);
}

// Useful function that plays a note, on a specific MIDI channel, Note Number, Duration in milliseconds, and Velocity.
void PlayNote(unsigned char Channel_param, unsigned char NoteNumber_param, int DurationMS_param, int Velocity_param)
{
    TPlayNoteMsg PlayNoteMsg;
    PlayNoteMsg.Channel = Channel_param;
    PlayNoteMsg.DurationMS = DurationMS_param;
    PlayNoteMsg.NoteNumber = NoteNumber_param;
    PlayNoteMsg.Velocity = Velocity_param;
    std::thread Thread(playNoteThread, PlayNoteMsg);
    setScheduling_RealTime_TopPriority(Thread.native_handle());
    Thread.detach();
}


// A wrapper function to play a MIDI note, used by the micro-midi-keyboard.
void midiPlay(int octave, unsigned char noteInScale)
{
    PlayNote(2, octave *12 + noteInScale, 1000, 100);
}


void SelectContextInPlaylist(std::list<TContext*> &ContextList, bool);
void SelectContextInPlaylist(std::list<TContext*> &ContextList);
void SelectContextInPlaylist(std::list<TContext*> &ContextList);

namespace MetronomeMaster
{
    bool flag_broadcast_tempo = false;
    void BroadcastTempo(void)
    {
        flag_broadcast_tempo = true;
    }

 
    void IncreaseCurrentTempo(void * foo)
    {
        // Protect PlaylistPosition from concurrent access
        std::lock_guard<std::mutex> lock(PlaylistPosition_mtx);

        PlaylistPosition->BaseTempo++;
        if ((PlaylistPosition)->BaseTempo < 30) (PlaylistPosition)->BaseTempo = 30;
        if ((PlaylistPosition)->BaseTempo > 200) (PlaylistPosition)->BaseTempo = 200;
    }

    void DecreaseCurrentTempo(void * foo)
    {
        // Protect PlaylistPosition from concurrent access
        std::lock_guard<std::mutex> lock(PlaylistPosition_mtx);
 
        (PlaylistPosition)->BaseTempo--;
        if ((PlaylistPosition)->BaseTempo < 30) (PlaylistPosition)->BaseTempo = 30;
    }

    bool FlashFlag = false; 

    void FlashToggle(void)
    {
        if (FlashFlag == false)
        {
            FlashFlag = true;
        }
        else
        {
            FlashFlag = false;
        }        
    }

    bool ClickFlag = false;
    void ClickToggle(void)
    {
        if (ClickFlag == false)
        {
            ClickFlag = true;
        }
        else
        {
            ClickFlag = false;
        }
    }

    TInt_0_127 ClickNoteNumber = TInt_0_127(76);
    void ClickNoteNumberIncrease(void * pVoid)
    {
        ClickNoteNumber++;
    }

    void ClickNoteNumberDecrease(void * pVoid)
    {
        ClickNoteNumber--;
    }

    // Display a metronome on the User Specific window.
    void threadMetronome (void)
    {
        const int PulseDuration = 100;
        TContext * pContext;
        init_pair(1, COLOR_WHITE, COLOR_BLACK);
        init_pair(2, COLOR_RED, COLOR_BLACK);
        init_pair(3, COLOR_GREEN, COLOR_BLACK);
        init_pair(4, COLOR_RED, COLOR_WHITE);
        init_pair(5, COLOR_BLUE, COLOR_WHITE);
        init_pair(6, COLOR_BLACK, COLOR_GREEN);

        TBoxedWindow FlashWindow;
        FlashWindow.Init("", LINES, COLS, 0, 0);
        FlashWindow.Erase();
        char * pFlashWindowContents = (char *) malloc(LINES * COLS +10);
        // Create a C string that contains enough spaces to fill a screen
        unsigned int i = 0;
        for (i = LINES * COLS; i--;)
        {
            *((pFlashWindowContents) +i) = ' ';
        }
        *((pFlashWindowContents) +LINES * COLS) = 0;
        wattron(FlashWindow.GetRef(), A_BOLD | A_REVERSE);
        mvwprintw(FlashWindow.GetRef(), 0, 0, pFlashWindowContents);
        FlashWindow.Refresh();
        FlashWindow.Hide();
        std::chrono::system_clock::time_point TimeStart;
        float Tempo = 30;
        float previous_Tempo = -1;
        float beat_number = 0;
        float delay_ms = 100;
        while (1)
        {
            // Get current Base Tempo of the song
            {
                // Protect PlaylistPosition from concurrent access
                std::lock_guard<std::mutex> lock(PlaylistPosition_mtx);
                pContext = PlaylistPosition;
                Tempo = pContext->BaseTempo;
            }

            if (Tempo != previous_Tempo)
            {
                 // Change in tempo detected
                previous_Tempo = Tempo;
                TimeStart = std::chrono::system_clock::now();
                beat_number = 0;
                // Compute wait time from the tempo
                if (Tempo != 0)
                {
                    delay_ms = 60000.0 / Tempo;
                }
                else
                {
                    delay_ms = 1000;
                }
                flag_broadcast_tempo = true;
            }


            if (flag_broadcast_tempo)
            {
                // Broadcast tempo information to MQTT
                flag_broadcast_tempo = false;
                std::string Tempo_str;
                Tempo_str = std::to_string( static_cast<int>(Tempo));
                unsigned long int TimeSinceEpoch_ms = std::chrono::duration_cast<std::chrono::milliseconds>(TimeStart.time_since_epoch()).count();
                std::string TimeSinceEpoch_ms_str = std::to_string(TimeSinceEpoch_ms);

                // Also send the tempo to the XV5080
                XV5080.System.SystemCommon.SystemTempo.Set(static_cast<int>(Tempo));
            }

            beat_number ++;
            
            // Compute time at which next beat must happen
            std::chrono::system_clock::time_point TimeNext = TimeStart + std::chrono::milliseconds((long int) (beat_number * delay_ms));
            std::this_thread::sleep_until(TimeNext);

            // Play metronome
            if (ClickFlag)
            {
                PlayNote(14, ClickNoteNumber, 20,100);
            }
            // Display metronome
      
            wattron(win_context_user_specific.GetRef(), COLOR_PAIR(1));
            wattron(win_context_user_specific.GetRef(), A_BOLD | A_REVERSE);
            mvwprintw(win_context_user_specific.GetRef(), 0, 0, "BASE TEMPO:%03d",(int)Tempo);
            win_context_user_specific.Refresh();
            if (FlashFlag)
            {
                FlashWindow.Show();
                waitMilliseconds(PulseDuration);
                FlashWindow.Hide();
            }
            else
            {

                waitMilliseconds(PulseDuration);
                wattroff(win_context_user_specific.GetRef(), A_BOLD | A_REVERSE);
                mvwprintw(win_context_user_specific.GetRef(), 0, 0, "BASE TEMPO:%03d",(int)Tempo);
                win_context_user_specific.Refresh();
            }
        }
    }
}





void TapTempo(void);



namespace MiniSynth
{
char noteInScale;
int octave = 2;
int program = 1;
int channel = 2;
int transpose = 5;

void StartNote(void * pVoid)
{
    int noteInScale = (long int) pVoid;
    MIDI_A.SendNoteOnEvent(channel, octave *12 + noteInScale + transpose, 100);
}

void StopNote(void * pVoid)
{
    int noteInScale = (long int) pVoid;
    MIDI_A.SendNoteOffEvent(channel, octave *12 + noteInScale + transpose, 0);
}

void Reset(void * pVoid)
{
    TContext * pContext = PlaylistPosition;
    if (!pContext->ResetMinisynth())
    {
        // This context did not hold any specific function to reset the minisynth
        // So do a generic reset here.
        octave = 2;
        program = 1;
        channel = 5;
        transpose = 0;    
    }
}

void OctaveLess(void * pVoid)
{
    octave--;
    wprintw(win_debug_messages.GetRef(), "Octave %i\n", octave);
}

void OctaveMore(void * pVoid)
{
    octave++;
    wprintw(win_debug_messages.GetRef(), "Octave %i\n", octave);
}

void ProgramLess(void * pVoid)
{
    program--;
    wprintw(win_debug_messages.GetRef(), "Program %i\n", program);
    {
        MIDI_A.SendProgramChange(channel, program);
    }
}

void ProgramMore(void * pVoid)
{
    program++;
    wprintw(win_debug_messages.GetRef(), "Program %i\n", program);
    {
        MIDI_A.SendProgramChange(channel, program);
    }
}


void ChannelLess(void * pVoid)
{
    channel--;
    wprintw(win_debug_messages.GetRef(), "Channel %i\n", channel);
}

void ChannelMore(void * pVoid)
{
    channel++;
    wprintw(win_debug_messages.GetRef(), "Channel %i\n", channel);

}

void TransposeLess(void * pVoid)
{
    transpose--;
    wprintw(win_debug_messages.GetRef(), "Transpose %i\n", transpose);
}

void TransposeMore(void * pVoid)
{
    transpose++;
    wprintw(win_debug_messages.GetRef(), "Transpose %i\n", transpose);
}

void Space(void * pVoid)
{
    ComputerKeyboard::DisableCallbacks();
    while(getch() != ERR)
    {
        // donothing();
    }
    // The function below handles computer keyboard by itself, through ncurses lib
    SelectContextInPlaylist(PlaylistData, false);
    // Done getting keyboard information from ncurses - back to the raw keyboard routine
    ComputerKeyboard::EnableCallbacks();
}

void B(void * pVoid)
{
    ComputerKeyboard::DisableCallbacks();
    SelectContextInPlaylist(PlaylistData_BySongName, false);
    ComputerKeyboard::EnableCallbacks();
}

void N(void * pVoid)
{
    ComputerKeyboard::DisableCallbacks();
    SelectContextInPlaylist(PlaylistData_ByAuthor, true);
    ComputerKeyboard::EnableCallbacks();
}


void Z_p(void * pVoid)
{

}

void Z_r(void * pVoid)
{

}


void X_p(void * pVoid)
{

}

void X_r(void * pVoid)
{

}


void V_p(void * pVoid)
{
    MetronomeMaster::FlashToggle();
}

void C_p(void * pVoid)
{
    MetronomeMaster::ClickToggle();
}

void ESC_Key_Pressed(void * pVoid)
{
    Reset(NULL);
    ESC_Key_Pressed_Flag = true;
}

void ESC_Key_Released(void * pVoid)
{
    ESC_Key_Pressed_Flag = false;
}

// Scan keyboard for events, and process keypresses accordingly.
void threadKeyboard(void)
{
    // create all the notes in
    ComputerKeyboard::RegisterEventCallbackPressed(KEY_Q, StartNote, 0);
    ComputerKeyboard::RegisterEventCallbackPressed(KEY_2, StartNote, (void *)1);
    ComputerKeyboard::RegisterEventCallbackPressed(KEY_W, StartNote, (void *)2);
    ComputerKeyboard::RegisterEventCallbackPressed(KEY_3, StartNote, (void *)3);
    ComputerKeyboard::RegisterEventCallbackPressed(KEY_E, StartNote, (void *)4);
    ComputerKeyboard::RegisterEventCallbackPressed(KEY_R, StartNote, (void *)5);
    ComputerKeyboard::RegisterEventCallbackPressed(KEY_5, StartNote, (void *)6);
    ComputerKeyboard::RegisterEventCallbackPressed(KEY_T, StartNote, (void *)7);
    ComputerKeyboard::RegisterEventCallbackPressed(KEY_6, StartNote, (void *)8);
    ComputerKeyboard::RegisterEventCallbackPressed(KEY_Y, StartNote, (void *)9);
    ComputerKeyboard::RegisterEventCallbackPressed(KEY_7, StartNote, (void *)10);
    ComputerKeyboard::RegisterEventCallbackPressed(KEY_U, StartNote, (void *)11);
    ComputerKeyboard::RegisterEventCallbackPressed(KEY_I, StartNote, (void *)12);
    ComputerKeyboard::RegisterEventCallbackPressed(KEY_9, StartNote, (void *)13);
    ComputerKeyboard::RegisterEventCallbackPressed(KEY_O, StartNote, (void *)14);
    ComputerKeyboard::RegisterEventCallbackPressed(KEY_0, StartNote, (void *)15);
    ComputerKeyboard::RegisterEventCallbackPressed(KEY_P, StartNote, (void *)16);
    ComputerKeyboard::RegisterEventCallbackPressed(KEY_LEFTBRACE, StartNote, (void *)17);
    ComputerKeyboard::RegisterEventCallbackPressed(KEY_EQUAL, StartNote, (void *)18);
    ComputerKeyboard::RegisterEventCallbackPressed(KEY_RIGHTBRACE, StartNote, (void *)19);
    ComputerKeyboard::RegisterEventCallbackPressed(KEY_UP, [](void * foo){MetronomeMaster::IncreaseCurrentTempo(0);}, 0);
    ComputerKeyboard::RegisterEventCallbackPressed(KEY_DOWN, [](void * foo){MetronomeMaster::DecreaseCurrentTempo(0);}, 0);
    ComputerKeyboard::RegisterEventCallbackPressed(KEY_LEFT, [](void * foo){MetronomeMaster::ClickNoteNumberDecrease(0);}, 0);
    ComputerKeyboard::RegisterEventCallbackPressed(KEY_RIGHT, [](void * foo){MetronomeMaster::ClickNoteNumberIncrease(0);}, 0);
    ComputerKeyboard::RegisterEventCallbackPressed(KEY_LEFTCTRL, [](void * foo){TapTempo();}, 0);
    


    ComputerKeyboard::RegisterEventCallbackReleased(KEY_Q, StopNote, 0);
    ComputerKeyboard::RegisterEventCallbackReleased(KEY_2, StopNote, (void *)1);
    ComputerKeyboard::RegisterEventCallbackReleased(KEY_W, StopNote, (void *)2);
    ComputerKeyboard::RegisterEventCallbackReleased(KEY_3, StopNote, (void *)3);
    ComputerKeyboard::RegisterEventCallbackReleased(KEY_E, StopNote, (void *)4);
    ComputerKeyboard::RegisterEventCallbackReleased(KEY_R, StopNote, (void *)5);
    ComputerKeyboard::RegisterEventCallbackReleased(KEY_5, StopNote, (void *)6);
    ComputerKeyboard::RegisterEventCallbackReleased(KEY_T, StopNote, (void *)7);
    ComputerKeyboard::RegisterEventCallbackReleased(KEY_6, StopNote, (void *)8);
    ComputerKeyboard::RegisterEventCallbackReleased(KEY_Y, StopNote, (void *)9);
    ComputerKeyboard::RegisterEventCallbackReleased(KEY_7, StopNote, (void *)10);
    ComputerKeyboard::RegisterEventCallbackReleased(KEY_U, StopNote, (void *)11);
    ComputerKeyboard::RegisterEventCallbackReleased(KEY_I, StopNote, (void *)12);
    ComputerKeyboard::RegisterEventCallbackReleased(KEY_9, StopNote, (void *)13);
    ComputerKeyboard::RegisterEventCallbackReleased(KEY_O, StopNote, (void *)14);
    ComputerKeyboard::RegisterEventCallbackReleased(KEY_0, StopNote, (void *)15);
    ComputerKeyboard::RegisterEventCallbackReleased(KEY_P, StopNote, (void *)16);
    ComputerKeyboard::RegisterEventCallbackReleased(KEY_LEFTBRACE, StopNote, (void *)17);
    ComputerKeyboard::RegisterEventCallbackReleased(KEY_EQUAL, StopNote, (void *)18);
    ComputerKeyboard::RegisterEventCallbackReleased(KEY_RIGHTBRACE, StopNote, (void *)19);

    ComputerKeyboard::RegisterEventCallbackPressed(KEY_ESC, ESC_Key_Pressed, 0);
    ComputerKeyboard::RegisterEventCallbackReleased(KEY_ESC, ESC_Key_Released, 0);
    ComputerKeyboard::RegisterEventCallbackPressed(KEY_F1, OctaveLess, 0);
    ComputerKeyboard::RegisterEventCallbackPressed(KEY_F2, OctaveMore, 0);
    ComputerKeyboard::RegisterEventCallbackPressed(KEY_F3, ProgramLess, 0);
    ComputerKeyboard::RegisterEventCallbackPressed(KEY_F4, ProgramMore, 0);
    ComputerKeyboard::RegisterEventCallbackPressed(KEY_F5, ChannelLess, 0);
    ComputerKeyboard::RegisterEventCallbackPressed(KEY_F6, ChannelMore, 0);
    ComputerKeyboard::RegisterEventCallbackPressed(KEY_F7, TransposeLess, 0);
    ComputerKeyboard::RegisterEventCallbackPressed(KEY_F8, TransposeMore, 0);

    ComputerKeyboard::RegisterEventCallbackPressed(KEY_SPACE, Space, 0);
    ComputerKeyboard::RegisterEventCallbackPressed(KEY_B, B, 0);
    ComputerKeyboard::RegisterEventCallbackPressed(KEY_N, N, 0);
    ComputerKeyboard::RegisterEventCallbackPressed(KEY_Z, Z_p, 0);
    ComputerKeyboard::RegisterEventCallbackReleased(KEY_Z, Z_r, 0);
    ComputerKeyboard::RegisterEventCallbackPressed(KEY_X, X_p, 0);
    ComputerKeyboard::RegisterEventCallbackPressed(KEY_V, V_p, 0);
    ComputerKeyboard::RegisterEventCallbackPressed(KEY_C, C_p, 0);

    // The pedal board digital pedals can be activated here, with keys QSDFGHJKLM
    ComputerKeyboard::RegisterEventCallbackPressed(KEY_A, [](void * foo){PlaylistPosition->Pedalboard.PedalsDigital[1].Press();}, 0);
    ComputerKeyboard::RegisterEventCallbackReleased(KEY_A, [](void * foo){PlaylistPosition->Pedalboard.PedalsDigital[1].Release();}, 0);
    ComputerKeyboard::RegisterEventCallbackPressed(KEY_S, [](void * foo){PlaylistPosition->Pedalboard.PedalsDigital[2].Press();}, 0);
    ComputerKeyboard::RegisterEventCallbackReleased(KEY_S, [](void * foo){PlaylistPosition->Pedalboard.PedalsDigital[2].Release();}, 0);
    ComputerKeyboard::RegisterEventCallbackPressed(KEY_D, [](void * foo){PlaylistPosition->Pedalboard.PedalsDigital[3].Press();}, 0);
    ComputerKeyboard::RegisterEventCallbackReleased(KEY_D, [](void * foo){PlaylistPosition->Pedalboard.PedalsDigital[3].Release();}, 0);
    ComputerKeyboard::RegisterEventCallbackPressed(KEY_F, [](void * foo){PlaylistPosition->Pedalboard.PedalsDigital[4].Press();}, 0);
    ComputerKeyboard::RegisterEventCallbackReleased(KEY_F, [](void * foo){PlaylistPosition->Pedalboard.PedalsDigital[4].Release();}, 0);
    ComputerKeyboard::RegisterEventCallbackPressed(KEY_G, [](void * foo){PlaylistPosition->Pedalboard.PedalsDigital[5].Press();}, 0);
    ComputerKeyboard::RegisterEventCallbackReleased(KEY_G, [](void * foo){PlaylistPosition->Pedalboard.PedalsDigital[5].Release();}, 0);
    ComputerKeyboard::RegisterEventCallbackPressed(KEY_H, [](void * foo){PlaylistPosition->Pedalboard.PedalsDigital[6].Press();}, 0);
    ComputerKeyboard::RegisterEventCallbackReleased(KEY_H, [](void * foo){PlaylistPosition->Pedalboard.PedalsDigital[6].Release();}, 0);
    ComputerKeyboard::RegisterEventCallbackPressed(KEY_J, [](void * foo){PlaylistPosition->Pedalboard.PedalsDigital[7].Press();}, 0);
    ComputerKeyboard::RegisterEventCallbackReleased(KEY_J, [](void * foo){PlaylistPosition->Pedalboard.PedalsDigital[7].Release();}, 0);
    ComputerKeyboard::RegisterEventCallbackPressed(KEY_K, [](void * foo){PlaylistPosition->Pedalboard.PedalsDigital[8].Press();}, 0);
    ComputerKeyboard::RegisterEventCallbackReleased(KEY_K, [](void * foo){PlaylistPosition->Pedalboard.PedalsDigital[8].Release();}, 0);
    ComputerKeyboard::RegisterEventCallbackPressed(KEY_L, [](void * foo){PlaylistPosition->Pedalboard.PedalsDigital[10].Press();}, 0);
    ComputerKeyboard::RegisterEventCallbackReleased(KEY_L, [](void * foo){PlaylistPosition->Pedalboard.PedalsDigital[10].Release();}, 0);
    ComputerKeyboard::RegisterEventCallbackPressed(KEY_SEMICOLON, [](void * foo){PlaylistPosition->Pedalboard.PedalsDigital[11].Press();}, 0);
    ComputerKeyboard::RegisterEventCallbackReleased(KEY_SEMICOLON, [](void * foo){PlaylistPosition->Pedalboard.PedalsDigital[11].Release();}, 0);

    while (1)
    {
        int ch = -1;
        if (ch == -1)
        {
            waitMilliseconds(1000);
            continue;
        }
    }
}


}

//http://www.cplusplus.com/forum/general/216928/
// Returns interpolated value at x from parallel arrays ( xData, yData )
// Assumes that xData has at least two elements, is sorted and is strictly monotonic increasing
// boolean argument extrapolate determines behaviour beyond ends of array (if needed)
double interpolate( std::vector<double> &xData, std::vector<double> &yData, double x, bool extrapolate )
{
    int size = xData.size();

    int i = 0;                                                                  // find left end of interval for interpolation
    if ( x >= xData[size - 2] )                                                 // special case: beyond right end
    {
        i = size - 2;
    }
    else
    {
        while ( x > xData[i+1] ) i++;
    }
    double xL = xData[i], yL = yData[i], xR = xData[i+1], yR = yData[i+1];      // points on either side (unless beyond ends)
    if ( !extrapolate )                                                         // if beyond ends of array and not extrapolating
    {
        if ( x < xL ) yR = yL;
        if ( x > xR ) yL = yR;
    }

    double dydx = ( yR - yL ) / ( xR - xL );                                    // gradient

    return yL + dydx * ( x - xL );                                              // linear interpolation
}



// Mute all sounds (that is, cancel any running note), on the Sound Canvas SC55.
// But after the call, the SC55 can play.
// Often tied to a "panic" key, to mute all sounds in case a midi NOTE OFF event would
// be missed. Or to clean up after the midi sequencer has finished playing.
void AllSoundsOff(void)
{
    // all sounds off for all channels
    for (unsigned int i = 1; i<=16; i++)
    {
        MIDI_A.SendControlChange(i, 0x78, 0);
        MIDI_A.SendControlChange(i, 0x79, 0);
        MIDI_A.SendControlChange(i, 0x7B, 0);
        MIDI_A.SendControlChange(i, 0x7C, 0);
    }
}




typedef struct
{
public:
    int NoteNumber;
    float NoteDuration; // In fraction of a pulse
} TNote;


class TSyncMsg
{
private:
    std::condition_variable cv;
    const int INVALID_MESSAGE = -999999;
    int msg = INVALID_MESSAGE;
    bool flag = false;
    std::mutex m;
public:
    TSyncMsg(void) {};

    void Send(void)
    {
        Send(1);
    }

    void Send(int msg_param)
    {
        {
            std::lock_guard<std::mutex> lk(m);
            msg = msg_param;
            flag = true;
        }
        cv.notify_one();
    }
#if 0
    int WaitTrue(int timeout_param)
    {
        return Wait(1, timeout_param);
    }

    int WaitTrue(void)
    {
        return Wait(1, 0);
    }
#endif
    int Wait(int msg_param, int timeout_ms_param)
    {
        return Wait_OR(msg_param, msg_param, timeout_ms_param);
    }

    int Wait_OR(int msg_param1, int msg_param2, int timeout_ms_param)
    {
        msg = INVALID_MESSAGE;
        if (timeout_ms_param == 0)
        {
            do
            {
                std::unique_lock<std::mutex> lk(m);
                cv.wait(lk, [&]{return flag;});
                flag = 0;
            } while (!( (msg == msg_param1) || (msg == msg_param2)));
            return msg;
        }
        else
        {
            do
            {
                std::unique_lock<std::mutex> lk(m);
                if (cv.wait_for(lk, std::chrono::milliseconds(timeout_ms_param), [&]{return flag;}) == true)
                {
                    // predicate true
                    flag = 0;
                    return msg;
                }
                else
                {
                    // timeout
                    flag = 0;
                    return -1;
                }
            } while (!( (msg == msg_param1) || (msg == msg_param2) ));
        }
    }
};


void TapTempo(void)
{
    static enum TTapTempoStateMachine {ttsmInit, ttsmWaitFirstTap, ttsmComputeTempo} TapTempoStateMachine = ttsmInit;
    static std::vector<struct timeval> TimeValue_vec = {};
    static std::vector<float> DeltaTime_vec = {};
    static struct timeval tv;
    switch (TapTempoStateMachine)
    {
        case ttsmInit:
        TimeValue_vec.clear();
        TapTempoStateMachine = ttsmWaitFirstTap;
        wprintw(win_debug_messages.GetRef(), "Tap tempo INIT\n");
        // Do more init stuff
        // no break;
        case ttsmWaitFirstTap:
        // First beat
        gettimeofday(&tv, NULL);  
        TimeValue_vec.push_back(tv);
        TapTempoStateMachine = ttsmComputeTempo;
        break;

        case ttsmComputeTempo:
        wprintw(win_debug_messages.GetRef(), "Tap tempo COMPUTE\n");
        bool flag_start = true;
        struct timeval last_timeval;
        gettimeofday(&tv, NULL);  
        TimeValue_vec.push_back(tv);
        DeltaTime_vec.clear();
        for (auto val : TimeValue_vec)
        {
            if (flag_start == true)
            {
                flag_start = false;
                last_timeval = val;
            }
            else
            {
                struct timeval tv1, tv2;
                tv1 = last_timeval;
                tv2 = val;
                float DeltaTime = ((tv2.tv_sec - tv1.tv_sec) + (tv2.tv_usec - tv1.tv_usec) / 1000000.0);
                DeltaTime_vec.push_back(DeltaTime);
                last_timeval = tv2;
            }
        }
        // If the last delta time is too large, it means this algorithm has paused for a long time
        // since the last time it was called, and that the last call was probably not a call to update
        // the tempo, but the first call of a new request to find a new tempo, so restart the algorithm,
        // while taking into account the last timeval that came in.
        if (DeltaTime_vec.back() > 4.0) // more than 4 seconds
        {
            DeltaTime_vec.clear();
            struct timeval tv;
            gettimeofday(&tv, NULL);  
            TimeValue_vec.clear();
            TimeValue_vec.push_back(tv);
            break;
        }
        else
        {
            // Do the tempo calculation
            float DeltaTimeMean = 0;
            float DeltaTimeMean_limit_low = 0;
            float DeltaTimeMean_limit_high = 0;
            for (auto val : DeltaTime_vec)
            {
                DeltaTimeMean += val;
            }
            DeltaTimeMean /= DeltaTime_vec.size();
            DeltaTimeMean_limit_low = 0.60 * DeltaTimeMean;
            DeltaTimeMean_limit_high = 1.4 * DeltaTimeMean;

            // Get rid of any measurement that deviates too much from the mean
            // This is an easy way of eliminating "outliers", e.g. if one beat was missed
            int MeasurementsCount = 0;
            float DeltaTimeMeanRobust = 0;
            for (auto val: DeltaTime_vec)
            {
                if (val < DeltaTimeMean_limit_low || val > DeltaTimeMean_limit_high)
                {
                    // Don't use that measurement                    
                }
                else
                {
                    // Do use that measurement
                    MeasurementsCount += 1;
                    DeltaTimeMeanRobust += val;
                }
            }
            wprintw(win_debug_messages.GetRef(), "Tap tempo meas=%i,total=%i\n", MeasurementsCount, DeltaTime_vec.size());

            if (MeasurementsCount >= 1)
            {
                DeltaTimeMeanRobust /= MeasurementsCount;
            }
            else
            {
                // Calculation error - restart state machine
                TapTempoStateMachine = ttsmInit;
                break;
            }
            

            if (DeltaTimeMeanRobust > 0.01)
            {
                float TempoBPM = 60.0 / DeltaTimeMeanRobust;
                TContext * pContext;
                {
                    // Protect PlaylistPosition from concurrent access
                    std::lock_guard<std::mutex> lock(PlaylistPosition_mtx);
                    pContext = PlaylistPosition;
                }
                pContext->BaseTempo = round(TempoBPM);
            }
            else
            {
                // Calculation error - restart state machine
                TapTempoStateMachine = ttsmInit;
                break;
            }
        }
    }
}


extern "C" void showlist(void);
extern "C" int main_TODO(int argc, char const **argv, int Tempo);
extern "C" void seq_midi_tempo_direct(int Tempo);
extern "C" void pmidiStop(void);

unsigned int SequencerRunning = 0;

pthread_t thread_sequencer = 0;

namespace RigUp
{

void Init(void)
{
    // For a reason that eludes me, changing the variation of the part
    // must be sent twice...
    // We want to select the Sine Wave (program 81, variation 8)
    MIDI_A.SendProgramChange(2, 81);
    MIDI_A.SendControlChange(2, 0, 8);
    MIDI_A.SendProgramChange(2, 81);
    MIDI_A.SendControlChange(2, 0, 8);

    // Initialize the XV5080 performance
    ResetXV5080Performance();
}


void WhiteNoiseUniform(void)
{
    system("aplay ./wav/whitenoise_gaussian_distribution.wav &");
}

void WhiteNoiseGaussian(void)
{
    system("aplay ./wav/whitenoise_uniform_distribution.wav &");
}

int CurrentNote = 0;
void SineWaveOn(void)
{
    MIDI_A.SendNoteOnEvent(2, CurrentNote, 100);
}


void SineWaveOff(void)
{
    MIDI_A.SendNoteOffEvent(2, CurrentNote, 0);
}

void SineWavePitch(int ccValue)
{
    SineWaveOff();
    CurrentNote = ccValue;
    SineWaveOn();
}

}

#if 0
namespace All_In_You
{

    void Init(void)
    {
        // Bass lead on part on part 1, midi channel 1
        XV5080.TemporaryPerformance.PerformancePart[0].SelectPatch(TXV5080::PatchGroup::PR_D, 48); 
        XV5080.TemporaryPerformance.PerformancePart[0].ReceiveMIDI1.Set(1);
        XV5080.TemporaryPerformance.PerformancePart[0].ReceiveSwitch.Set(1);
        XV5080.TemporaryPerformance.PerformancePart[0].ReceiveChannel.Set_1_16(1);
//        XV5080.TemporaryPerformance.PerformancePart[1].PartOutputAssign.ToOutput1();
/*        XV5080.TemporaryPatchRhythm_InPerformanceMode[0].TemporaryPatch.PatchTone[0].  ToneRandomPanDepth.Set(0);
        XV5080.TemporaryPatchRhythm_InPerformanceMode[1].TemporaryPatch.PatchTone[1].ToneRandomPanDepth.Set(0);
        XV5080.TemporaryPatchRhythm_InPerformanceMode[1].TemporaryPatch.PatchTone[2].ToneRandomPanDepth.Set(0);
        XV5080.TemporaryPatchRhythm_InPerformanceMode[1].TemporaryPatch.PatchTone[3].ToneRandomPanDepth.Set(0);
        XV5080.TemporaryPatchRhythm_InPerformanceMode[1].TemporaryPatch.PatchTone[0].ToneAlternatePanDepth.Set(0);
        XV5080.TemporaryPatchRhythm_InPerformanceMode[1].TemporaryPatch.PatchTone[1].ToneAlternatePanDepth.Set(0);
        XV5080.TemporaryPatchRhythm_InPerformanceMode[1].TemporaryPatch.PatchTone[2].ToneAlternatePanDepth.Set(0);
        XV5080.TemporaryPatchRhythm_InPerformanceMode[1].TemporaryPatch.PatchTone[3].ToneAlternatePanDepth.Set(0);
*/
        // On this song, the bass lead is played from computer keyboard
        MiniSynth::octave = 2;
        MiniSynth::channel = 1;
        XV5080.System.SystemCommon.SystemControl1Source.Set(1); // Use CC01 as SYS-CTRL1 (mod)
        XV5080.System.SystemCommon.SystemControl2Source.Set(8); // Use CC08 as SYS-CTRL2 (filter cutoff)
        XV5080.System.SystemCommon.SystemControl3Source.Set(9); // Use CC09 as SYS-CTRL3 (resonance)

        // Our miniphaser (or whatever bass we use) tempo source should be the system tempo, not patch tempo
        XV5080.TemporaryPatchRhythm_InPerformanceMode[0].TemporaryPatch.PatchCommon.PatchClockSource.Set(1);
        // Switch to monophonic, much easier to play on a computer keyboard...
        XV5080.TemporaryPatchRhythm_InPerformanceMode[0].TemporaryPatch.PatchCommon.MonoPoly.Set(0);

        // We are in the key of "D" - adjust so that azertyuiop corresponds to that scale
        XV5080.TemporaryPatchRhythm_InPerformanceMode[0].TemporaryPatch.PatchCommon.PatchCoarseTune.Set(61); // -3 semitones = 64-3 = 61from C to A 

        // Now takle the soaring lead
        // On midi channel 2
        // Tied to the second analog pedal
//        XV5080.TemporaryPerformance.PerformancePart[1].SelectPatch(TXV5080::PatchGroup::PR_F, 11); // Square Roots
        XV5080.TemporaryPerformance.PerformancePart[1].SelectPatch(TXV5080::PatchGroup::PR_B, 4); // guitar
        XV5080.TemporaryPerformance.PerformancePart[1].ReceiveMIDI1.Set(1);
        XV5080.TemporaryPerformance.PerformancePart[1].ReceiveSwitch.Set(1);
        XV5080.TemporaryPerformance.PerformancePart[1].ReceiveChannel.Set_1_16(2);

        XV5080.TemporaryPerformance.PerformancePart[2].SelectPatch(TXV5080::PatchGroup::PR_G, 77); // guitar
        XV5080.TemporaryPerformance.PerformancePart[2].ReceiveMIDI1.Set(1);
        XV5080.TemporaryPerformance.PerformancePart[2].ReceiveSwitch.Set(1);
        XV5080.TemporaryPerformance.PerformancePart[2].ReceiveChannel.Set_1_16(2);


        XV5080.TemporaryPerformance.PerformancePart[3].ReceiveSwitch.Set(0);
        XV5080.TemporaryPerformance.PerformancePart[4].ReceiveSwitch.Set(0);
        XV5080.TemporaryPerformance.PerformancePart[5].ReceiveSwitch.Set(0);
        XV5080.TemporaryPerformance.PerformancePart[6].ReceiveSwitch.Set(0);
        XV5080.TemporaryPerformance.PerformancePart[7].ReceiveSwitch.Set(0);
        XV5080.TemporaryPerformance.PerformancePart[8].ReceiveSwitch.Set(0);
        XV5080.TemporaryPerformance.PerformancePart[9].ReceiveSwitch.Set(0);
        XV5080.TemporaryPerformance.PerformancePart[10].ReceiveSwitch.Set(0);
        XV5080.TemporaryPerformance.PerformancePart[11].ReceiveSwitch.Set(0);
        XV5080.TemporaryPerformance.PerformancePart[12].ReceiveSwitch.Set(0);
        XV5080.TemporaryPerformance.PerformancePart[13].ReceiveSwitch.Set(0);
        XV5080.TemporaryPerformance.PerformancePart[14].ReceiveSwitch.Set(0);
        XV5080.TemporaryPerformance.PerformancePart[15].ReceiveSwitch.Set(0);

    }

    void Filter(int Value)
    {
        using std::vector;
        vector<double> In_Pedal = {0, 64, 127}; // position of the pedal, from 0 to 127
        vector<double> Out_CC01 = {0, 0, 64}; // corresponding values of the control change event 99
        vector<double> Out_CC08 = {127, 0, 0}; // corresponding values of the control change event 99
        vector<double> Out_CC09 = {0, 32, 127}; // corresponding values of the control change event 99


        MIDI_A.SendControlChange(1, 1, interpolate(In_Pedal, Out_CC01, Value, false));        
        MIDI_A.SendControlChange(1, 8, interpolate(In_Pedal, Out_CC08, Value, false));        
        MIDI_A.SendControlChange(1, 9, interpolate(In_Pedal, Out_CC09, Value, false));        
    }

    void SoaringLead(int Value)
    {
        static enum TSoaringLeadStateMachine {smInit,smWaitStart, smStartSound, smSoaring} SoaringLeadStateMachine = smInit;
        // Callback from the second analog pedal
        switch (SoaringLeadStateMachine)
        {
            case smInit:
            // Wait until Value is less than 10
            if (Value < 10)
            {
                SoaringLeadStateMachine = smWaitStart;
            }
            break;

            case smWaitStart:
            if (Value > 15)
            {
                // Ok - start
                // Sound ON
                MIDI_A.SendNoteOnEvent(2, 45, 127); // Send dyad - note 1
                MIDI_A.SendNoteOnEvent(2, 57, 127); // Dyad - note 2, one octave higher
                MIDI_A.SendControlChange(2, 7, Value); // CC07 is volume
                SoaringLeadStateMachine = smSoaring;
            }
            break;

            case smSoaring:
            if ((Value <110) && (Value >= 10))
            {
                MIDI_A.SendControlChange(2, 7, Value); // CC07 is volume
            }
            else
            {
                // Sound off
                MIDI_A.SendNoteOffEvent(2, 45, 0);
                MIDI_A.SendNoteOffEvent(2, 57, 0);
                MIDI_A.SendControlChange(2, 7, 0); // CC07 is volume
                // Reset state machine
                SoaringLeadStateMachine = smInit;
            }
            break;

            default:
            SoaringLeadStateMachine = smInit;
            break;
        }
    }
}

namespace Rehab
{
    void Init(void)
    {
        // Bells on part 1, midi channel 4
        XV5080.TemporaryPerformance.PerformancePart[0].SelectPatch(TXV5080::PatchGroup::PR_F, 70); // Chime bell
        XV5080.TemporaryPerformance.PerformancePart[0].ReceiveMIDI1.Set(1);
        XV5080.TemporaryPerformance.PerformancePart[0].ReceiveSwitch.Set(1);
        XV5080.TemporaryPerformance.PerformancePart[0].ReceiveChannel.Set_1_16(4);
        //XV5080.TemporaryPerformance.PerformancePart[1].PartOutputAssign.ToOutput1();
        // This bell is nice, but it has a randomized panoramic effect that does not work
        // for us at all, since we're often in Mono, taking one single output (left or right)
        // This means the overall volume, in our mix, is randomly too low.
        // Change the pan radomization parameter for that patch, make it flat-center
        XV5080.TemporaryPatchRhythm_InPerformanceMode[0].TemporaryPatch.PatchTone[0].ToneRandomPanDepth.Set(0);
        XV5080.TemporaryPatchRhythm_InPerformanceMode[0].TemporaryPatch.PatchTone[1].ToneRandomPanDepth.Set(0);
        XV5080.TemporaryPatchRhythm_InPerformanceMode[0].TemporaryPatch.PatchTone[2].ToneRandomPanDepth.Set(0);
        XV5080.TemporaryPatchRhythm_InPerformanceMode[0].TemporaryPatch.PatchTone[3].ToneRandomPanDepth.Set(0);
        XV5080.TemporaryPatchRhythm_InPerformanceMode[0].TemporaryPatch.PatchTone[0].ToneAlternatePanDepth.Set(0);
        XV5080.TemporaryPatchRhythm_InPerformanceMode[0].TemporaryPatch.PatchTone[1].ToneAlternatePanDepth.Set(0);
        XV5080.TemporaryPatchRhythm_InPerformanceMode[0].TemporaryPatch.PatchTone[2].ToneAlternatePanDepth.Set(0);
        XV5080.TemporaryPatchRhythm_InPerformanceMode[0].TemporaryPatch.PatchTone[3].ToneAlternatePanDepth.Set(0);
    }

    void Bells1(void)
    {
        PlayNote(4, 40, 1, 127);
        PlayNote(4, 40 +12, 1, 127);
        
    }

    void Bells2(void)
    {
        PlayNote(4, 45, 1, 127);
        PlayNote(4, 45 +12, 1, 127);
    }

    void Bells3(void)
    {
        PlayNote(4, 41, 1, 127);
        PlayNote(4, 41 +12, 1, 127);
    }

    void Bells4(void)
    {
        PlayNote(4, 44, 1, 127);
        PlayNote(4, 44 +12, 1, 127);
    }
}
#endif



// This hook function is called whenever a Note ON event was received on
// MIDI A IN.
void MIDI_A_IN_NoteOnEvent(TInt_1_16 rxChannel, TInt_0_127 rxNote, TInt_0_127 rxVolume)
{
    if (rxChannel != 2)
    {
        // The FCB1010 should send Note ON on Channel 2
        return;
    }

    // If Channel 2 == FCB1010: action the corresponding digital pedal
    if (rxNote >= 1 && rxNote <= 10)
    {
        TContext * pContext;
        {
            // Protect PlaylistPosition from concurrent access
            std::lock_guard<std::mutex> lock(PlaylistPosition_mtx);
            pContext = PlaylistPosition;
        }

        if (pContext->Pedalboard.PedalsDigital.count(rxNote) == 1)
        {
            // That specific pedal number exists
            if (rxVolume > 0)
            {
                pContext->Pedalboard.PedalsDigital[rxNote].Press();
            }
            if (rxVolume == 0)
            {
                pContext->Pedalboard.PedalsDigital[rxNote].Release();
            }
        }
    }
}


// This hook function is called whenever a Note ON event was received on
// MIDI C IN.
// ************************************************************
// Retransmit MIDI information to the XV5080.
// ************************************************************
void MIDI_C_IN_NoteOnEvent(TInt_1_16 rxChannel, TInt_0_127 rxNote, TInt_0_127 rxVolume)
{
    // Forward notes to XV5080 Midi IN, plugged on MidiSport Midi OUT A
    MIDI_A.SendNoteOnEvent(MIDI_CHANNEL_ARTURIA, rxNote, rxVolume);
}


// This hook function is called whenever a Note OFF event was received on
// MIDI C IN.
void MIDI_C_IN_NoteOffEvent(TInt_1_16 rxChannel, TInt_0_127 rxNote, TInt_0_127 rxVolume)
{
    // rxVolume is probably already equal to zero. But we override this here to make
    // sure the note is turned OFF.
    //   rxVolume = 0;
    MIDI_A.SendNoteOffEvent(MIDI_CHANNEL_ARTURIA, rxNote, rxVolume);
}

// This hook function is called whenever a Pitch Bend event was received on
// MIDI C IN.
void MIDI_C_IN_PB_Event(TInt_1_16 const rxChannel, TInt_14bits const rxPitchBendChangeValue_param)
{
    MIDI_A.SendPitchBendChange(MIDI_CHANNEL_ARTURIA, rxPitchBendChangeValue_param);
}


/**
 * Focusing on a master keyboard: keep tabs on which notes are ON, and which are OFF.
 * This is an array of 128 queues, one for each note.
 * If the queue is empty, it means the corresponding note is OFF.
 * If the queue has something in it, it means the note is ON.
 * Most of the time, each queue will contain either one element, or nothing.
 * But in some special cases, it is possible to turn the same note ON several times.
 * That note must receive the "Note OFF" event as many times to turn it off.
 * This is why one queue per note is required, as opposed to a mere boolean array.
 */
//std::array<std::queue<bool>, 128> KeyboardNotesState;

/** This variable holds the number of notes currently in ON state, for the keyboard */
std::atomic<int> KeyboardNotesON_Count(0);


// This hook function is called whenever a Note ON event was received on
// MIDI B IN.
// ************************************************************
// Retransmit external Keyboard MIDI information to the XV5080.
// ************************************************************
void MIDI_B_IN_NoteOnEvent(TInt_1_16 rxChannel, TInt_0_127 rxNote, TInt_0_127 rxVolume)
{
    static TInt_1_16 KbdMidiChannelTx;
    KbdMidiChannelTx = MIDI_CHANNEL_MASTER_KBD_XV5080;


    if (rxNote >= 1 && rxNote <= 127)
    {
        // Keep tabs on how many notes are currently ON
        // That will be used to switch ON or OFF the MIDI Receive of specific parts, which
        // can be done only when all the notes are OFF (else, one part may receive more
        // Note ON events than Note OFF, which leaves unterminated notes on that part - very bad)
        KeyboardNotesON_Count ++;

        // Forward notes to XV5080 Midi IN, plugged on MidiSport Midi OUT A
        MIDI_A.SendNoteOnEvent(KbdMidiChannelTx, rxNote, rxVolume);
    }
}


template<typename T>
class ThreadSafeQueue
{
public:
    void push( const T& value )
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_queque.push(value);
    }

    void pop(void)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_queque.pop();
    }

    bool empty(void)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_queque.empty();
    }

    T front(void)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_queque.front();
    }


private:
    std::queue<T> m_queque;
    mutable std::mutex m_mutex;
};


// Queue of Performance Parts that should be muted.
ThreadSafeQueue<int> TPerformancePartToBeMuted;


void AdjustPerformancePartVolume_Mute()
{
   // Check whether all notes are OFF now
    if (KeyboardNotesON_Count == 0)
    {
        while (!TPerformancePartToBeMuted.empty())
        {
            XV5080.TemporaryPerformance.PerformancePart[TPerformancePartToBeMuted.front()].ReceiveSwitch.Set(0);
            TPerformancePartToBeMuted.pop();
        }
    }
    else
    {
        // There are still notes "ON" on the keyboard (be it one note or a chord). Don't turn off a Part
        // while notes are being played - that would result in dangling notes (unterminated notes that 
        // sound forever)
    }
}


// This hook function is called whenever a Note OFF event was received on
// MIDI B IN.
void MIDI_B_IN_NoteOffEvent(TInt_1_16 rxChannel, TInt_0_127 rxNote, TInt_0_127 rxVolume)
{
    TInt_1_16 KbdMidiChannelTx;
    KbdMidiChannelTx = MIDI_CHANNEL_MASTER_KBD_XV5080;

    MIDI_A.SendNoteOffEvent(KbdMidiChannelTx, rxNote, rxVolume);
    KeyboardNotesON_Count --;
    if (KeyboardNotesON_Count < 0)
    {
        wprintw(win_debug_messages.GetRef(), "/!\\ Negative count of KBD notes ON\n");
        KeyboardNotesON_Count = 0;
    }
    AdjustPerformancePartVolume_Mute();
}


void AdjustPerformancePartVolume(int VolumeValue, int PartIndex)
{
    // Volume for first part assigned to master keyboard
    if (VolumeValue < 5)
    {
        TPerformancePartToBeMuted.push(PartIndex);
        AdjustPerformancePartVolume_Mute();
    }
    else
    {
        XV5080.TemporaryPerformance.PerformancePart[VolumeValue].ReceiveSwitch.Set(1);
    }
    XV5080.TemporaryPerformance.PerformancePart[VolumeValue].PartLevel.Set(VolumeValue);
}


// This hook function is called whenever the master keyboard sends a Controller Change event
void MIDI_B_IN_CC_Event(TInt_1_16 const rxChannel, TInt_0_127 const rxControllerNumber, TInt_0_127 const rxControllerValue)
{
    // Put here code to handle CC events
    switch (rxChannel)
    {
    case 1:
        // This is the default value of the keyboard
        switch (rxControllerNumber)
        {
        case 73:
            // Volume for first part assigned to master keyboard
            if (rxControllerValue < 5)
            {
                XV5080.TemporaryPerformance.PerformancePart[MASTER_KBD_PART_INDEX].ReceiveSwitch.Set(0);
            }
            else
            {
                XV5080.TemporaryPerformance.PerformancePart[MASTER_KBD_PART_INDEX].ReceiveSwitch.Set(1);
            }
            XV5080.TemporaryPerformance.PerformancePart[MASTER_KBD_PART_INDEX].PartLevel.Set(rxControllerValue);
            break;

        case 75:
            // Volume for second part assigned to master keyboard
            if (rxControllerValue < 5)
            {
                XV5080.TemporaryPerformance.PerformancePart[MASTER_KBD_PART_INDEX+1].ReceiveSwitch.Set(0);
            }
            else
            {
                XV5080.TemporaryPerformance.PerformancePart[MASTER_KBD_PART_INDEX+1].ReceiveSwitch.Set(1);
            }
            XV5080.TemporaryPerformance.PerformancePart[MASTER_KBD_PART_INDEX+1].PartLevel.Set(rxControllerValue);
            break;

        case 79:
            // Volume for third part assigned to master keyboard
            if (rxControllerValue < 5)
            {
                XV5080.TemporaryPerformance.PerformancePart[MASTER_KBD_PART_INDEX+2].ReceiveSwitch.Set(0);
            }
            else
            {
                XV5080.TemporaryPerformance.PerformancePart[MASTER_KBD_PART_INDEX+2].ReceiveSwitch.Set(1);
            }

            XV5080.TemporaryPerformance.PerformancePart[MASTER_KBD_PART_INDEX+2].PartLevel.Set(rxControllerValue);
            break;

        case 72:
            // Volume for third part assigned to master keyboard
            if (rxControllerValue < 5)
            {
                XV5080.TemporaryPerformance.PerformancePart[MASTER_KBD_PART_INDEX+3].ReceiveSwitch.Set(0);
            }
            else
            {
                XV5080.TemporaryPerformance.PerformancePart[MASTER_KBD_PART_INDEX+3].ReceiveSwitch.Set(1);
            }

            XV5080.TemporaryPerformance.PerformancePart[MASTER_KBD_PART_INDEX+3].PartLevel.Set(rxControllerValue);
            break;

        case 80:
            // Volume for third part assigned to master keyboard
            if (rxControllerValue < 5)
            {
                XV5080.TemporaryPerformance.PerformancePart[MASTER_KBD_PART_INDEX+4].ReceiveSwitch.Set(0);
            }
            else
            {
                XV5080.TemporaryPerformance.PerformancePart[MASTER_KBD_PART_INDEX+4].ReceiveSwitch.Set(1);
            }

            XV5080.TemporaryPerformance.PerformancePart[MASTER_KBD_PART_INDEX+4].PartLevel.Set(rxControllerValue);
            break;

        case 81:
            // Volume for third part assigned to master keyboard
            if (rxControllerValue < 5)
            {
                XV5080.TemporaryPerformance.PerformancePart[MASTER_KBD_PART_INDEX+5].ReceiveSwitch.Set(0);
            }
            else
            {
                XV5080.TemporaryPerformance.PerformancePart[MASTER_KBD_PART_INDEX+5].ReceiveSwitch.Set(1);
            }

            XV5080.TemporaryPerformance.PerformancePart[MASTER_KBD_PART_INDEX+5].PartLevel.Set(rxControllerValue);
            break;

        case 82:
            // Volume for third part assigned to master keyboard
            if (rxControllerValue < 5)
            {
                XV5080.TemporaryPerformance.PerformancePart[MASTER_KBD_PART_INDEX+6].ReceiveSwitch.Set(0);
            }
            else
            {
                XV5080.TemporaryPerformance.PerformancePart[MASTER_KBD_PART_INDEX+6].ReceiveSwitch.Set(1);
            }

            XV5080.TemporaryPerformance.PerformancePart[MASTER_KBD_PART_INDEX+6].PartLevel.Set(rxControllerValue);
            break;

        case 83:
            // Volume for third part assigned to master keyboard
            if (rxControllerValue < 5)
            {
                XV5080.TemporaryPerformance.PerformancePart[MASTER_KBD_PART_INDEX+7].ReceiveSwitch.Set(0);
            }
            else
            {
                XV5080.TemporaryPerformance.PerformancePart[MASTER_KBD_PART_INDEX+7].ReceiveSwitch.Set(1);
            }

            XV5080.TemporaryPerformance.PerformancePart[MASTER_KBD_PART_INDEX+7].PartLevel.Set(rxControllerValue);
            break;

        case 85:
//            XV5080.TemporaryPerformance.PerformanceCommonReverb_.
            break;



        case 74: // first potentiometer
            // Reverb for first patch assigned to master keyboard
            if (rxControllerValue != 0)
            {
                XV5080.System.SystemCommon.ReverbSwitch.Set(1);
            }
            XV5080.TemporaryPerformance.PerformanceCommonReverb_.ReverbType_.Set(1);
            XV5080.TemporaryPerformance.PerformancePart[MASTER_KBD_PART_INDEX].PartReverbSendLevel.Set(rxControllerValue);
            XV5080.TemporaryPerformance.PerformanceCommonReverb_.ReverbLevel_.Set(rxControllerValue);
            XV5080.TemporaryPatchRhythm_InPerformanceMode[MASTER_KBD_PART_INDEX].TemporaryPatch.PatchCommonReverb.ReverbType.Set(1);
            XV5080.TemporaryPatchRhythm_InPerformanceMode[MASTER_KBD_PART_INDEX].TemporaryPatch.PatchCommonReverb.ReverbLevel.Set(rxControllerValue);
            break;

        default:
            // Forward CC events as they are
            MIDI_A.SendControlChange(rxChannel, rxControllerNumber, rxControllerValue);
            break;
        }
        break;

        // Do nothing
        // MIDI_A.SendControlChange(rxChannel, rxControllerNumber, rxControllerValue); // Just forward to XV5080
    }
}


void MIDI_B_IN_PB_Event(TInt_1_16 const rxChannel, TInt_14bits const rxPitchBendChangeValue_param)
{
    // Disregard rxChannel. Forward to XV5080 parts attributed to the master keyboard.
    MIDI_A.SendPitchBendChange(MIDI_CHANNEL_MASTER_KBD_XV5080, rxPitchBendChangeValue_param);
}

// This hook function is called whenever a Controller Change event is
// received on MIDI A IN
void MIDI_A_IN_CC_Event(TInt_1_16 const rxChannel, TInt_0_127 const rxControllerNumber, TInt_0_127 const rxControllerValue)
{
    TContext * pContext;
    {
        // Protect PlaylistPosition from concurrent access
        std::lock_guard<std::mutex> lock(PlaylistPosition_mtx);
        pContext = PlaylistPosition;
    }

    if (pContext->Pedalboard.PedalsAnalog.count((int) rxControllerNumber) == 1)
    {
        // This analog pedal exists
        pContext->Pedalboard.PedalsAnalog[rxControllerNumber].Change(rxControllerValue);
    }
}


// This function initializes all the TContext objects, and then gathers
// them all in a so-called PlaylistData object.
// You must manually edit the code in there to add new contexts (songs)
// or to alter the order of the playlist.
void InitializePlaylist(void)
{
    cFirstContext.Author = "---";
    cFirstContext.SongName = "---";

    cRigUp.Author = "_";
    cRigUp.SetInitFunc(RigUp::Init);
    cRigUp.SongName = "OXFORD RIG UP";
    cRigUp.Comments = "Miscellaneous tools for band rigup";
    cRigUp.Pedalboard.PedalsDigital[1] = TPedalDigital(RigUp::WhiteNoiseUniform, NULL, "White noise, uniform");
    cRigUp.Pedalboard.PedalsDigital[2] = TPedalDigital(RigUp::WhiteNoiseGaussian, NULL, "White noise, gaussian");
    cRigUp.Pedalboard.PedalsDigital[3] = TPedalDigital(RigUp::SineWaveOn, NULL, "Sine Wave ON");
    cRigUp.Pedalboard.PedalsDigital[4] = TPedalDigital(RigUp::SineWaveOff, NULL, "Sine Wave OFF");
    cRigUp.Pedalboard.PedalsAnalog[1] = TPedalAnalog(RigUp::SineWavePitch, "Adjust sine wave pitch");


    // PLAYLIST ORDER IS DEFINED HERE:
    // SETLIST BLANGY-SUR-BRESLE 19-JUN-2921
    PlaylistData.clear();
    PlaylistData.push_back(&cFirstContext); // Always keep that one in first
    PlaylistData.push_back(&cRigUp);

    // Set the current active context here.
    // By default: that would be PlaylistData.begin()...
    // Note that std::list cannot be accessed randomly.
    {
        // Protect PlaylistPosition from concurrent access
        std::lock_guard<std::mutex> lock(PlaylistPosition_mtx);
        PlaylistPosition = *(PlaylistData.begin());
    }

    // These so-called Playlists are a bit fictive and only a copy of the original playlist, but sorted by author or by song name.
    PlaylistData_ByAuthor = PlaylistData;
    PlaylistData_ByAuthor.sort(CompareTContextByAuthor);
    PlaylistData_BySongName = PlaylistData;
    PlaylistData_BySongName.sort(CompareTContextBySongName);
}

// Redraw screen 5 times a second.
void threadRedraw(void)
{
    TContext * pContext;
    while(1)
    {
        waitMilliseconds(200);
        {
            // Protect PlaylistPosition from concurrent access
            std::lock_guard<std::mutex> lock(PlaylistPosition_mtx);
            pContext = PlaylistPosition;
        }
        win_context_current.Erase();
        mvwprintw(win_context_current.GetRef(), 0,0, pContext->SongName.c_str());

        win_context_usage.Erase();

        for (auto element : pContext->Pedalboard.PedalsDigital)
        {
            wprintw(win_context_usage.GetRef(), "Digital Pedal %i: %s\n", element.first, element.second.GetComment().c_str());
        }

        for (auto element : pContext->Pedalboard.PedalsAnalog)
        {
            wprintw(win_context_usage.GetRef(), "Expression CC %i: %s\n", element.first, element.second.GetComment().c_str());
        }

        win_context_current.Refresh();
        win_context_next.Refresh();
        win_context_prev.Refresh();
        win_context_usage.Refresh();
        win_debug_messages.Refresh();
        win_midi_in.Refresh();
        win_midi_out.Refresh();
    }

}


void SelectContextByName(std::string Name)
{
    // Protect PlaylistPosition from concurrent access
    std::lock_guard<std::mutex> lock(PlaylistPosition_mtx);
    std::list<TContext*>::iterator it;
    std::list<TContext*> ContextList = PlaylistData;

    for (it = ContextList.begin(); it != ContextList.end(); it++)
    {
        TContext Context = **it;
        if (Context.SongName == Name)
        {
            // Initialize context for said song
            PlaylistPosition = *it;
            PlaylistPosition->Init();
        }
    }
}

void SelectContextInPlaylist (std::list<TContext*> &ContextList, bool ShowAuthor)
{
    // Protect PlaylistPosition from concurrent access
    std::lock_guard<std::mutex> lock(PlaylistPosition_mtx);
 
    /* Declare variables. */
    CDKSCREEN *cdkscreen = 0;
    CDKSCROLL *scrollList = 0;

    char **item = 0;
    const char *mesg[5];
    int selection;
    std::list<std::string> ListOfStrings;

    win_midi_in.Hide();
    win_midi_out.Hide();
    win_context_prev.Hide();
    win_context_current.Hide();
    win_context_next.Hide(); 
    win_debug_messages.Hide();
    win_context_usage.Hide();
    win_context_user_specific.Hide();
    win_context_select_menu.Hide();
    win_context_select_menu.PutOnTop();
    win_context_select_menu.Show();


    cdkscreen = initCDKScreen(win_context_select_menu.GetRef());

    // Populate scrolling list with songnames
    char  ** itemlist = (char  **) malloc(ContextList.size() * sizeof(char *));
    int idx = 0;
    int initial_position = 0; // this is the initial position highlighted in the menu list.
    std::list<TContext*>::iterator it;
    for (it = ContextList.begin(); it != ContextList.end(); it++)
    {
        TContext Context = **it;
        char * pMenuStr = (char *)malloc(Context.SongName.size()+1);
        strcpy(pMenuStr, Context.SongName.c_str());
        itemlist[idx] = (char *)(**it).SongName.c_str();

        if(ShowAuthor)
        {
            std::string tmpString;
            tmpString = Context.SongName + "(" + Context.Author + ")";
            ListOfStrings.push_back(tmpString);
            itemlist[idx] = (char *) ListOfStrings.back().c_str();
        }
        // Question: we are currently pointing to a context ( *PlaylistPosition ).
        // But which position is this in the list we will display?
        // Let's find out:
        if (*it == PlaylistPosition)
        {
            initial_position = idx;
        }
        idx++;
    }


    /* Create the scrolling list. */
    scrollList = newCDKScroll (cdkscreen, CENTER, CENTER, RIGHT, 20, 70, "Context selection", itemlist, idx, TRUE, A_REVERSE, TRUE, FALSE);


    /* Is the scrolling list null? */
    if (scrollList == 0)
    {
        /* Exit CDK. */
        destroyCDKScreen (cdkscreen);
        endCDK ();

        printf ("Cannot make scrolling list. Is the window too small?\n");
        return;
    }

    setCDKScrollPosition (scrollList, initial_position);

    /* Activate the scrolling list. */
    selection = activateCDKScroll (scrollList, 0);

    /* Determine how the widget was exited. */
    if (scrollList->exitType == vESCAPE_HIT)
    {
        mesg[0] = "<C>You hit escape. No file selected.";
        mesg[1] = "";
        mesg[2] = "<C>Press any key to continue.";
        popupLabel (cdkscreen, (CDK_CSTRING2) mesg, 3);
    }
    else if (scrollList->exitType == vNORMAL)
    {
        // Iterate through the list to set the correct Context.
        idx = 0;
        for (it = ContextList.begin(); it != ContextList.end(); it++)
        {
            if (idx == scrollList->currentItem)
            {
                PlaylistPosition = *it;
                PlaylistPosition->Init();
                break;
            }
            idx++;

        }
    }

    /* Clean up. */
    CDKfreeStrings (item);
    free(itemlist);
    destroyCDKScroll (scrollList);
    destroyCDKScreen (cdkscreen);
    endCDK ();

    // Show the main screen back.
    win_context_select_menu.Hide();
    win_midi_in.Show();
    win_midi_out.Show();
    win_context_prev.Show();
    win_context_current.Show();
    win_context_next.Show();
    win_debug_messages.Show();
    win_context_usage.Show();
    win_context_user_specific.Show();
}



/**
 * On the XV5080, reset the Parts of the Performance tied to the
 * MIDI keyboard to default Patches.
 */
void ResetXV5080Performance(void)
{
    XV5080.PerformanceSelect(TXV5080::PerformanceGroup::PR_A, TInt_1_128(1));
    XV5080.TemporaryPerformance.PerformancePart[0].ReceiveSwitch.Set(1);
    XV5080.TemporaryPerformance.PerformancePart[1].ReceiveSwitch.Set(0);
    XV5080.TemporaryPerformance.PerformancePart[2].ReceiveSwitch.Set(0);

    XV5080.TemporaryPerformance.PerformancePart[MASTER_KBD_PART_INDEX].ReceiveChannel.Set_1_16(MIDI_CHANNEL_MASTER_KBD_XV5080);
    XV5080.TemporaryPerformance.PerformancePart[MASTER_KBD_PART_INDEX].SelectPatch(TXV5080::PatchGroup::PR_A, 4); // Nice Piano

    XV5080.TemporaryPerformance.PerformancePart[MASTER_KBD_PART_INDEX+1].ReceiveChannel.Set_1_16(MIDI_CHANNEL_MASTER_KBD_XV5080);
    XV5080.TemporaryPerformance.PerformancePart[MASTER_KBD_PART_INDEX+1].SelectPatch(TXV5080::PatchGroup::PR_C, 59); // Warmth

    XV5080.TemporaryPerformance.PerformancePart[MASTER_KBD_PART_INDEX+2].ReceiveChannel.Set_1_16(MIDI_CHANNEL_MASTER_KBD_XV5080);
    XV5080.TemporaryPerformance.PerformancePart[MASTER_KBD_PART_INDEX+2].SelectPatch(TXV5080::PatchGroup::PR_E, 55); // Ethereal Strings

    //TXV5080::PatchGroup::PR_C, 36); // Warm Strings

    XV5080.TemporaryPerformance.PerformancePart[MASTER_KBD_PART_INDEX+3].ReceiveChannel.Set_1_16(MIDI_CHANNEL_MASTER_KBD_XV5080);
    XV5080.TemporaryPerformance.PerformancePart[MASTER_KBD_PART_INDEX+3].SelectPatch(TXV5080::PatchGroup::PR_E, 35); // Rocker Organ

    XV5080.TemporaryPerformance.PerformancePart[MASTER_KBD_PART_INDEX+4].ReceiveChannel.Set_1_16(MIDI_CHANNEL_MASTER_KBD_XV5080);
    XV5080.TemporaryPerformance.PerformancePart[MASTER_KBD_PART_INDEX+4].SelectPatch(TXV5080::PatchGroup::PR_E, 14); // Rhodes tremolo

    XV5080.TemporaryPerformance.PerformancePart[MASTER_KBD_PART_INDEX+5].ReceiveChannel.Set_1_16(MIDI_CHANNEL_MASTER_KBD_XV5080);
    XV5080.TemporaryPerformance.PerformancePart[MASTER_KBD_PART_INDEX+5].SelectPatch(TXV5080::PatchGroup::PR_E, 98); // New R&R Brass

    XV5080.TemporaryPerformance.PerformancePart[MASTER_KBD_PART_INDEX+6].ReceiveChannel.Set_1_16(MIDI_CHANNEL_MASTER_KBD_XV5080);
    XV5080.TemporaryPerformance.PerformancePart[MASTER_KBD_PART_INDEX+6].SelectPatch(TXV5080::PatchGroup::PR_F, 75); // Andreas' Cave

    XV5080.TemporaryPerformance.PerformancePart[MASTER_KBD_PART_INDEX+7].ReceiveChannel.Set_1_16(MIDI_CHANNEL_MASTER_KBD_XV5080);
    XV5080.TemporaryPerformance.PerformancePart[MASTER_KBD_PART_INDEX+7].SelectRhythmSet(TXV5080::RhythmSetGroup::CD_A, 1);


    XV5080.TemporaryPerformance.PerformanceCommon.PerformanceName.Set("OXFORD      ");

    // All samples are located in a single Rhythm Set.
    // The samples are read from the XV5080 memory card to the internal RAM.
    // Then, Drum Kit CD-A:001 is supposed to play those samples.
    // Add Drum Kit CD-A:001 to the temporary performance, make sure MIDI Rx is ON
    // Let's put it on Part 16 (numbered 15 below).
    // Let's set Part 16 MIDI Rx channel to be 16 too.
    // So to play a sample, just send midi nodes to MIDI channel 16.
    XV5080.TemporaryPerformance.PerformancePart[15].SelectRhythmSet(TXV5080::RhythmSetGroup::CD_A, 1);
    XV5080.TemporaryPerformance.PerformancePart[15].ReceiveMIDI1.Set(1);
    XV5080.TemporaryPerformance.PerformancePart[15].ReceiveSwitch.Set(1);
    XV5080.TemporaryPerformance.PerformancePart[15].ReceiveChannel.Set_1_16(16);


#if 0
    // BASS SYNTH - from the B2M module, as an insert loop in the Avid Eleven Rack
    // Part #15 (identified as 14 below) will receive data with MIDI channel MIDI_CHANNEL_BASS_SYNTH
    XV5080.TemporaryPerformance.PerformancePart[14].SelectPatch(TXV5080::PatchGroup::PR_A, TInt_1_128(40)); // This is the default patch used for the bass synth
    XV5080.TemporaryPerformance.PerformancePart[14].ReceiveMIDI1.Set(1);
    XV5080.TemporaryPerformance.PerformancePart[14].ReceiveSwitch.Set(1);
    XV5080.TemporaryPerformance.PerformancePart[14].ReceiveChannel.Set_1_16(MIDI_CHANNEL_BASS_SYNTH);
#endif
    // Part #14 (identified as 13 below) is the Drum part - including the metronome click
    // That part must be played MONO through output 8 (this goes to our drummer's ears only)
    // It is *also* configured to play on MIDI channel 14.
    XV5080.TemporaryPerformance.PerformancePart[13].SelectRhythmSet(TXV5080::RhythmSetGroup::PR_A, TInt_1_128(1));
    XV5080.TemporaryPerformance.PerformancePart[13].ReceiveMIDI1.Set(1);
    XV5080.TemporaryPerformance.PerformancePart[13].ReceiveSwitch.Set(1);
    XV5080.TemporaryPerformance.PerformancePart[13].ReceiveChannel.Set_1_16(14);
    XV5080.TemporaryPerformance.PerformancePart[13].PartOutputAssign.ToOutput8();


}


int main(int argc, char** argv)
{
    int term_lines, term_cols;

#if 1
    InitializePlaylist();

    initscr();
    curs_set(0);
    term_lines = LINES;
    term_cols = COLS;
    if (can_change_color() == TRUE)
    {
        start_color();
        init_color(COLOR_BLACK, 0, 0, 200);
        init_color(COLOR_WHITE, 1000, 1000, 0);
    }
    else
    {
        printf("CANNOT SUPPORT COLORS");
    }
    cbreak(); // Disable line buffering, pass on all data
    keypad(stdscr, TRUE); // support F- keys
    nodelay(stdscr, TRUE); // ncurses handles the keyboard in a non-blocking manner
    noecho();


    win_midi_in.Init("IN", term_lines -3, 6, 3, term_cols-6-6);
    win_midi_out.Init("OUT", term_lines -3, 6, 3, term_cols-6);
    win_context_prev.Init("CONTEXT PREV", 3, 0.33*term_cols, 0, 0);
    win_context_current.Init("CONTEXT CURRENT", 3, 0.33*term_cols, 0, 0.33*term_cols +1);
    win_context_next.Init("CONTEXT NEXT", 3, 0.33*term_cols, 0, 0.33*term_cols +1 + 0.33*term_cols +1);
    win_debug_messages.Init("DEBUG MESSAGES", term_lines - 3 -1 -(term_lines -3)/2, term_cols -6-6, (term_lines -3)/2 +3, 0);
    win_context_usage.Init("CONTEXT USAGE", (term_lines -3)/2, (term_cols -6-6)/2, 3, 0);
    win_context_user_specific.Init("CONTEXT SPECIFIC", (term_lines -3)/2, (term_cols -6-6)/2, 3, (term_cols -6-6)/2);
    win_context_select_menu.Init("CONTEXT SELECTION MENU", term_lines, term_cols, 0, 0);
    win_context_select_menu.Hide();

    wprintw(win_debug_messages.GetRef(), "Terminal LINES: %i, COLUMNS: %i\n", term_lines, term_cols);
    attron(A_BOLD | A_REVERSE);
    mvprintw(term_lines-1, 0, "[SPACE]");
    attroff(A_BOLD | A_REVERSE);
    printw(":Select by playlist ");
    attron(A_BOLD + A_REVERSE);
    printw("[b]");
    attroff(A_BOLD | A_REVERSE);
    printw(":by song name");
    attron(A_BOLD + A_REVERSE);
    printw("[n]");
    attroff(A_BOLD | A_REVERSE);
    printw(":by artist");
    attron(A_BOLD + A_REVERSE);
    printw("[v]");
    attroff(A_BOLD | A_REVERSE);
    printw(":tempo flash");
    refresh();

    // Initialize this banner at the full terminal width
    Banner.Init(term_cols, term_lines/2 -4, 0, 800);
    Banner.SetMessage("SYNTH HEAVEN");

    // Query ALSA for the MIDI names (identifiers) of MIDI hardware
    device_list();

    // Create thread that scans midi messages
    // Initialize MIDI port A - this will spawn a new thread that parses MIDI IN
    MIDI_A.Init(name_midi_hw_MIDISPORT_A, MIDI_A_IN_NoteOnEvent, NULL, MIDI_A_IN_CC_Event, NULL);

    // Same for MIDI port B
    MIDI_B.Init(name_midi_hw_MIDISPORT_B, MIDI_B_IN_NoteOnEvent, MIDI_B_IN_NoteOffEvent, MIDI_B_IN_CC_Event, MIDI_B_IN_PB_Event);

    // Same for MIDI port C
    MIDI_C.Init(name_midi_hw_MIDISPORT_C, MIDI_C_IN_NoteOnEvent, MIDI_C_IN_NoteOffEvent, NULL, MIDI_C_IN_PB_Event);

    // Create task that redraws screen at fixed intervals
    std::thread thread2(threadRedraw);

    // Create the thread that refreshes the metronome
    std::thread thread3(MetronomeMaster::threadMetronome);

    ComputerKeyboard::Initialize();

    // Create thread that scans the keyboard
    std::thread thread4(MiniSynth::threadKeyboard);

    // Setup the Master Keyboard default patches on each part
    std::this_thread::sleep_for(std::chrono::seconds(1));

    ResetXV5080Performance();

#ifdef TEST_XV5080
    test_XV5080();
#endif

#endif

    // Do nothing
    while(1)
    {
        sleep(1);

    }
}


