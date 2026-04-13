#include <alsa/asoundlib.h>
#include <stdio.h>
#include "code/base/base.h"
void ListMidiDevices()
{
    snd_seq_t *Seq;
    snd_seq_open(&Seq, "default", SND_SEQ_OPEN_DUPLEX, 0);
    snd_seq_set_client_name(Seq, "MIDI Lister");
    
    snd_seq_client_info_t *ClientInfo;
    snd_seq_port_info_t   *PortInfo;
    
    snd_seq_client_info_alloca(&ClientInfo);
    snd_seq_port_info_alloca(&PortInfo);
    
    snd_seq_client_info_set_client(ClientInfo, -1);
    while(snd_seq_query_next_client(Seq, ClientInfo) >= 0)
    {
        int Client = snd_seq_client_info_get_client(ClientInfo);
        
        snd_seq_port_info_set_client(PortInfo, Client);
        snd_seq_port_info_set_port(PortInfo, -1);
        while(snd_seq_query_next_port(Seq, PortInfo) >= 0)
        {
            unsigned int Caps = snd_seq_port_info_get_capability(PortInfo);
            unsigned int Type = snd_seq_port_info_get_type(PortInfo);
            
            if(!(Type & SND_SEQ_PORT_TYPE_MIDI_GENERIC) &&
               !(Type & SND_SEQ_PORT_TYPE_MIDI_GM)      &&
               !(Type & SND_SEQ_PORT_TYPE_SYNTHESIZER))
            {
                continue;
            }
            
            b32 CanRead    = (Caps & SND_SEQ_PORT_CAP_READ)       != 0;
            b32 CanWrite   = (Caps & SND_SEQ_PORT_CAP_WRITE)      != 0;
            b32 SubsRead   = (Caps & SND_SEQ_PORT_CAP_SUBS_READ)  != 0;
            b32 SubsWrite  = (Caps & SND_SEQ_PORT_CAP_SUBS_WRITE) != 0;
            
            if(!CanRead && !CanWrite) continue;
            
            b32 IsReadOnly  = CanRead  && SubsRead  && !CanWrite && !SubsWrite;
            b32 IsWriteOnly = CanWrite && SubsWrite && !CanRead  && !SubsRead;
            b32 IsBidir     = (CanRead || SubsRead) && (CanWrite || SubsWrite);
            
            char *Mode;
            if     (IsReadOnly)  Mode = "OUTPUT only";
            else if(IsWriteOnly) Mode = "INPUT only";
            else if(IsBidir)     Mode = "BIDIRECTIONAL";
            else                 Mode = "UNKNOWN";
            
            printf("Client %3d: %-30s Port %3d: %-25s [%s]\n",
                   Client,
                   snd_seq_client_info_get_name(ClientInfo),
                   snd_seq_port_info_get_port(PortInfo),
                   snd_seq_port_info_get_name(PortInfo),
                   Mode);
        }
    }
    
    snd_seq_close(Seq);
}

// Send a MIDI message to a device (client:port)
void SendMidiMessage(int DestClient, int DestPort, u8 Status, u8 Data1, u8 Data2)
{
    snd_seq_t *Seq;
    snd_seq_open(&Seq, "default", SND_SEQ_OPEN_OUTPUT, 0);
    snd_seq_set_client_name(Seq, "MIDI Sender");
    
    int OutPort = snd_seq_create_simple_port(Seq, "Out",
                                             SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ,
                                             SND_SEQ_PORT_TYPE_MIDI_GENERIC);
    
    snd_seq_connect_to(Seq, OutPort, DestClient, DestPort);
    
    snd_seq_event_t Event;
    snd_seq_ev_clear(&Event);
    snd_seq_ev_set_direct(&Event);
    snd_seq_ev_set_source(&Event, OutPort);
    snd_seq_ev_set_dest(&Event, DestClient, DestPort);
    
    // Build raw MIDI event from status/data bytes
    // e.g. Status=0x90 (note on), Data1=60 (middle C), Data2=127 (velocity)
    u8 MsgBytes[3] = { Status, Data1, Data2 };
    snd_seq_ev_set_sysex(&Event, sizeof(MsgBytes), MsgBytes);
    
    snd_seq_event_output(Seq, &Event);
    snd_seq_drain_output(Seq);
    
    snd_seq_close(Seq);
}

// Receive MIDI messages from a device (client:port)
// Calls Callback for each event received, runs until Callback returns false
void ReceiveMidiMessages(int SrcClient, int SrcPort,
                         b32 (*Callback)(u8 Status, u8 Data1, u8 Data2))
{
    snd_seq_t *Seq;
    snd_seq_open(&Seq, "default", SND_SEQ_OPEN_INPUT, 0);
    snd_seq_set_client_name(Seq, "MIDI Receiver");
    
    int InPort = snd_seq_create_simple_port(Seq, "In",
                                            SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE,
                                            SND_SEQ_PORT_TYPE_MIDI_GENERIC);
    
    snd_seq_connect_from(Seq, InPort, SrcClient, SrcPort);
    
    snd_seq_event_t *Event;
    while(snd_seq_event_input(Seq, &Event) >= 0)
    {
        u8 Status = 0, Data1 = 0, Data2 = 0;
        
        switch(Event->type)
        {
            case SND_SEQ_EVENT_NOTEON:
            Status = 0x90 | Event->data.note.channel;
            Data1  = Event->data.note.note;
            Data2  = Event->data.note.velocity;
            break;
            case SND_SEQ_EVENT_NOTEOFF:
            Status = 0x80 | Event->data.note.channel;
            Data1  = Event->data.note.note;
            Data2  = Event->data.note.velocity;
            break;
            case SND_SEQ_EVENT_CONTROLLER:
            Status = 0xB0 | Event->data.control.channel;
            Data1  = Event->data.control.param;
            Data2  = Event->data.control.value;
            break;
            case SND_SEQ_EVENT_PITCHBEND:
            Status = 0xE0 | Event->data.control.channel;
            Data1  = Event->data.control.value & 0x7F;
            Data2  = (Event->data.control.value >> 7) & 0x7F;
            break;
            default:
            continue;
        }
        
        if(!Callback(Status, Data1, Data2)) break;
    }
    
    snd_seq_close(Seq);
}

int main(void)
{
    ListMidiDevices();
    
    
    return 0;
}