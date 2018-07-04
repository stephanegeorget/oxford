// Assuming that ALSA is used throughout.
// Make sure you have installed libasound2-dev, to get the headers
// Make sure you also have ncurses: libncurses5-dev libncursesw5-dev
//
// The USB midiman MidiSport 2x2 hardware requires midisport-firmware: install it with
// apt-get intstall midisport-firmware
//
// use amidi -l to list midi hardware devices
// don't forget to link with asound, pthread, cdk (sometimes called libcdk), and panel
//
// use pmidi -l to list midi devices for pmidi, which is for midi file playback
// you probably must build cdk locally, get in the cdk folder, ./configure, then make,
// and make sure that code blocks links with the library generated in ???
// run ldconfig as root
// you must also have the aubio library; go to aubio, then ./waf configure, ./waf build
// ./waf install is not really needed, just have codeblocks link with the aubio library
// generated in: build/source/libaubio.so



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

int stop = 0;
// Array of two pointers to a snd_rawmidi_t
// Position 0 for the first port of the midisport,
// Position 1 for the second port of the midisport.
// E.g. handle_midi_in[0] gets us a handle for the first midi port.
// Initialize with zero = invalid pointers.
std::array<snd_rawmidi_t*,2> handle_midi_hw_in = {0, 0};
// Same for the midisport midi outputs (two of them).
std::array<snd_rawmidi_t*,2> handle_midi_hw_out = {0, 0};

// Array which holds the string name of the hardware device,
// one for the first midi port (IN1/OUT1) and second for the
// second midi port (IN2/OUT2).
std::array<std::string, 2> name_midi_hw = {"hw:1,0,0", "hw:1,0,1"}; // obtained with amidi -l

std::string midi_sequencer_name = "20:0"; // obtained with pmidi -l

// Mutex used to prevent ncurses refresh routines from being called from
// concurrent threads.
std::mutex ncurses_mutex;




// This is a ncurses cdk panel, with a boxed window on it, that can't
// be touched, and a "free text" window inside the boxed window.
// It makes it easy to manage showing (and hiding, thanks panel), of
// text, including automatically scrolling it, _inside_ a nice clean
// window with a box.
// To get a reference to the inner window (on which to display text), for instance
// with wprinw, use ::GetRef(). E.g.: wprintw(MyBoxedWindow.GetRef(), "Hello World!\n");
// Of course, call ::Init(...) beforehand.
class TBoxedWindow
{
private:
    PANEL * Panel; // Panel is the cdk object that allows showing/hiding parts of the screen.
    WINDOW * BoxedWindow; // outer window, shown with a box, prevent writing on that
    WINDOW * SubWindow; // inner space of the window, usable to display text
public:
    TBoxedWindow(void) {};
    void Init(char* name, int height, int width, int starty, int startx)
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

    void Show(void)
    {
        std::lock_guard<std::mutex> lock(ncurses_mutex);
        show_panel(Panel);
        update_panels();
        doupdate();
    }

    void Hide(void)
    {
        std::lock_guard<std::mutex> lock(ncurses_mutex);
        hide_panel(Panel);
        update_panels();
        doupdate();
    }

    WINDOW * GetRef(void)
    {
        std::lock_guard<std::mutex> lock(ncurses_mutex);
        return SubWindow;
    }

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

    void PutOnTop(void)
    {
        std::lock_guard<std::mutex> lock(ncurses_mutex);
        top_panel(Panel);
    }
    void Erase(void)
    {
        std::lock_guard<std::mutex> lock(ncurses_mutex);
        wclear(SubWindow);
    }
};


// Create various boxed windows to construct the display
TBoxedWindow win_midi_in;
TBoxedWindow win_midi_out;
TBoxedWindow win_debug_messages;
TBoxedWindow win_context_prev;
TBoxedWindow win_context_current;
TBoxedWindow win_context_next;
TBoxedWindow win_context_usage;
TBoxedWindow win_context_user_specific;
TBoxedWindow win_context_select_menu;
TBoxedWindow win_big_message;









// Wait "milliseconds" milliseconds.
#pragma reentrant
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


// Intermediate structure used to communicate information to a spawned thread.
typedef struct
{
    void (*pFunction)(void *);
    unsigned long int Delay_ms;
    void * pFuncParam;
} TExecuteAfterTimeoutStruct;

// Thread used to support the functionality of function ExecuteAfterTimeout.
#pragma reentrant
void ExecuteAfterTimeout_Thread(TExecuteAfterTimeoutStruct Message)
{
    // Delay execution
    waitMilliseconds(Message.Delay_ms);
    // Call "pFunction(pFuncParam)"
    (Message.pFunction)( Message.pFuncParam );
}

// This function, "ExecuteAfterTimeout", returns to the caller right away, but it spawns a thread,
// that waits Timeout_ms milliseconds, and then makes a call to function pFunc, which must have the prototype: void MyFunction(void * myParam);
// Then the thread disappears.
#pragma reentrant
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


/** Everything needed to print Big Character banners */
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
        canvas = new(char[canvas_width*canvas_height]);
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
        int i=0;

        wattron(pBoxedWindow->GetRef(), COLOR_PAIR(7));

        mvwprintw(pBoxedWindow->GetRef(), 0, 0, "");
        for (unsigned int lin = 0; lin < canvas_height; lin++)
        {
            for (unsigned int col = 0+offset; col < canvas_width+offset; col++)
            {
                int teststring_index = col / char_width;
                //printf("%i   ", teststring_index);
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


// This is the main banner object. Call its member function "SetMessage" to
// temporarily display a message in very large characters on the screen.
TBanner Banner;


// I try to avoid the use of function prototypes and headers,
// but these are required due to circular references in the program.
void ContextPreviousPress(void);
void ContextPreviousRelease(void);
void ContextNextPress(void);
void ContextNextRelease(void);
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


#include <linux/input.h>
#undef _INPUT_EVENT_CODES_H
#include <linux/input-event-codes.h>
namespace Keyboard
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
        exit(10);
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











// A pedal, from the pedalboard (Behringer FCB1010), with analog action
// (i.e. can take a range of positions between two bounds).
// Such a pedal must be programmed (from the FCB1010) to send Midi Control Change
// events on a particular "Controller Number".
// When this program receives CC events from ControllerNumber, the OnChange callback
// function is called.
// These callback functions must be set up properly during the initialization of the
// TPedalAnalog object.
// As of today, only CC on Midi channel 1 are recognized.
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




// A pedal, from the pedalboard (Behringer FCB1010), with digital action
// (i.e. can take two positions: pressed, or released.)
// Such a pedal must be programmed (from the FCS1010) to send Midi Note ON events,
// Each Digital Pedal is associated with a particular Note Number.
// When this program receives Note ON event for Note Number "Number", the corresponding
// OnPress and OnRelease callback functions are called. These callback functions must be
// set up properly during the initialization of the TPedalDigital object.
//
// As of today, only Midi events sent on Midi Channel 2 are recognized.
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





// A pedalboard is a collection of digital and analog pedals.
// In the case of the Behringer FCB1010, there are 10 digital pedals
// and 2 analog pedals.
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


// A Context is a collection of features that belong to a particular performance. In other
// words, a context gathers everything that is needed to play a song:
// Song name and Author are used to search through all the available Contexts,
// Then the base tempo, arrangement of the pedalboards (i.e. which pedal does what), and
// initialization (with the Init function) that is performed when that particular
// context is activated.
//
// Only one context can be active at a point in time. Two contexts cannot be active at the same time.
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

// Very important variable:
// Gather all the data about the playlist, sorted by arbitrary "playlist" order.
// It is that variables that represents a playlist.
// If is oksy to define several playlists, but then assign one to PlaylistData.
// Reads: PlaylistData is a list of TContext pointers. Please note that PlaylistData
// keeps track of pointer to contexts, not actual contexts. This is useful to generate
// lists with different orders, yet keeping the pool of context objects the same, and only
// REFERENCED by these lists (not copied into them).
std::list<TContext *> PlaylistData;

// Take the raw information of PlaylistData, and sort alphabetically by Author
std::list<TContext *> PlaylistData_ByAuthor;

// Same, sorted alphabetically by Song Name
std::list<TContext *> PlaylistData_BySongName;

// Very important global variable: points to the current context in the playlist.
// This is a list::iterator, so dereferencing it (e.g. *PlaylistPosition)
// gives an element, that is a pointer to a Context. Then dereference another
// time to get to the context.
// E.g: TContext MyContext;
//      MyContext = **PlaylistPosition;
//      printf("%s", MyContext.SongName.c_str());

std::list<TContext *>::iterator PlaylistPosition;



// Here, the actual TContext objects are instanciated. One for each context, which
// more or less represents one for each *song* in the playlist, in the band/musical sense.
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


// This function is needed to sort lists of elements.
// Sorting a list requires that a comparison function be provided.
// But how can we say "that context comes before that one"? It depends on
// how we want to sort, namely by Author...
bool CompareTContextByAuthor(const TContext* first, const TContext* second)
{
    std::string first_nocase = first->Author;
    std::string second_nocase = second->Author;
    std::transform(first_nocase.begin(), first_nocase.end(), first_nocase.begin(), ::tolower);
    std::transform(second_nocase.begin(), second_nocase.end(), second_nocase.begin(), ::tolower);

    return first_nocase < second_nocase;
}

// ... and by Song Name.
bool CompareTContextBySongName(const TContext* first, const TContext* second)
{
    std::string first_nocase = first->SongName;
    std::string second_nocase = second->SongName;
    std::transform(first_nocase.begin(), first_nocase.end(), first_nocase.begin(), ::tolower);
    std::transform(second_nocase.begin(), second_nocase.end(), second_nocase.begin(), ::tolower);

    return first_nocase < second_nocase;
}


// This function is called each time one wants to go to the previous context.
// Since a FCB1010 pedal is allocated to that (normally pedal 6), two events are generated:
// When you press on the pedal...
void ContextPreviousPress(void)
{
    if (PlaylistPosition != PlaylistData.begin())
    {
        PlaylistPosition--;
        TContext Context = **PlaylistPosition;
        Context.Init();
    }
}

// ... and when you release the pedal.
void ContextPreviousRelease(void)
{
    // And in that case, do nothing.
}

// Sames goes with Pedal 7, go to go the next context (you'll say the *next song*...).

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

void ContextNextRelease(void)
{
    // And in that case, do nothing.
}


// Talk to ALSA and open a midi port in RAW mode, for data going IN.
// (into the computer, into this program)
// There are two physical IN ports on the midisport, called: 0 and 1,
// that must be passed as a parameter.
void StartRawMidiIn(int portnum)
{
    if (handle_midi_hw_in[portnum] == 0)
    {
        int err = snd_rawmidi_open(&handle_midi_hw_in[portnum], NULL, name_midi_hw[portnum].c_str(), 0);
        if (err)
        {
            wprintw(win_debug_messages.GetRef(), "snd_rawmidi_open %s failed: %d\n", name_midi_hw[portnum].c_str(), err);
        }
    }
}


// Talk to ALSA and open a midi port in RAW mode, for data going OUT.
// (from this program, out of the computer, to the expander, to the Rack Eleven, etc.)
// There are two physical IN ports on the midisport, called: 0 and 1,
// that must be passed as a parameter.
void StartRawMidiOut(int portnum)
{
    if (handle_midi_hw_out[portnum] == 0)
    {
        int err = snd_rawmidi_open(NULL, &handle_midi_hw_out[portnum], name_midi_hw[portnum].c_str(), 0);
        if (err)
        {
            wprintw(win_debug_messages.GetRef(), "snd_rawmidi_open %s failed: %d\n", name_midi_hw[portnum].c_str(), err);
        }
    }
}


// Close the RAW midi IN port, for port "portnum" (0 or 1)
void StopRawMidiIn(int portnum)
{
    if (handle_midi_hw_in[portnum] != 0)
    {
        snd_rawmidi_drain(handle_midi_hw_in[portnum]);
        snd_rawmidi_close(handle_midi_hw_in[portnum]);
        handle_midi_hw_in[portnum] = 0;
    }

}

// Close the RAW midi OUT port, for port "portnum" (0 or 1)
void StopRawMidiOut(int portnum)
{
    if(handle_midi_hw_out[portnum] != 0)
    {
        snd_rawmidi_drain(handle_midi_hw_out[portnum]);
        snd_rawmidi_close(handle_midi_hw_out[portnum]);
        handle_midi_hw_out[portnum] = 0;
    }
}


// Shows usage - but real programmers don't need that; they look at the code :-)
static void usage(void)
{
    wprintw(win_debug_messages.GetRef(), "usage: fix me\n");
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


// Helper structure used to gather some information required to merely play a note.
// Including how long it should last.
typedef struct
{
    unsigned char NoteNumber;
    unsigned char Velocity;
    unsigned char Channel;
    unsigned int DurationMS;
} TPlayNoteMsg;


// This class sends a Note ON event on MIDI channel managed by handle_out.
// Example: TMidiNoteOnEvent NoteOnEvent(2, 60, 100);
// The real stuff happends upon the object instanciation. Optionally, one can send the message again
// after by calling NoteOnEvent.sendToMidi(), but that would send it a second time.
class TMidiNoteOnEvent
{
public:
    unsigned char NoteOnField = 9; // See MIDI specifications
    unsigned char charArray[3];
    int PortNumber = 0;

    void sendToMidi(void)
    {
        wprintw(win_midi_out.GetRef(), "%i\n%i\n%i\n", (int) charArray[0], (int) charArray[1], (int) charArray[2]);
        if (handle_midi_hw_out[PortNumber] != 0)
        {
            snd_rawmidi_write(handle_midi_hw_out[PortNumber], &charArray, sizeof(charArray));
        }
    }

    TMidiNoteOnEvent(TPlayNoteMsg * PlayNoteMsg);

    TMidiNoteOnEvent(unsigned int Channel, unsigned int NoteNumber,
                     unsigned int Velocity);

    TMidiNoteOnEvent(unsigned int Channel,
                     unsigned int NoteNumber, unsigned int Velocity, int PortNumber_param);

    void Init(unsigned int Channel, unsigned int NoteNumber,
              unsigned int Velocity);


    void Init(unsigned int Channel, unsigned int NoteNumber,
              unsigned int Velocity, int PortNumber_param);

};

TMidiNoteOnEvent::TMidiNoteOnEvent(unsigned int Channel,
                                   unsigned int NoteNumber, unsigned int Velocity)
{
    Init(Channel, NoteNumber, Velocity, 0);
}

TMidiNoteOnEvent::TMidiNoteOnEvent(unsigned int Channel,
                                   unsigned int NoteNumber, unsigned int Velocity, int PortNumber_param)
{
    Init(Channel, NoteNumber, Velocity, PortNumber_param);
}


void TMidiNoteOnEvent::Init(unsigned int Channel, unsigned int NoteNumber,
                            unsigned int Velocity)
{

    if (Channel < 1)
        Channel = 1;
    if (Channel > 16)
        Channel = 16;

    charArray[0] = (NoteOnField << 4) + ((Channel - 1) & 0x0F);
    charArray[1] = NoteNumber & 0x7F;
    charArray[2] = Velocity;
    wprintw(win_debug_messages.GetRef(), "Note number: %i\n", (int) charArray[1]);

    sendToMidi();

}


void TMidiNoteOnEvent::Init(unsigned int Channel, unsigned int NoteNumber,
                            unsigned int Velocity, int PortNumber_param)

{
    PortNumber = PortNumber_param;
    Init(Channel, NoteNumber, Velocity);

}

TMidiNoteOnEvent::TMidiNoteOnEvent(TPlayNoteMsg * PlayNoteMsg)
{
    Init(PlayNoteMsg->Channel, PlayNoteMsg->NoteNumber, PlayNoteMsg->Velocity);
}


// Same for Note Off MIDI event.
// If you want to send out some MIDI notes,
// you're better off using PlayNote(), rather than drilling down to the Note ON / OFF events...
class TMidiNoteOffEvent: public TMidiNoteOnEvent
{
    using TMidiNoteOnEvent::TMidiNoteOnEvent;

public:
    unsigned char NoteOnField = 8;
//    explicit TMidiNoteOffEvent(TPlayNoteMsg * PlayNoteMsg);
};

class TMidiProgramChange
{
private:
    unsigned char MidiFunctionID = 0xC; // See MIDI specifications
    unsigned char charArray[2];
    int PortNumber = 0;

    void sendToMidi(void)
    {
        wprintw(win_midi_out.GetRef(), "%i\n%i\n", (int) charArray[0], (int) charArray[1]);
        if(handle_midi_hw_out[PortNumber] != 0)
        {
            snd_rawmidi_write(handle_midi_hw_out[PortNumber], &charArray, sizeof(charArray));
        }
    }
    ;

public:

    TMidiProgramChange(unsigned char Channel, unsigned char Program)
    {
        TMidiProgramChange(Channel, Program, 0);
    }

    TMidiProgramChange(unsigned char Channel, unsigned char Program, int PortNumber_param)
    {
        PortNumber = PortNumber_param;

        if (Channel < 1)
            Channel = 1;
        if (Channel > 16)
            Channel = 16;

        charArray[0] = (MidiFunctionID << 4) + ((Channel - 1) & 0x0F);
        charArray[1] = (Program - 1) & 0x7F;
        wprintw(win_debug_messages.GetRef(), "Program Change: Channel %i; Program %i\n", (int) Channel, (int) Program);
        sendToMidi();
    }

};

// Send out a CC (Control Change) MIDI event.
class TMidiControlChange
{
private:
    unsigned char MidiFunctionID = 0xB;
    unsigned char charArray[3];
    int PortNumber = 0;

    void sendToMidi(void)
    {
        if (handle_midi_hw_out[PortNumber] != 0)
        {
            wprintw(win_midi_out.GetRef(), "%i\n%i\n%i\n", (int) charArray[0], (int) charArray[1], (int) charArray[2]);
            snd_rawmidi_write(handle_midi_hw_out[PortNumber], &charArray, sizeof(charArray));
        }
    }
    ;

public:

    TMidiControlChange(unsigned char Channel, unsigned char ControlNumber,
                       unsigned char ControllerValue)
    {
        TMidiControlChange(Channel, ControlNumber, ControllerValue, 0);
    }

    TMidiControlChange(unsigned char Channel, unsigned char ControlNumber,
                       unsigned char ControllerValue, int PortNumber_param)
    {
        PortNumber = PortNumber_param;
        if (Channel < 1)
            Channel = 1;
        if (Channel > 16)
            Channel = 16;

        charArray[0] = (MidiFunctionID << 4) + ((Channel - 1) & 0x0F);
        charArray[1] = (ControlNumber) & 0x7F;
        charArray[2] = (ControllerValue);
        wprintw(win_debug_messages.GetRef(), "Control Change - Controller Number: %i; Controller Value: %i\n", (int) charArray[1], (int) charArray[2]);
        sendToMidi();
    }

};


// Thread used by PlayNote() to keep track of time, for each note.
void playNoteThread(TPlayNoteMsg msg)
{
    TMidiNoteOnEvent MidiNoteOnEvent(&msg);
    waitMilliseconds(msg.DurationMS);
    wprintw(win_debug_messages.GetRef(), "playNoteThread\n");
    msg.Velocity = 0;
    TMidiNoteOffEvent MidiNoteOffEvent(&msg);
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
    TMidiNoteOnEvent NoteOn(channel, octave *12 + noteInScale, 100);
}

void StopNote(void * pVoid)
{
    int noteInScale = (long int) pVoid;
    TMidiNoteOnEvent NoteOff(channel, octave *12 + noteInScale, 100);
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
        TMidiProgramChange PC1(channel, program);
    }
}

void ProgramMore(void * pVoid)
{
    program++;
    wprintw(win_debug_messages.GetRef(), "Program %i\n", program);
    {
        TMidiProgramChange PC2(channel, program);
    }
}


void ChannelLess(void * pVoid)
{
    channel--;
}

void ChannelMore(void * pVoid)
{
    channel++;
}

void Space(void * pVoid)
{
    SelectContextInPlaylist(PlaylistData, false);
}

void B(void* pVoid)
{
    SelectContextInPlaylist(PlaylistData_BySongName, false);
}

void N(void * pVoid)
{
    SelectContextInPlaylist(PlaylistData_ByAuthor, true);
}

void Z(void * pVoid)
{
    system("aplay ./wav/FXCarpenterLaserBig.wav &");
}

// Scan keyboard for events, and process keypresses accordingly.
void threadKeyboard(void)
{
    char *message;


    // create all the notes in
    Keyboard::RegisterEventCallbackPressed(KEY_Q, StartNote, 0);
    Keyboard::RegisterEventCallbackPressed(KEY_2, StartNote, (void *)1);
    Keyboard::RegisterEventCallbackPressed(KEY_W, StartNote, (void *)2);
    Keyboard::RegisterEventCallbackPressed(KEY_3, StartNote, (void *)3);
    Keyboard::RegisterEventCallbackPressed(KEY_E, StartNote, (void *)4);
    Keyboard::RegisterEventCallbackPressed(KEY_4, StartNote, (void *)5);
    Keyboard::RegisterEventCallbackPressed(KEY_R, StartNote, (void *)6);
    Keyboard::RegisterEventCallbackPressed(KEY_T, StartNote, (void *)7);
    Keyboard::RegisterEventCallbackPressed(KEY_6, StartNote, (void *)8);
    Keyboard::RegisterEventCallbackPressed(KEY_Y, StartNote, (void *)9);
    Keyboard::RegisterEventCallbackPressed(KEY_7, StartNote, (void *)10);
    Keyboard::RegisterEventCallbackPressed(KEY_U, StartNote, (void *)11);
    Keyboard::RegisterEventCallbackPressed(KEY_I, StartNote, (void *)12);
    Keyboard::RegisterEventCallbackPressed(KEY_9, StartNote, (void *)13);
    Keyboard::RegisterEventCallbackPressed(KEY_O, StartNote, (void *)14);
    Keyboard::RegisterEventCallbackPressed(KEY_0, StartNote, (void *)15);
    Keyboard::RegisterEventCallbackPressed(KEY_P, StartNote, (void *)16);
    Keyboard::RegisterEventCallbackPressed(KEY_MINUS, StartNote, (void *)17);
    Keyboard::RegisterEventCallbackPressed(KEY_LEFTBRACE, StartNote, (void *)18);
    Keyboard::RegisterEventCallbackPressed(KEY_EQUAL, StartNote, (void *)19);
    Keyboard::RegisterEventCallbackPressed(KEY_RIGHTBRACE, StartNote, (void *)20);



    Keyboard::RegisterEventCallbackReleased(KEY_Q, StopNote, 0);
    Keyboard::RegisterEventCallbackReleased(KEY_2, StopNote, (void *)1);
    Keyboard::RegisterEventCallbackReleased(KEY_W, StopNote, (void *)2);
    Keyboard::RegisterEventCallbackReleased(KEY_3, StopNote, (void *)3);
    Keyboard::RegisterEventCallbackReleased(KEY_E, StopNote, (void *)4);
    Keyboard::RegisterEventCallbackReleased(KEY_4, StopNote, (void *)5);
    Keyboard::RegisterEventCallbackReleased(KEY_R, StopNote, (void *)6);
    Keyboard::RegisterEventCallbackReleased(KEY_T, StopNote, (void *)7);
    Keyboard::RegisterEventCallbackReleased(KEY_6, StopNote, (void *)8);
    Keyboard::RegisterEventCallbackReleased(KEY_Y, StopNote, (void *)9);
    Keyboard::RegisterEventCallbackReleased(KEY_7, StopNote, (void *)10);
    Keyboard::RegisterEventCallbackReleased(KEY_U, StopNote, (void *)11);
    Keyboard::RegisterEventCallbackReleased(KEY_I, StopNote, (void *)12);
    Keyboard::RegisterEventCallbackReleased(KEY_9, StopNote, (void *)13);
    Keyboard::RegisterEventCallbackReleased(KEY_O, StopNote, (void *)14);
    Keyboard::RegisterEventCallbackReleased(KEY_0, StopNote, (void *)15);
    Keyboard::RegisterEventCallbackReleased(KEY_P, StopNote, (void *)16);
    Keyboard::RegisterEventCallbackReleased(KEY_MINUS, StopNote, (void *)17);
    Keyboard::RegisterEventCallbackReleased(KEY_LEFTBRACE, StopNote, (void *)18);
    Keyboard::RegisterEventCallbackReleased(KEY_EQUAL, StopNote, (void *)19);
    Keyboard::RegisterEventCallbackReleased(KEY_RIGHTBRACE, StopNote, (void *)20);


    Keyboard::RegisterEventCallbackPressed(KEY_F1, OctaveLess, 0);
    Keyboard::RegisterEventCallbackPressed(KEY_F2, OctaveMore, 0);
    Keyboard::RegisterEventCallbackPressed(KEY_F3, ProgramLess, 0);
    Keyboard::RegisterEventCallbackPressed(KEY_F4, ProgramMore, 0);
    Keyboard::RegisterEventCallbackPressed(KEY_F5, ChannelLess, 0);
    Keyboard::RegisterEventCallbackPressed(KEY_F6, ChannelMore, 0);

    Keyboard::RegisterEventCallbackPressed(KEY_SPACE, Space, 0);
    Keyboard::RegisterEventCallbackPressed(KEY_B, B, 0);
    Keyboard::RegisterEventCallbackPressed(KEY_N, N, 0);
    Keyboard::RegisterEventCallbackPressed(KEY_Z, Z, 0);

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
    using std::vector;
    vector<double> xData113 = { 0, 127/5*1, 127/5*2, 127/5*3, 127/5*4,  120 };
    vector<double> yData113 = { 100, 100, 80, 60, 40,  64  };
    TMidiControlChange cc113(1, 113, interpolate(xData113, yData113, val, false), 1); // last ,1: send to 11R

    vector<double> xData114 = { 0, 127/5*1, 127/5*2, 127 };
    vector<double> yData114 = { 0, 0, 64, 64 };
    TMidiControlChange cc114(1, 114, interpolate(xData114, yData114, val, false), 1); // last ,1: send to 11R

    vector<double> xData115 = { 0, 127/5*2, 127/5*3, 127 };
    vector<double> yData115 = { 0, 0, 64, 64 };
    TMidiControlChange cc115(1, 115, interpolate(xData115, yData115, val, false), 1); // last ,1: send to 11R

    vector<double> xData96 = { 0, 127/5*3, 127/5*4, 127 };
    vector<double> yData96 = { 0, 0, 64, 64 };
    TMidiControlChange cc96(1, 96, interpolate(xData96, yData96, val, false), 1); // last ,1: send to 11R

    vector<double> xData97 = { 0, 127/5*4, 127/5*5, 127 };
    vector<double> yData97 = { 0, 0, 64, 64 };
    TMidiControlChange cc97(1, 97, interpolate(xData97, yData97, val, false), 1); // last ,1: send to 11R


}

void LoPassFilterEnable(void)
{
    // channel 1, control 86 (FX2), value 127 (please turn ON), send to midisport out B (to 11R)
    TMidiControlChange cc(1, 86, 127, 1);
}

void LoPassFilterDisable(void)
{
    TMidiControlChange cc(1, 86, 0, 1);
}

}
}
namespace Rihanna
{
namespace WildThoughts
{
void Chord1_On(void)
{
    TMidiProgramChange pc(2, 96);
    TMidiNoteOnEvent no(2, 53, 100);
    TMidiNoteOnEvent no1(2, 56, 60);

}

void Chord1_Off_Thread(void * pGobble)
{
    TMidiNoteOffEvent no(2, 53, 0);
    TMidiNoteOffEvent no1(2, 56, 0);
}

void Chord1_Off(void)
{
    ExecuteAfterTimeout(Chord1_Off_Thread, 600, NULL);
}

void Chord2_On(void)
{
    TMidiNoteOnEvent no(2, 60, 100);
    TMidiNoteOnEvent no1(2, 63, 100);
}

void Chord2_Off_Thread(void * pGobble)
{
    TMidiNoteOffEvent no(2, 60, 0);
    TMidiNoteOffEvent no1(2, 63, 0);
}

void Chord2_Off(void)
{
    ExecuteAfterTimeout(Chord2_Off_Thread, 600, NULL);
}

void Chord3_On(void)
{
    TMidiNoteOnEvent no(2, 50, 100);
    TMidiNoteOnEvent no1(2, 43, 100);
}

void Chord3_Off_Thread(void * pGobble)
{
    TMidiNoteOffEvent no(2, 50, 0);
    TMidiNoteOffEvent no1(2, 43, 0);
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
        TMidiControlChange cc(i, 0x78, 0);
        TMidiControlChange cc2(i, 0x79, 0);
        TMidiControlChange cc3(i, 0x7B, 0);
        TMidiControlChange cc4(i, 0x7C, 0);
    }
}





typedef struct
{
public:
    int NoteNumber;
    float NoteDuration; // In fraction of a pulse
} TNote;


// Represents and plays a short sequence of notes, at a tempo that is computed from the time
// that mapsed between the calls to Start_PedalPressed and Start_PedalReleased.
// Upon Start_PedalPressed, it starts playing the sequence (first note),
// Upon Start_PedalReleased, it continues to play the sequence at said tempo.
// Of course, there is a requirement that the time duration between the first and second note
// should be 1, i.e. one "unit of time", that lapsed between the pedal pressed and pedal released.
// Stop_PedalPressed stops the sequence in progress, if any.
class TSequence
{
private:
    struct timeval tv1, tv2;
    pthread_t thread;
    int event_flag = 0;
    int BeatTime_ms = 0;
    std::list<TNote> MelodyNotes;
    std::list<TNote>::iterator it;
    TNote CurrentNote;
    void (*pFuncNoteOn)(int NoteNumber) = NULL;
    void (*pFuncNoteOff)(int NoteNumber) = NULL;
    int RootNoteNumber = 60;

    static void * SequencerThread(void * pParam)
    {
        TSequence * pSeq = (TSequence *) pParam;
        pSeq->Sequencer();
    }

    void Sequencer(void)
    {
        event_flag = 1;
        while(event_flag)
        {
            pFuncNoteOff(CurrentNote.NoteNumber + RootNoteNumber);
            it++;
            if (it == MelodyNotes.end())
            {
                event_flag = 0;
            }
            else
            {
                CurrentNote = *it;

                pFuncNoteOn(CurrentNote.NoteNumber + RootNoteNumber);
                waitMilliseconds(CurrentNote.NoteDuration * (float)BeatTime_ms);
            }
        }
    }



public:

    TSequence(std::list<TNote> MelodyNotes_param,
              void (*pFuncNoteOn_param)(int NoteNumber),
              void (*pFuncNoteOff_param)(int NoteNumber),
              int RootNoteNumber_param)
    {
        MelodyNotes = MelodyNotes_param;
        pFuncNoteOn = pFuncNoteOn_param;
        pFuncNoteOff = pFuncNoteOff_param;
        RootNoteNumber = RootNoteNumber_param;
    }

    // Downswing of the start sequencer pedal (step 1)
    void Start_PedalPressed(void)
    {
        it = MelodyNotes.begin();
        CurrentNote = *it;
        pFuncNoteOn(CurrentNote.NoteNumber + RootNoteNumber);
        gettimeofday(&tv1, NULL);
    }

    // Upswing of the start sequencer pedal (step 2)
    void Start_PedalReleased(void)
    {
        gettimeofday(&tv2, NULL);
        // Compute time lapse between pedal down and pedal up
        // that will set our "one beat" tempo time
        BeatTime_ms = (tv2.tv_sec - tv1.tv_sec) * 1000.0 + (tv2.tv_usec - tv1.tv_usec) / 1000.0;

        // Next, let the sequencer run in its own thread
        int iret1;
        iret1 = pthread_create(&thread, NULL, SequencerThread, this);
        if (iret1)
        {
            fprintf(stderr, "Error - pthread_create() return code: %d\n", iret1);
            exit(EXIT_FAILURE);
        }
    }

    // Stop sequencer
    void Stop_PedalPressed(void)
    {
        // Stop sequence if any
        event_flag = 0;
        AllSoundsOff();
    }
};



namespace CapitaineFlam
{


void Partial_On(int NoteNumber)
{
    TMidiNoteOnEvent no1(3, NoteNumber, 100);
    TMidiNoteOnEvent no2(4, NoteNumber, 100);
    TMidiNoteOnEvent no3(5, NoteNumber, 100);
}

void Partial_Off(int NoteNumber)
{
    TMidiNoteOffEvent no1(3, NoteNumber, 0);
    TMidiNoteOffEvent no2(4, NoteNumber, 0);
    TMidiNoteOffEvent no3(5, NoteNumber, 0);
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
        TMidiNoteOnEvent no(2, 30, 100);
        waitMilliseconds(100);
//        TMidiNoteOffEvent noff(2, 30, 0);
        //waitMilliseconds(30);

    }

}

pthread_t thread;


void Init(void)
{
    // For a reason that eludes me, changing the variation of the part
    // must be sent twice...
    // We want to select the LaserGun (program 128, variation 2)
    TMidiProgramChange pc(2, 128);
    TMidiControlChange cc(2, 0, 2);
    TMidiProgramChange pc1(2, 128);
    TMidiControlChange cc1(2, 0, 2);

    TMidiProgramChange pc3(3, 57); // channel 3, trumpet
    TMidiProgramChange pc4(4, 64); // channel 3, synth brass
    TMidiProgramChange pc5(5, 61); // channel 3, trumpet

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
    system("aplay ./wav/yeah_001.wav &");
}

void Hooh(void)
{
    system("aplay ./wav/hooh_echo_001.wav &");
}

void Siren(void)
{
    system("aplay ./wav/alarm_001.wav &");
}

void Cuica(void)
{
    system("aplay ./wav/cuica_001.wav &");
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
        TMidiProgramChange pc(2, 49);
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
    TMidiProgramChange pc1(2, 92);
    TMidiProgramChange pc2(3, 101);
    TMidiProgramChange pc3(4, 89);
//    TMidiProgramChange pc4(1, 4, 1);




}


void Partial_Off(void * pParam)
{
    TMidiControlChange cc1(2,0x42, 0);
    TMidiControlChange cc2(3,0x42, 0);
    TMidiControlChange cc3(4,0x42, 0);

}


void Partial_On(int NoteNumber)
{
    PlayNote(2, NoteNumber -12, 400, 100);
    PlayNote(2, NoteNumber, 400, 80);
    PlayNote(3, NoteNumber -12, 400, 100);
    PlayNote(3, NoteNumber, 400, 80);
    PlayNote(4, NoteNumber -12, 400, 100);
    PlayNote(4, NoteNumber, 400, 100);
    TMidiControlChange cc1(2,0x42, 127);
    TMidiControlChange cc2(3,0x42, 127);
    TMidiControlChange cc3(4,0x42, 127);

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



extern "C" void showlist(void);
extern "C" int main_TODO(int argc, char const **argv, int Tempo);
extern "C" void seq_midi_tempo_direct(int Tempo);
extern "C" void pmidiStop(void);

static int Tempo = 120;
unsigned int SequencerRunning = 0;

pthread_t thread_sequencer = NULL;


// Stop the pmidi sequencer.
void StopSequencer(void)
{
    if (thread_sequencer != NULL)
    {

        pmidiStop();

        pthread_cancel(thread_sequencer);
        thread_sequencer = NULL;

        sleep(1);

        StartRawMidiOut(0);
        AllSoundsOff();
    }
}

// Run the pmidi sequencer in its own thread.
void * ThreadSequencerFunction (void * params)
{


    StopRawMidiOut(0);

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
    StartRawMidiOut(0);

    thread_sequencer = NULL;

}

// Start the full-fledged pmidi sequencer for file MidiFilename
void StartSequencer(char * MidiFilename)
{
    int iret1;
    if(thread_sequencer == NULL)
    {
        iret1 = pthread_create(&thread_sequencer, NULL, ThreadSequencerFunction, (void*) MidiFilename);
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
    if (thread_sequencer != NULL)
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
    TMidiProgramChange pc(2, 81);
    TMidiControlChange cc(2, 0, 8);
    TMidiProgramChange pc1(2, 81);
    TMidiControlChange cc1(2, 0, 8);


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
    TMidiNoteOnEvent no(2, CurrentNote, 100);
}


void SineWaveOff(void)
{
    TMidiNoteOffEvent no(2, CurrentNote, 0);
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
        TMidiControlChange cc(1, 50, 127, 1);
        TMidiControlChange cc2(1, 86, 127, 1);

    }
    else
    {
        TMidiControlChange cc(1, 50, 0, 1);
        TMidiControlChange cc2(1, 86, 0, 1);

    }

}



}


}

namespace ElevenRack
{

void DistOFF(void)
{
    // Midi channel 1, controller number 25, value 0, send to Midisport port B
    TMidiControlChange cc(1, 25, 0, 1);
    Banner.SetMessage("Dist OFF");
}

void DistON(void)
{
    TMidiControlChange cc(1, 25, 127, 1);
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
    // Midi channel 1, controller number 25, value 0, send to Midisport port B
    TMidiControlChange cc(1, 50, 0, 1);
    Banner.SetMessage("Mod OFF");
}

void ModON(void)
{
    TMidiControlChange cc(1, 50, 127, 1);
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
    // Midi channel 1, controller number 25, value 0, send to Midisport port B
    TMidiControlChange cc(1, 43, 0, 1);
    Banner.SetMessage("WAH OFF");
}

void WahON(void)
{
    TMidiControlChange cc(1, 43, 127, 1);
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
    TMidiControlChange cc(1, 4, Val, 1);
}

void Init(void)
{
    // Rack 11 bank change: select user-defined
    // Manual page 124.
    // Midi channel 1, Control number 32, Value 0, on Midisport port B
    // (going to rack eleven)
    TMidiControlChange cc(1, 32, 0, 1);
    TMidiProgramChange pc(1, 104, 1);

    DistOFF();
    ModOFF();
    WahOFF();
}

}

// Inspect MIDI IN for events, and dispatch then accordingly as Control Change events,
// midi notes assigned to pedals of the pedalboard, or other.
void threadMidiAutomaton(void)
{
    int err;
    int thru = 0;
    int fd_in = -1, fd_out = -1;

    wprintw(win_debug_messages.GetRef(), "Using: \n");
    wprintw(win_debug_messages.GetRef(), "Input & output: ");
    wprintw(win_debug_messages.GetRef(), "device %s\n", name_midi_hw[0]);
    wprintw(win_debug_messages.GetRef(), "device %s\n", name_midi_hw[1]);

    StartRawMidiIn(0);
    StartRawMidiOut(0);
    StartRawMidiIn(1);
    StartRawMidiOut(1);
//    signal(SIGINT,sighandler);
    if (!thru)
    {
        if (handle_midi_hw_in[0])
        {
            unsigned char ch;
            unsigned int rxNote, rxVolume, rxControllerNumber,
                     rxControllerValue;
            enum
            {
                smInit,
                smWaitMidiChar1,
                smWaitMidiControllerChangeChar2,
                smWaitMidiControllerChangeChar3,
                smProcessControllerChange,
                smWaitMidiNoteChar2,
                smWaitMidiNoteChar3,
                smProcessNoteEvent,
                smInterpretMidiNote,
                smSendMidiNotes
            } stateMachine = smInit;

            while (!stop)
            {
                wprintw(win_midi_in.GetRef(), "0x%02x", ch);

                switch (stateMachine)
                {
                case smInit:
                    stateMachine = smWaitMidiChar1;
                    break;

                case smWaitMidiChar1:
                    snd_rawmidi_read(handle_midi_hw_in[0], &ch, 1);
                    if (ch == 0x91)
                        stateMachine = smWaitMidiNoteChar2;
                    if (ch == 0xb0)
                        stateMachine = smWaitMidiControllerChangeChar2;
                    break;

                case smWaitMidiNoteChar2:
                    snd_rawmidi_read(handle_midi_hw_in[0], &ch, 1);
                    if (ch >= 1 && ch <= 10)
                    {
                        stateMachine = smWaitMidiNoteChar3;
                        rxNote = ch;

                    }
                    else
                    {
                        stateMachine = smWaitMidiChar1;
                    }

                    break;

                case smWaitMidiNoteChar3:
                    snd_rawmidi_read(handle_midi_hw_in[0], &ch, 1);
                    rxVolume = ch;
                    stateMachine = smProcessNoteEvent;
                    break;

                case smWaitMidiControllerChangeChar2:
                    snd_rawmidi_read(handle_midi_hw_in[0], &ch, 1);
                    if (ch >= 0 && ch <= 119)
                    {
                        stateMachine = smWaitMidiControllerChangeChar3;
                        rxControllerNumber = ch;

                    }
                    else
                    {
                        stateMachine = smWaitMidiChar1;
                    }

                    break;

                case smWaitMidiControllerChangeChar3:
                    snd_rawmidi_read(handle_midi_hw_in[0], &ch, 1);
                    stateMachine = smProcessControllerChange;
                    rxControllerValue = ch;
                    wprintw(win_debug_messages.GetRef(), "Controller Change: Control Number %i; Control Value %i\n", rxControllerNumber, rxControllerValue);
                    break;

                case smProcessNoteEvent:
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


                stateMachine = smWaitMidiChar1;
                break;

                case smProcessControllerChange:
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
                stateMachine = smWaitMidiChar1;

                break;

                default:
                    stateMachine = smInit;

                }
            }
        }
    }

    wprintw(win_debug_messages.GetRef(), "Closing\n");

    StopRawMidiIn(0);
    StopRawMidiOut(0);
    StopRawMidiIn(1);
    StopRawMidiOut(1);

    if (fd_in != -1)
    {
        close(fd_in);
    }
    if (fd_out != -1)
    {
        close(fd_out);
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

    cJohnnyBeGood.Author = "";
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

//   cILoveRocknRoll = "I love Rock'n'Roll";
    //  cIloveRocknRoll.BaseTempo = 91;

    //cIWantAHighwayToHell = "I want a highway to hell"

    // PLAYLIST ORDER IS DEFINED HERE:
    PlaylistData.clear();
    PlaylistData.push_back(&cFirstContext); // Always keep that one in first
    PlaylistData.push_back(&cRigUp);
    PlaylistData.push_back(&cFlyMeToTheMoon);
    PlaylistData.push_back(&cAllOfMe);
    PlaylistData.push_back(&cCryMeARiver);
    PlaylistData.push_back(&cJustAGigolo);
    PlaylistData.push_back(&cSuperstition);
    PlaylistData.push_back(&cStandByMe);
    PlaylistData.push_back(&cGetBack);
    PlaylistData.push_back(&cAllShookUp);
    PlaylistData.push_back(&cBackToBlack);
    PlaylistData.push_back(&cUnchainMyHeart);
    PlaylistData.push_back(&cFaith);
    PlaylistData.push_back(&cIsntSheLovely);
    PlaylistData.push_back(&cJammin);
    PlaylistData.push_back(&cRehab);
    PlaylistData.push_back(&cIFeelGood);
    PlaylistData.push_back(&cPapasGotABrandNewBag);
    PlaylistData.push_back(&cLongTrainRunning);
    PlaylistData.push_back(&cMasterBlaster);
    PlaylistData.push_back(&cAuxChampsElysees);
    PlaylistData.push_back(&cProudMary);
    PlaylistData.push_back(&cMonAmantDeSaintJean);
    PlaylistData.push_back(&cAroundTheWorld);
    PlaylistData.push_back(&cGetLucky);
    PlaylistData.push_back(&cIllusion);
    PlaylistData.push_back(&cDockOfTheBay);
    PlaylistData.push_back(&cLockedOutOfHeaven);
    PlaylistData.push_back(&cWhatsUp);
    PlaylistData.push_back(&cLesFillesDesForges);
    PlaylistData.push_back(&cThatsAllRight);
    PlaylistData.push_back(&cJohnnyBeGood);
    PlaylistData.push_back(&cBebopALula);
    PlaylistData.push_back(&cUptownFunk);
    PlaylistData.push_back(&cLeFreak);
    PlaylistData.push_back(&cRappersDelight);
    PlaylistData.push_back(&cMachistador);
    PlaylistData.push_back(&cAnotherOneBiteTheDust);
    PlaylistData.push_back(&cWot);
    PlaylistData.push_back(&cKnockOnWood);
    PlaylistData.push_back(&cHotelCalifornia);
    PlaylistData.push_back(&cRaggamuffin);
    PlaylistData.push_back(&cManDown);
    PlaylistData.push_back(&cShouldIStay);
    PlaylistData.push_back(&cMercy);
    PlaylistData.push_back(&cAveMaria);
    PlaylistData.push_back(&cCapitaineFlam);
    PlaylistData.push_back(&cWildThoughts);
    PlaylistData.push_back(&cGangstaParadise);
    PlaylistData.push_back(&cBeatIt);
    PlaylistData.push_back(&cLady);


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
                wprintw(win_context_usage.GetRef(), "Expressionm CC %i: %s\n", PedalAnalog.GetControllerNumber(), PedalAnalog.GetComment().c_str());
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


static const char *menulist[MAX_MENU_ITEMS][MAX_SUB_ITEMS];


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
    char temp[256];
    int selection, count;
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

extern float PCMInputTempoConfidence;
extern float PCMInputTempoValue;

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
    cbreak();                       /* Line buffering disabled, Pass on
                                    /* every thing to me           */
    keypad(stdscr, TRUE);           /* I need that nifty F1         */



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
    std::thread thread1(threadMidiAutomaton);

    // Create task that redraws screen at fixed intervals
    std::thread thread2(threadRedraw);

    // Create the thread that refreshes the metronome
    std::thread thread3(threadMetronome);

    Keyboard::Initialize();

    // Create thread that scans the keyboard
    std::thread thread4(MiniSynth::threadKeyboard);


#endif

    extern int extract_tempo ();

    //extract_tempo();
    // Do nothing
    while(1)
    {
        sleep(1);

    }
}
