#include <windows.h>
#include <mmsystem.h>
#include <stdio.h>

#pragma comment(lib, "winmm.lib")

typedef struct
{
    MIDIHDR hdr;
    MIDIEVENT events[2];
} midi_buffer;

int main(void)
{
    HMIDISTRM stream;
    UINT deviceID = MIDI_MAPPER;
    
    // 1. Open stream
    if (midiStreamOpen(&stream, &deviceID, 1, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR)
    {
        printf("Failed to open MIDI stream\n");
        return 1;
    }
    
    // 2. Set time division (ticks per quarter note)
    MIDIPROPTIMEDIV div = {0};
    div.cbStruct = sizeof(div);
    div.dwTimeDiv = 480; // standard PPQ
    
    midiStreamProperty(stream, (LPBYTE)&div, MIDIPROP_SET | MIDIPROP_TIMEDIV);
    
    // 3. Create buffer
    midi_buffer buf = {0};
    
    // --- Note ON ---
    DWORD noteOn = 0x90; // channel 0
    noteOn |= (60 << 8);   // middle C
    noteOn |= (100 << 16); // velocity
    
    buf.events[0].dwDeltaTime = 0;
    buf.events[0].dwStreamID = 0;
    buf.events[0].dwEvent = MEVT_SHORTMSG | noteOn;
    
    // --- Note OFF ---
    DWORD noteOff = 0x80;
    noteOff |= (60 << 8);
    
    buf.events[1].dwDeltaTime = 480; // one quarter note later
    buf.events[1].dwStreamID = 0;
    buf.events[1].dwEvent = MEVT_SHORTMSG | noteOff;
    
    // 4. Setup header
    buf.hdr.lpData = (LPSTR)buf.events;
    buf.hdr.dwBufferLength = sizeof(MIDIEVENT) * 2;
    
    midiOutPrepareHeader((HMIDIOUT)stream, &buf.hdr, sizeof(MIDIHDR));
    
    // 5. Send buffer
    midiStreamOut(stream, &buf.hdr, sizeof(MIDIHDR));
    
    // 6. Start playback
    midiStreamRestart(stream);
    
    // Wait for playback (simple hack)
    Sleep(1000);
    
    // 7. Cleanup
    midiOutUnprepareHeader((HMIDIOUT)stream, &buf.hdr, sizeof(MIDIHDR));
    midiStreamClose(stream);
    
    printf("Done\n");
    return 0;
}