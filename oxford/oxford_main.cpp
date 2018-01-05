// use amidi -l to list midi hardware devices
// don't forget to link with asound and pthread
// use pmidi -l to list midi devices for pmidi

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
//#include <sstream>


int stop = 0;
snd_rawmidi_t *handle_in = 0, *handle_out = 0;
char device_in_str[] = "hw:1,0,0";
char device_out_str[] = "hw:1,0,0";






class TBoxedWindow
{
private:
    PANEL * Panel;
    WINDOW * BoxedWindow; // outer window
    WINDOW * SubWindow;
public:
    TBoxedWindow(void) {};
    void Init(char* name, int height, int width, int starty, int startx)
    {
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
        show_panel(Panel);
        update_panels();
        doupdate();
    }

    void Hide(void)
    {
        hide_panel(Panel);
        update_panels();
        doupdate();
    }

    WINDOW * GetRef(void)
    {
        return SubWindow;
    }

    void Refresh(void)
    {
        if (!panel_hidden(Panel))
        {
            touchwin(BoxedWindow);
//       wrefresh(SubWindow);
            update_panels();
            doupdate();
        }
    }

    void PutOnTop(void)
    {
        top_panel(Panel);
    }
    void Erase(void)
    {
        wclear(SubWindow);
    }
};









TBoxedWindow win_midi_in;
TBoxedWindow win_midi_out;
TBoxedWindow win_debug_messages;
TBoxedWindow win_context_prev;
TBoxedWindow win_context_current;
TBoxedWindow win_context_next;
TBoxedWindow win_context_usage;
TBoxedWindow win_context_user_specific;
TBoxedWindow win_context_select_menu;






void ContextDecreaseStart(void);
void ContextDecreaseStop(void);
void ContextIncreaseStart(void);
void ContextIncreaseStop(void);


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






class TPedalboard
{
public:
    std::list<TPedalDigital> PedalsDigital;
    std::list<TPedalAnalog> PedalsAnalog;
    TPedalboard(void)
    {
//        TPedalDigital myPedalDigital(6,NULL,NULL);
        PedalsDigital.push_back(TPedalDigital(6, ContextDecreaseStart, ContextDecreaseStop, "Playlist: previous song"));
        PedalsDigital.push_back(TPedalDigital(7, ContextIncreaseStart, ContextIncreaseStop, "Playlist: next song"));
    }
};


class TContext
{
public:
    std::string Author;
    std::string SongName;
    TPedalboard Pedalboard;
};


//TContext cAveMaria = {"", "Ave Maria", {{AveMaria::  }   }}

std::list<TContext> PlaylistData;
std::list<TContext> PlaylistData_ByAuthor;
std::list<TContext> PlaylistData_BySongName;

std::list<TContext>::iterator Playlist;

TContext cAveMaria, cCapitaineFlam, cWildThoughts, cGangstaParadise;


// Compare contexts by author (not case sensitive)
bool CompareTContextByAuthor(const TContext &first, const TContext &second)
{
    std::string first_nocase = first.Author;
    std::string second_nocase = second.Author;
    std::transform(first_nocase.begin(), first_nocase.end(), first_nocase.begin(), ::tolower);
    std::transform(second_nocase.begin(), second_nocase.end(), second_nocase.begin(), ::tolower);

    return first_nocase < second_nocase;
}

// Compare contexts by song name (not case sensitive)
bool CompareTContextBySongName(const TContext &first, const TContext &second)
{
    std::string first_nocase = first.SongName;
    std::string second_nocase = second.SongName;
    std::transform(first_nocase.begin(), first_nocase.end(), first_nocase.begin(), ::tolower);
    std::transform(second_nocase.begin(), second_nocase.end(), second_nocase.begin(), ::tolower);

    return first_nocase < second_nocase;
}







int startx, starty, width, height;
int ch;






void ContextDecreaseStart(void)
{
    if (Playlist != PlaylistData.begin())
    {
        Playlist--;
    }

}

void ContextDecreaseStop(void)
{



}


void ContextIncreaseStart(void)
{
    // Note that .end() returns an iterator that is already outside the bounds
    // of the container.
    std::list<TContext>::iterator it;
    it = Playlist;
    it++;
    if (it != PlaylistData.end())
    {
        Playlist++;
    }
}

void ContextIncreaseStop(void)
{



}





void StartRawMidiIn(void)
{
    if (handle_in == 0)
    {
        int err = snd_rawmidi_open(&handle_in, NULL, device_in_str, 0);
        if (err)
        {
            wprintw(win_debug_messages.GetRef(), "snd_rawmidi_open %s failed: %d\n", device_in_str, err);
        }
    }
}


void StartRawMidiOut(void)
{
    if (handle_out == 0)
    {
        int err = snd_rawmidi_open(NULL, &handle_out, device_out_str, 0);
        if (err)
        {
            wprintw(win_debug_messages.GetRef(), "snd_rawmidi_open %s failed: %d\n", device_out_str, err);
        }
    }
}


void StopRawMidiIn(void)
{
    if (handle_in != 0)
    {
        snd_rawmidi_drain(handle_in);
        snd_rawmidi_close(handle_in);
        handle_in = 0;
    }

}

void StopRawMidiOut(void)
{
    if(handle_out != 0)
    {
        snd_rawmidi_drain(handle_out);
        snd_rawmidi_close(handle_out);
        handle_out = 0;
    }

}




#define EV_PRESSED 1
#define EV_RELEASED 0
#define EV_REPEAT 2


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

typedef struct
{
    void (*pFunction)(void *);
    unsigned long int Delay_ms;
    void * pFuncParam;
} TExecuteAfterTimeoutStruct;

#pragma reentrant
void * ExecuteAfterTimeout_Thread(void * pMessage)
{
    waitMilliseconds(((TExecuteAfterTimeoutStruct*) pMessage)->Delay_ms);

    // call pFunction(pFuncParam)
    (((TExecuteAfterTimeoutStruct *) pMessage)->pFunction)( ((TExecuteAfterTimeoutStruct *) pMessage)->pFuncParam );
    free(pMessage);
}

#pragma reentrant
void ExecuteAfterTimeout(void (*pFunc)(void *), unsigned long int Timeout_ms, void * pFuncParam)
{
    pthread_t thread;
    TExecuteAfterTimeoutStruct * pExecuteAfterTimeoutStruct;
    pExecuteAfterTimeoutStruct = (TExecuteAfterTimeoutStruct *) malloc(
                                     sizeof(TExecuteAfterTimeoutStruct));
    pExecuteAfterTimeoutStruct->Delay_ms = Timeout_ms;
    pExecuteAfterTimeoutStruct->pFunction = pFunc;

    int iret1;
    iret1 = pthread_create(&thread, NULL, ExecuteAfterTimeout_Thread,
                           (void*) pExecuteAfterTimeoutStruct);
    if (iret1)
    {
        wprintw(win_debug_messages.GetRef(), "Error - pthread_create() return code: %d\n", iret1);
        exit(EXIT_FAILURE);
    }
}

#pragma reentrant
void ExecuteAsynchronous(void (*pFunc)(void *), void * pFuncParam)
{
    ExecuteAfterTimeout(pFunc, 0, pFuncParam);
}


static void usage(void)
{
    wprintw(win_debug_messages.GetRef(), "usage: fix me\n");
}



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

typedef struct
{
    unsigned char NoteNumber;
    unsigned char Velocity;
    unsigned char Channel;
    unsigned int DurationMS;
    pthread_t thread;
} TPlayNoteMsg;

class TMidiNoteOnEvent
{

public:
    unsigned char NoteOnField = 9;
    unsigned char charArray[3];

    void sendToMidi(void)
    {
        wprintw(win_midi_out.GetRef(), "%i\n%i\n%i\n", (int) charArray[0], (int) charArray[1], (int) charArray[2]);
        if (handle_out != 0)
        {
            snd_rawmidi_write(handle_out, &charArray, sizeof(charArray));
        }
    }
    ;

    TMidiNoteOnEvent(TPlayNoteMsg * PlayNoteMsg);

    TMidiNoteOnEvent(unsigned int Channel, unsigned int NoteNumber,
                     unsigned int Velocity);

    void Init(unsigned int Channel, unsigned int NoteNumber,
              unsigned int Velocity);

};

TMidiNoteOnEvent::TMidiNoteOnEvent(unsigned int Channel,
                                   unsigned int NoteNumber, unsigned int Velocity)
{
    Init(Channel, NoteNumber, Velocity);
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

TMidiNoteOnEvent::TMidiNoteOnEvent(TPlayNoteMsg * PlayNoteMsg)
{
    Init(PlayNoteMsg->Channel, PlayNoteMsg->NoteNumber, PlayNoteMsg->Velocity);
}

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
    unsigned char MidiFunctionID = 0xC;
    unsigned char charArray[2];

    void sendToMidi(void)
    {
        wprintw(win_debug_messages.GetRef(), "Program Change: %i\n", (int) charArray[1]);
        if(handle_out != 0)
        {
            snd_rawmidi_write(handle_out, &charArray, sizeof(charArray));
        }
    }
    ;

public:

    TMidiProgramChange(unsigned char Channel, unsigned char Program)
    {
        if (Channel < 1)
            Channel = 1;
        if (Channel > 16)
            Channel = 16;

        charArray[0] = (MidiFunctionID << 4) + ((Channel - 1) & 0x0F);
        charArray[1] = (Program - 1) & 0x7F;
        wprintw(win_debug_messages.GetRef(), "Program Change: %i\n", (int) charArray[1]);
        sendToMidi();
    }

};

class TMidiControlChange
{
private:
    unsigned char MidiFunctionID = 0xB;
    unsigned char charArray[3];

    void sendToMidi(void)
    {
        wprintw(win_debug_messages.GetRef(), "Control Change - Controller Number: %i\n", (int) charArray[1]);
        wprintw(win_debug_messages.GetRef(), "Control Change - Controller Value: %i\n", (int) charArray[2]);
        if (handle_out != 0)
        {
            snd_rawmidi_write(handle_out, &charArray, sizeof(charArray));
        }
    }
    ;

public:

    TMidiControlChange(unsigned char Channel, unsigned char ControlNumber,
                       unsigned char ControllerValue)
    {
        if (Channel < 1)
            Channel = 1;
        if (Channel > 16)
            Channel = 16;

        charArray[0] = (MidiFunctionID << 4) + ((Channel - 1) & 0x0F);
        charArray[1] = (ControlNumber) & 0x7F;
        charArray[2] = (ControllerValue);
        sendToMidi();
    }

};

void * playNoteThread(void * msg)
{
//    TMidiProgramChange MidiProgramChange(2, ((TPlayNoteMsg*) msg)->Program);
    TMidiNoteOnEvent MidiNoteOnEvent((TPlayNoteMsg *) msg);
    waitMilliseconds(((TPlayNoteMsg*) msg)->DurationMS);
    wprintw(win_debug_messages.GetRef(), "playNoteThread\n");
    ((TPlayNoteMsg *) msg)->Velocity = 0;
    TMidiNoteOffEvent MidiNoteOffEvent((TPlayNoteMsg *) msg);

    delete ((TPlayNoteMsg *) msg);
    return 0;
}


void PlayNote(unsigned char Channel_param, unsigned char NoteNumber_param, int DurationMS_param, int Velocity_param)
{
    TPlayNoteMsg * PlayNoteMsg = new TPlayNoteMsg;
    PlayNoteMsg->Channel = Channel_param;
    PlayNoteMsg->DurationMS = DurationMS_param;
    PlayNoteMsg->NoteNumber = NoteNumber_param;
    PlayNoteMsg->Velocity = Velocity_param;

    int iret;
    pthread_t thread;
    // Create midi automaton thread
    iret = pthread_create(&thread, NULL, playNoteThread, (void*) PlayNoteMsg);
    PlayNoteMsg->thread = thread;
    if (iret)
    {
        wprintw(win_debug_messages.GetRef(), "Error - pthread_create() return code: %d\n", iret);
        exit(EXIT_FAILURE);
    }

}

void midiPlay(int octave, unsigned char noteInScale)
{
    PlayNote(2, octave *12 + noteInScale, 1000, 100);
}



int getkey()
{
    int character;
    struct termios orig_term_attr;
    struct termios new_term_attr;

    waitMilliseconds(1);

    /* set the terminal to raw mode */
    tcgetattr(fileno(stdin), &orig_term_attr);
    memcpy(&new_term_attr, &orig_term_attr, sizeof(struct termios));
    new_term_attr.c_lflag &= ~(ECHO | ICANON);
    new_term_attr.c_cc[VTIME] = 0;
    new_term_attr.c_cc[VMIN] = 0;
    tcsetattr(fileno(stdin), TCSANOW, &new_term_attr);

    /* read a character from the stdin stream without blocking */
    /*   returns EOF (-1) if no character is available */
    character = fgetc(stdin);

    /* restore the original terminal attributes */
    tcsetattr(fileno(stdin), TCSANOW, &orig_term_attr);

    return character;
}

void * threadKeyboard(void * ptr)
{
    char *message;
    char noteInScale;
    int octave = 2;
    int program = 1;
    message = (char *) ptr;
    wprintw(win_debug_messages.GetRef(), "%s \n", message);

    while (1)
    {
        int ch = getkey();
        if (ch == -1)
        {
            waitMilliseconds(10);
            continue;
        }
        switch (ch)
        {
        case ')':
            octave--;
            wprintw(win_debug_messages.GetRef(), "Octave %i\n", octave);
            break;
        case '=':
            octave++;
            wprintw(win_debug_messages.GetRef(), "Octave %i\n", octave);
            break;

        case 195:
            program--;
            wprintw(win_debug_messages.GetRef(), "Program %i\n", program);
            {
                TMidiProgramChange PC1(2, program);
            }
            break;
        case '*':
            program++;
            wprintw(win_debug_messages.GetRef(), "Program %i\n", program);
            {
                TMidiProgramChange PC2(2, program);
            }
            break;

        case 'a':
            noteInScale = 0;
            midiPlay(octave, noteInScale);
            break;
        case 'é':
            noteInScale = 1;
            midiPlay(octave, noteInScale);
            break;
        case 'z':
            noteInScale = 2;
            midiPlay(octave, noteInScale);
            break;
        case '"':
            noteInScale = 3;
            midiPlay(octave, noteInScale);
            break;
        case 'e':
            noteInScale = 4;
            midiPlay(octave, noteInScale);
            break;
        case 'r':
            noteInScale = 5;
            midiPlay(octave, noteInScale);
            break;
        case '(':
            noteInScale = 6;
            midiPlay(octave, noteInScale);
            break;
        case 't':
            noteInScale = 7;
            midiPlay(octave, noteInScale);
            break;
        case '-':
            noteInScale = 8;
            midiPlay(octave, noteInScale);
            break;
        case 'y':
            noteInScale = 9;
            midiPlay(octave, noteInScale);
            break;
        case 'è':
            noteInScale = 10;
            midiPlay(octave, noteInScale);
            break;
        case 'u':
            noteInScale = 11;
            midiPlay(octave, noteInScale);
            break;
        case 'i':
            noteInScale = 12;
            midiPlay(octave, noteInScale);
            break;
        case 'ç':
            noteInScale = 13;
            midiPlay(octave, noteInScale);
            break;
        case 'o':
            noteInScale = 14;
            midiPlay(octave, noteInScale);
            break;
        case 'à':
            noteInScale = 15;
            midiPlay(octave, noteInScale);
            break;
        case 'p':
            noteInScale = 16;
            midiPlay(octave, noteInScale);
            break;



        case ' ':
            extern void SelectContextInPlaylist(void);
            SelectContextInPlaylist();

            break;

        case 'b':

            win_context_select_menu.Show();
            break;

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

namespace CapitaineFlam
{

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

void Laser_On(void)
{
    TMidiProgramChange pc(2, 128);
    TMidiControlChange cc(2, 0, 2);

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



extern "C" void showlist(void);
extern "C" int main_TODO(int argc, char **argv);
extern "C" void seq_midi_tempo_direct(float skew, float bpm_min, float bpm_max);
extern "C" void pmidiStop(void);

unsigned int SequencerRunning = 0;

pthread_t thread_sequencer = NULL;


void StopSequencer(void)
{
    if (thread_sequencer != NULL)
    {

        pmidiStop();

        pthread_cancel(thread_sequencer);
        thread_sequencer = NULL;

        sleep(1);

        StartRawMidiOut();

        // all sounds off for all channels
        for (unsigned int i = 1; i<=16; i++)
        {
            TMidiControlChange cc(i, 0x78, 0);
            TMidiControlChange cc2(i, 0x79, 0);
            TMidiControlChange cc3(i, 0x7B, 0);
            TMidiControlChange cc4(i, 0x7C, 0);
        }
    }
}


void * ThreadSequencerFunction (void * params)
{


    StopRawMidiOut();

    SequencerRunning = 1;
    showlist();

    int argc = 4;
    char str10[] = "fakename";
    char str11[] = "-p";
    char str12[] = "20:0";
    char * str13 = (char*) params;
    char * (argv1[4]) =
    { str10, str11, str12, str13 };
    main_TODO(argc, argv1);
    StartRawMidiOut();

    thread_sequencer = NULL;

}


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
    seq_midi_tempo_direct(((float)Value- 60)/60, 100, 160);
}
}

void *threadMidiAutomaton(void * ptr)
{
    int err;
    int thru = 0;
    int verbose = 1;

    int fd_in = -1, fd_out = -1;

    if (verbose)
    {
        wprintw(win_debug_messages.GetRef(), "Using: \n");
        wprintw(win_debug_messages.GetRef(), "Input: ");
        if (device_in_str)
        {
            wprintw(win_debug_messages.GetRef(), "device %s\n", device_in_str);
        }
        else
        {
            wprintw(win_debug_messages.GetRef(), "NONE\n");
        }
        wprintw(win_debug_messages.GetRef(), "Output: ");
        if (device_out_str)
        {
            wprintw(win_debug_messages.GetRef(), "device %s\n", device_out_str);
        }
        else
        {
            wprintw(win_debug_messages.GetRef(), "NONE\n");
        }
    }

    StartRawMidiIn();
    StartRawMidiOut();
//    signal(SIGINT,sighandler);
    if (!thru)
    {
        if (handle_in)
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
                if (verbose)
                {
                    wprintw(win_midi_in.GetRef(), "0x%02x", ch);
                }

                switch (stateMachine)
                {
                case smInit:
                    stateMachine = smWaitMidiChar1;
                    break;

                case smWaitMidiChar1:
                    snd_rawmidi_read(handle_in, &ch, 1);
                    if (ch == 0x91)
                        stateMachine = smWaitMidiNoteChar2;
                    if (ch == 0xb0)
                        stateMachine = smWaitMidiControllerChangeChar2;
                    break;

                case smWaitMidiNoteChar2:
                    snd_rawmidi_read(handle_in, &ch, 1);
                    if (ch >= 1 && ch <= 8)
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
                    snd_rawmidi_read(handle_in, &ch, 1);
                    rxVolume = ch;
                    stateMachine = smProcessNoteEvent;
                    break;

                case smWaitMidiControllerChangeChar2:
                    snd_rawmidi_read(handle_in, &ch, 1);
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
                    snd_rawmidi_read(handle_in, &ch, 1);
                    stateMachine = smProcessControllerChange;
                    rxControllerValue = ch;
                    wprintw(win_debug_messages.GetRef(), "Controller Change: Control Number %i; Control Value %i\n", rxControllerNumber, rxControllerValue);
                    break;

                case smProcessNoteEvent:
                {
                    TContext context;
                    context = *Playlist;
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
                    context = *Playlist;
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

    StopRawMidiIn();
    StopRawMidiOut();

    if (fd_in != -1)
    {
        close(fd_in);
    }
    if (fd_out != -1)
    {
        close(fd_out);
    }
    return 0;
}






void InitializePlaylist(void)
{
    cAveMaria.Author = "";
    cAveMaria.SongName = "Ave Maria";
    cAveMaria.Pedalboard.PedalsDigital.push_back(TPedalDigital(1, AveMaria::AveMaria_Start, NULL, "MIDI sequencer start"));
    cAveMaria.Pedalboard.PedalsDigital.push_back(TPedalDigital(2, AveMaria::AveMaria_Stop, NULL, "MIDI sequencer stop"));
    cAveMaria.Pedalboard.PedalsAnalog.push_back(TPedalAnalog(1, AveMaria::ChangeTempo, "Adjust tempo"));

    cCapitaineFlam.Author = "Jean-Jacques Debout";
    cCapitaineFlam.SongName = "Capitaine Flam";
    cCapitaineFlam.Pedalboard.PedalsDigital.push_back(TPedalDigital(1, CapitaineFlam::Laser_On, CapitaineFlam::Laser_Off, "Laser pulses"));

    cWildThoughts.Author = "Rihanna";
    cWildThoughts.SongName = "Wild Thoughts";
    cWildThoughts.Pedalboard.PedalsDigital.push_back(TPedalDigital(1, Rihanna::WildThoughts::Chord1_On, Rihanna::WildThoughts::Chord1_Off, "First chord"));
    cWildThoughts.Pedalboard.PedalsDigital.push_back(TPedalDigital(2, Rihanna::WildThoughts::Chord2_On, Rihanna::WildThoughts::Chord2_Off, "Second chord"));
    cWildThoughts.Pedalboard.PedalsDigital.push_back(TPedalDigital(3, Rihanna::WildThoughts::Chord3_On, Rihanna::WildThoughts::Chord3_Off, "Third chord"));

    cGangstaParadise.Author = "Coolio";
    cGangstaParadise.SongName = "Gangsta's paradise";
    cGangstaParadise.Pedalboard.PedalsDigital.push_back(TPedalDigital(1, Gansta_s_Paradise::Start_NoteOn, Gansta_s_Paradise::Start_NoteOff, "Sequence; Press: first note, Release: set tempo and loop"));
    cGangstaParadise.Pedalboard.PedalsDigital.push_back(TPedalDigital(2, Gansta_s_Paradise::Stop, NULL, "Sequence; Press: first note, Release: set tempo and loop"));



    PlaylistData.clear();
    PlaylistData.push_back(cAveMaria);
    PlaylistData.push_back(cCapitaineFlam);
    PlaylistData.push_back(cWildThoughts);
    PlaylistData.push_back(cGangstaParadise);


    Playlist = PlaylistData.begin();

    PlaylistData_ByAuthor = PlaylistData;
    PlaylistData_ByAuthor.sort(CompareTContextByAuthor);

    PlaylistData_BySongName = PlaylistData;
    PlaylistData_BySongName.sort(CompareTContextBySongName);


//    cAveMaria.Pedalboard =  { { {1, AveMaria::AveMaria_Start, NULL}, {2, AveMaria::AveMaria_Stop, NULL} }, { {1, AveMaria::ChangeTempo}} };



}


void * threadRedraw(void * pMessage)
{
    TContext Context;
    while(1)
    {
        waitMilliseconds(200);
        Context = *Playlist;
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

        //wnoutrefresh(win_context_current);
        win_context_current.Refresh();
        win_context_next.Refresh();
        win_context_prev.Refresh();
        win_context_usage.Refresh();
        win_debug_messages.Refresh();
        win_midi_in.Refresh();
        win_midi_out.Refresh();

        //doupdate();
//        wrefresh(win_context_select_arrows);
    }

}



static const char *menulist[MAX_MENU_ITEMS][MAX_SUB_ITEMS];


void SelectContextByName(void)
{

}

void SelectContextInPlaylist (void)
{
    /* Declare variables. */
    CDKSCREEN *cdkscreen = 0;
    CDKSCROLL *scrollList = 0;

    char **item = 0;
    const char *mesg[5];
    char temp[256];
    int selection, count;

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

    /* Set up CDK Colors. */
    //initCDKColor ();



    /* Use the current diretory list to fill the radio list. */
    //count = CDKgetDirectoryContents (".", &item);
    char ** itemlist = (char **) malloc(PlaylistData.size() * sizeof(char *));
    int idx = 0;
    std::list<TContext>::iterator it;
    for (it = PlaylistData.begin(); it != PlaylistData.end(); it++)
    {
        TContext Context = *it;
        char * pMenuStr = (char *)malloc(Context.SongName.size()+1);
        strcpy(pMenuStr, Context.SongName.c_str());
        itemlist[idx] = pMenuStr;
        idx++;
    }


    /* Create the scrolling list. */
    scrollList = newCDKScroll (cdkscreen, CENTER, CENTER, RIGHT, 20, 70, "Context selection, by playlist order", itemlist, idx, TRUE, A_REVERSE, TRUE, FALSE);


    /* Is the scrolling list null? */
    if (scrollList == 0)
    {
        /* Exit CDK. */
        destroyCDKScreen (cdkscreen);
        endCDK ();

        printf ("Cannot make scrolling list. Is the window too small?\n");
        return;
    }

    // if (CDKparamNumber (&params, 'c'))
    // {
    //    setCDKScrollItems (scrollList, (CDK_CSTRING2) item, count, TRUE);
    // }
#if 0
    drawCDKScroll (scrollList, 1);

    setCDKScrollPosition (scrollList, 10);
    drawCDKScroll (scrollList, 1);
    sleep (3);

    setCDKScrollPosition (scrollList, 20);
    drawCDKScroll (scrollList, 1);
    sleep (3);

    setCDKScrollPosition (scrollList, 30);
    drawCDKScroll (scrollList, 1);
    sleep (3);

    setCDKScrollPosition (scrollList, 70);
    drawCDKScroll (scrollList, 1);
    sleep (3);
#endif
//   bindCDKObject (vSCROLL, scrollList, 'a', addItemCB, NULL);
//bindCDKObject (vSCROLL, scrollList, 'i', insItemCB, NULL);
//  bindCDKObject (vSCROLL, scrollList, 'd', delItemCB, NULL);

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

        idx = 0;
        for (it = PlaylistData.begin(); it != PlaylistData.end(); it++)
        {
            if (idx == scrollList->currentItem)
            {
                Playlist = it;

                break;
            }
            idx++;

        }


//freeChar (theItem);
#if 0
        char *theItem = chtype2Char (scrollList->item[selection]);
        mesg[0] = "<C>You selected the following file";
        sprintf (temp, "<C>%.*s", (int)(sizeof (temp) - 20), theItem);
        mesg[1] = temp;
        mesg[2] = "<C>Press any key to continue.";
        popupLabel (cdkscreen, (CDK_CSTRING2) mesg, 3);
        freeChar (theItem);
#endif
    }

    /* Clean up. */
    CDKfreeStrings (item);
    destroyCDKScroll (scrollList);
    destroyCDKScreen (cdkscreen);
    endCDK ();





    win_context_select_menu.Hide();

    win_midi_in.Show();
    win_midi_out.Show();
    win_context_prev.Show();
    win_context_current.Show();
    win_context_next.Show();
    win_debug_messages.Show();
    win_context_usage.Show();
    win_context_user_specific.Show();





















#if 0




    /* *INDENT-EQLS* */
    CDKSCREEN *cdkscreen = 0;
    CDKLABEL *infoBox    = 0;
    CDKMENU *menu        = 0;
    int submenusize[2], menuloc[2];
    const char *mesg[5];
    char temp[256];
    int selection;

    win_context_select_menu.Show();
    cdkscreen = initCDKScreen (win_context_select_menu.GetRef());

    /* Set up the menu. */
    menulist[0][0] = "By playlist      ";
    unsigned idx = 0;
    std::list<TContext>::iterator it;
    for (it = PlaylistData.begin(); it != PlaylistData.end(); it++)
    {
        idx++;
        TContext Context = *it;
        char * pMenuStr = (char *)malloc(Context.SongName.size()+1);
        strcpy(pMenuStr, Context.SongName.c_str());
        menulist[0][idx] = pMenuStr;
    }
    submenusize[0] = idx +1;

    menulist[1][0] = "By song name      ";
    idx = 0;
    for (it = PlaylistData_BySongName.begin(); it != PlaylistData_BySongName.end(); it++)
    {
        idx++;
        TContext Context = *it;
        char * pMenuStr = (char *) malloc(Context.SongName.size()+1);
        strcpy(pMenuStr, Context.SongName.c_str());
        menulist[1][idx] = pMenuStr;
    }
    submenusize[1] = idx+1;


//    submenusize[2] = 3;

    menuloc[0] = LEFT;
    menuloc[1] = LEFT;

    /* Create the label window. */
    mesg[0] = "                                          ";
    mesg[1] = "                                          ";
    mesg[2] = "                                          ";
    mesg[3] = "                                          ";
//    infoBox = newCDKLabel (cdkscreen, CENTER, CENTER,
    //                         (CDK_CSTRING2) mesg, 4,
    //                       TRUE, TRUE);

    /* Create the menu. */
    menu = newCDKMenu (cdkscreen, menulist, 2, submenusize, menuloc,
                       TOP, A_UNDERLINE, A_REVERSE);

    /* Create the post process function. */
//    setCDKMenuPostProcess (menu, displayCallback, infoBox);

    /* Draw the CDK screen. */
    refreshCDKScreen (cdkscreen);

    /* Activate the menu. */
    selection = activateCDKMenu (menu, 0);

    /* Determine how the user exited from the widget. */
    if (menu->exitType == vEARLY_EXIT)
    {
        // You hit escape. No menu item was selected.
    }
    else if (menu->exitType == vNORMAL)
    {
        // You selected menu #%d, submenu #%d",
        selection / 100,
                  selection % 100);
        mesg[0] = temp;
        mesg[1] = "",
                  mesg[2] = "<C>Press any key to continue.";
        popupLabel (cdkscreen, (CDK_CSTRING2) mesg, 3);
    }

    /* Clean up. */
    destroyCDKMenu (menu);
    destroyCDKLabel (infoBox);

    endCDK ();
    win_context_select_menu.Hide();

#endif

}




int main(int argc, char** argv)
{
    int term_lines, term_cols;

    InitializePlaylist();


    initscr();                      /* Start curses mode            */
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
    win_debug_messages.Init("DEBUG MESSAGES", 11, term_cols -6-6, term_lines-11, 0);
    win_context_usage.Init("CONTEXT USAGE", (term_lines -3)/2, (term_cols -6-6)/2, 3, 0);
    win_context_user_specific.Init("CONTEXT SPECIFIC", (term_lines -3)/2, (term_cols -6-6)/2, 3, (term_cols -6-6)/2);
    win_context_select_menu.Init("CONTEXT SELECTION MENU", term_lines, term_cols, 0, 0);
    win_context_select_menu.Hide();

    wprintw(win_debug_messages.GetRef(), "Terminal LINES: %i, COLUMNS: %i\n", term_lines, term_cols);

    pthread_t thread1;
    // Create midi automaton thread
    const char *message1 = "";
    int iret1;
    iret1 = pthread_create(&thread1, NULL, threadMidiAutomaton, (void*) message1);
    if (iret1)
    {
        fprintf(stderr, "threadMidiAutomaton Error - pthread_create() return code: %d\n", iret1);
        exit(EXIT_FAILURE);
    }

    pthread_t thread2;
    // Create task that redraws screen at fixed intervals
    const char *message2 = "";
    int iret2;
    iret2 = pthread_create(&thread2, NULL, threadRedraw,
                           (void*) message2);
    if (iret2)
    {
        fprintf(stderr, "threadRedraw Error - pthread_create() return code: %d\n", iret2);
        exit(EXIT_FAILURE);
    }


    // ContextSelection_Playlist();

    threadKeyboard(0);
    while(1) //(ch = getch()) != KEY_ESC)
    {
        sleep(1);

    }




}

