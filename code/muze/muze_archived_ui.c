//~ Keyboard
case 'p':
{
    for EachIndex(VoiceIdx, App->Muze.VoiceCount)
    {
        voice *VoiceAt = App->Muze.Voices + VoiceIdx;
        StopRecording(Memory, App, VoiceAt, Input->dtForFrame);
        
        if(VoiceAt->IsPlaying)
        {
            VoiceAt->IsPlaying = false;
        }
        else
        {
            StopAllPlayingNotes(Memory, App, VoiceAt, Input->dtForFrame);
            VoiceAt->IsPlaying = true;
        }
    }
} break;

case ' ':
{
    if(Voice->IsRecording)
    {
        StopRecording(Memory, App, Voice, Input->dtForFrame);
    }
    else
    {
        StopAllPlayingNotes(Memory, App, Voice, Input->dtForFrame);
        StartRecording(Voice);
    }
} break;

//- Panels 
case 'p':
{
    if(!Shift)
    {
        App->SelectedPanel = SplitPanel(App->SelectedPanel, Axis2_X, false);
        MakePanelMuze(App->SelectedPanel, App);
    }
    else
    {
        App->SelectedPanel = ClosePanel(App, App->SelectedPanel);
    }
} break;

case '-':
{
    if(!Shift)
    {                        
        App->SelectedPanel = SplitPanel(App->SelectedPanel, Axis2_Y, false);
        MakePanelMuze(App->SelectedPanel, App);
    }
} break;

case ',':
{
    App->SelectedPanel = (IsNilPanel(App->SelectedPanel) ?
                          PanelNextLeaf(App->FirstPanel, true) :
                          (!Shift ?
                           PanelNextLeaf(App->SelectedPanel, false) :
                           PanelNextLeaf(App->SelectedPanel, true)));
} break;

//~ UI
UI_Push()
UI_SemanticWidth(UI_SizePx(100.f, 1.f))
//UI_SemanticWidth(UI_SizeText(4.f, 1.f))
UI_SemanticHeight(UI_SizeText(2.f, 1.f))
{
    UI_SemanticHeight(UI_SizeChildren(1.f))
        UI_List(Axis2_Y, S8("Input Devices"))
    {
        b32 DeviceChanged = !Memory->Initialized;
        
        // Animation speed slider
        {                        
            local_persist f32 AnimSpeed = 30.f;
            AnimSpeed = UI_Slider(1.f,  64.f, 0.f, AnimSpeed, S8("Speed"), Color_Blue, true);
            UI_State->AnimSpeed = (1.f - powf(2.f, -AnimSpeed*UI_State->Input->dtForFrame));
        }
        
        if(UI_ToggleButton(S8("Virtual Keyboard"), App->Muze.IsInputVirtualKeyboard, Color_Yellow))
        {
            if(!App->Muze.IsInputVirtualKeyboard)
            {
                App->Muze.IsInputVirtualKeyboard = true;
            }
        }
        
        for EachIndex(Idx, DevicesArray.Count)
        {
            platform_midi_device *Device = DevicesArray.Devices + Idx;
            if(Device->IsInput)
            {
                b32 Selected = (!App->Muze.IsInputVirtualKeyboard && 
                                (Device->Id == App->Muze.In.Id));
                
                if(UI_ToggleButton(Device->Name, Selected, Color_Yellow))
                {
                    if(!Selected)
                    {
                        StopAllPlayingNotes(Memory, App, Voice, Input->dtForFrame);
                        
                        App->Muze.IsInputVirtualKeyboard = false;
                        App->Muze.In = *Device;
                        Memory->PlatformMIDIListen(App->Muze.In);
                    }
                }
            }
        }
    }
    
    UI_List(Axis2_Y, S8("Output Devices"))
    {
        
        UI_State->RectDebugMode ^= UI_Checkbox(S8("Debug"), UI_State->RectDebugMode);
        
        if(UI_Checkbox(S8("Record"), Voice->IsRecording))
        {                                        
            ToggleRecording(Memory, App, Voice, Input->dtForFrame);
        }
        
        if(UI_ToggleButton(S8("Virtual Synth"), App->Muze.IsOutputSynth, Color_Yellow))
        {
            if(!App->Muze.IsOutputSynth)
            {
                StopAllPlayingNotes(Memory, App, Voice, Input->dtForFrame);
                App->Muze.IsOutputSynth = true;
            }
        }
        
        for EachIndex(Idx, DevicesArray.Count)
        {         
            platform_midi_device *Device = DevicesArray.Devices + Idx;
            if(Device->IsOutput)
            {                            
                b32 Selected = (!App->Muze.IsOutputSynth && (Device->Id == App->Muze.Out.Id));
                
                if(UI_ToggleButton(Device->Name, Selected, Color_Yellow))
                {
                    if(!Selected)
                    {
                        StopAllPlayingNotes(Memory, App, Voice, Input->dtForFrame);
                        
                        App->Muze.IsOutputSynth = false;
                        App->Muze.Out = *Device;
                    }
                }
            }
        }
    }
    
    UI_List(Axis2_Y, S8("Controls"))
    {
        if(UI_Button(S8("Reset")))
        {
            VoiceReset(Voice);
            Voice->IsRecording = false;
            Voice->IsPlaying = false;
            StopAllPlayingNotes(Memory, App, Voice, Input->dtForFrame);
        }
        
        if(UI_Checkbox(S8("Record"), Voice->IsRecording))
        {                                        
            if(Voice->IsRecording)
            {
                StopRecording(Memory, App, Voice, Input->dtForFrame);
            }
            else
            {
                StartRecording(Voice);
            }
            Voice->IsPlaying = false;
        }
        
        if(UI_ToggleButton(S8("Play"), Voice->IsPlaying, Color_Green))
        {
            StopRecording(Memory, App, Voice, Input->dtForFrame);
            
            if(Voice->IsPlaying)
            {
                Voice->IsPlaying = false;
            }
            else
            {
                Voice->IsPlaying = true;
            }
        }
        
        if(UI_Button(S8("Stop")))
        {
            StopRecording(Memory, App, Voice, Input->dtForFrame);
            Voice->IsPlaying = false;
            Voice->PlayPos = 0.f;
        }
    }
    
    UI_List(Axis2_Y,  S8("Edits"))
    {
        if(UI_Button(S8("Trim")))
        {
            if(!IsNilNote(Voice->FirstNote))
            {
                StopAllPlayingNotes(Memory, App, Voice, Input->dtForFrame);
                
                note *LastNote = Voice->LastNote;
                
                f32 LastNoteEnd = (LastNote->Timestamp + LastNote->Duration); 
                //- Find the last note's end.
                {
                    for EachNote(Note, Voice->FirstNote)
                    {
                        if(Note->Kind == NoteKind_Note)
                        {
                            LastNoteEnd = Max(LastNoteEnd, (Note->Timestamp + Note->Duration));
                        }
                    }
                }
                
                Voice->RecordLength = LastNoteEnd;
                
                note *FirstNote = Voice->FirstNote;
                //- Find the first note played.
                {
                    for EachNote(Note, FirstNote)
                    {
                        if(Note->Kind == NoteKind_Note)
                        {
                            FirstNote = Note;
                            break;
                        }
                    }
                }
                
                f32 StartSilence = FirstNote->Timestamp;
                
                //- Get new voice length 
                {                                    
                    Voice->RecordLength -= StartSilence;
                    
                    f32 BeatsPerSecond = App->Muze.BPM/60.f;
                    f32 SecondsPerBeat = 1.f/BeatsPerSecond;
                    
                    f32 BarTime = SecondsPerBeat*(f32)App->Muze.TimeSig;
                    // TODO(luca): Intrinsic
                    f32 Pad = ceilf(Voice->RecordLength/BarTime)*BarTime;
                    
                    Voice->RecordLength = Pad;
                }
                
                //- Update notes .
                {
                    for EachNote(Note, Voice->FirstNote)
                    {
                        
                        if(Note->Kind == NoteKind_Pedal)
                        {
                            // If the pedal note is before the first note, then we forward it up to the firstnote.
                            f32 Diff = (FirstNote->Timestamp - Note->Timestamp);
                            if(Diff > 0.f)
                            {
                                Note->Duration -= Diff;
                                Note->Timestamp += Diff;
                            }
                            
                            // Clamp the end to the record's length.
                            {
                                f32 NewTimestamp = Note->Timestamp - StartSilence;
                                
                                f32 NoteEnd = NewTimestamp + Note->Duration;
                                f32 NewEnd = Min(NoteEnd, Voice->RecordLength);
                                Note->Duration = (NewEnd - NewTimestamp);
                            }
                        }
                        
                        Note->Timestamp -= StartSilence;
                        
                    }
                }
            }
        }
        
        if(UI_Button(S8("Round")))
        {
            f32 BPS = App->Muze.BPM/60.f;
            
            for EachNoteNode(Note, Voice->NoteSel)
            {
                f32 NoteLength = Note->Duration*BPS;
                f32 DenomOfFraction = roundf(-log2f(NoteLength));
                f32 NoteValue = (DenomOfFraction > 0.f ? 
                                 (1.f/2.f*DenomOfFraction) : 
                                 roundf(NoteLength));
                f32 NewDuration = NoteValue/BPS;
                Note->Duration = NewDuration;
            }
        }
        
        if(UI_Button(S8("Sync")))
        {
            f32 BPS = App->Muze.BPM/60.f;
            for EachNoteNode(Note, Voice->NoteSel)
            {
                f32 NoteLength = Note->Duration*BPS;
                f32 DenomOfFraction = roundf(-log2f(NoteLength));
                f32 NoteValue = (DenomOfFraction > 0.f ? 
                                 (1.f/2.f*DenomOfFraction) : 
                                 roundf(NoteLength));
                
                f32 Timestamp = BPS*Note->Timestamp;
                
                f32 NewTimestamp = Note->Timestamp;
                NewTimestamp = (NoteValue >= 1.f ? 
                                roundf(Timestamp) : 
                                (roundf(Timestamp/NoteValue)*NoteValue));
                
                f32 RealNewTimestamp = NewTimestamp/BPS;
                Note->Timestamp = RealNewTimestamp;
                
            }
        }
        
        if(UI_Button(S8("Clear")))
        {
            Voice->NoteSel = 0;
        }
        
        if(UI_Button(S8("Delete")))
        {
            for(note_node *Node = Voice->NoteSel;
                Node;)
            {
                Voice->NoteCount -= 1;
                
                note *Note = Node->Value;
                
                // Remove the note
                {                                
                    if(!IsNilNote(Note->Next))
                    {
                        Note->Next->Prev = Note->Prev;
                    }
                    
                    if(!IsNilNote(Note->Prev))
                    {
                        Note->Prev->Next = Note->Next;
                    }
                    
                    if(Note == Voice->FirstNote)
                    {
                        Voice->FirstNote = Note->Next;
                    }
                    
                    if(Note == Voice->LastNote)
                    {
                        Voice->LastNote = Note->Prev;
                    }
                }
                
                note_node *NextNode = Node->Next;
                
                // Remove from selection
                {                                
                    // Remove links
                    {
                        if(Node->Prev)
                        {
                            Node->Prev->Next = Node->Next;
                        }
                        
                        if(Node->Next)
                        {
                            Node->Next->Prev = Node->Prev;
                        }
                        
                        if(Node == Voice->NoteSel)
                        {
                            Voice->NoteSel = (Node->Next ? Node->Next : Node->Prev);
                        }
                    }
                    
                    // Put on free list
                    {
                        Node->Next = App->Muze.FirstFreeNode;
                        App->Muze.FirstFreeNode = Node;
                    }
                }
                
                Node = NextNode;
            }
        }
        
        if(UI_Button(S8("GuessBPM")))
        {
            note_node *Node = Voice->NoteSel;
            
            f32 AveragedDiff = 0.f;
            f32 Diff = 0.f;
            
            if(Node && Node->Next)
            {
                note *Start = Node->Value;
                note *End = Node->Next->Value;
                
                if(Start->Timestamp > End->Timestamp) Swap(Start, End);
                
                Diff = (End->Timestamp - Start->Timestamp);
                
                Node = Node->Next;
                
                // Time that goes in a bar
                f32 SecondsPerBeat = Diff/(f32)App->Muze.TimeSig;
                App->Muze.BPM = (1.f/SecondsPerBeat)*60.f;
            }
            
        }
    }
    
    UI_List(Axis2_Y, S8("Set Length"))
    {
        if(UI_Button(S8("4")))
        {
            for EachNoteNode(Note, Voice->NoteSel)
            {
                f32 Ratio = 4.f;
                f32 SecondsPerBeat = (1.f/(App->Muze.BPM/60.f));
                Note->Duration = Ratio*SecondsPerBeat;
            }
        }
        if(UI_Button(S8("2")))
        {
            for EachNoteNode(Note, Voice->NoteSel)
            {
                f32 Ratio = 2.f;
                f32 SecondsPerBeat = (1.f/(App->Muze.BPM/60.f));
                Note->Duration = Ratio*SecondsPerBeat;
            }
        }
        if(UI_Button(S8("1")))
        {
            for EachNoteNode(Note, Voice->NoteSel)
            {
                f32 Ratio = 1.f;
                f32 SecondsPerBeat = (1.f/(App->Muze.BPM/60.f));
                Note->Duration = Ratio*SecondsPerBeat;
            }
        }
        if(UI_Button(S8("1/2")))
        {
            for EachNoteNode(Note, Voice->NoteSel)
            {
                f32 Ratio = 1.f/2.f;
                f32 SecondsPerBeat = (1.f/(App->Muze.BPM/60.f));
                Note->Duration = Ratio*SecondsPerBeat;
            }
        }
        if(UI_Button(S8("1/4")))
        {
            for EachNoteNode(Note, Voice->NoteSel)
            {
                f32 Ratio = 1.f/4.f;
                f32 SecondsPerBeat = (1.f/(App->Muze.BPM/60.f));
                Note->Duration = Ratio*SecondsPerBeat;
            }
        }
        if(UI_Button(S8("1/8")))
        {
            for EachNoteNode(Note, Voice->NoteSel)
            {
                f32 Ratio = 1.f/8.f;
                f32 SecondsPerBeat = (1.f/(App->Muze.BPM/60.f));
                Note->Duration = Ratio*SecondsPerBeat;
            }
        }
    }
    
    UI_List(Axis2_Y, S8("Music"))
    {         
        App->Muze.BPM = UI_Slider(30.f, 180.f, .1f, App->Muze.BPM, S8("BPM"), Color_Orange, true);
        App->Muze.TimeSig = (s32)UI_Slider(1.f, 4.f, .25f, (f32)App->Muze.TimeSig, S8("TimeSig"), Color_Orange, true);
    }
    
    UI_List(Axis2_Y, S8("Voices"))
    {
        if(UI_Button(S8("+")))
        {
            if((u64)App->Muze.VoiceCount < App->Muze.MaxVoiceCount)
            {
                voice *New = VoiceAdd(App);
                
                App->Muze.LastSelectedVoice = New;
                App->SelectedPanel->Voice = New;
            }
        }
        
        for EachIndex(Idx, App->Muze.VoiceCount)
        {
            voice *VoiceAt = App->Muze.Voices + Idx;
            b32 Selected = (VoiceAt == App->SelectedPanel->Voice);
            if(UI_ToggleButton(Str8Fmt("%lu", Idx), Selected, Color_Yellow))
            {
                App->SelectedPanel->Voice = VoiceAt;
                App->Muze.LastSelectedVoice = VoiceAt;
            }
        }
    }
    
    UI_List(Axis2_Y, S8("Instruments"))
    {
        // TODO(luca): Proper lister
        s32 Count = tsf_get_presetcount(GlobalTSF);;
        for EachIndex(Idx, Count)
        {
            if(UI_ToggleButton(Str8Fmt("%s", tsf_get_presetname(GlobalTSF, Idx)), (Voice->PresetIdx == Idx), Color_Yellow))
            {
                Voice->PresetIdx = Idx;
                tsf_channel_note_off_all(GlobalTSF, Voice->Channel);
                tsf_channel_set_presetindex(GlobalTSF, Voice->Channel, Voice->PresetIdx);
            }
            
            if(Idx == 5) break;
        }
    }
    
#if MUZE_INTERNAL        
    if(0)
    {                    
        UI_List(Axis2_Y, S8("Recording"))
        {                  
            UI_Labelf("Length: %.2f/%.2f###RecordLength", Voice->RecordLength, ((f32)App->Muze.BPM/60.f)*Voice->RecordLength);
        }
        
        UI_List(Axis2_Y, S8("Playing"))
        {                    
            UI_Labelf("Pos: %.2f###RecordEnd", Voice->PlayPos);
        }
        
        if(Voice->NoteSel)
        {
            UI_List(Axis2_Y, S8("Note"))
            {                    
                note *Sel = Voice->NoteSel->Value;
                f32 BPS = App->Muze.BPM/60.f;
                UI_Labelf("Duration: %.2f/%.2f###Duration", Sel->Duration, BPS*Sel->Duration);
                UI_Labelf("Start: %.2f/%.2f###Start", Sel->Timestamp, BPS*Sel->Timestamp);
                UI_Labelf("Pitch/Vel: %d/%d###PitchAndVelocity", Sel->Pitch, Sel->Velocity);
            }
        }
        
        UI_List(Axis2_Y, S8("UI"))
        {                    
            UI_Labelf("ScrollX: %.2f###ScrollX", ScrollX);
            
            ui_box *Hot = UI_BoxFromKey(UI_State->Hot);
            ui_box *Active = UI_BoxFromKey(UI_State->Active);
            str8 ActiveDisplayString = Active->DisplayString;
            str8 HotDisplayString = Hot->DisplayString;
            if(ActiveDisplayString.Size == 0) ActiveDisplayString = S8("null");
            if(HotDisplayString.Size == 0) HotDisplayString = S8("null");
            
            UI_Labelf("Active: " S8Fmt, S8Arg(ActiveDisplayString));
            UI_Labelf("Hot: " S8Fmt, S8Arg(HotDisplayString));
            
            UI_Labelf("Active->tHot = %.2f", Active->tHot);
            UI_Labelf("Active->tActive = %.2f", Active->tActive);
            UI_Labelf("Hot->tHot = %.2f", Hot->tHot);
            UI_Labelf("Hot->tActive = %.2f", Hot->tActive);
        }
    }
    
    UI_List(Axis2_Y, S8("Memory"))
    {
        arena *Arena = App->Muze.Arena;
        ArenaLabel(S8("Perm"), PermanentArena);
        ArenaLabel(S8("Frame"), FrameArena);
        ArenaLabel(S8("R/O"), App->ReadOnlyArena);
        ArenaLabel(S8("Font"), App->FontAtlasArena);
        ArenaLabel(S8("UI"), App->UIArena);
        ArenaLabel(S8("Panel"), App->PanelArena);
        ArenaLabel(S8("Muze"), App->Muze.Arena);
        ArenaLabel(S8("TSF"), App->TSFArena);
    }
    
#endif
}
#endif