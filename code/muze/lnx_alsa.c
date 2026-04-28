#include <alsa/asoundlib.h>
#include <stdio.h>
#include "base/base.h"
#include "base/base.c"

internal b32
ReceiveMidiMessages(int SrcClient, int SrcPort)
{
    b32 Result = 0;
    
    local_persist snd_seq_t *Seq = 0;
    local_persist int InPort = 0;
    if(!Seq)
{        
    snd_seq_open(&Seq, "default", SND_SEQ_OPEN_INPUT, 0);
    snd_seq_set_client_name(Seq, "MIDI Receiver");
    
    InPort = snd_seq_create_simple_port(Seq, "In",
                                            SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE,
                                            SND_SEQ_PORT_TYPE_MIDI_GENERIC);
        
        snd_seq_connect_from(Seq, InPort, SrcClient, SrcPort);
    }
    
    snd_seq_event_t *Event;
    while(snd_seq_event_input(Seq, &Event) >= 0)
    {
        u8 Status = 0, Data1 = 0, Data2 = 0;
        
        switch(Event->type)
        {
            case SND_SEQ_EVENT_NOTEON:
{
            Status = 0x90 | Event->data.note.channel;
            Data1  = Event->data.note.note;
                Data2  = Event->data.note.velocity;
                Log("NoteOn\n");
} break;
            case SND_SEQ_EVENT_NOTEOFF:
{
            Status = 0x80 | Event->data.note.channel;
            Data1  = Event->data.note.note;
            Data2  = Event->data.note.velocity;
} break;
            case SND_SEQ_EVENT_CONTROLLER:
{
            Status = 0xB0 | Event->data.control.channel;
            Data1  = (u8)Event->data.control.param;
            Data2  = (u8)Event->data.control.value;
} break;
            case SND_SEQ_EVENT_PITCHBEND:
{
            Status = 0xE0 | Event->data.control.channel;
            Data1  = Event->data.control.value & 0x7F;
            Data2  = (Event->data.control.value >> 7) & 0x7F;
} break;
        }
    }
    
    return true;
}

internal inline u32
MicroSecondsFromSeconds(u32 Seconds)
{
    u32 Result = Seconds * 1000 * 1000;
    return Result;
}

ENTRY_POINT(EntryPoint)
{
    if(LaneIndex() == 0)
{
        while(ReceiveMidiMessages(16, 0) != 0);
    }

    return 0;
}
