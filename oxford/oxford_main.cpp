// Hardware:
// Behringer FCB1010 Midi Out       == connected to ==   MidiSport port A In
// Roland XV5080 Midi In 1          == connected to ==   MidiSport port A Out
// Arturia Master Keyboard Midi Out == connected to ==   MidiSport port B In
// Eleven Rack (11R) Midi In        == connected to ==   MidiSport port B Out
// MidiSport USB                    == connected to ==   Raspberry Pi USB
// Sonuus B2M                       == connected to ==   ???

// Assuming that ALSA is used throughout.
// Make sure you have installed libasound2-dev, to get the headers
// Make sure you also have ncurses: libncurses5-dev libncursesw5-dev
//
// The USB midiman MidiSport 2x2 hardware requires midisport-firmware: install it with
// apt-get install midisport-firmware
//
// use amidi -l to list midi hardware devices
// don't forget to link with asound, pthread, cdk (sometimes called libcdk), and panel
//
// use pmidi -l to list midi devices for pmidi, which is for midi file playback
// you probably must build cdk locally, get in the cdk folder, ./configure, then make,
// and make sure that code blocks links with the library generated in ???
// run ldconfig as root
// you must also have the aubio library; go to aubio, then type make.
// Navigate to the library (normally aubio-xxxx/build/src) and type ldconfig $(pwd)
// to add that path to the library search.
// [[OOLLDD: ./waf configure, ./waf build, ./waf install is not really needed,
//  just have codeblocks link with the aubio library generated in: build/source/libaubio.so]]

// If running into issues with missing libraries, 3 options:
// 1/ make sure the library is in a "common" place with all other system libraries
// 2/ /lib/ld-linux.so.2 --library-path PATH EXECUTABLE
// 3/ export LD_LIBRARY_PATH=/usr/local/lib



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
#include <algorithm>
#include <cdk.h>
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

static int const MASTER_KBD_PART_INDEX = 3; // Master Keybard talks to parts 4 and up on XV5080

// Midi Channel, on XV5080, that receives MIDI traffic from the master keyboard
// Normally set to 2. This program forwards all master keyboard traffic to channel 2
// on the XV5080. On the XV5080 side, the parts that are link to the master keyboard
// all listen to channel 2.
static int const MASTER_KBD_XV5080_MIDI_CHANNEL = 2;


int stop = 0;

// midi sequencer name
// obtained with pmidi -l
const std::string midi_sequencer_name = "20:0";

// String name of the hardware device for the first midi port (IN1/OUT1)
// obtained with amidi -l
const std::string name_midi_hw_MIDISPORT_A = "hw:1,0,0";

// String name of the hardware device for the first midi port (IN2/OUT2)
// obtained with amidi -l
const std::string name_midi_hw_MIDISPORT_B = "hw:1,0,1";

// Mutex used to prevent ncurses refresh routines from being called from
// concurrent threads.
std::mutex ncurses_mutex;

// Start threads with top priority
#define REALTIME



// Test XV5080 class
#undef TEST_XV5080

// Test TSequence class
#define TEST_TSEQUENCE

#define TEST_TSEQUENCE_DEBUG

#undef MIDI_KEYBOARD_CONTROLS_ON_KEYS



typedef subrange::subrange<subrange::ordinal_range<int, 0, 127>, subrange::saturated_arithmetic> TInt_0_127;
typedef subrange::subrange<subrange::ordinal_range<int, 1, 16>, subrange::saturated_arithmetic> TInt_1_16;
typedef subrange::subrange<subrange::ordinal_range<int, 1, 128>, subrange::saturated_arithmetic> TInt_1_128;
typedef subrange::subrange<subrange::ordinal_range<int, 0, 16383>, subrange::saturated_arithmetic> TInt_14bits;
typedef subrange::subrange<subrange::ordinal_range<int, 1, 4>, subrange::saturated_arithmetic> TInt_1_4;


#if 0

std::ostream &operator <<(std::ostream &stream, IntRange ir)
{
    cout << ir.GetValue();
    return stream;
}

#endif // 0



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
    };

    void SetMessage(std::string message_param)
    {
        // Avoid concurrency issues
        std::lock_guard<std::mutex> lock(m);
        // Change whole string to UPPER characters
        message = StringToUpper(message_param);
        // Prepend and append spaces
        message.insert(0, "   ");
        message.insert(message.end(), 3, ' ');
        message_size = message.size();
        offset_max = (message_size - char_displayed_on_canvas) * char_width;
        if (offset_max <= 0)
        {
            offset_max = 1;
        }
        offset = 0;
        pBoxedWindow->Show();
        ExecuteAfterTimeout(PutBannerOnTop, 100, pBoxedWindow);
        /*void * pVoid = pBoxedWindow;
               TBoxedWindow * pBoxedWindow_local;
        pBoxedWindow_local  = reinterpret_cast<TBoxedWindow*>(pVoid);
        pBoxedWindow_local->PutOnTop();*/
    }

private:
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
    std::string message;
    int message_size;
    int offset_max;
    int display_time_ms;
    int offset;

    static void PutBannerOnTop(void* pVoid)
    {
        TBoxedWindow * pBoxedWindow_local;
        pBoxedWindow_local  = reinterpret_cast<TBoxedWindow*>(pVoid);
        pBoxedWindow_local->PutOnTop();
    }

    std::string StringToUpper(std::string strToConvert)
    {
        std::transform(strToConvert.begin(), strToConvert.end(), strToConvert.begin(), ::toupper);

        return strToConvert;
    }

    void BannerProcess(void)
    {
        // Avoid concurrency issues
        std::lock_guard<std::mutex> lock(m);

        if (offset <= offset_max)
        {
            print_big();
            offset++;
        }
        else
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(display_time_ms));
            pBoxedWindow->Hide();
        }
    }

    static void BannerThread(TBanner * pBanner)
    {
        while(1)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(40));
            pBanner->BannerProcess();
        }
    }

    void print_big(void)
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

namespace I_Follow_Rivers
{
void Sequence_1_Start_PedalPressed(void);
void Sequence_1_Start_PedalReleased(void);
}


namespace ElevenRack
{
void DistON(void);
void DistOFF(void);
void DistToggle(void);
void Init(void);
void ModON(void);
void ModOFF(void);
void ModToggle(void);
void WahON(void);
void WahOFF(void);
void WahToggle(void);
void WahSetValue(int);
}


// There is an ugly hack, whereby we need some #defines located in two different headers,
// but some of them are redefined differently in each header...
// The key mapping we need is contained in input-event-codes.h. Any other "source" of keymaps
// is not what we look for.
#include <linux/input.h>
#undef _INPUT_EVENT_CODES_H
#include <linux/input-event-codes.h>
namespace ComputerKeyboard
{

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
    int ControllerNumber;
    void (* OnChange)(int);
    std::string Comment;

public:
    TPedalAnalog(int ControllerNumber_param, void (*Function1_param)(int), std::string Comment_param)
    {
        ControllerNumber = 0;
        OnChange = NULL;
        if (ControllerNumber_param < 1 || ControllerNumber_param > 127)
        {

            wprintw(win_debug_messages.GetRef(), "TPedalAnalog: wrong parameter");
            return;
        }
        ControllerNumber = ControllerNumber_param;
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

    int GetControllerNumber(void)
    {
        return ControllerNumber;
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
    int Number;
    void (* OnPress)(void);
    void (* OnRelease)(void);
    std::string Comment;

public:
    TPedalDigital(int Number_param, void (*Function1_param)(void), void (*Function2_param)(void), std::string Comment_param)
    {
        Number = 0;
        OnPress = NULL;
        OnRelease = NULL;
        if (Number_param < 1 || Number_param > 10)
        {

            wprintw(win_debug_messages.GetRef(), "TPedalDigital: wrong parameter");
            return;
        }
        Number = Number_param;
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

    int GetNumber(void)
    {
        return Number;
    }

    std::string GetComment(void)
    {
        return Comment;
    }
};




/**
 * A pedalboard is a collection of digital and analog pedals.
 * In the case of the Behringer FCB1010, there are 10 digital pedals
 * and 2 analog pedals.
 */
class TPedalboard
{
public:
    std::list<TPedalDigital> PedalsDigital;
    std::list<TPedalAnalog> PedalsAnalog;
    TPedalboard(void)
    {
        // By default: we reserve Digital pedals associated to Midi Note On 6 and 7 (so, if mapped one-to-one
        // with the FCB1010 numbers written on the pedals, the two pedals at the left of the top row), to
        // the context switch actions, i.e. move from one context to the following context, as listed in object
        // PlaylistData. In short, Pedal 6 goes back one song, Pedal 7 goes to the next song.
        PedalsDigital.push_back(TPedalDigital(6, ContextPreviousPress, ContextPreviousRelease, "Playlist: previous song"));
        PedalsDigital.push_back(TPedalDigital(7, ContextNextPress, ContextNextRelease, "Playlist: next song"));

        // Let's also reserve more pedals to control the Rack Eleven:
        // Pedal 8 for distortion
        // Pedal 9 for mod
        // Pedal 10 for wah
        // Analog pedal 2 for wah value
        PedalsDigital.push_back(TPedalDigital(8, ElevenRack::DistToggle, NULL, "11R DIST"));
        PedalsDigital.push_back(TPedalDigital(9, ElevenRack::ModToggle, NULL, "11R MOD"));
        PedalsDigital.push_back(TPedalDigital(10, ElevenRack::WahToggle, NULL, "11R WAH"));
        PedalsAnalog.push_back(TPedalAnalog(2, ElevenRack::WahSetValue, "11R WAH VAL"));
    }
};


void ResetKeyboardPerformance(void);

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

public:
    std::string Author; // used to search by Author
    std::string SongName; // used to search by Song Name
    std::string Comments; // used to display various info, cheat sheet, and so on.
    float BaseTempo; // base tempo of the song. The metronome always pulse at the tempo of the active context.
    TPedalboard Pedalboard;
    void Init(void)
    {
        // Upon a context change, always first re-initialize the Eleven Rack
        ElevenRack::Init();
        // Initialize the XV5080 performance
        ResetKeyboardPerformance();

        if(InitFunc != NULL)
        {
            // Then call the user-defined initialization function
            InitFunc();
        }
        Banner.SetMessage(SongName);
    }
    void SetInitFunc( void (*InitFunc_param)(void))
    {
        InitFunc = InitFunc_param;
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
 * This is a list::iterator, so dereferencing it (e.g. *PlaylistPosition)
 * gives an element, that is a pointer to a Context. Then dereference another
 * time to get to the context.
 * E.g: TContext MyContext;
 *      MyContext = **PlaylistPosition;
 *      printf("%s", MyContext.SongName.c_str());
 */
std::list<TContext *>::iterator PlaylistPosition;

/**
 * Here, the actual TContext objects are instantiated. One for each context, which
 * more or less represents one for each *song* in the playlist, in the band/musical sense.
 */
TContext cFirstContext;
TContext cRigUp;
TContext cAveMaria;
TContext cCapitaineFlam;
TContext cWildThoughts;
TContext cGangstaParadise;
TContext cBeatIt;
TContext cFlyMeToTheMoon;
TContext cAllOfMe;
TContext cCryMeARiver;
TContext cJustAGigolo;
TContext cSuperstition;
TContext cStandByMe;
TContext cGetBack;
TContext cAllShookUp;
TContext cBackToBlack;
TContext cUnchainMyHeart;
TContext cFaith;
TContext cIsntSheLovely;
TContext cJammin;
TContext cRehab;
TContext cIFeelGood;
TContext cPapasGotABrandNewBag;
TContext cLongTrainRunning;
TContext cMasterBlaster;
TContext cAuxChampsElysees;
TContext cProudMary;
TContext cMonAmantDeSaintJean;
TContext cAroundTheWorld;
TContext cGetLucky;
TContext cIllusion;
TContext cDockOfTheBay;
TContext cLockedOutOfHeaven;
TContext cWhatsUp;
TContext cLesFillesDesForges;
TContext cThatsAllRight;
TContext cJohnnyBeGood;
TContext cBebopALula;
TContext cUptownFunk;
TContext cLeFreak;
TContext cRappersDelight;
TContext cMachistador;
TContext cAnotherOneBiteTheDust;
TContext cWot;
TContext cKnockOnWood;
TContext cHotelCalifornia;
TContext cRaggamuffin;
TContext cManDown;
TContext cShouldIStay;
TContext cMercy;
TContext cLady;
TContext cLesCitesDOr;
TContext cHuman;
TContext cNewYorkAvecToi;
TContext cBillieJean;
TContext cILoveRockNRoll;
TContext cHighwayToHell;
TContext c25years;
TContext cAllumerLeFeu;
TContext cChandelier;
TContext cAllInYou;
TContext cThisGirl;
TContext cEncoreUnMatin;
TContext cQuandLaMusiqueEstBonne;
TContext cDumbo;
TContext cIFollowRivers;
TContext cIsThisLove;
TContext cPeople;
TContext cTakeOnMe;

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
    if (PlaylistPosition != PlaylistData.begin())
    {
        PlaylistPosition--;
        TContext Context = **PlaylistPosition;
        Context.Init();
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
    // Note that .end() returns an iterator that is already outside the bounds
    // of the container.
    std::list<TContext*>::iterator it;
    it = PlaylistPosition;
    it++;
    if (it != PlaylistData.end())
    {
        PlaylistPosition++;
        TContext Context = **PlaylistPosition;
        Context.Init();
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
        wprintw(win_debug_messages.GetRef(), "Note ON number: %i\n", (int) charArray[1]);

        wprintw(win_midi_out.GetRef(), "%i\n%i\n%i\n", (int) charArray[0], (int) charArray[1], (int) charArray[2]);
        if (handle_midi_hw_out != 0)
        {
            snd_rawmidi_write(handle_midi_hw_out, &charArray, sizeof(charArray));
        }
    }


    void SendNoteOnEvent(TPlayNoteMsg * PlayNoteMsg)
    {
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
            snd_rawmidi_write(handle_midi_hw_out, &charArray, sizeof(charArray));
        }
    }

    void SendNoteOffEvent(TPlayNoteMsg * PlayNoteMsg)
    {
        SendNoteOffEvent(PlayNoteMsg->Channel, PlayNoteMsg->NoteNumber, PlayNoteMsg->Velocity);
    }

    void SendProgramChange(unsigned char Channel, unsigned char Program)
    {
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
            snd_rawmidi_write(handle_midi_hw_out, &charArray, sizeof(charArray));
        }
    }

    // Send out a CC (Control Change) MIDI event.
    void SendControlChange(unsigned char Channel, unsigned char ControlNumber,
                           unsigned char ControllerValue)
    {
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
            snd_rawmidi_write(handle_midi_hw_out, &charArray, sizeof(charArray));
        }
    }


    // Send out a PB (Pitch Bend Change) MIDI event.
    void SendPitchBendChange(unsigned char Channel, TInt_14bits PitchBendChangeValue)
    {
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
            snd_rawmidi_write(handle_midi_hw_out, &charArray, sizeof(charArray));
        }
    }

    // Send out raw midi data
    void SendRawData(std::vector<unsigned char> data)
    {
        for (auto x : data)
        {
            wprintw(win_midi_out.GetRef(), "%02x\n", (int) x);
        }

        if (handle_midi_hw_out != 0)
        {
            snd_rawmidi_write(handle_midi_hw_out, &data[0], data.size());
        }
    }

    // Talk to ALSA and open a midi port in RAW mode, for data going OUT.
    // (from this program, out of the computer, to the expander, to the Rack Eleven, etc.)
    void StartRawMidiOut(void)
    {
        if (handle_midi_hw_out == 0)
        {
            int err = snd_rawmidi_open(NULL, &handle_midi_hw_out, name_midi_hw.c_str(), 0);
            if (err)
            {
                wprintw(win_debug_messages.GetRef(), "snd_rawmidi_open %s failed: %d\n", name_midi_hw.c_str(), err);
            }
        }
    }

    // Close the RAW midi OUT port, for port "portnum" (0 or 1)
    void StopRawMidiOut(void)
    {
        if(handle_midi_hw_out != 0)
        {
            snd_rawmidi_drain(handle_midi_hw_out);
            snd_rawmidi_close(handle_midi_hw_out);
            handle_midi_hw_out = 0;
        }
    }

private:
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

    // Talk to ALSA and open a midi port in RAW mode, for data going IN.
    // (into the computer, into this program, from the pedalboard, from the keyboard...)
    void StartRawMidiIn(void)
    {
        if (handle_midi_hw_in == 0)
        {
            // SND_RAWMIDI_SYNC enforces that all calls to snd_rawmidi_write are atomic (i.e. automatically flushed)
            int err = snd_rawmidi_open(&handle_midi_hw_in, NULL, name_midi_hw.c_str(), SND_RAWMIDI_SYNC);
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
        struct sched_param param;
        param.__sched_priority = sched_get_priority_max(SCHED_RR);
        //https://linux.die.net/man/3/pthread_setschedparam
        int s = pthread_setschedparam(pthread_self(), SCHED_RR, &param);
        if (s != 0)
        {
            wprintw(win_debug_messages.GetRef(), "MIDI: error initializing thread priority, returned %i\n", s);
        }
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
                    if ( ( (ch) & 0xF0 ) == 0xe0 && HookProcessControllerChangeEvent)
                    {
                        // It's a Pitch Bend Change event.
                        // The channel is encoded offset by one. Bring back the value in the 0-16 range (+1 below)
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
                    HookProcessNoteONEvent(rxChannel, rxNote, rxVolume);
                }
                stateMachine = smWaitMidiChar1;
                break;

                case smProcessNoteOffEvent:
                {
                    HookProcessNoteOFFEvent(rxChannel, rxNote, rxVolume);
                }
                stateMachine = smWaitMidiChar1;
                break;

                case smProcessControllerChange:
                {
                    HookProcessControllerChangeEvent(rxChannel, rxControllerNumber, rxControllerValue);
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

    enum class PatchGroup {USER, PR_A, PR_B, PR_C, PR_D, PR_E, PR_F, PR_G,
                        CD_A, CD_B, CD_C, CD_D, CD_E, CD_F, CD_G, CD_H,
                        XP_A, XP_B, XP_C, XP_D, XP_E, XP_F, XP_G, XP_H};

    enum class RhythmSetGroup {USER, PR_A, PR_B, PR_C, PR_D, PR_E, PR_F, PR_G,
                        CD_A, CD_B, CD_C, CD_D, CD_E, CD_F, CD_G, CD_H};

//                        enum RythmSetGoup {USER, PR_A, PR_B, PR_C};
    enum class PerformanceGroup {USER, PR_A, PR_B,
                        CD_A, CD_B, CD_C, CD_D, CD_E, CD_F, CD_G, CD_H};


    /**
     * Switch to performance mode and jump to a specific performance
     */
    void PerformanceSelect(TXV5080::PerformanceGroup PerformanceGroup_param, TInt_1_128 const PatchNumber_param)
    {
        System.SystemCommon.SoundMode.Perform();
//        System.SystemCommon.PerformanceControlChannel.Set(1);
        switch (PerformanceGroup_param)
        {
        case PerformanceGroup::USER:
            // From user's manual page 21
//            pMIDI_Port.SendControlChange(1, 0, 85);
//            pMIDI_Port.SendControlChange(1, 32, 0);
            System.SystemCommon.PerformanceBankSelectMSB.Set(85);
            System.SystemCommon.PerformanceBankSelectLSB.Set(0);
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

/**
Definition of the TScaleName type
*/
typedef std::string TScaleName;



/**
This class represents a music note
*/
class TNote__
{
public:
    TNote__(std::string const NameUS_param,
                std::string const NameEuropean_param,
                float Frequency_param,
                int MidiNote_param)
    {
        NameUS = NameUS_param;
        NameEuropean = NameUS_param;
        Frequency = Frequency_param;
        MidiNote = MidiNote_param;
    };

    operator int()
    {
        return static_cast<int>(MidiNote);
    }

    operator std::string()
    {
        return "blah";
    }

    operator TInt_0_127()
    {
        return MidiNote;
    }

private:
    static std::map<TScaleName, std::string[]> NoteNamesUS;
//    static std::map<int,std::string> map_MidiNoteNumber__NameUS = {0, "C-2}

    std::string NameUS;
    std::string NameEuropean;
    float Frequency;
    TInt_0_127 MidiNote;

    void Test(void)
    {
        TNote__ testNote("B", "Si", 1234, 20);
    }

};

#if 0


static std::map<TScaleName, std::vector<std::string>> NoteNamesUS =
{
    {"C", {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"}},
    {"Db", {"Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "Cb", "C"}},
    {"Dmaj", {"D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B", "C", "C#"}},
    {"Eb", {"Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "Cb", "C", "Db", "D"}},
    {"E", {"E", "F", "Gb", "G", "Ab", "A", "Bb", "Cb", "C", "C", "Db", "D"}},
    {"Eb", {"Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "Cb", "C", "Db", "D"}},
    {"Eb", {"Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "Cb", "C", "Db", "D"}},
    {"Eb", {"Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "Cb", "C", "Db", "D"}},
    {"Eb", {"Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "Cb", "C", "Db", "D"}},
    {"Eb", {"Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "Cb", "C", "Db", "D"}},
    {"Eb", {"Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "Cb", "C", "Db", "D"}},
    {"Eb", {"Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "Cb", "C", "Db", "D"}},
    {"Eb", {"Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "Cb", "C", "Db", "D"}},
    {"Eb", {"Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "Cb", "C", "Db", "D"}},
    {"Eb", {"Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "Cb", "C", "Db", "D"}},
};

//static std::map<int,std::string> TNote__::map_MidiNoteNumber__NameUS = {0, "C-2}

#endif // 0

/**
This class represent a musical chord
*/
class TChord
{
public:
    TChord(void) {};
    void Set(TNote__ Note) {};
    void Set(std::vector<TNote__> v_Notes) {};

};


/**
This class allows to filter notes outside a given key or set of allowed notes.
*/
class TNoteFilter
{
public:
    TNoteFilter(void) {};

    void Allow(TChord) {};
//    void Allow(std::vector<TToneHeight> v_ToneHeight) {};


};


//XV5080.SetSoundMode(sm_perform);




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
    Thread.detach();
}


// A wrapper function to play a MIDI note, used by the micro-midi-keyboard.
void midiPlay(int octave, unsigned char noteInScale)
{
    PlayNote(2, octave *12 + noteInScale, 1000, 100);
}


/**
Overload function for midiPlay, which takes a pointer to void parameter
*/
void midiPlay_wrapper(void * pVoid)
{

}



void SelectContextInPlaylist(std::list<TContext*> &ContextList, bool);
void SelectContextInPlaylist(std::list<TContext*> &ContextList);
void SelectContextInPlaylist(std::list<TContext*> &ContextList);

namespace MiniSynth
{
char noteInScale;
int octave = 2;
int program = 1;
int channel = 2;

void StartNote(void * pVoid)
{
    int noteInScale = (long int) pVoid;
    MIDI_A.SendNoteOnEvent(channel, octave *12 + noteInScale, 100);
}

void StopNote(void * pVoid)
{
    int noteInScale = (long int) pVoid;
    MIDI_A.SendNoteOffEvent(channel, octave *12 + noteInScale, 0);
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

void Space(void * pVoid)
{
    SelectContextInPlaylist(PlaylistData, false);
}

void B(void * pVoid)
{
    SelectContextInPlaylist(PlaylistData_BySongName, false);
}

void N(void * pVoid)
{
    SelectContextInPlaylist(PlaylistData_ByAuthor, true);
}


void Z_p(void * pVoid)
{
    //system("aplay ./wav/FXCarpenterLaserBig.wav &");
    /*    XV5080.System.SystemCommon.PerformanceBankSelectMSB.Set(87);
        XV5080.System.SystemCommon.PerformanceBankSelectLSB.Set(64);
        XV5080.System.SystemCommon.PerformanceProgramNumber.Set(10);
        XV5080.System.SystemCommon.SystemTempo.Set(130);*/
    //Kungs_This_Girl::Sequence_1_Start_PedalPressed();
    I_Follow_Rivers::Sequence_1_Start_PedalPressed();
}

void Z_r(void * pVoid)
{
    //Kungs_This_Girl::Sequence_1_Start_PedalReleased();
    I_Follow_Rivers::Sequence_1_Start_PedalReleased();
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


    ComputerKeyboard::RegisterEventCallbackPressed(KEY_F1, OctaveLess, 0);
    ComputerKeyboard::RegisterEventCallbackPressed(KEY_F2, OctaveMore, 0);
    ComputerKeyboard::RegisterEventCallbackPressed(KEY_F3, ProgramLess, 0);
    ComputerKeyboard::RegisterEventCallbackPressed(KEY_F4, ProgramMore, 0);
    ComputerKeyboard::RegisterEventCallbackPressed(KEY_F5, ChannelLess, 0);
    ComputerKeyboard::RegisterEventCallbackPressed(KEY_F6, ChannelMore, 0);

    ComputerKeyboard::RegisterEventCallbackPressed(KEY_SPACE, Space, 0);
    ComputerKeyboard::RegisterEventCallbackPressed(KEY_B, B, 0);
    ComputerKeyboard::RegisterEventCallbackPressed(KEY_N, N, 0);
    ComputerKeyboard::RegisterEventCallbackPressed(KEY_Z, Z_p, 0);
    ComputerKeyboard::RegisterEventCallbackReleased(KEY_Z, Z_r, 0);

//    kbd_callbacks[KEY_A] =

    while (1)
    {
        int ch = -1;
        if (ch == -1)
        {
            waitMilliseconds(100);
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


namespace DaftPunk
{
namespace AroundTheWorld
{
void LowPassFilter(int val)
{
    // MIDI_B == send to 11R
    using std::vector;
    vector<double> xData113 = { 0, 127/5*1, 127/5*2, 127/5*3, 127/5*4,  120 };
    vector<double> yData113 = { 100, 100, 80, 60, 40,  64  };
    MIDI_B.SendControlChange(1, 113, interpolate(xData113, yData113, val, false));

    vector<double> xData114 = { 0, 127/5*1, 127/5*2, 127 };
    vector<double> yData114 = { 0, 0, 64, 64 };
    MIDI_B.SendControlChange(1, 114, interpolate(xData114, yData114, val, false));

    vector<double> xData115 = { 0, 127/5*2, 127/5*3, 127 };
    vector<double> yData115 = { 0, 0, 64, 64 };
    MIDI_B.SendControlChange(1, 115, interpolate(xData115, yData115, val, false));

    vector<double> xData96 = { 0, 127/5*3, 127/5*4, 127 };
    vector<double> yData96 = { 0, 0, 64, 64 };
    MIDI_B.SendControlChange(1, 96, interpolate(xData96, yData96, val, false));

    vector<double> xData97 = { 0, 127/5*4, 127/5*5, 127 };
    vector<double> yData97 = { 0, 0, 64, 64 };
    MIDI_B.SendControlChange(1, 97, interpolate(xData97, yData97, val, false));
}

void LoPassFilterEnable(void)
{
    // channel 1, control 86 (FX2), value 127 (please turn ON), send to midisport out B (to 11R)
    MIDI_B.SendControlChange(1, 86, 127);
}

void LoPassFilterDisable(void)
{
    MIDI_B.SendControlChange(1, 86, 0);
}

}
}
namespace Rihanna
{

    namespace ManDown
    {
        void BoatSiren(void)
        {
            PlayNote(16, 21, 2000, 127);
        }
    }

namespace WildThoughts
{
void Chord1_On(void)
{
    MIDI_A.SendProgramChange(2, 96);
    MIDI_A.SendNoteOnEvent(2, 53, 100);
    MIDI_A.SendNoteOnEvent(2, 56, 60);

}

void Chord1_Off_Thread(void * pGobble)
{
    MIDI_A.SendNoteOffEvent(2, 53, 0);
    MIDI_A.SendNoteOffEvent(2, 56, 0);
}

void Chord1_Off(void)
{
    ExecuteAfterTimeout(Chord1_Off_Thread, 600, NULL);
}

void Chord2_On(void)
{
    MIDI_A.SendNoteOnEvent(2, 60, 100);
    MIDI_A.SendNoteOnEvent(2, 63, 100);
}

void Chord2_Off_Thread(void * pGobble)
{
    MIDI_A.SendNoteOffEvent(2, 60, 0);
    MIDI_A.SendNoteOffEvent(2, 63, 0);
}

void Chord2_Off(void)
{
    ExecuteAfterTimeout(Chord2_Off_Thread, 600, NULL);
}

void Chord3_On(void)
{
    MIDI_A.SendNoteOnEvent(2, 50, 100);
    MIDI_A.SendNoteOnEvent(2, 43, 100);
}

void Chord3_Off_Thread(void * pGobble)
{
    MIDI_A.SendNoteOffEvent(2, 50, 0);
    MIDI_A.SendNoteOffEvent(2, 43, 0);
}

void Chord3_Off(void)
{
    ExecuteAfterTimeout(Chord3_Off_Thread, 600, NULL);
}

}

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
                    flag = 0;
                    return -1;
                }
            } while (!( (msg == msg_param1) || (msg == msg_param2) ));
        }
    }
};


/**
Represents and plays a short sequence of notes

If TimeoutFactor_param is set to 0, the sequence is played at the tempo
inferred (measured) from the time
that lapsed between the calls to Start_PedalPressed and Start_PedalReleased.
Upon Start_PedalPressed, it starts playing the sequence (first note),
Upon Start_PedalReleased, it continues to play the sequence at said tempo.
Of course, there is a requirement that the time duration between the first and second note
should be 1, i.e. one "unit of time", that lapsed between the pedal pressed and pedal released.
Stop_PedalPressed stops the sequence in progress, if any.

If TimeoutFactor_param is non-zero, the sequence is played one note at a time,
each time Start_PedalPressed OR Start_PedalReleased is called. If the last note of the sequence
is played on a PedalPressed event, that is in the case of an odd number of notes in the sequence,
the next call to PedalReleased does no action, so the performer can release his/her foot from the pedal
to rest.
If no pedal action occurred within Timeout_param s, the whole sequence is cancelled, and it will resume
on the next call to Start_PedalPressed (and not Start_PedalPressed, just in case it timed out while the pedal is pressed)

It is the user's responsibility to call Init() after main() is running, to spawn the threads required by
this object.
*/
class TSequence
{
private:
    struct timeval tv1, tv2;
    pthread_t thread;
    long int BeatTime_ms = 0;
    std::list<TNote> MelodyNotes;
    std::list<TNote>::iterator it;
    TNote CurrentNote;
    void (*pFuncNoteOn)(int NoteNumber) = NULL;
    void (*pFuncNoteOff)(int NoteNumber) = NULL;
    int RootNoteNumber = 60;
    float Timeout = 1;
    bool PhraseInProgress = false;
    bool WatchdogFlag = false;
    std::queue<int> QueueNotesOn; // keep track of which notes have been turned ON - so as to not forget to turn them OFF later
    typedef enum {btsmWaitStartingPulse, btsmWaitSecondPulse, btsmWaitNextPulse, btsmCancel} TBeatTimeStateMachine;
    TBeatTimeStateMachine BeatTimeStateMachine = btsmWaitStartingPulse;
    enum {msgBeatReceived = 10, msgReset = 20} MessageToPhraseSequencer;
    typedef enum {pssmNote1, pssmNote2, pssmOtherNotes} TPhraseSequencerStateMachine;
    TPhraseSequencerStateMachine PhraseSequencerStateMachine = pssmNote1;
    enum {msgPedalPressed = 1, msgPedalReleased = 2} MessageToBeatTime;
    TSyncMsg MsgToBeatTime;
    TSyncMsg MsgToPhraseSequencer;
    bool PedalEvent;
    int PedalBeats = 0; // number of times the pedal sent an even used to compute beat time
    bool ThreadsStartedFlag = false;


    void TurnNoteOn(int value)
    {
        if (value < 128)
        {
            pFuncNoteOn(value);
            QueueNotesOn.push(value);
        }
        else
        {
            // Silent note
            QueueNotesOn.push(value);
        }
    }

    void TurnPreviousNoteOff(void)
    {
        if (!QueueNotesOn.empty())
        {
            int NoteNumber = QueueNotesOn.front();
            QueueNotesOn.pop();
            if (NoteNumber > 127)
            {
                // Silent note
                return;
            }
            else
            {
                pFuncNoteOff(NoteNumber);
            }
        }
        else
        {
            // queue empty - do nothing
            return;
        }
    }

    static void GetBeatTimeFromPedalThreadStatic(TSequence * self)
    {
        self->GetBeatTimeFromPedalThread();
    }

    void GetBeatTimeFromPedalThread(void)
    {
        // TSyncMsg must be used after main() is called
        //std::this_thread::sleep_for(std::chrono::seconds(1));
        BeatTime_ms = 0;

        while (1)
        {
            switch(BeatTimeStateMachine)
            {
            case btsmWaitStartingPulse:
                PedalBeats = 0;
                MsgToBeatTime.Wait(msgPedalPressed, 0);
                MsgToPhraseSequencer.Send(msgBeatReceived);
                gettimeofday(&tv1, NULL);
                BeatTimeStateMachine = btsmWaitSecondPulse;
                break;

            case btsmWaitSecondPulse:
                if (MsgToBeatTime.Wait(msgPedalReleased, Timeout * 1000.0) == -1)
                {
                    // timeout
                    BeatTimeStateMachine = btsmWaitStartingPulse;
                    MsgToPhraseSequencer.Send(msgReset);
                }
                else
                {
                    // message received
                    PedalBeats ++;
                    // Upswing of the start sequencer pedal (step 2)
                    gettimeofday(&tv2, NULL);
                    // Compute time lapse between pedal down and pedal up
                    // that will set our "one beat" tempo time
                    BeatTime_ms = ((tv2.tv_sec - tv1.tv_sec) * 1000.0 + (tv2.tv_usec - tv1.tv_usec) / 1000.0) / ((float) PedalBeats);
                    MsgToPhraseSequencer.Send(msgBeatReceived);
                    BeatTimeStateMachine = btsmWaitNextPulse;
                }
                break;

            case btsmWaitNextPulse:
                if (MsgToBeatTime.Wait_OR(msgPedalPressed, msgPedalReleased, Timeout * 1000) == -1)
                {
                    // timeout =>  restart
                    // Did sequencer finish it phrase already?
                    if (BeatTimeStateMachine == btsmCancel)
                    {
                        // yes - back to beginning
                        break;
                    }
                    else
                    {
                        BeatTimeStateMachine = btsmWaitStartingPulse;
                        MsgToPhraseSequencer.Send(msgReset);
                    }
                }
                else
                {
                    // message received
                    // Did sequencer finish it phrase already?
                    if (BeatTimeStateMachine == btsmCancel)
                    {
                        // yes - back to beginning
                        break;
                    }
                    else
                    {// else: update beat time
                        PedalBeats ++;
                        // Upswing of the start sequencer pedal (step 2)
                        gettimeofday(&tv2, NULL);
                        // Compute time lapse between pedal down and pedal up
                        // that will set our "one beat" tempo time
                        BeatTime_ms = ((tv2.tv_sec - tv1.tv_sec) * 1000.0 + (tv2.tv_usec - tv1.tv_usec) / 1000.0) / ((float) PedalBeats);
                        MsgToPhraseSequencer.Send(msgBeatReceived);
                    }
                }
                break;

            default:
                BeatTimeStateMachine = btsmWaitStartingPulse;
            }
        }
    }

    /// This member function run as a thread, as such it must be static.
    static void PhraseSequencerThreadStatic(TSequence * self)
    {
        self->PhraseSequencerThread();
    }


    /// This function runs in its own thread ()
    void PhraseSequencerThread(void)
    {

        //std::this_thread::sleep_for(std::chrono::seconds(1));
        //wprintw(win_debug_messages.GetRef(), "THIS:%p\n", this);

        std::chrono::system_clock::time_point TimeStart;

        PhraseSequencerStateMachine = pssmNote1;

        while(1)
        {
            switch(PhraseSequencerStateMachine)
            {
            case pssmNote1:
                if (MsgToPhraseSequencer.Wait_OR(msgBeatReceived, msgReset, 0) == msgReset)
                {
                    // Reset state machine
                    TurnPreviousNoteOff();
                    PhraseSequencerStateMachine = pssmNote1;
                    break;
                }
                // First note
                TimeStart = std::chrono::system_clock::now();
                it = MelodyNotes.begin();
                CurrentNote = *it;
                wprintw(win_debug_messages.GetRef(), "START\n");
                TurnNoteOn(CurrentNote.NoteNumber + RootNoteNumber);
                PhraseSequencerStateMachine = pssmNote2;
                break;

            case pssmNote2:
                if (MsgToPhraseSequencer.Wait_OR(msgBeatReceived, msgReset, 0) == msgReset)
                {
                    // Reset state machine
                    TurnPreviousNoteOff();
                    PhraseSequencerStateMachine = pssmNote1;
                    break;
                }

                // Second note
/*
                TurnPreviousNoteOff();
                it++;
                CurrentNote = *it;
                TurnNoteOn(CurrentNote.NoteNumber + RootNoteNumber);*/
                PhraseSequencerStateMachine = pssmOtherNotes;

                break;


            case pssmOtherNotes:
                TurnPreviousNoteOff();
                // Point to next note
                it++;
                if (it == MelodyNotes.end())
                {
                    // That was the end of the sequence
                    PhraseSequencerStateMachine = pssmNote1;
                    BeatTimeStateMachine = btsmCancel; // prepare message to thread
                    MsgToBeatTime.Send(msgPedalReleased); // force thread to awaken
                    break;
                }
                // else
                CurrentNote = *it;
                TurnNoteOn(CurrentNote.NoteNumber + RootNoteNumber);

                // Fully recompute time to wait, from the beginning of the sequence.
                // This is to accommodate any change in tempo
                // NOTE - TODO: of course this is inefficient - rewrite that
                float FullLength_ms = 0;
                for (std::list<TNote>::iterator i = MelodyNotes.begin(); i != MelodyNotes.end(); i++)
                {
                    TNote Note = *i;
                    FullLength_ms += Note.NoteDuration * 1000.0;
                    if (i == it)
                    {
                        // currently pointing to the note we're playing
                        // we can end here
                        break;
                    }
                }
                // Adjust time as per current beat length
                FullLength_ms *= (((float) BeatTime_ms) / 1000.0);
                wprintw(win_debug_messages.GetRef(), "FullLength_ms %f\n", FullLength_ms);
                std::chrono::system_clock::time_point TimeNext = TimeStart + std::chrono::milliseconds((long int)FullLength_ms);
                std::this_thread::sleep_until(TimeNext);
                break;
            }
        }
    }



public:

    TSequence(std::list<TNote> MelodyNotes_param,
              void (*pFuncNoteOn_param)(int NoteNumber),
              void (*pFuncNoteOff_param)(int NoteNumber),
              int RootNoteNumber_param)
    {
        TSequence(MelodyNotes_param, pFuncNoteOn_param, pFuncNoteOff_param, RootNoteNumber_param, 0);
    }


    TSequence(std::list<TNote> MelodyNotes_param,
              void (*pFuncNoteOn_param)(int NoteNumber),
              void (*pFuncNoteOff_param)(int NoteNumber),
              int RootNoteNumber_param, float Timeout_param)
    {
        MelodyNotes = MelodyNotes_param;
        pFuncNoteOn = pFuncNoteOn_param;
        pFuncNoteOff = pFuncNoteOff_param;
        RootNoteNumber = RootNoteNumber_param;
        Timeout = Timeout_param;
    }

    void Init(void)
    {
        if (ThreadsStartedFlag == false)
        {
            ThreadsStartedFlag = true;
            std::thread th1(PhraseSequencerThreadStatic, this);
            th1.detach();
            std::thread th2(GetBeatTimeFromPedalThreadStatic, this);
            th2.detach();
        }
        else
        {
            // Threads were already started earlier
        }
    }

    void Start_PedalPressed(void)
    {
        MsgToBeatTime.Send(msgPedalPressed);
    }


    void Start_PedalReleased(void)
    {
        MsgToBeatTime.Send(msgPedalReleased);
    }

    #if 0
    static void WatchdogStatic(void * pVoid)
    {
        TSequence * pSequence = (TSequence *) pVoid;
        pSequence->Watchdog();
    }

    void Watchdog(void)
    {
        if(WatchdogFlag == false && PhraseInProgress == true)
        {
            Stop_PedalPressed();
        }
        else
        {
            WatchdogFlag = false;
        }
    }

    void PlayNextNote(void)
    {
        if (PhraseInProgress == false)
        {
            PhraseInProgress = true;
            it = MelodyNotes.begin();
            CurrentNote = *it;
            TurnNoteOn(CurrentNote.NoteNumber + RootNoteNumber);
            WatchdogFlag = true;
            ExecuteAfterTimeout(WatchdogStatic, Timeout * 1000.0, this);
        }
        else
        {
            WatchdogFlag = true;
            TurnPreviousNoteOff(CurrentNote.NoteNumber + RootNoteNumber);
            it++;
            if (it == MelodyNotes.end())
            {
                PhraseInProgress = false;
            }
            else
            {
                CurrentNote = *it;
                TurnNoteOn(CurrentNote.NoteNumber + RootNoteNumber);
                ExecuteAfterTimeout(WatchdogStatic, Timeout * 1000.0, this);
            }
        }
    }

    #endif
    // Stop sequencer
    void Stop_PedalPressed(void)
    {
        // Stop sequence if any
        PhraseInProgress = false;
        AllSoundsOff();
    }
};



namespace CapitaineFlam
{


void Partial_On(int NoteNumber)
{
    MIDI_A.SendNoteOnEvent(3, NoteNumber, 100);
    MIDI_A.SendNoteOnEvent(4, NoteNumber, 100);
    MIDI_A.SendNoteOnEvent(5, NoteNumber, 100);
}

void Partial_Off(int NoteNumber)
{
    MIDI_A.SendNoteOffEvent(3, NoteNumber, 0);
    MIDI_A.SendNoteOffEvent(4, NoteNumber, 0);
    MIDI_A.SendNoteOffEvent(5, NoteNumber, 0);
}


TSequence Sequence({{0, 1}, {4, 0.5}, {0, 0.25}, {4, 0.25}, {7, 2}},
Partial_On, Partial_Off, 72);


void Sequence_Start_PedalDown(void)
{
    Sequence.Start_PedalPressed();
}

void Sequence_Start_PedalUp(void)
{
    Sequence.Start_PedalReleased();
}


void Sequence_Stop_PedalDown(void)
{
    Sequence.Stop_PedalPressed();
}

int poorMansSemaphore = 0;

void * Laser_Cycle(void * pMessage)
{
    while (poorMansSemaphore)
    {
        MIDI_A.SendNoteOnEvent(2, 30, 100);
        waitMilliseconds(100);
//        TMidiNoteOffEvent noff(2, 30, 0);
        //waitMilliseconds(30);

    }
    return 0;
}

pthread_t thread;


void Init(void)
{
    Sequence.Init();
    // For a reason that eludes me, changing the variation of the part
    // must be sent twice...
    // We want to select the LaserGun (program 128, variation 2)
    MIDI_A.SendProgramChange(2, 128);
    MIDI_A.SendControlChange(2, 0, 2);
    MIDI_A.SendProgramChange(2, 128);
    MIDI_A.SendControlChange(2, 0, 2);

    MIDI_A.SendProgramChange(3, 57); // channel 3, trumpet
    MIDI_A.SendProgramChange(4, 64); // channel 3, synth brass
    MIDI_A.SendProgramChange(5, 61); // channel 3, trumpet

}


void Laser_On(void)
{

    poorMansSemaphore = 1;

    int iret1;
    iret1 = pthread_create(&thread, NULL, Laser_Cycle, (void*) NULL);
    if (iret1)
    {
        fprintf(stderr, "Error - pthread_create() return code: %d\n", iret1);
        exit(EXIT_FAILURE);
    }

}

void Laser_Off(void)
{
    poorMansSemaphore = 0;

}

void Starship1(void)
{
    system("aplay ./wav/FXCarpenterLaserBig.wav &");
}


void Starship2(void)
{
    system("aplay ./wav/FXAlienWoosh.wav &");
}


}


namespace BrunoMars
{
namespace LockedOutOfHeaven
{
void Yeah(void)
{
//    system("aplay ./wav/yeah_001.wav &");
    // Sample is on note 27 of the "all samples" Rhythm Set, which should listen on channel 16.
    PlayNote(16, 27, 500, 127);
}

void Hooh(void)
{
//    system("aplay ./wav/hooh_echo_001.wav &");
    PlayNote(16, 28, 500, 127);

}

void Siren(void)
{
//    system("aplay ./wav/alarm_001.wav &");
    PlayNote(16, 22, 2000, 127);

}

void Cuica(void)
{
//    system("aplay ./wav/cuica_001.wav &");
}
}
}





namespace Gansta_s_Paradise
{
struct timeval tv1, tv2;
pthread_t thread;
unsigned int const MelodySize = 8;
int Melody[MelodySize] = {0, 0, 0, 0, -1, -1, 0, -5}; // do do do do si si do sol  (and loop)
unsigned int waitTime_ms = 1000; // type must be atomic
unsigned int rootNote = 60;
unsigned int index = 0;
unsigned int mutex = 0;
void indexBumpUp(void)
{
    index++;
    index%=MelodySize;
}

void PlayChord(void)
{
    PlayNote(2, Melody[index] + rootNote -12, waitTime_ms /5, 100);
    waitMilliseconds(50);
    PlayNote(2, Melody[index] + rootNote, waitTime_ms /4, 100);
}

void * RunSequence(void *)
{
    mutex = 1;
    while(1)
    {
        indexBumpUp();
        PlayChord();
        waitMilliseconds(waitTime_ms);

    }
}

void Start_NoteOn(void)
{
    if(mutex == 0)
    {
        MIDI_A.SendProgramChange(2, 49);
        index = 0;
        PlayChord();
        gettimeofday(&tv1, NULL);
    }
}


void Start_NoteOff(void)
{
    gettimeofday(&tv2, NULL);
    // Compute time lapse between key press and release
    // that will be our initial tempo
    waitTime_ms = (tv2.tv_sec - tv1.tv_sec) * 1000.0 + (tv2.tv_usec - tv1.tv_usec) / 1000.0;

    int iret1;
    iret1 = pthread_create(&thread, NULL, RunSequence, NULL);
    if (iret1)
    {
        fprintf(stderr, "Error - pthread_create() return code: %d\n", iret1);
        exit(EXIT_FAILURE);
    }
}


void Stop(void)
{
    if(mutex == 1)
    {
        pthread_cancel(thread);
    }
    mutex = 0;
}

}

namespace HighwayToHell
{
//en sol




}

namespace MickaelJackson
{
namespace BeatIt
{

void Init(void)
{
    MIDI_A.SendProgramChange(2, 92);
    MIDI_A.SendProgramChange(3, 101);
    MIDI_A.SendProgramChange(4, 89);
//    TMidiProgramChange pc4(1, 4, 1);
}


void Partial_Off(void * pParam)
{
    MIDI_A.SendControlChange(2,0x42, 0);
    MIDI_A.SendControlChange(3,0x42, 0);
    MIDI_A.SendControlChange(4,0x42, 0);

}


void Partial_On(int NoteNumber)
{
    PlayNote(2, NoteNumber -12, 400, 100);
    PlayNote(2, NoteNumber, 400, 80);
    PlayNote(3, NoteNumber -12, 400, 100);
    PlayNote(3, NoteNumber, 400, 80);
    PlayNote(4, NoteNumber -12, 400, 100);
    PlayNote(4, NoteNumber, 400, 100);
    MIDI_A.SendControlChange(2,0x42, 127);
    MIDI_A.SendControlChange(3,0x42, 127);
    MIDI_A.SendControlChange(4,0x42, 127);

//    ExecuteAfterTimeout(Partial_Off, 1700, NULL);
}


void Chord1_On(void)
{
    Partial_On(50-1);
}

void Chord1_Off(void)
{
    Partial_Off(NULL);
}


void Chord2_On(void)
{
    Partial_On(47-1);
}

void Chord2_Off(void)
{
    Partial_Off(NULL);
}

void Chord3_On(void)
{
    Partial_On(45-1);
}


void Chord3_Off(void)
{
    Partial_Off(NULL);
}


void Chord4_On(void)
{
    Partial_On(48-1);
}

void Chord4_Off(void)
{
    Partial_Off(NULL);
}

}

}


namespace Kungs_This_Girl
{
void Trumpet_On(int NoteNumber)
{
    MIDI_A.SendNoteOnEvent(1, NoteNumber, 100);
}

void Trumpet_Off(int NoteNumber)
{
    MIDI_A.SendNoteOffEvent(1, NoteNumber, 0);
}



TSequence Sequence_1({{0, 1}, {0, 1}, {4, 1}, {0, 1}, {2, 1}}, Trumpet_On, Trumpet_Off, 63, 1.5);

TSequence Sequence_2({{2, 1}, {99, 1}, {2, 1}, {99, 1}, {2, 1}, {2, 1}, {2, 1}, {0, 1}}, Trumpet_On, Trumpet_Off, 63, 1.5);

TSequence Sequence_3({{2, 1}, {2, 1}, {2, 1}, {2, 1}, {2, 1}, {2, 1}, {2, 1}, {0, 1}}, Trumpet_On, Trumpet_Off, 63, 1.5);

void Sequence_1_Start_PedalPressed(void)
{
    Sequence_1.Start_PedalPressed();
}

void Sequence_1_Start_PedalReleased(void)
{
    Sequence_1.Start_PedalReleased();
}

void Sequence_2_Start_PedalPressed(void)
{
    Sequence_2.Start_PedalPressed();
}

void Sequence_2_Start_PedalReleased(void)
{
    Sequence_2.Start_PedalReleased();
}

void Sequence_3_Start_PedalPressed(void)
{
    Sequence_3.Start_PedalPressed();
}

void Sequence_3_Start_PedalReleased(void)
{
    Sequence_3.Start_PedalReleased();
}

void Sequences_Stop(void)
{
    Sequence_1.Stop_PedalPressed();
    Sequence_2.Stop_PedalPressed();
    Sequence_3.Stop_PedalPressed();
}

void Init(void)
{
    Sequence_1.Init();
    Sequence_2.Init();
    Sequence_3.Init();

    // Force XV5080 to performance mode
    XV5080.System.SystemCommon.SoundMode.Perform();
    XV5080.TemporaryPerformance.PerformancePart[0].ReceiveChannel.Set_1_16(1);
    XV5080.TemporaryPerformance.PerformancePart[0].SelectPatch(TXV5080::PatchGroup::PR_B, 126); // 2 Trumpets
    XV5080.TemporaryPerformance.PerformancePart[0].ReceiveSwitch.Set(1);

    // Set up keyboard for Ani:
    // First Part is a piano, second part is a pad, third part is the trumpet
    XV5080.TemporaryPerformance.PerformancePart[MASTER_KBD_PART_INDEX].SelectPatch(TXV5080::PatchGroup::PR_D, 1); // Echo Piano
    XV5080.TemporaryPerformance.PerformancePart[MASTER_KBD_PART_INDEX+1].SelectPatch(TXV5080::PatchGroup::PR_E, 55); // Ethereal Strings
    XV5080.TemporaryPerformance.PerformancePart[MASTER_KBD_PART_INDEX+1].ReceiveSwitch.Set(1);
    XV5080.TemporaryPerformance.PerformancePart[MASTER_KBD_PART_INDEX+2].SelectPatch(TXV5080::PatchGroup::PR_B, 126); // 2 trumpets
    XV5080.TemporaryPerformance.PerformancePart[MASTER_KBD_PART_INDEX+2].ReceiveSwitch.Set(1);

}
}



namespace I_Follow_Rivers
{
    void SynthTom_ON(int NoteNumber)
    {
        MIDI_A.SendNoteOnEvent(1, NoteNumber, 100);
    }

    void SynthTom_OFF(int NoteNumber)
    {
        MIDI_A.SendNoteOffEvent(1, NoteNumber, 0);
    }


    // Rhythm:
    //
    //   x   O       x     x     x   x x x x   x
    //   | | | | | | | | | | | | | | | | | | | |------
    //   v   ^   v   ^   v   ^   v   ^   v   ^  (v)---
    //
    // Pulse lengths:
    //   x---O------x-----x-----x---x-x-x-x---x
    //     1    2     1.5   1.5   1 .5.5.5  1   (0.5 to terminate last note)
    // Note names:
    // Re Fa# Fa# Fa# Mi Re Mi Re Re
    //
    // Note numbers, assuming root is Re = 74
    // 12 5   5   5   3  1  3  1  1
    //
    // Note there must be a note event on the first press and first release - in our case,
    // the first release is simple a note OFF (special value 999)

    TSequence Sequence_1({{84, 1}, {999, 2}, {76, 1.5}, {76, 1.5}, {76, 1}, {72, 0.5}, {69, 0.5}, {72, 0.5}, {69, 1}, {69, 0.5}}, SynthTom_ON, SynthTom_OFF, 0, 1.5);

    void Sequence_1_Start_PedalPressed(void)
    {
        Sequence_1.Start_PedalPressed();
    }

    void Sequence_1_Start_PedalReleased(void)
    {
        Sequence_1.Start_PedalReleased();
    }

    void Init(void)
    {
        Sequence_1.Init();

        XV5080.TemporaryPerformance.PerformancePart[0].SelectPatch(TXV5080::PatchGroup::PR_C, 106); // Dyno Toms
        XV5080.TemporaryPerformance.PerformancePart[0].ReceiveSwitch.Set(1);
        XV5080.TemporaryPerformance.PerformancePart[1].ReceiveSwitch.Set(0);

        // Ani's piano is a piano with a bit of reverb
        XV5080.TemporaryPerformance.PerformancePart[MASTER_KBD_PART_INDEX].SelectPatch(TXV5080::PatchGroup::PR_D, 1); // Echo Piano

    }
}


namespace People_Help_The_People
{
    void Init(void)
    {
        // Ani's piano is a piano with strings
        XV5080.TemporaryPerformance.PerformancePart[MASTER_KBD_PART_INDEX].SelectPatch(TXV5080::PatchGroup::PR_E, 2); // Contemplate Piano
        XV5080.TemporaryPerformance.PerformancePart[MASTER_KBD_PART_INDEX+1].ReceiveSwitch.Set(1);
        XV5080.TemporaryPerformance.PerformancePart[MASTER_KBD_PART_INDEX+1].SelectPatch(TXV5080::PatchGroup::PR_E, 73); // Lush Strings
        // Then Ani will do the mix herself

    }
}

extern "C" void showlist(void);
extern "C" int main_TODO(int argc, char const **argv, int Tempo);
extern "C" void seq_midi_tempo_direct(int Tempo);
extern "C" void pmidiStop(void);

static int Tempo = 120;
unsigned int SequencerRunning = 0;

pthread_t thread_sequencer = 0;


// Stop the pmidi sequencer.
void StopSequencer(void)
{
    if (thread_sequencer != 0)
    {

        pmidiStop();

        pthread_cancel(thread_sequencer);
        thread_sequencer = 0;

        sleep(1);

        MIDI_A.StartRawMidiOut();
        AllSoundsOff();
    }
}

// Run the pmidi sequencer in its own thread.
void * ThreadSequencerFunction (void * params)
{


    MIDI_A.StopRawMidiOut();

    SequencerRunning = 1;
    showlist();

    int argc = 4;
    char str10[] = "fakename";
    char str11[] = "-p";
    char const * str12 = midi_sequencer_name.c_str();
    char * str13 = (char*) params;
    char const * (argv1[4]) =
    { str10, str11, str12, str13 };
    main_TODO(argc, argv1, Tempo);
    MIDI_A.StartRawMidiOut();

    thread_sequencer = 0;

    return 0;
}

// Start the full-fledged pmidi sequencer for file MidiFilename
void StartSequencer(char * MidiFilename)
{
    int iret1;
    if(thread_sequencer == 0)
    {
        iret1 = pthread_create(&thread_sequencer, 0, ThreadSequencerFunction, (void*) MidiFilename);
        if (iret1)
        {
            fprintf(stderr, "Error - pthread_create() return code: %d\n", iret1);
            exit(EXIT_FAILURE);
        }

    }
    else
    {
        wprintw(win_debug_messages.GetRef(), "StartSequencer called twice\n");
    }

}


// Validate the tempo passed on to the pmidi sequencer by ChangeTempoSequencer.
// First call ChangeTempoSequencer, then call SetTempoSequencer.
void SetTempoSequencer(void)
{
    if (thread_sequencer != 0)
    {
        seq_midi_tempo_direct(Tempo);
    }
}


// Pass new tempo to pmidi sequencer, based on the analog pedal controller value
// which is usually from 0 to 127.
void ChangeTempoSequencer(float controllerValue, float bpm_min, float bpm_max)
{
    float bpm_average = (bpm_min + bpm_max)/2;
    float skew = (controllerValue- 60)/60;
    Tempo = (bpm_average + skew * (bpm_average - bpm_min));
    SetTempoSequencer();
}


namespace AveMaria
{

char AveMaria_MidiName[] = "am.mid";






void AveMaria_Start(void)
{
    StartSequencer(AveMaria_MidiName);
}


void AveMaria_Stop(void)
{
    StopSequencer();
}


void ChangeTempo(int Value)
{
//    waitMilliseconds(10);
    ChangeTempoSequencer(Value, 60, 100);
}
}


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

namespace Modjo
{
namespace Lady
{
void Init(void)
{
    // Do nothing - Eleven Rack is always initialized when changing context.
}

bool SoloON = false;

// Solo means: MOD + FX2
// Note: MOD works best with C1 CHOR VIB, medium depth, sync on beat, assuming tempo is right
//       FX2 works best with: graphic EQ, cut 100Hz, boost 800 and 2k
void Solo_On_Off(void)
{
    // Toggle solo flag
    SoloON = !SoloON;
    if (SoloON)
    {
        MIDI_B.SendControlChange(1, 50, 127);
        MIDI_B.SendControlChange(1, 86, 127);

    }
    else
    {
        MIDI_B.SendControlChange(1, 50, 0);
        MIDI_B.SendControlChange(1, 86, 0);
    }
}

}

}

namespace AllumerLeFeu
{
    void Init(void)
    {
        XV5080.TemporaryPerformance.PerformancePart[MASTER_KBD_PART_INDEX].SelectPatch(TXV5080::PatchGroup::PR_E, 35); // Rocker Organ
    }
}

namespace ElevenRack
{

void DistOFF(void)
{
    // Midi channel 1, controller number 25, value 0, send to Midisport port B
    MIDI_B.SendControlChange(1, 25, 0);
    Banner.SetMessage("Dist OFF");
}

void DistON(void)
{
    MIDI_B.SendControlChange(1, 25, 127);
    Banner.SetMessage("Dist ON");
}

void DistToggle(void)
{
    static unsigned int flag; // static => initialized at 0 by compiler
    if(flag == 0)
    {
        flag = 1;
        DistON();
    }
    else
    {
        flag = 0;
        DistOFF();
    }
}


void ModOFF(void)
{
    // Midi channel 1, controller number 50, value 0, send to Midisport port B
    MIDI_B.SendControlChange(1, 50, 0);
    Banner.SetMessage("Mod OFF");
}

void ModON(void)
{
    MIDI_B.SendControlChange(1, 50, 127);
    Banner.SetMessage("Mod ON");
}

void ModToggle(void)
{
    static unsigned int flag; // static => initialized at 0 by compiler
    if(flag == 0)
    {
        flag = 1;
        ModON();
    }
    else
    {
        flag = 0;
        ModOFF();
    }
}




void WahOFF(void)
{
    // Midi channel 1, controller number 43, value 0, send to Midisport port B
    MIDI_B.SendControlChange(1, 43, 0);
    Banner.SetMessage("WAH OFF");
}

void WahON(void)
{
    MIDI_B.SendControlChange(1, 43, 127);
    Banner.SetMessage("WAH ON");
}

void WahToggle(void)
{
    static unsigned int flag; // static => initialized at 0 by compiler
    if(flag == 0)
    {
        flag = 1;
        WahON();
    }
    else
    {
        flag = 0;
        WahOFF();
    }
}


void WahSetValue(int Val)
{
    MIDI_B.SendControlChange(1, 4, Val);
}

void Init(void)
{
    // Rack 11 bank change: select user-defined
    // Manual page 124.
    // Midi channel 1, Control number 32, Value 0, on Midisport port B
    // (going to rack eleven)
    MIDI_B.SendControlChange(1, 32, 0);
    MIDI_B.SendProgramChange(1, 104);

    DistOFF();
    ModOFF();
    WahOFF();
}

}


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
        TContext context;
        context = **PlaylistPosition;
        for (std::list<TPedalDigital>::iterator it = context.Pedalboard.PedalsDigital.begin(); \
                it != context.Pedalboard.PedalsDigital.end(); \
                it++)
        {
            TPedalDigital PedalDigital = *it;
            if (rxNote == PedalDigital.GetNumber())
            {
                if (rxVolume > 0)
                {
                    PedalDigital.Press();
                }
                if (rxVolume == 0)
                {
                    PedalDigital.Release();
                }
            }
        }
    }
}


// This hook function is called whenever a Note ON event was received on
// MIDI B IN.
// ************************************************************
// Retransmit external Keyboard MIDI information to the XV5080.
// ************************************************************
void MIDI_B_IN_NoteOnEvent(TInt_1_16 rxChannel, TInt_0_127 rxNote, TInt_0_127 rxVolume)
{
    static int KeyboardNoteLow = 0;
    static int KeyboardNoteHigh = 127;
    static TInt_0_127 MasterVolume;
    MasterVolume = 127;
    static TInt_1_16 KbdMidiChannelTx;
    KbdMidiChannelTx = MASTER_KBD_XV5080_MIDI_CHANNEL;


    // Calibrate keyboard size
    // User must hit once the lowest key and the highest key of the keyboard.
    #if KBD_CALIBRATE
    if ((KeyboardNoteLow == 0 || KeyboardNoteHigh == 127))
    {
        if (rxNote < 64)
        {
            KeyboardNoteLow = rxNote;
        }

        if (rxNote > 64)
        {
            KeyboardNoteHigh = rxNote;
        }
        return;
    }
    #endif

    if (rxNote >= 1 && rxNote <= 127)
    {
        #if MIDI_KEYBOARD_CONTROLS_ON_KEYS
        if (rxNote == KeyboardNoteHigh) // Last key of the keyboard
        {
            MasterVolume = MasterVolume + 5;
            Banner.SetMessage("KBD VOL +");
            XV5080.System.SystemCommon.MasterLevel.Set(MasterVolume);
            return;
        }

        if (rxNote == KeyboardNoteHigh -1) // Last key -1
        {
            MasterVolume = MasterVolume + (0 - 5);
            Banner.SetMessage("KBD VOL -");
            XV5080.System.SystemCommon.MasterLevel.Set(MasterVolume);
            return;
        }

        if (rxNote == KeyboardNoteHigh -2) // Last key -2
        {
            KbdMidiChannelTx = KbdMidiChannelTx + 1;
            Banner.SetMessage("KBD CHN" + std::to_string(KbdMidiChannelTx));
            return;
        }

        if (rxNote == KeyboardNoteHigh -4) // Last key -4
        {
            KbdMidiChannelTx = KbdMidiChannelTx - 1;
            Banner.SetMessage("KBD CHN" + std::to_string(KbdMidiChannelTx));
            return;
        }

        #endif // MIDI_KEYBOARD_CONTROLS_ON_KEYS

        // Forward notes to XV5080 Midi IN, plugged on MidiSport Midi OUT A
        MIDI_A.SendNoteOnEvent(KbdMidiChannelTx, rxNote, rxVolume);
    }
}

// This hook function is called whenever a Note OFF event was received on
// MIDI B IN.
void MIDI_B_IN_NoteOffEvent(TInt_1_16 rxChannel, TInt_0_127 rxNote, TInt_0_127 rxVolume)
{
    TInt_1_16 KbdMidiChannelTx;
    KbdMidiChannelTx = MASTER_KBD_XV5080_MIDI_CHANNEL;

    MIDI_A.SendNoteOffEvent(KbdMidiChannelTx, rxNote, rxVolume);
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
            // Volume for first patch assigned to master keyboard
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
            // Volume for second patch assigned to master keyboard
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
            // Volume for third patch assigned to master keyboard
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
            // Volume for third patch assigned to master keyboard
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
            // Volume for third patch assigned to master keyboard
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
            // Volume for third patch assigned to master keyboard
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
            // Volume for third patch assigned to master keyboard
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
            // Volume for third patch assigned to master keyboard
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
    MIDI_A.SendPitchBendChange(MASTER_KBD_XV5080_MIDI_CHANNEL, rxPitchBendChangeValue_param);
}

// This hook function is called whenever a Controller Change event is
// received on MIDI A IN
void MIDI_A_IN_CC_Event(TInt_1_16 rxChannel, TInt_0_127 rxControllerNumber, TInt_0_127 rxControllerValue)
{
    TContext context;
    context = **PlaylistPosition;
    for (std::list<TPedalAnalog>::iterator it = context.Pedalboard.PedalsAnalog.begin(); \
            it != context.Pedalboard.PedalsAnalog.end(); \
            it++)
    {
        TPedalAnalog PedalAnalog = *it;
        if (rxControllerNumber == PedalAnalog.GetControllerNumber())
        {
            PedalAnalog.Change(rxControllerValue);
        }
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
    cRigUp.Pedalboard.PedalsDigital.push_back(TPedalDigital(1, RigUp::WhiteNoiseUniform, NULL, "White noise, uniform"));
    cRigUp.Pedalboard.PedalsDigital.push_back(TPedalDigital(2, RigUp::WhiteNoiseGaussian, NULL, "White noise, gaussian"));
    cRigUp.Pedalboard.PedalsDigital.push_back(TPedalDigital(3, RigUp::SineWaveOn, NULL, "Sine Wave ON"));
    cRigUp.Pedalboard.PedalsDigital.push_back(TPedalDigital(4, RigUp::SineWaveOff, NULL, "Sine Wave OFF"));
    cRigUp.Pedalboard.PedalsAnalog.push_back(TPedalAnalog(1, RigUp::SineWavePitch, "Adjust sine wave pitch"));


    cFlyMeToTheMoon.Author = "Count Basie";
    cFlyMeToTheMoon.SongName = "Fly me to the moon";
    cRigUp.BaseTempo = 0;

    cAllOfMe.Author = "John Legend";
    cAllOfMe.SongName = "All of me";

    cCryMeARiver.Author = "Ella Fitzerald";
    cCryMeARiver.SongName = "Cry me a river";

    cJustAGigolo.Author = "Louis Prima";
    cJustAGigolo.SongName = "Just a gigolo";
    cJustAGigolo.BaseTempo = 124;

    cSuperstition.Author = "Stevie Wonder";
    cSuperstition.SongName = "Superstition";
    cSuperstition.BaseTempo = 100;

    cStandByMe.Author = "Ben E. King";
    cStandByMe.SongName = "Stand by me";
    cStandByMe.BaseTempo = 110;


    cGetBack.Author = "Beatles";
    cGetBack.SongName = "Get back";
    cGetBack.BaseTempo = 115;


    cAllShookUp.Author = "Elvis Presley";
    cAllShookUp.SongName = "All shook up";
    cAllShookUp.BaseTempo = 135;



    cBackToBlack.Author = "Amy Winehouse";
    cBackToBlack.SongName = "Back to black";
    cBackToBlack.BaseTempo = 122;


    cUnchainMyHeart.Author = "Joe Cooker";
    cUnchainMyHeart.SongName = "Unchain my heart";
    cUnchainMyHeart.BaseTempo = 120;

    cFaith.Author = "George Michael";
    cFaith.SongName = "Faith";

    cIsntSheLovely.Author = "Stevie Wonder";
    cIsntSheLovely.SongName = "Isn't she lovely";
    cIsntSheLovely.BaseTempo = 120;

    cJammin.Author = "Bob Marley";
    cJammin.SongName = "Jammin'";
    cJammin.BaseTempo = cIsntSheLovely.BaseTempo;

    cRehab.Author = "Amy Winehouse";
    cRehab.SongName = "Rehab";

    cIFeelGood.Author = "James Brown";
    cIFeelGood.SongName = "I feel good";
    cIFeelGood.BaseTempo = 140;

    cPapasGotABrandNewBag.Author = "James Brown";
    cPapasGotABrandNewBag.SongName = "Papa's got a brand new bag";
    cPapasGotABrandNewBag.BaseTempo = 130;

    cLongTrainRunning.Author = "Doobie Brothers";
    cLongTrainRunning.SongName = "Long train running";
    cLongTrainRunning.BaseTempo = 110;


    cMasterBlaster.Author = "Stevie Wonder";
    cMasterBlaster.SongName = "Master Blaster";
    cMasterBlaster.BaseTempo = 131;


    cAuxChampsElysees.Author = "";
    cAuxChampsElysees.SongName = "Aux champs élysées";

    cProudMary.Author = "Tina Turner";
    cProudMary.SongName = "Proud mary";
    cProudMary.BaseTempo = 142;

    cMonAmantDeSaintJean.Author = "";
    cMonAmantDeSaintJean.SongName = "Mon amant de St. Jean";
    cMonAmantDeSaintJean.BaseTempo = 240;

    cAroundTheWorld.Author = "Daft Punk";
    cAroundTheWorld.SongName = "Around The World";
    cAroundTheWorld.BaseTempo = 113;
    cAroundTheWorld.Pedalboard.PedalsAnalog.push_back(TPedalAnalog(1, DaftPunk::AroundTheWorld::LowPassFilter, "Low Pass Filter"));
    cAroundTheWorld.Pedalboard.PedalsDigital.push_back(TPedalDigital(1, DaftPunk::AroundTheWorld::LoPassFilterEnable, NULL, "LowPass ON"));
    cAroundTheWorld.Pedalboard.PedalsDigital.push_back(TPedalDigital(2, DaftPunk::AroundTheWorld::LoPassFilterDisable, NULL, "LowPass OFF"));
    cAroundTheWorld.SetInitFunc(DaftPunk::AroundTheWorld::LoPassFilterEnable);

    cGetLucky.Author = "Daft Punk";
    cGetLucky.SongName = "Get lucky";
    cGetLucky.BaseTempo = 113;

    cIllusion.Author = "";
    cIllusion.SongName = "Illusion";
    cIllusion.BaseTempo = cGetLucky.BaseTempo;

    cDockOfTheBay.Author = "Otis Redding";
    cDockOfTheBay.SongName = "Dock of the bay";
    cDockOfTheBay.BaseTempo = 103;

    cLockedOutOfHeaven.Author = "Bruno Mars";
    cLockedOutOfHeaven.SongName = "Locked out of heaven";
    cLockedOutOfHeaven.BaseTempo = 140;
    cLockedOutOfHeaven.Pedalboard.PedalsDigital.push_back(TPedalDigital(1, BrunoMars::LockedOutOfHeaven::Yeah, BrunoMars::LockedOutOfHeaven::Yeah, "Yeah x8 + Hooh"));
    cLockedOutOfHeaven.Pedalboard.PedalsDigital.push_back(TPedalDigital(2, BrunoMars::LockedOutOfHeaven::Hooh, NULL, "Hooh"));
    cLockedOutOfHeaven.Pedalboard.PedalsDigital.push_back(TPedalDigital(3, BrunoMars::LockedOutOfHeaven::Siren, NULL, "Siren"));
    cLockedOutOfHeaven.Pedalboard.PedalsDigital.push_back(TPedalDigital(4, BrunoMars::LockedOutOfHeaven::Cuica, NULL, "Cuica"));



    cWhatsUp.Author = "4 Non Blondes";
    cWhatsUp.SongName = "What's up";
    cWhatsUp.BaseTempo = 127;


    cLesFillesDesForges.Author = "";
    cLesFillesDesForges.SongName = "Les filles des forges";
    cLesFillesDesForges.BaseTempo = 150;

    cThatsAllRight.Author = "Elvis Presley";
    cThatsAllRight.SongName = "That's all right";
    cThatsAllRight.BaseTempo = 163;

    cJohnnyBeGood.Author = "Chuck Berry";
    cJohnnyBeGood.SongName = "Johnny be good";
    cJohnnyBeGood.BaseTempo = 165;

    cBebopALula.Author = "Elvis Presley";
    cBebopALula.SongName = "Bebop a lula";

    cUptownFunk.Author = "Bruno Mars";
    cUptownFunk.SongName = "Uptown Funk";

    cLeFreak.Author = "Chic";
    cLeFreak.SongName = "Le freak";

    cRappersDelight.Author = "The Sugarhill Gang";
    cRappersDelight.SongName = "Rapper's Delight";

    cMachistador.Author = "M";
    cMachistador.SongName = "Machistador";

    cAnotherOneBiteTheDust.Author = "Queen";
    cAnotherOneBiteTheDust.SongName = "Another one bite the dust";

    cWot.Author = "Captain Sensible";
    cWot.SongName = "Wot";

    cKnockOnWood.Author = "Eddy Floyd";
    cKnockOnWood.SongName = "Knock on wood";
    cKnockOnWood.BaseTempo = 123;

    cHotelCalifornia.Author = "Eagles";
    cHotelCalifornia.SongName = "Hotel california";
    cHotelCalifornia.BaseTempo = 135;

    cRaggamuffin.Author = "Selah Sue";
    cRaggamuffin.SongName = "Raggamuffin";

    cManDown.Author = "Rihanna";
    cManDown.SongName = "Man Down";
    cManDown.BaseTempo = 145;
    cManDown.Pedalboard.PedalsDigital.push_back(TPedalDigital(1, Rihanna::ManDown::BoatSiren, NULL, "Boat Siren"));

    cShouldIStay.Author = "The Clash";
    cShouldIStay.SongName = "Should I stay";
    cShouldIStay.BaseTempo = 118;

    cMercy.Author = "Duffy";
    cMercy.SongName = "Mercy";
    cMercy.BaseTempo = 130;

    cAveMaria.Author = "";
    cAveMaria.SongName = "Ave Maria";
    cAveMaria.Pedalboard.PedalsDigital.push_back(TPedalDigital(1, AveMaria::AveMaria_Start, NULL, "MIDI sequencer start"));
    cAveMaria.Pedalboard.PedalsDigital.push_back(TPedalDigital(2, AveMaria::AveMaria_Stop, NULL, "MIDI sequencer stop"));
    cAveMaria.Pedalboard.PedalsAnalog.push_back(TPedalAnalog(1, AveMaria::ChangeTempo, "Adjust tempo"));

    cCapitaineFlam.Author = "Jean-Jacques Debout";
    cCapitaineFlam.SongName = "Capitaine Flam";
    cCapitaineFlam.Pedalboard.PedalsDigital.push_back(TPedalDigital(1, CapitaineFlam::Laser_On, CapitaineFlam::Laser_Off, "Laser pulses"));
    cCapitaineFlam.Pedalboard.PedalsDigital.push_back(TPedalDigital(2, CapitaineFlam::Starship1, NULL, "Starship Fx 1"));
    cCapitaineFlam.Pedalboard.PedalsDigital.push_back(TPedalDigital(3, CapitaineFlam::Starship2, NULL, "Starship Fx 2"));
    cCapitaineFlam.Pedalboard.PedalsDigital.push_back(TPedalDigital(4, CapitaineFlam::Sequence_Start_PedalDown, CapitaineFlam::Sequence_Start_PedalUp, "Trumpets - Down/Up=Tempo"));
    cCapitaineFlam.Pedalboard.PedalsDigital.push_back(TPedalDigital(5, CapitaineFlam::Sequence_Stop_PedalDown, NULL, "Trumpets - Stop/Cancel"));
    cCapitaineFlam.SetInitFunc(CapitaineFlam::Init);

    cWildThoughts.Author = "Rihanna";
    cWildThoughts.SongName = "Wild Thoughts";
    cWildThoughts.Pedalboard.PedalsDigital.push_back(TPedalDigital(1, Rihanna::WildThoughts::Chord1_On, Rihanna::WildThoughts::Chord1_Off, "First chord"));
    cWildThoughts.Pedalboard.PedalsDigital.push_back(TPedalDigital(2, Rihanna::WildThoughts::Chord2_On, Rihanna::WildThoughts::Chord2_Off, "Second chord"));
    cWildThoughts.Pedalboard.PedalsDigital.push_back(TPedalDigital(3, Rihanna::WildThoughts::Chord3_On, Rihanna::WildThoughts::Chord3_Off, "Third chord"));

    cGangstaParadise.Author = "Coolio";
    cGangstaParadise.SongName = "Gangsta's paradise";
    cGangstaParadise.Pedalboard.PedalsDigital.push_back(TPedalDigital(1, Gansta_s_Paradise::Start_NoteOn, Gansta_s_Paradise::Start_NoteOff, "Sequence; Press: first note, Release: set tempo and loop"));
    cGangstaParadise.Pedalboard.PedalsDigital.push_back(TPedalDigital(2, Gansta_s_Paradise::Stop, NULL, "Sequence; Press: first note, Release: set tempo and loop"));

    cBeatIt.Author = "Mickael Jackson";
    cBeatIt.SongName = "Beat It";
    cBeatIt.Comments = "In C# -- C#, B, C#, B, A, B, C#, B";
    cBeatIt.BaseTempo = 137;
    cBeatIt.SetInitFunc(MickaelJackson::BeatIt::Init);
    cBeatIt.Pedalboard.PedalsDigital.push_back(TPedalDigital(1, MickaelJackson::BeatIt::Chord1_On, MickaelJackson::BeatIt::Chord1_Off, "Chord 1"));
    cBeatIt.Pedalboard.PedalsDigital.push_back(TPedalDigital(2, MickaelJackson::BeatIt::Chord2_On, MickaelJackson::BeatIt::Chord2_Off, "Chord 2"));
    cBeatIt.Pedalboard.PedalsDigital.push_back(TPedalDigital(3, MickaelJackson::BeatIt::Chord3_On, MickaelJackson::BeatIt::Chord3_Off, "Chord 3"));
    cBeatIt.Pedalboard.PedalsDigital.push_back(TPedalDigital(4, MickaelJackson::BeatIt::Chord4_On, MickaelJackson::BeatIt::Chord4_Off, "Chord 4"));

    cLady.Author = "Modjo";
    cLady.SongName = "Lady (Hear me tonight)";
    cLady.BaseTempo = 122;
    cLady.SetInitFunc(Modjo::Lady::Init);
    cLady.Pedalboard.PedalsDigital.push_back(TPedalDigital(1, Modjo::Lady::Solo_On_Off, NULL, "Solo ON/OFF (Avid11)"));

    cLesCitesDOr.Author = "Unknown";
    cLesCitesDOr.SongName = "Les mysterieuses cites d'or";
    cLesCitesDOr.BaseTempo = 30;


    cHuman.Author = "Rag'n'Bone Man";
    cHuman.SongName = "Human (I'm only)";


    cNewYorkAvecToi.Author = "Telephone";
    cNewYorkAvecToi.SongName = "New-York Avec Toi";

    cBillieJean.Author = "Mickael Jackson";
    cBillieJean.SongName = "Billie Jean";

    cILoveRockNRoll.Author = "Unknown";
    cILoveRockNRoll.SongName = "I Love Rock'n'Roll";

    cHighwayToHell.Author = "AC/DC";
    cHighwayToHell.SongName = "Highway To Hell";


    c25years.Author = "4 Non Blondes";
    c25years.SongName = "25 years";

    cAllumerLeFeu.Author = "Johnny Halliday";
    cAllumerLeFeu.SongName = "Allumer Le Feu";
    cAllumerLeFeu.SetInitFunc(AllumerLeFeu::Init);

    cAllInYou.Author = "Synapson";
    cAllInYou.SongName = "All In You";

    cChandelier.Author = "Sia";
    cChandelier.SongName = "Chandelier";

    cThisGirl.Author = "Kung";
    cThisGirl.SongName = "This Girl";
    cThisGirl.Pedalboard.PedalsDigital.push_back(TPedalDigital(1, Kungs_This_Girl::Sequence_1_Start_PedalPressed, Kungs_This_Girl::Sequence_1_Start_PedalReleased,"Sequence 1"));
    cThisGirl.Pedalboard.PedalsDigital.push_back(TPedalDigital(2, Kungs_This_Girl::Sequence_2_Start_PedalPressed, Kungs_This_Girl::Sequence_2_Start_PedalReleased,"Sequence 2"));
    cThisGirl.Pedalboard.PedalsDigital.push_back(TPedalDigital(3, Kungs_This_Girl::Sequence_3_Start_PedalPressed, Kungs_This_Girl::Sequence_3_Start_PedalReleased,"Sequence 3"));
    cThisGirl.SetInitFunc(Kungs_This_Girl::Init);


    cEncoreUnMatin.Author = "Jean-Jacques Goldman";
    cEncoreUnMatin.SongName = "Encore Un Matin";

    cQuandLaMusiqueEstBonne.Author = "Jean-Jacques Goldman";
    cQuandLaMusiqueEstBonne.SongName = "Quand La Musique Est Bonne";

    cDumbo.Author = "Viannay";
    cDumbo.SongName = "Dumbo";

    cIFollowRivers.Author = "Likke Li";
    cIFollowRivers.SongName = "I Follow Rivers";
    cIFollowRivers.Pedalboard.PedalsDigital.push_back(TPedalDigital(1, I_Follow_Rivers::Sequence_1_Start_PedalPressed, I_Follow_Rivers::Sequence_1_Start_PedalReleased, "Synth tom sequence"));
    cIFollowRivers.SetInitFunc(I_Follow_Rivers::Init);

    cIsThisLove.Author = "Bob Marley";
    cIsThisLove.SongName = "Is This Love";

    cPeople.Author = "Birdie";
    cPeople.SongName = "People Help The People";
    cPeople.SetInitFunc(People_Help_The_People::Init);

    cTakeOnMe.Author = "A-ha";
    cTakeOnMe.SongName = "Take On Me";


//   cILoveRocknRoll = "I love Rock'n'Roll";
    //  cIloveRocknRoll.BaseTempo = 91;

    //cIWantAHighwayToHell = "I want a highway to hell"

    // PLAYLIST ORDER IS DEFINED HERE:
    PlaylistData.clear();
    PlaylistData.push_back(&cFirstContext); // Always keep that one in first
    PlaylistData.push_back(&cRigUp);

    PlaylistData.push_back(&cLockedOutOfHeaven);
    PlaylistData.push_back(&cGetLucky);
    PlaylistData.push_back(&cIllusion);
    PlaylistData.push_back(&cManDown);
    PlaylistData.push_back(&cBackToBlack);
    PlaylistData.push_back(&cILoveRockNRoll);
    PlaylistData.push_back(&c25years);
    PlaylistData.push_back(&cHuman);
    PlaylistData.push_back(&cPeople);
    PlaylistData.push_back(&cIFollowRivers);
    PlaylistData.push_back(&cTakeOnMe);
    PlaylistData.push_back(&cLesFillesDesForges);
    PlaylistData.push_back(&cHighwayToHell);
    PlaylistData.push_back(&cAllumerLeFeu);
    PlaylistData.push_back(&cProudMary);


    PlaylistData.push_back(&cBillieJean);
    PlaylistData.push_back(&cNewYorkAvecToi);
    PlaylistData.push_back(&cIFeelGood);
    PlaylistData.push_back(&cShouldIStay);
    PlaylistData.push_back(&cAllInYou);


    PlaylistData.push_back(&cJammin);
    PlaylistData.push_back(&cMonAmantDeSaintJean);
    PlaylistData.push_back(&cLongTrainRunning);
    PlaylistData.push_back(&cGetBack);
    PlaylistData.push_back(&cThatsAllRight);
    PlaylistData.push_back(&cWhatsUp);
    PlaylistData.push_back(&cLesCitesDOr);
    PlaylistData.push_back(&cUptownFunk);
    PlaylistData.push_back(&cLeFreak);
    PlaylistData.push_back(&cRappersDelight);
    PlaylistData.push_back(&cMachistador);
    PlaylistData.push_back(&cAnotherOneBiteTheDust);
    PlaylistData.push_back(&cWot);
    PlaylistData.push_back(&cCapitaineFlam);
    PlaylistData.push_back(&cAllShookUp);
    PlaylistData.push_back(&cUnchainMyHeart);
    PlaylistData.push_back(&cIsntSheLovely);
    PlaylistData.push_back(&cRehab);
    PlaylistData.push_back(&cKnockOnWood);
    PlaylistData.push_back(&cMercy);
    PlaylistData.push_back(&cFlyMeToTheMoon);
    PlaylistData.push_back(&cAllOfMe);
    PlaylistData.push_back(&cCryMeARiver);
    PlaylistData.push_back(&cJustAGigolo);
    PlaylistData.push_back(&cSuperstition);
    PlaylistData.push_back(&cStandByMe);
    PlaylistData.push_back(&cFaith);
    PlaylistData.push_back(&cPapasGotABrandNewBag);
    PlaylistData.push_back(&cMasterBlaster);
    PlaylistData.push_back(&cAuxChampsElysees);
    PlaylistData.push_back(&cAroundTheWorld);
    PlaylistData.push_back(&cDockOfTheBay);
    PlaylistData.push_back(&cJohnnyBeGood);
    PlaylistData.push_back(&cBebopALula);
    PlaylistData.push_back(&cHotelCalifornia);
    PlaylistData.push_back(&cRaggamuffin);
    PlaylistData.push_back(&cAveMaria);
    PlaylistData.push_back(&cWildThoughts);
    PlaylistData.push_back(&cGangstaParadise);
    PlaylistData.push_back(&cBeatIt);
    PlaylistData.push_back(&cLady);
    PlaylistData.push_back(&cChandelier);
    PlaylistData.push_back(&cThisGirl);
    PlaylistData.push_back(&cEncoreUnMatin);
    PlaylistData.push_back(&cQuandLaMusiqueEstBonne);
    PlaylistData.push_back(&cDumbo);
    PlaylistData.push_back(&cIsThisLove);


    // Set the current active context here.
    // By default: that would be PlaylistData.begin()...
    // Note that std::list cannot be accessed randomly.
    PlaylistPosition = PlaylistData.begin();
    TContext Context = **PlaylistPosition;
    // Context.Init(); // The .Init() function must be called manually. I tried to overload the assignment operator but did not find a good way to do it.


    // These so-called Playlists are a bit fictive and only a copy of the original playlist, but sorted by author or by song name.
    PlaylistData_ByAuthor = PlaylistData;
    PlaylistData_ByAuthor.sort(CompareTContextByAuthor);
    PlaylistData_BySongName = PlaylistData;
    PlaylistData_BySongName.sort(CompareTContextBySongName);
}


// Redraw screen 5 times a second.
void threadRedraw(void)
{
    TContext Context;
    while(1)
    {
        waitMilliseconds(200);
        Context = **PlaylistPosition;
        win_context_current.Erase();
        mvwprintw(win_context_current.GetRef(), 0,0, Context.SongName.c_str());

        win_context_usage.Erase();
        {
            auto it = Context.Pedalboard.PedalsDigital.begin();
            while(it != Context.Pedalboard.PedalsDigital.end())
            {
                TPedalDigital PedalDigital = *it;
                wprintw(win_context_usage.GetRef(), "Digital Pedal %i: %s\n", PedalDigital.GetNumber(), PedalDigital.GetComment().c_str());
                it++;
            }
        }
        {
            auto it = Context.Pedalboard.PedalsAnalog.begin();
            while(it != Context.Pedalboard.PedalsAnalog.end())
            {
                TPedalAnalog PedalAnalog = *it;
                wprintw(win_context_usage.GetRef(), "Expression CC %i: %s\n", PedalAnalog.GetControllerNumber(), PedalAnalog.GetComment().c_str());
                it++;
            }
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


void SelectContextByName(void)
{

}

void SelectContextInPlaylist (std::list<TContext*> &ContextList, bool ShowAuthor)
{
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
        if (*it == *PlaylistPosition)
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
        //char *theItem = chtype2Char (scrollList->item[selection]);

        // Iterate through the list to set the correct Context.
        idx = 0;
        for (it = ContextList.begin(); it != ContextList.end(); it++)
        {
            if (idx == scrollList->currentItem)
            {
                PlaylistPosition = it;
                TContext Context = **PlaylistPosition;
                Context.Init();

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

// Fixme: create routine that computes tempo
// In the past, was using aubio library, but that did not work well enough
// Use MIDI events instead
// Not implemented yet
float PCMInputTempoConfidence = 0;
float PCMInputTempoValue = 0;

// Display a metronome on the User Specific window.
void threadMetronome (void)
{
    const int PulseDuration = 100;
    // Interval of confidence of live tempo above which suggestions are made.
    // below that level, suggestions are not made.
    const float ConfidenceThreshold = 0.13;
    TContext Context;
    init_pair(1, COLOR_WHITE, COLOR_BLACK);
    init_pair(2, COLOR_RED, COLOR_BLACK);
    init_pair(3, COLOR_GREEN, COLOR_BLACK);
    init_pair(4, COLOR_RED, COLOR_WHITE);
    init_pair(5, COLOR_BLUE, COLOR_WHITE);
    init_pair(6, COLOR_BLACK, COLOR_GREEN);


    while (1)
    {
        // Get current Base Tempo of the song
        Context = **PlaylistPosition;
        float Tempo = Context.BaseTempo;
        if (Tempo < 30) Tempo = 30;
        if (Tempo > 200) Tempo = 200;
        // Compute wait time from the tempo
        unsigned long int delay_ms = 60000.0 / Tempo - PulseDuration;
        // Display metronome
        wattron(win_context_user_specific.GetRef(), COLOR_PAIR(1));
        wattron(win_context_user_specific.GetRef(), A_BOLD | A_REVERSE);
        mvwprintw(win_context_user_specific.GetRef(), 0, 0, "BASE TEMPO:%03d",(int)Tempo);
        win_context_user_specific.Refresh();
        waitMilliseconds(PulseDuration);
        wattroff(win_context_user_specific.GetRef(), A_BOLD | A_REVERSE);
        mvwprintw(win_context_user_specific.GetRef(), 0, 0, "BASE TEMPO:%03d",(int)Tempo);
        if (PCMInputTempoConfidence < ConfidenceThreshold)
        {
            wattron(win_context_user_specific.GetRef(), COLOR_PAIR(2));
        }
        else
        {
            wattron(win_context_user_specific.GetRef(), COLOR_PAIR(3));
        }
        mvwprintw(win_context_user_specific.GetRef(), 1, 0, "CURRENT TEMPO:%03d (%02.2f)",(int) PCMInputTempoValue, PCMInputTempoConfidence);
        float DeltaTempo = PCMInputTempoValue - Tempo;
        if (PCMInputTempoConfidence > ConfidenceThreshold && DeltaTempo < -2)
        {
            wattron(win_context_user_specific.GetRef(), COLOR_PAIR(4));
            mvwprintw(win_context_user_specific.GetRef(), 2, 0, "SPEED UP  BY %03d BPM", -(int)DeltaTempo);
        }
        else if (PCMInputTempoConfidence > ConfidenceThreshold && DeltaTempo > 2)
        {
            wattron(win_context_user_specific.GetRef(), COLOR_PAIR(5));
            mvwprintw(win_context_user_specific.GetRef(), 2, 0, "SLOW DOWN BY %03d BPM", (int)DeltaTempo);
        }
        else if (PCMInputTempoConfidence > ConfidenceThreshold)
        {
            wattron(win_context_user_specific.GetRef(), COLOR_PAIR(6));
            mvwprintw(win_context_user_specific.GetRef(), 2, 0, "TEMPO IS ABOUT RIGHT");
        }
        else if (PCMInputTempoConfidence < ConfidenceThreshold)
        {
            wattron(win_context_user_specific.GetRef(), COLOR_PAIR(2));
            mvwprintw(win_context_user_specific.GetRef(), 2, 0, "********************");
        }


        win_context_user_specific.Refresh();
        waitMilliseconds(delay_ms);
    }
}


void ResetKeyboardPerformance(void)
{
    XV5080.PerformanceSelect(TXV5080::PerformanceGroup::USER, TInt_1_128(1));
    XV5080.TemporaryPerformance.PerformancePart[0].ReceiveSwitch.Set(1);
    XV5080.TemporaryPerformance.PerformancePart[1].ReceiveSwitch.Set(0);
    XV5080.TemporaryPerformance.PerformancePart[2].ReceiveSwitch.Set(0);

    XV5080.TemporaryPerformance.PerformancePart[MASTER_KBD_PART_INDEX].ReceiveSwitch.Set(1);
    XV5080.TemporaryPerformance.PerformancePart[MASTER_KBD_PART_INDEX].ReceiveChannel.Set_1_16(MASTER_KBD_XV5080_MIDI_CHANNEL);
    XV5080.TemporaryPerformance.PerformancePart[MASTER_KBD_PART_INDEX].SelectPatch(TXV5080::PatchGroup::PR_A, 4); // Nice Piano

    XV5080.TemporaryPerformance.PerformancePart[MASTER_KBD_PART_INDEX+1].ReceiveSwitch.Set(0);
    XV5080.TemporaryPerformance.PerformancePart[MASTER_KBD_PART_INDEX+1].ReceiveChannel.Set_1_16(MASTER_KBD_XV5080_MIDI_CHANNEL);
    XV5080.TemporaryPerformance.PerformancePart[MASTER_KBD_PART_INDEX+1].SelectPatch(TXV5080::PatchGroup::PR_C, 59); // Warmth

    XV5080.TemporaryPerformance.PerformancePart[MASTER_KBD_PART_INDEX+2].ReceiveSwitch.Set(0);
    XV5080.TemporaryPerformance.PerformancePart[MASTER_KBD_PART_INDEX+2].ReceiveChannel.Set_1_16(MASTER_KBD_XV5080_MIDI_CHANNEL);
    XV5080.TemporaryPerformance.PerformancePart[MASTER_KBD_PART_INDEX+2].SelectPatch(TXV5080::PatchGroup::PR_E, 55); // Ethereal Strings

    //TXV5080::PatchGroup::PR_C, 36); // Warm Strings

    XV5080.TemporaryPerformance.PerformancePart[MASTER_KBD_PART_INDEX+3].ReceiveSwitch.Set(0);
    XV5080.TemporaryPerformance.PerformancePart[MASTER_KBD_PART_INDEX+3].ReceiveChannel.Set_1_16(MASTER_KBD_XV5080_MIDI_CHANNEL);
    XV5080.TemporaryPerformance.PerformancePart[MASTER_KBD_PART_INDEX+3].SelectPatch(TXV5080::PatchGroup::PR_E, 35); // Rocker Organ

    XV5080.TemporaryPerformance.PerformancePart[MASTER_KBD_PART_INDEX+4].ReceiveSwitch.Set(0);
    XV5080.TemporaryPerformance.PerformancePart[MASTER_KBD_PART_INDEX+4].ReceiveChannel.Set_1_16(MASTER_KBD_XV5080_MIDI_CHANNEL);
    XV5080.TemporaryPerformance.PerformancePart[MASTER_KBD_PART_INDEX+4].SelectPatch(TXV5080::PatchGroup::PR_E, 14); // Rhodes tremolo

    XV5080.TemporaryPerformance.PerformancePart[MASTER_KBD_PART_INDEX+5].ReceiveSwitch.Set(0);
    XV5080.TemporaryPerformance.PerformancePart[MASTER_KBD_PART_INDEX+5].ReceiveChannel.Set_1_16(MASTER_KBD_XV5080_MIDI_CHANNEL);
    XV5080.TemporaryPerformance.PerformancePart[MASTER_KBD_PART_INDEX+5].SelectPatch(TXV5080::PatchGroup::PR_E, 98); // New R&R Brass

    XV5080.TemporaryPerformance.PerformancePart[MASTER_KBD_PART_INDEX+6].ReceiveSwitch.Set(0);
    XV5080.TemporaryPerformance.PerformancePart[MASTER_KBD_PART_INDEX+6].ReceiveChannel.Set_1_16(MASTER_KBD_XV5080_MIDI_CHANNEL);
    XV5080.TemporaryPerformance.PerformancePart[MASTER_KBD_PART_INDEX+6].SelectPatch(TXV5080::PatchGroup::PR_F, 75); // Andreas' Cave

    XV5080.TemporaryPerformance.PerformancePart[MASTER_KBD_PART_INDEX+7].ReceiveSwitch.Set(0);
    XV5080.TemporaryPerformance.PerformancePart[MASTER_KBD_PART_INDEX+7].ReceiveChannel.Set_1_16(MASTER_KBD_XV5080_MIDI_CHANNEL);
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
        init_color(COLOR_BLACK, 400, 200, 0);
        init_color(COLOR_WHITE, 1000, 700, 400);
    }
    else
    {
        printf("CANNOT SUPPORT COLORS");
    }
    cbreak(); // Disable line buffering, pass on all data
    keypad(stdscr, TRUE); // support F- keys



    win_midi_in.Init("IN", term_lines -3, 6, 3, term_cols-6-6);
    win_midi_out.Init("OUT", term_lines -3, 6, 3, term_cols-6);
    win_context_prev.Init("CONTEXT PREV", 3, 0.33*term_cols, 0, 0);
    win_context_current.Init("CONTEXT CURRENT", 3, 0.33*term_cols, 0, 0.33*term_cols +1);
    win_context_next.Init("CONTEXT NEXT", 3, 0.33*term_cols, 0, 0.33*term_cols +1 + 0.33*term_cols +1);
    win_debug_messages.Init("DEBUG MESSAGES", 10, term_cols -6-6, term_lines-11, 0);
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
    refresh();

    // Initialize this banner at the full terminal width
    Banner.Init(term_cols, term_lines/2 -4, 0, 800);
    Banner.SetMessage("OXFORD - LE GROUPE");

    // Create thread that scans midi messages
    // Initialize MIDI port A - this will spawn a new thread that parses MIDI IN
    MIDI_A.Init(name_midi_hw_MIDISPORT_A, MIDI_A_IN_NoteOnEvent, NULL, MIDI_A_IN_CC_Event, NULL);

    // Same for MIDI port B
    MIDI_B.Init(name_midi_hw_MIDISPORT_B, MIDI_B_IN_NoteOnEvent, MIDI_B_IN_NoteOffEvent, MIDI_B_IN_CC_Event, MIDI_B_IN_PB_Event);

    // Create task that redraws screen at fixed intervals
    std::thread thread2(threadRedraw);

    // Create the thread that refreshes the metronome
    std::thread thread3(threadMetronome);

    ComputerKeyboard::Initialize();

    // Create thread that scans the keyboard
    std::thread thread4(MiniSynth::threadKeyboard);

    // Setup the Master Keyboard default patches on each part
    std::this_thread::sleep_for(std::chrono::seconds(1));

    ResetKeyboardPerformance();



#ifdef TEST_XV5080
    test_XV5080();
#endif

#endif

    extern int extract_tempo ();

    //extract_tempo();
    // Do nothing
    while(1)
    {
        sleep(1);

    }
}
