
uint8_t stm[12];
uint8_t channel = 0;
uint8_t voice = 0;
uint8_t bank = 0;
uint8_t effect = 0;
uint8_t misc = 0;
uint8_t tune_m = 0x04;
uint8_t tune_l = 0x00;
uint8_t transp = 0x40;
uint8_t reverb = 0x40;
uint8_t chorus = 0x00;
uint8_t celeste = 0x00;
uint8_t flanger = 0x00;
char *v_text[] = {"GrandPno","BritePno","E.Grand","HnkyTonk","E.Piano1","E.Piano2","Harpsi.","Clavi.","Celesta","Glocken","MusicBox","Vibes","Marimba","Xylophon","TubulBel","Dulcimer","DrawOrgn","PercOrgn","RockOrgn","ChrchOrg","ReedOrgn","Acordion","Harmnica","TangoAcd","NylonGtr","SteelGtr","JazzGtr","CleanGtr","Mute.Gtr","Ovrdrive","Dist.Gtr","GtrHarmo","Aco.Bass","FngrBass","PickBass","Fretless","SlapBas1","SlapBas2","SynBass1","SynBass2","Violin","Viola","Cello","Contrabs","Trem.Str","Pizz.Str","Harp","Timpani","Strings1","Strings2","Syn.Str1","Syn.Str2","ChoirAah","VoiceOoh","SynVoice","Orch.Hit","Trumpet","Trombone","Tuba","Mute.Trp","Fr.Horn","BrasSect","SynBras1","SynBras2","SprnoSax","AltoSax","TenorSax","Bari.Sax","Oboe","Eng.Horn","Bassoon","Clarinet","Piccolo","Flute","Recorder","PanFlute","Bottle","Shakhchi","Whistle","Ocarina","SquareLd","Saw.Lead","CaliopLd","ChiffLd","CharanLd","VoiceLd","FifthLd","Bass&Ld","NewAgePd","WarmPad2","PolySyPd","ChoirPad","BowedPad","MetalPad","HaloPad","SweepPad","Rain","SoundTrk","Crystal","Atmosphr","Bright","Goblins","Echoes","SciFi","Sitar","Banjo","Shamisen","Koto","Kalimba","Bagpipe","Fiddle","Shanai","TnklBell","Agogo","SteelDrm","WoodBlok","TaikoDrm","MelodTom","Syn.Drum","RevCymbl","FretNoiz","BrthNoiz","Seashore","Tweet","Telphone","Helicptr","Applause","Gunshot"};
//uint8_t var_tab[] = {0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0A,0x0B,0x14,0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4A,0x4B,0x4C,0x4D,0x4E};
uint32_t eff_tab[] = {0x100,0x101,0x200,0x201,0x202,0x300,0x301,0x400,0x1000,0x1100,0x1300,0x204100,0x204101,0x204102,0x204108,0X204200,0x204201,0x204202,0x204208,0x204300,0x204301,0x204308};
char *eff_text[] = {"Hall_1","Hall_2","Room_1","Room_2","Room_3","Stage_1","Stage_2","Plate","Wh.Room","Tunnel","Base","Chorus_1","Chorus_2","Chorus_3","Chorus_4","Celeste_1","Celeste_2","Celeste_3","Celeste_4","Flanger_1","Flanger_2","Flanger_3"};
char *var_text[] = {"---","Hall","Room","Stage","Plate","3Delay","2Delay","Echo","Cross","Refl","GatedR","RevGate","Kara","Chorus","Celeste","Flanger","Symph","Rot","Trem","AutoP","Phaser","Dist","Overdr","AmpSim","EQ3","EQ2","AutoW"};
char *misc_text[] = {"Reset","Transpose","Tune"};
char *menu_text[] = {"Channel","Voice","Bank","Effect","Misc"};

