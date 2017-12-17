#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <alsa/asoundlib.h>
#include <signal.h>
#include <pthread.h>
#include <linux/input.h>
#include <termios.h>
#include <math.h>
//#include <sstream>

namespace pmidi
{

}



int stop = 0;
snd_rawmidi_t *handle_in = 0, *handle_out = 0;
char device_in_str[] = "hw:1,0,0";
char device_out_str[] = "hw:1,0,0";





void StartRawMidiIn(void)
{
    if (handle_in == 0)
    {
        int err = snd_rawmidi_open(&handle_in, NULL, device_in_str, 0);
        if (err)
        {
            fprintf(stderr, "snd_rawmidi_open %s failed: %d\n", device_in_str, err);
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
            fprintf(stderr, "snd_rawmidi_open %s failed: %d\n", device_out_str,
                    err);
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

// use amidi -l to list midi devices

// don't forget to link with asound and pthread

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
    void (*pFunction)(void);
    unsigned long int Delay_ms;
} TExecuteAfterTimeoutStruct;

#pragma reentrant
void * ExecuteAfterTimeout_Thread(void * pMessage)
{
    printf("ExecuteAfterTimeout_Thread called\n");
    waitMilliseconds(((TExecuteAfterTimeoutStruct*) pMessage)->Delay_ms);

    (((TExecuteAfterTimeoutStruct *) pMessage)->pFunction)();
    free(pMessage);
}

#pragma reentrant
void ExecuteAfterTimeout(void (*pFunc)(void), unsigned long int Timeout_ms)
{
    printf("ExecuteAfterTimeout called\n");
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
        fprintf(stderr, "Error - pthread_create() return code: %d\n", iret1);
        exit(EXIT_FAILURE);
    }
    printf("ExecuteAfterTimeout exits\n");
}

static void usage(void)
{
    fprintf(stderr, "usage: fix me\n");
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
    unsigned int Program;
    pthread_t thread;

} TPlayNoteMsg;

class TMidiNoteOnEvent
{

public:
    unsigned char NoteOnField = 9;
    unsigned char charArray[3];

    void sendToMidi(void)
    {
        printf("Note number: %i\n", (int) charArray[1]);
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
    printf("Note number: %i\n", (int) charArray[1]);

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
        printf("Note number: %i\n", (int) charArray[1]);
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
        printf("Program Change: %i\n", (int) charArray[1]);
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
        printf("Control Change - Controller Number: %i\n", (int) charArray[1]);
        printf("Control Change - Controller Value: %i\n", (int) charArray[2]);
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
        printf("Program Change: %i\n", (int) charArray[1]);
        sendToMidi();
    }

};

void * playNoteThread(void * msg)
{
    TMidiProgramChange MidiProgramChange(2, ((TPlayNoteMsg*) msg)->Program);
    TMidiNoteOnEvent MidiNoteOnEvent((TPlayNoteMsg *) msg);
    sleep(1);
    printf("blah");
    ((TPlayNoteMsg *) msg)->Velocity = 0;
    TMidiNoteOffEvent MidiNoteOffEvent((TPlayNoteMsg *) msg);

    delete ((TPlayNoteMsg *) msg);
    return 0;
}

void midiPlay(int octave, unsigned char noteInScale, int program)
{
    TPlayNoteMsg * PlayNoteMsg = new TPlayNoteMsg;
    PlayNoteMsg->Channel = 2;
    PlayNoteMsg->DurationMS = 1000;
    PlayNoteMsg->NoteNumber = octave * 12 + noteInScale;
    PlayNoteMsg->Velocity = 100;
    PlayNoteMsg->Program = program;

    int iret;
    pthread_t thread;
    // Create midi automaton thread
    iret = pthread_create(&thread, NULL, playNoteThread, (void*) PlayNoteMsg);
    PlayNoteMsg->thread = thread;
    if (iret)
    {
        fprintf(stderr, "Error - pthread_create() return code: %d\n", iret);
        exit(EXIT_FAILURE);
    }

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
    printf("%s \n", message);

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
            printf("Octave %i\n", octave);
            break;
        case '=':
            octave++;
            printf("Octave %i\n", octave);
            break;

        case 195:
            program--;
            printf("Program %i\n", program);
            break;
        case '*':
            program++;
            printf("Program %i\n", program);
            break;

        case 'a':
            noteInScale = 0;
            midiPlay(octave, noteInScale, program);
            break;
        case 'é':
            noteInScale = 1;
            midiPlay(octave, noteInScale, program);
            break;
        case 'z':
            noteInScale = 2;
            midiPlay(octave, noteInScale, program);
            break;
        case '"':
            noteInScale = 3;
            midiPlay(octave, noteInScale, program);
            break;
        case 'e':
            noteInScale = 4;
            midiPlay(octave, noteInScale, program);
            break;
        case 'r':
            noteInScale = 5;
            midiPlay(octave, noteInScale, program);
            break;
        case '(':
            noteInScale = 6;
            midiPlay(octave, noteInScale, program);
            break;
        case 't':
            noteInScale = 7;
            midiPlay(octave, noteInScale, program);
            break;
        case '-':
            noteInScale = 8;
            midiPlay(octave, noteInScale, program);
            break;
        case 'y':
            noteInScale = 9;
            midiPlay(octave, noteInScale, program);
            break;
        case 'è':
            noteInScale = 10;
            midiPlay(octave, noteInScale, program);
            break;
        case 'u':
            noteInScale = 11;
            midiPlay(octave, noteInScale, program);
            break;
        case 'i':
            noteInScale = 12;
            midiPlay(octave, noteInScale, program);
            break;
        case 'ç':
            noteInScale = 13;
            midiPlay(octave, noteInScale, program);
            break;
        case 'o':
            noteInScale = 14;
            midiPlay(octave, noteInScale, program);
            break;
        case 'à':
            noteInScale = 15;
            midiPlay(octave, noteInScale, program);
            break;
        case 'p':
            noteInScale = 16;
            midiPlay(octave, noteInScale, program);
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

void Chord1_Off_Thread(void)
{
    TMidiNoteOffEvent no(2, 53, 0);
    TMidiNoteOffEvent no1(2, 56, 0);
}

void Chord1_Off(void)
{
    ExecuteAfterTimeout(Chord1_Off_Thread, 600);
}

void Chord2_On(void)
{
    TMidiNoteOnEvent no(2, 60, 100);
    TMidiNoteOnEvent no1(2, 63, 100);
}

void Chord2_Off_Thread(void)
{
    TMidiNoteOffEvent no(2, 60, 0);
    TMidiNoteOffEvent no1(2, 63, 0);
}

void Chord2_Off(void)
{
    ExecuteAfterTimeout(Chord2_Off_Thread, 600);
}

void Chord3_On(void)
{
    TMidiNoteOnEvent no(2, 50, 100);
    TMidiNoteOnEvent no1(2, 43, 100);
}

void Chord3_Off_Thread(void)
{
    TMidiNoteOffEvent no(2, 50, 0);
    TMidiNoteOffEvent no1(2, 43, 0);
}

void Chord3_Off(void)
{
    ExecuteAfterTimeout(Chord3_Off_Thread, 600);
}

}

}

namespace JJDebout
{

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

}

namespace Coolio
{
namespace Gansta_s_Paradise
{
    struct timeval tv1, tv2;

    char Melody[] = {0, 0, 0, 0, -1, -1, 0, -5}; // do do do do si si do sol  (and loop)
    void Note_ShortDuration(int NoteNumber)
    {
        TMidiNoteOnEvent no1(2,NoteNumber, 100);
        ExecuteAfterTimeout(Chord1_Off_Thread, 200);
        TMidiNoteOffEvent no2(2, NoteNumber, 0);
    }

    void Start_NoteOn(void)
    {
        gettimeofday(&tv1, NULL);
        TMidiNoteOnEvent
    ExecuteAfterTimeout(Chord1_Off_Thread, 600);

    }


    void Start_NoteOff(void)
    {


    }


    void Stop(void)
    {


    }

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
        printf("StartSequencer called twice\n");
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


}

void *threadMidiAutomaton(void * ptr)
{
    int err;
    int thru = 0;
    int verbose = 1;

    int fd_in = -1, fd_out = -1;

    if (verbose)
    {
        fprintf(stderr, "Using: \n");
        fprintf(stderr, "Input: ");
        if (device_in_str)
        {
            fprintf(stderr, "device %s\n", device_in_str);
        }
        else
        {
            fprintf(stderr, "NONE\n");
        }
        fprintf(stderr, "Output: ");
        if (device_out_str)
        {
            fprintf(stderr, "device %s\n", device_out_str);
        }
        else
        {
            fprintf(stderr, "NONE\n");
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
                smProcessRequest,
                smInterpretMidiNote,
                smSendMidiNotes
            } stateMachine = smInit;

            while (!stop)
            {
                if (verbose)
                {
                    fprintf(stderr, "read %02x\n", ch);
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
                    if (ch >= 1 && ch <= 6)
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
                    stateMachine = smProcessRequest;
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
                    printf("Controller Change: Control Number %i; Control Value %i\n", rxControllerNumber, rxControllerValue);
                    break;

                case smProcessRequest:
                    if (rxNote == 1 && rxVolume > 0)
                    {
                        // Rihanna, Wild Thoughts
                        Rihanna::WildThoughts::Chord1_On();
                    }

                    if (rxNote == 1 && rxVolume == 0)
                    {
                        // Rihanna, Wild Thoughts
                        Rihanna::WildThoughts::Chord1_Off();

                    }

                    if (rxNote == 2 && rxVolume > 0)
                    {
                        // Rihanna, Wild Thoughts
                        Rihanna::WildThoughts::Chord2_On();
                    }

                    if (rxNote == 2 && rxVolume == 0)
                    {
                        // Rihanna, Wild Thoughts
                        Rihanna::WildThoughts::Chord2_Off();
                    }

                    if (rxNote == 3 && rxVolume > 0)
                    {
                        // Rihanna, Wild Thoughts
                        Rihanna::WildThoughts::Chord3_On();
                    }

                    if (rxNote == 3 && rxVolume == 0)
                    {
                        // Rihanna, Wild Thoughts
                        Rihanna::WildThoughts::Chord3_Off();
                    }

                    if (rxNote == 4 && rxVolume > 0)
                    {
                        // Capitaine Flam - Laser
                        JJDebout::CapitaineFlam::Laser_On();

                    }

                    if (rxNote == 4 && rxVolume == 0)
                    {
                        // Capitaine Flam - Laser
                        JJDebout::CapitaineFlam::Laser_Off();

                    }

                    if (rxNote == 5 && rxVolume > 0)
                    {
                        // Ave Maria
                        AveMaria::AveMaria_Start();


                    }


                    if (rxNote == 6 && rxVolume > 0)
                    {
                        // Ave Maria
                        AveMaria::AveMaria_Stop();


                    }


                    stateMachine = smWaitMidiChar1;
                    break;

                case smProcessControllerChange:
                    if (rxControllerNumber == 50)
                    {
                        if(SequencerRunning == 1)
                        {
                            seq_midi_tempo_direct(((float)rxControllerValue- 60)/60, 100, 160);
                        }
                    }

                default:
                    stateMachine = smInit;

                }

            }
        }
    }

    fprintf(stderr, "Closing\n");

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

int main(int argc, char** argv)
{
    pthread_t thread1;

    // Create midi automaton thread
    const char *message1 = "";
    int iret1;
    iret1 = pthread_create(&thread1, NULL, threadMidiAutomaton,
                           (void*) message1);
    if (iret1)
    {
        fprintf(stderr, "Error - pthread_create() return code: %d\n", iret1);
        exit(EXIT_FAILURE);
    }

    if (argc != 1)
    {
        usage();
        exit(0);
    }

    threadKeyboard(0);

}
