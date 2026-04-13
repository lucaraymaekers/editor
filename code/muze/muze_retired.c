
// NOTE(luca): I like this one, so I'd like to repurpose it later.
#if 0
UI_CUSTOM_DRAW(CustomDrawSheetMusic)
{
    muze_box_data *Data = (muze_box_data *)CustomDrawData;
    ui_box *Box = Data->Box;
    app_state *App = Data->App;
    
    DrawRect(Box->Rec, Color_Snow2, 0.f, 0.f, 0.f);
    
    f32 StartX = Box->FixedPosition.X + 30.f;
    f32 StartY = Box->FixedPosition.Y + 30.f;
    
    for EachIndex(Idx, App->NotesCount)
    {        
        note *Note = App->Notes + Idx;
        
        Assert(!(App->IsPlaying && Note->Duration == 0.f));
        
        b32 IsOnNote = false;
        {
            f32 RelTimestamp = (Note->Timestamp - App->RecordStart);
            IsOnNote = (App->PlayPos >= RelTimestamp &&
                        App->PlayPos < RelTimestamp + Note->Duration);
        }
        
        b32 IsPlaying = ((App->IsRecording && Note->Duration == 0.f) ||
                         (App->IsPlaying && IsOnNote));
        if(IsPlaying)
        {
            u8 MaxPitch = 12;
            u8 PitchClass = Note->Pitch % MaxPitch;
            
            f32 NoteSize = 14.f;
            
            s32 Length = 1;
            
            f32 NoteBorderSize = 0.f;
            
            f32 Duration = Note->Duration;
            if(Duration == 0.f)
            {
                Assert(App->IsRecording);
                f32 Now = GetWallTime();
                Duration = (Now - Note->Timestamp);
            }
            
            v2 NotePos = V2(StartX, StartY + (f32)(MaxPitch - PitchClass)*NoteSize);
            
            f32 BPM = App->BPM;
            f32 NoteLength = (Duration*(BPM/60.f));
            
            if(NoteLength < 4.f)
            {
                f32 TailHeight = 36.f;
                f32 X = NotePos.X + NoteSize - NoteBorderSize - 2.f;
                f32 Y = NotePos.Y + .5f*NoteSize;
                v4 Dest = RectFromSize(V2(X, Y - TailHeight),
                                       V2(2.f, TailHeight));
                rect_instance *Inst = DrawRect(Dest, Color_Black, 0.f, 0.f, 0.f);
                Inst->CornerRadii.e[2] = .5f;
                
                
                if(NoteLength <= .5f)
                {
                    f32 NotePow2 = log2f(1.f/NoteLength);
                    f32 NearestPow2 = powf(2.0f, roundf(NotePow2));
                    s32 NumOfSideTails = (s32)log2f(NearestPow2);
                    for EachIndex(TailIdx, NumOfSideTails)
                    {
                        f32 SideTailY = ((Y + 5.f*(f32)TailIdx) - TailHeight);
                        v4 TailDest = RectFromSize(V2(X, SideTailY), V2(8.f, 3.f));
                        
                        DrawRect(TailDest, Color_Black, 0.f, 0.f, 0.f);
                    }
                }
                
            }
            
            if(NoteLength >= 2.f)
            {
                NoteBorderSize = 2.f;
            }
            
            {
                v2 NoteDim = V2(NoteSize, NoteSize);
                v4 Dest = RectFromSize(NotePos, NoteDim);
                DrawRect(Dest, Color_Black, .5f*NoteSize, NoteBorderSize, .5);
            }
            
            {
                str8 NoteString = PushS8(FrameArena, 3);
                
                Assert(ArrayCount(NotePianoColors) == ArrayCount(NotePitches));
                Assert(ArrayCount(NotePitches) == 12);
                
                str8 LengthString = Str8Fmt(S8Fmt " %.2f", S8Arg(NotePitches[PitchClass]), NoteLength);
                
                font_atlas *Atlas = UI_State->Atlas;
                
                rune Shift = UI_GetShiftForFont(Box->FontKind);
                
                v2 Cur = V2(NotePos.X + NoteSize, NotePos.Y);
                for EachIndex(CharIdx, LengthString.Size)
                {
                    rune Char = (rune)(LengthString.Data[CharIdx]) + Shift;
                    f32 CharWidth = (Atlas->PackedChars[Char - Atlas->FirstCodepoint].xadvance);
                    f32 CharHeight = (Atlas->HeightPx);
                    
                    v2 CurMax = V2AddV2(Cur, V2(CharWidth, CharHeight));
                    if(IsInsideRectV2(Cur, Box->Rec) &&
                       IsInsideRectV2(CurMax, Box->Rec))
                    {
                        DrawRectChar(Atlas, Cur, Char, Box->TextColor);
                        
                        Cur.X += CharWidth;
                    }
                }
            }
        }
        
        // TODO(luca): Write note name A,B,C etc.
        // TODO(luca): Write duration fraction next to it as well.
    }
    
}
#endif
