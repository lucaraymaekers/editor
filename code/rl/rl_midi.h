/* date = April 16th 2026 11:27 pm */

#pragma once

typedef union midi_message midi_message;
union midi_message
{
    u32 U32[1];
    u8 U8[4];
};

typedef enum midi_event_type midi_event_type;
enum midi_event_type
{
    MIDIEventType_NoteOn = 0x90,
    MIDIEventType_NoteOff = 0x80
};
