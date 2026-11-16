#ifndef __CONTROLLER_H
#define __CONTROLLER_H

#include <cctype>

#include "Dac.h"
#include "BeatList.h"
#include "Euclid.h"
#include "Global.h"
#include "Lcd.h"
#include "fatfs.h"
#include "main.h"
#include "sdmmc.h"
#include "stm32h7xx_hal.h"
#include "string.h"

// clang-format off

/* Memory Allocation In D1 Ram ----------------------------------- */

extern uint32_t i2s_rxBuffer[I2S_TOTAL_BUFFER_SIZE];
extern uint32_t i2s_txBuffer[I2S_TOTAL_BUFFER_SIZE];

/* Memory Allocation In D3 Ram ----------------------------------- */

extern __attribute__((section(".RAM_D3"))) uint8_t i2c3_rxData[2];
extern __attribute__((section(".RAM_D3"))) uint8_t i2c3_txData[2];
extern __attribute__((section(".RAM_D3"))) uint8_t i2c4_rxData[2];
extern __attribute__((section(".RAM_D3"))) uint8_t i2c4_txData[2];

/* Sync And Midi Buffers ----------------------------------------- */

// The byte the MIDI-in DMA writes: alone in its own 32-byte cache line (the data cache build
// invalidates that line before the byte is read).
struct MidiRxBox {
    uint8_t value;
    uint8_t pad[31];
} __attribute__((aligned(32)));
extern "C" MidiRxBox midiRxBox;
#define midiRxData midiRxBox.value

/* Peripheral Handles -------------------------------------------- */

extern "C" UART_HandleTypeDef huart1;                                   // Midi-rx
extern "C" UART_HandleTypeDef huart6;                                   // Midi-tx

/* Timer Handles ------------------------------------------------- */

extern "C" TIM_HandleTypeDef htim4;                                     // Midi clock timer
extern "C" TIM_HandleTypeDef htim5;                                     // Transition timer
extern "C" TIM_HandleTypeDef htim6;                                     // System preset timer
extern "C" TIM_HandleTypeDef htim7;                                     // Encoder preset timer
extern "C" TIM_HandleTypeDef htim8;                                     // Eq timer
extern "C" TIM_HandleTypeDef htim12;                                    // Up long press timer
extern "C" TIM_HandleTypeDef htim13;                                    // Down long press timer
extern "C" TIM_HandleTypeDef htim14;                                    // Key long press timer
extern "C" TIM_HandleTypeDef htim15;                                    // Play timer
extern "C" TIM_HandleTypeDef htim16;                                    // Text timer
extern "C" TIM_HandleTypeDef htim17;                                    // Sd check timer
extern "C" TIM_HandleTypeDef htim23;                                    // Beat sync timer
extern "C" TIM_HandleTypeDef htim24;                                    // Limit alert timer

class Controller {
   private:
    /* Core System Modules --------------------------------------- */

    Io io;                                                              // Hardware I/O abstraction (Sync, MIDI etc.)
    Mixer mixer;                                                        // Master mixer for volume and routing
    Rhythm rhythm;                                                      // Rhythm configuration (BPM, Time Signature)
    Metronome metronome;                                                // Metronome timing and sound generator
    Lpf lpf[kLpfLibrarySize];                                           // Master Low-Pass Filters for anti-aliasing/tone
    Eq eq;                                                              // Global Equalizer
    Filter filter[kFilterLibrarySize];                                  // Multimode filters for instrument layers
    Effect effect[kEffectLibrarySize];                                  // Multi-effects (Delay, Chorus, etc.)
    Reverb reverb;                                                      // Global Reverb engine

    /* Preset Data Buffers --------------------------------------- */

    char dataSystemPreset[kSystemPresetDataSize]{0};                    // Buffer for global system settings
    char dataEncoderGlobalPreset[kEncoderGlobalPresetDataSize]{0};      // Shared encoder mappings
    char dataEncoderLocalPreset[kEncoderLocalPresetDataSize]{0};        // Context-specific encoder values

    /* Libraries & Data Structures ------------------------------- */

    Lfo lfoLibrary[kLfoLibrarySize]{{0}, {1}, {2}, {3}, {4}, {5}, {6}, {7}, {8}, {9}};                                    // Collection of all LFO configurations
    Layer layerLibrary[kLayerLibrarySize]{{0}, {1}, {2}, {3}, {4}, {5}, {6}, {7}, {8}, {9}};                              // Collection of all instrument layers (Drum kits/Synths)

    /* Clipboard / Buffered Operations --------------------------- */

    LfoCopy lfoCopy;                                                    // Temporary storage for LFO copy/paste
    LayerInstCopy layerInstCopy;                                        // Temporary storage for Instrument settings
    LayerSongCopy layerSongCopy;                                        // Temporary storage for Sequencer patterns

    /* Real-Time Playback Data ----------------------------------- */

    MetroPlayData metroPlayData;                                        // Current state of metronome playback
    LfoPlayData lfoPlayData[10];                                        // Real-time phase/value for active LFOs
    LayerPlayData layerPlayData[10];                                    // Sample playback state for instrument layers

    SampleSector sampleSectorLibrary[kSampleSectorLibrarySize];         // SD Card sample indexing
    uint8_t sdReadFailCount = 0;                                        // Consecutive sample-load read failures; triggers SD remount at 3

    volatile uint32_t i2sOvrCount = 0;                                  // I2S3 rx overruns; nonzero means word alignment may have shifted
    volatile uint32_t i2sUdrCount = 0;                                  // I2S3 tx underruns; nonzero means the audio isr missed its deadline
    RoutePatch routePatch = kDefaultRoutePatch;                         // where each module's output goes (Routing.h)
    uint8_t metroOut = NODE_REV;                                        // metronome destination (V1.04 default: reverb)
    RouteState routeState;                                              // module output fades (audio interrupt only)
    RouteFade lineInFade;                                               // line-in route fade (audio interrupt only)
    RouteFade metroFade;                                                // metronome route fade (audio interrupt only)
    // CPU and MEM monitor (LoadMeter.h): the audio engine's load, measured in every build.
    // Layer meters and the status line (StatusLine.h).
    volatile float layerPeak[kLayerLibrarySize] = {0};                  // peak since the last readout (audio interrupt)
    uint16_t meterShown[kLayerLibrarySize] = {0};                       // level on the bar, in pixels
    uint32_t meterClipUntil[kLayerLibrarySize] = {0};                   // the clip square stays until then
    uint32_t meterDrawn[kLayerLibrarySize];                             // what the screen shows (pixels, clip), 0xFFFFFFFF = nothing
    uint32_t meterTick = 0;
    char statusDrawn[kStatusMaxChars + 1] = "";                         // the text on the status line
    uint32_t statusTick = 0;
    uint32_t autosaveLastAt = 0;                                        // when the last autosave was written
    bool autosaveWritten = false;
    void lcd_drawLayerMeters();
    void lcd_drawStatusLine();
#ifdef DSP_PROFILE
    // Per-stage audio cost (make PROFILE=1): cycles summed since the last readout. Stages: 0 line-in,
    // 1 metronome, 2 layers, 3 EQ, 4 Filter 1, 5 Filter 2, 6 Effect 1, 7 Effect 2, 8 reverb,
    // 9 routing (sources scattered into the nodes), 10 output stage.
    volatile uint32_t profSum[11] = {0};
#endif
    LoadMeter dspLoad;                                                  // filled by processAudioBlock
    bool monitorFull = true;                                            // draw the labels (first time)
    uint32_t monitorTick = 0;
    uint32_t monitorCpuShown = 0;                                       // tenths of a percent on the bar
    uint32_t monitorCpuMax = 0;                                         // worst block since power-on
    uint32_t monitorCpuDrawn = 0xFFFFFFFFu;                             // what the screen shows (number, bar, colour)
    uint32_t monitorMemDrawn = 0xFFFFFFFFu;

    /* Audio Buffers (I2s Block Processing) ---------------------- */

    float audioReceive[I2S_BLOCK_SIZE];                                 // Incoming audio from Line-In
    float audioMetronome[I2S_BLOCK_SIZE];                               // Metronome click buffer
    float audioLayer[kLayerLibrarySize][I2S_BLOCK_SIZE];                // Individual layer audio buffers

    float audioEq[I2S_BLOCK_SIZE];                                      // EQ node: routed input, processed in place
    float audioFilter[kFilterLibrarySize][I2S_BLOCK_SIZE];              // Filter 1/2 nodes: routed input, processed in place
    float audioEffect[kEffectLibrarySize][I2S_BLOCK_SIZE];              // FX 1/2 nodes: routed input, processed in place
    float audioReverb_L[I2S_BLOCK_SIZE];                                // Reverb node: routed input, then left output
    float audioReverb_R[I2S_BLOCK_SIZE];                                // Reverb Right channel
    float audioSend_L[I2S_BLOCK_SIZE];                                  // Master pair: everything routed to MASTER
    float audioSend_R[I2S_BLOCK_SIZE];                                  // Final Master Right buffer
    float audioPeak_L;                                                  // Peak meter value for Left channel
    float audioPeak_R;                                                  // Peak meter value for Right channel

    /* Navigation & Selection State ------------------------------ */

    int8_t selectedLayerNum;                                            // Currently focused instrument layer
    int8_t selectedBeatNum;                                             // Currently focused sequencer step
    int8_t selectedLfoNum;                                              // Currently focused LFO
    uint8_t activeEncoderBank;                                          // Current mapping for rotary encoders
    uint8_t activeSongBank;                                             // Currently playing pattern bank
    uint8_t targetSongBank;                                             // Bank queued for next transition

    bool bankShiftFlag;                                                 // Indicates bank switch is pending
    bool bankActionFlag;                                                // Trigger for bank transition logic

    /* Menu & UI State ------------------------------------------- */

    Menu menu;                                                          // Current active menu
    Menu preMenu;                                                       // Previous menu for "Back" functionality
    int8_t menuTab;                                                     // Current horizontal tab index
    int8_t preMenuTab;                                                  // Previous tab index

    /* Pattern Recording Presets --------------------------------- */

    int8_t preBeatFillType;                                             
    int8_t preBeatFillPattern;
    int8_t preBeatFillTime;
    int8_t preBeatFillLevel;

    /* Sd Card & Filesystem -------------------------------------- */

    bool sdInsertCheck = false;                                         // Hardware detection of SD card presence
    bool loadAction = false;                                            // Flag to trigger file loading
    bool saveAction = false;                                            // Flag to trigger file saving
    uint16_t fileLibrarySize;                                           // Total projects found on SD
    uint16_t drumkitLibrarySize;                                        // Total drum kits found on SD
    uint16_t instLibrarySize;                                           // Total instruments found on SD
    uint16_t sampleLibrarySize[kInstLibraryMaxSize];                    // Sample count per instrument folder
    uint32_t totalSampleLibrarySize;

    /* Sequencer Timing & Intervals ------------------------------ */

    uint16_t songInterval;                                              // Total samples in the full loop
    uint16_t barInterval;                                               // Total samples in one bar
    uint16_t measureInterval;                                           // Total samples in one measure (beat)
    uint16_t playInterval;                                              // Current sample position in the loop
    uint16_t stopInterval;                                              // Target position for planned stop
    uint16_t resetInterval;                                             // Target position for planned reset

    bool stopFlag;                                                      // Request to stop transport on next boundary
    bool resetFlag;                                                     // Request to reset transport on next boundary

    /* Alerts & Overlays ----------------------------------------- */

    bool alertFlag;                                                     // True if a confirmation dialog is active
    AlertType alertType;                                                // Type of alert (Save?, Overwrite?, etc.)

    /* UI Icons -------------------------------------------------- */

    Icon resetIcon;
    Icon playIcon;
    Icon stopIcon;
    Icon recordIcon;
    bool resetPlayFlag;

    FileStatus fileStatus;                                              // Current operation status of project files
    FileStatus drumkitStatus;                                           // Current operation status of drum kits

    TriggerMode triggerMode = TRIG_BAR;                                 // Quantization setting
    bool playActive;                                                    // Transport status: Playing
    bool recordActive;                                                  // Transport status: Recording
    uint8_t precountPendingBeat[kLayerLibrarySize];                     // Layer hits during precount (0 none, velocity + 1), committed when playback starts
    bool playArmed;                                                     // Slave mode: waiting for master START to play
    bool recordArmed;                                                   // Slave mode: waiting for master START to record
    bool ignoreNextMidiStopAfterSpecialCmd;

    uint32_t playTimerPeriod;                                           // Calculated period for hardware Play Timer
    uint32_t measureTimerPeriod;                                        // Calculated period for hardware Measure Pulse

    uint32_t midiClockPeriod;                                           // Calculated period for Midi Clock Timer

    /* UI Visuals ------------------------------------------------ */

    uint16_t playX;                                                     // Horizontal pixel position of the playhead
    float playXRatio;                                                   // Normalized 0.0-1.0 position of the playhead
    RGB16Color playColor;                                               // Color of the UI playhead

    /* Action Flags (Handled In Update Loop) --------------------- */   // Logic: 'KeyFlag' detects button hold; 'ActionFlag' executes the logic

    bool lfoCopyKeyFlag, lfoCopyActionFlag;
    bool lfoPasteKeyFlag, lfoPasteActionFlag;
    bool lfoResetKeyFlag, lfoResetActionFlag;
    bool layerInstCopyKeyFlag, layerInstCopyActionFlag;
    bool layerInstPasteKeyFlag, layerInstPasteActionFlag;
    bool layerInstResetKeyFlag, layerInstResetActionFlag;
    bool layerSongCopyKeyFlag, layerSongCopyActionFlag;
    bool layerSongPasteKeyFlag, layerSongPasteActionFlag;
    bool layerSongResetKeyFlag, layerSongResetActionFlag;

    bool layerBeatKeyFlag;
    bool layerFillCopyKeyFlag;
    bool layerFillPasteKeyFlag;

    bool rhythmUnlockKeyFlag, rhythmUnlockActionFlag;
    bool rhythmLockKeyFlag, rhythmLockActionFlag;

    /* Mixer & Amplifier Flags ----------------------------------- */

    bool masterUnmuteKeyFlag, masterUnmuteActionFlag;
    bool masterMuteKeyFlag, masterMuteActionFlag;
    bool hpMuteKeyFlag, hpMuteActionFlag;
    bool hpUnmuteKeyFlag, hpUnmuteActionFlag;
    bool loMuteKeyFlag, loMuteActionFlag;
    bool loUnmuteKeyFlag, loUnmuteActionFlag;
    bool liMuteKeyFlag, liMuteActionFlag;
    bool liUnmuteKeyFlag, liUnmuteActionFlag;

    /* Ui & Notification Flags ----------------------------------- */

    bool textCopyFlag, textPasteFlag, textClearFlag, textArmedFlag, textFullFlag;
    bool limitAlertShowFlag, limitAlertClearFlag, limitAlertActive;
    bool limitLayerFlag[kLayerLibrarySize];

    bool systemPresetSaveFlag, systemPresetSaveActive;
    bool encoderPresetSaveFlag, encoderPresetSaveActive;

    FillCopyData fillCopy;              

    int8_t fileMenuCounter;           
    int8_t drumkitMenuCounter;

    /* Transition & Animation Flags ------------------------------ */

    uint8_t transitionShowFlag;
    bool transitionClearFlag;
    bool transitionShowMenu, transitionClearMenu;
    bool transitionShowEncoder[kEncoderSize], transitionClearEncoder[kEncoderSize];

    uint8_t layerBeatKeyStage;                                        
    bool layerSongAddArmed;  // ADD pressed on Layer Song: a short press inserts a beat
    bool layerSongAddUsed;   // another key or encoder was used while ADD was held
    int32_t layerSongShift;  // net SHIFT ALL since the page was opened, in intervals
    bool layerModKeyUsed;    // a layer pad was pressed while L, I or S was held (so its release is not a tap)

    // EUCLID page (settings kept while powered; the layer's beats from before are in euclidSnapshot)
    uint8_t euclidSteps = 16;
    uint8_t euclidHits = 4;
    uint8_t euclidRotate = 0;
    uint8_t euclidGrid = kInitialQuantize;  // index into kQuantizeInterval
    bool euclidMerge = false;
    uint8_t euclidAccentCount = 0;  // 0 = off (every hit full); else that many accents, the other hits GHOST
    bool euclidApplied = false;  // the layer holds a pattern; CLEAR restores euclidSnapshot
    uint16_t euclidDropped = 0;  // hits that did not fit in the bank
    uint8_t euclidBank = 0;      // song bank euclidSnapshot was taken from

    // Sample preview (Preview.h): the ring and voice are shared with the audio interrupt, which only
    // renders; the main loop streams the file (preview_update).
    PreviewRing previewRing{reinterpret_cast<float*>(RAM_PREVIEW_ADDRESS), kPreviewRingSize - 1, 0, 0};
    PreviewVoice previewVoice{};
    FIL previewFile;
    bool previewFileOpen = false;
    uint32_t previewBytesLeft = 0;
    PreviewFormat previewFormat{0, 0, 0};
    bool previewPending = false;        // start once the voice is idle and the knob has rested
    uint32_t previewRequestTick = 0;
    int8_t previewLayer = -1;
    // Autosave (Autosave.h): interval on the Metronome page, CRC of the last safe state.
    AutosaveTiming autosaveTiming;
    uint32_t autosaveCleanCrc = 0;
    bool autosavePending = false;           // due and changed: write when safe
    uint32_t lastInputMs = 0;               // last key or encoder (autosave waits while editing)
    volatile bool loopEndFlag = false;      // the sequencer wrapped (set in interruptPlay)
    const char* fileNameOverride = nullptr; // set: sd_checkFile/sd_loadFile/sd_saveFile use this path
    void file_encodeKit(uint8_t* data);
    void file_encodeLayerSong(uint8_t layerNum_, uint8_t* data);
    uint32_t file_stateCrc();
    uint16_t file_noteEntryCount();
    uint32_t file_noteCrc(uint32_t crc_);
    bool file_writeNotes();
    void file_readNotes(const char* fileName_);
    uint16_t layerSong_quantizeInterval(uint16_t interval_) const;
    // Trig conditions (BeatNote.h): loops of the active bank, each layer's PRE, fills skipped whole.
    volatile uint16_t noteLoop = 0;
    bool notePre[kLayerLibrarySize] = {true, true, true, true, true, true, true, true, true, true};
    bool noteFillSkip[kLayerLibrarySize] = {false};
    bool note_plays(uint8_t layerNum_, uint16_t note_);
    void note_restartLoops();
    void layerPage_bankChanged();
    // SOLO (left S key + pads, V1.14): soloed layers, not saved.
    uint16_t soloMask = 0;
    bool layerAudible(uint8_t layerNum_) const;
    RGB16Color layerColor(uint8_t layerNum_) const;
    // Full-area views over the lanes (LaneView.h): EUCLID rings, SONG timeline, ROUTING diagram.
    Menu laneViewShown = INIT_MENU;         // the page whose view is on screen (INIT_MENU: the lanes are)
    bool viewDirty = false;                 // something the view shows changed: redraw its data
    bool viewFull = false;                  // something drew over it (an alert): redraw all of it
    uint32_t viewDrawnMs = 0;
    bool ringPlayShown = false;
    int16_t ringPlayX = 0, ringPlayY = 0;
    uint16_t ringPlayRowX = 0;
    uint32_t songThumbDrawn[kChainBanks] = {0};  // what each bank's thumbnail shows (songThumbState), 0 = nothing
    uint16_t songPlayBarX = 0, songPlayBarW = 0, songPlayShown = 0;  // the playing loop's progress bar
    bool laneViewHidden() const;
    void lcd_drawLaneView();
    void lcd_drawLaneViewData();
    void lcd_updateLaneView();
    void lcd_restoreLanes();
    void lcd_drawEuclidViewData();
    void lcd_drawEuclidViewPlay();
    uint32_t songThumbState(uint8_t bank_, uint8_t color_) const;
    void lcd_drawSongViewData();
    void lcd_drawSongViewPlay();
    void lcd_drawRoutingViewData();
    bool filterViewDrawn = false;           // curves are on screen (and stored in the view scratch)
    bool filterViewOther = false;           // the other filter's curve is among them
    uint16_t filterViewMarkX = 0;           // the cutoff line
    uint32_t filterViewStateDrawn = 0;      // the settings those curves were drawn from
    uint32_t filterViewState() const;
    void lcd_drawFilterCurve(const uint16_t* y_, bool thick_);
    void lcd_drawFilterViewData();
    uint16_t eqViewMarkX[6] = {0}, eqViewMarkY[6] = {0};  // the EQ view's band handles
    uint32_t eqViewState() const;
    void lcd_drawCurveAxes(float topDb_, float bottomDb_, bool labels_);
    void lcd_drawEqViewData();
    uint32_t sampleViewKey = 0;             // which sample the peak columns belong to (0 = none)
    uint32_t sampleViewStateDrawn = 0;      // the settings the view was drawn from
    uint8_t sampleViewStart = 0, sampleViewEnd = 100;  // the handles as drawn
    uint32_t sampleViewState() const;
    void lcd_drawSampleViewData();
    void lcd_drawSampleColumns(uint16_t from_, uint16_t to_, uint8_t start_, uint8_t end_);
    void lcd_drawSampleHandles(uint8_t start_, uint8_t end_, bool clear_);

    // DUB mode (DubMix.h)
    DubMix dubMix;                                          // the strips; on = DUB mode (a song setting)
    char missingDetail[40] = "";                           // MISSING SAMPLE alert: the first missing layer, instrument and sample ("L3 Kick/Name")
    void missing_note(uint8_t layer_, const char* inst_, const char* sample_);
    float pingWidth = 0.0f;                                 // ping-pong width of the echo in this block (0 = centred; audio interrupt)
    float effectDelta = 0.0f;                               // right minus left of the effect's last sample (audio interrupt)
    float fxDryScale = 1.0f;                                // 0 in DUB mode: effects and reverb run without their DRY
    uint8_t dubPage = 0;                                    // 0: the first DUB page, 1: the second (pan, FX send, echo width)
    uint8_t dubStripSel = 0;                                // the strip the DUB page edits (a pad press selects it)
    uint32_t dubStripDrawn[kDubStrips] = {0};               // the desk: what each strip shows (a fingerprint), 0 = nothing
    uint32_t dubReturnDrawn[2] = {0, 0};
    uint16_t dubMeterDrawn[kDubStrips];                     // the desk's meter heights as drawn (0xFFFF = nothing)
    uint32_t dubViewStateDrawn = 0;
    uint32_t dubViewState() const;
    void lcd_drawDubViewData();
    void lcd_drawDubMeters();
    void lcd_drawDeskKnob(uint16_t cx_, uint16_t cy_, uint8_t percent_, RGB16Color color_);
    void lcd_drawDeskColumn(uint8_t column_);
    void lcd_drawDeskSegments(uint8_t column_, uint8_t lit_, bool audible_);
    uint32_t noteViewKey = 0;               // the step edit screen: which layer, bank and layout it was drawn for
    int16_t noteViewDrawnLast = -1;         // lastActiveBeatNum when it was drawn
    uint32_t noteViewStateDrawn = 0;
    uint32_t noteViewState() const;
    void lcd_drawNoteViewData();
    void lcd_drawNoteViewBeat(uint8_t beatNum_, bool selected_);
    void layerInst_setPan(uint8_t layerNum_, uint8_t pan_);
    bool layerPanActive(uint8_t layerNum_) const;
    void lcd_drawLayerInst_PanData(uint8_t layerNum_);
    void dub_select();
    void dub_step(EncoderFunc function_, bool up_);
    void lcd_drawDubMenu();
    void lcd_drawDub_Data();
    uint32_t file_dubCrc(uint32_t crc_);
    void layerInst_setSolo(uint8_t layerNum_, bool solo_);
    void layerInst_clearSolo();
    void lcd_drawLayerInst_StyleData(uint8_t layerNum_);
    // Undo / redo of beat and bank edits (UndoLog.h); the stored banks live in SDRAM.
    UndoLog<kUndoEntries> undoLog;
    bool undoRecordOpen = false;
    bool textUndoFlag = false, textRedoFlag = false, textUndoNone = false;
    bool textFkeyFlag = false;  // F key held: show FKEY
    void undo_capture(UndoKind kind_, int16_t layerNum_, uint8_t bankNum_);
    void undo_captureAllLayers(UndoKind kind_, uint8_t bankNum_);
    void undo_captureRecord(uint8_t layerNum_, uint8_t bankNum_);
    void undo_apply(bool redo_);
    void note_select();
    bool note_validSelection();
    uint8_t note_effectiveVelocity(const Beat& beat_) const;
    void note_step(EncoderFunc function_, bool up_);
    void note_clear();
    void lcd_drawNoteMenu();
    void lcd_drawNote_Data();
    void lcd_drawNoteOverview();
    void autosave_setInterval(uint8_t interval_);
    void autosave_markClean();
    void autosave_scanFile(uint8_t slot_, AutosaveFile& file_);
    bool autosave_write();
    bool autosave_writeFiles();
    uint32_t autosaveLastMs = 0;            // how long the last autosave took (Metronome page)
    bool autosaveLastOk = true;
    bool textAutosaveErrFlag = false;
    void lcd_drawStatusNow(const char* text5_, RGB16Color color_);
    // Crash record (CrashRecord.h): the last crash, shown at start-up.
    char crashText[32] = "";
    bool crash_report();
    void watchdog_start();
    void watchdog_stage(uint32_t stage_);
    void autosave_deleteAll();
    void autosave_update();
    void autosave_offerRestore();
    void autosave_restore();

    bool previewOn = true;              // Metronome page, saved in the system preset
    uint8_t previewLevel = 70;

    // Song mode (SongChain.h). chainPlay advances in the sequencer interrupt at loop ends.
    SongChain songChain;
    ChainPlay chainPlay;
    uint8_t songStepSel = 0;            // step shown/edited on the SONG page
    bool songKeyUsed = false;           // a pad was pressed while SONG was held (bank pick, not a tap)
    volatile bool chainActionFlag = false;  // the interrupt moved the chain: redraw the SONG page
    // Whole-bank copy/paste on the SONG page (long COPY / PASTE): all 10 layers of the active bank.
    bool bankCopyKeyFlag = false, bankPasteKeyFlag = false;
    volatile bool bankCopyActionFlag = false, bankPasteActionFlag = false;
    bool bankCopyActive = false;
    uint8_t bankCopySource = 0;

    uint8_t loadFormatVersion = 0;   // format version of the kit/song file being loaded (FileFormat.h)
    uint32_t pitchTurnTick = 0;      // last PITCH encoder step, for fast-turn octave steps
    uint32_t pitchOctaveTick = 0;    // last octave step
    uint32_t lfoValuesDrawTick = 0;  // last refresh of LFO-modulated values on the instrument page
    uint8_t lfoValuesLive = 0;       // instrument fields currently showing a modulated value (bit per LayerShown)

    /* Input Engine Status --------------------------------------- */

    bool keyboardActive;                                                // Gate for processing button presses
    bool encoderActive;                                                 // Gate for processing rotary rotations
    bool encoderFastMode;                                               // High-speed increment modifier
    bool encoderSlowMode;                                               // Fine-tuning increment modifier
    bool upDownKeyLongActive;                                           // Enables long-press repeat behavior

    /* Led Output State ------------------------------------------ */

    bool ledEqPlayMode;                                                 // LED bars act as visualizer
    bool ledLayerPlayMode;                                              // LED pads act as beat indicators
    bool ledEqReadFlag[2], ledEqDrawFlag[2];                            // Peak data exchange flags
    float ledEqReadData[2];                                             // Raw peak values from DSP
    uint16_t ledEqDrawData[2];                                          // Calculated bitmask for I2C LED expanders
    bool ledLayerSelectFlag;                                            // Highlight selected layer on pads
    uint16_t ledLayerSelectData;
    bool ledLayerPlayFlag;                                              // Flash current layer trigger
    uint16_t ledLayerPlayData;

    float ledEqCurrentLevel[2];                                         // Internal smoothing for LED meters
    uint16_t ledEqLastSentData[2];                                      // Previous I2C value (to avoid redundant writes)
    uint8_t ledEqPriorityChannel;                                       // Used for multi-meter balancing

    /* Miscellaneous --------------------------------------------- */

    bool animation;                                                     // Toggle for UI animations
    bool textShow;                                                      // Toggle for descriptive text overlays
    bool sampleFadeOut;                                                 // Triggers volume ramp down on stop
    uint16_t counterA = 0;                                              // General purpose debug counters
    uint16_t counterB = 0;

   public:
    /* Lifecycle ------------------------------------------------- */

    Controller();                                                       // Constructor: Initializes internal variables to zero/default
    ~Controller();                                                      // Destructor: Handles cleanup of dynamic memory or peripherals

    /* Hardware Peripherals -------------------------------------- */

    Lcd lcd{LANDSCAPE_1, WHITE, BLACK, FONT_07x09};                     // Main graphical display controller
    Dac dac{};                                                          // High-fidelity audio output interface
    Sd  sd{};                                                           // SD card filesystem manager

    /* Input Hardware -------------------------------------------- */

    Keyboard keyboard{};                                                // Button matrix handler (Layers, Left, Right keypads)

    Encoder encoder[kEncoderSize];                                      // Physical rotary encoder instances
    EncoderFunc encoderGlobalFunction[kEncoderBankSize][kEncoderSize] = {};

    uint16_t encCounter;                                                // Global counter for tracking aggregate encoder activity

    /* Visual Output Hardware ------------------------------------ */

    Led ledEqLeft{};                                                    // Left-channel LED VU/EQ meter
    Led ledEqRight{};                                                   // Right-channel LED VU/EQ meter
    Led ledLayer{};                                                     // Multi-color performance pad indicators

    /* State Accessors (Getters) --------------------------------- */

    Io     getIo()    { return io; }                                    // Get MIDI/Sync state
    void   midi_countRxError() { io.midi.rxErrors = io.midi.rxErrors + 1; }  // MIDI IN UART error (diagnostics)
    Mixer  getMixer() { return mixer; }                                 // Get Master audio levels
    Menu   getMenu()  { return menu; }                                  // Get currently active UI screen

    TestMode testMode = TEST_NONE;                                      // Current hardware diagnostic sub-mode

    /* Main Functions -------------------------------------------- */

    void initialize();
    void update();
    void test();

    /* Preset Functions ------------------------------------------ */

    void loadSystemPreset();
    void saveSystemPreset();

    void loadEncoderPreset();
    void saveEncoderPreset();

    /* Sync Functions -------------------------------------------- */

    bool enqueueMidiTx(const uint8_t* data, uint8_t len);
    void serviceMidiTx();
    void receiveMidiCommand();
    void midi_thru(uint8_t byte_);
    void midi_receiveClock(uint32_t nowCycles_);
    void midi_clockEngage(bool fromMidi_);
    void midi_clockRelease();
    bool midi_txTransportOn() const;
    void midi_clockOnPlayStart();
    void syncOutClockTick();

    /* Song Functions -------------------------------------------- */

    void calculateSongInterval();
    uint16_t calculateTriggerInterval();
    void adjustMeasureBarTiming();
    void updatePlayTimerPeriod();

    /* Keyboard Functions ---------------------------------------- */

    void keyboard_initialize();
    void keyboard_read();
    void keyboard_action();
    void keyboard_test();

    void keyboard_enable();
    void keyboard_disable();

    void keyboard_upLong();
    void keyboard_downLong();

    void keyboard_resetI2C();

    /* Encoder Functions ----------------------------------------- */

    void encoder_initialize();
    void encoder_read();
    void encoder_action();
    void encoder_update();
    void encoder_test();

    void encoder_enable();
    void encoder_disable();

    void encoder_selectBank(uint8_t bankNum_);
    void encoder_setGlobalFunction(uint8_t bankNum_, uint8_t encoderNum_, EncoderFunc function_);

    void encoder_incValue(uint8_t encoderNum_);
    void encoder_decValue(uint8_t encoderNum_);

    /* Led Functions --------------------------------------------- */

    void led_initialize(bool animate_);
    void led_action();

    /* Timer Functions ------------------------------------------- */

    void startMidiClockTimer() { HAL_TIM_Base_Start_IT(&htim4); }
    void stopMidiClockTimer() { HAL_TIM_Base_Stop_IT(&htim4); }
    void startTransitionTimer() { HAL_TIM_Base_Start_IT(&htim5); }
    void stopTransitionTimer() {
        HAL_TIM_Base_Stop_IT(&htim5);
        __HAL_TIM_SET_COUNTER(&htim5, 0);
    }
    void startSystemPresetTimer() { HAL_TIM_Base_Start_IT(&htim6); }
    void stopSystemPresetTimer() {
        HAL_TIM_Base_Stop_IT(&htim6);
        __HAL_TIM_SET_COUNTER(&htim6, 0);
    }
    void startEncoderPresetTimer() { HAL_TIM_Base_Start_IT(&htim7); }
    void stopEncoderPresetTimer() {
        HAL_TIM_Base_Stop_IT(&htim7);
        __HAL_TIM_SET_COUNTER(&htim7, 0);
    }
    void startEqTimer() { HAL_TIM_Base_Start_IT(&htim8); }
    void stopEqTimer() {
        HAL_TIM_Base_Stop_IT(&htim8);
        __HAL_TIM_SET_COUNTER(&htim8, 0);
    }
    void startUpKeyTimer() { HAL_TIM_Base_Start_IT(&htim12); }
    void stopUpKeyTimer() {
        HAL_TIM_Base_Stop_IT(&htim12);
        __HAL_TIM_SET_COUNTER(&htim12, 0);
    }
    void startDownKeyTimer() { HAL_TIM_Base_Start_IT(&htim13); }
    void stopDownKeyTimer() {
        HAL_TIM_Base_Stop_IT(&htim13);
        __HAL_TIM_SET_COUNTER(&htim13, 0);
    }
    void startLongKeyTimer() { HAL_TIM_Base_Start_IT(&htim14); }
    void stopLongKeyTimer() {
        HAL_TIM_Base_Stop_IT(&htim14);
        __HAL_TIM_SET_COUNTER(&htim14, 0);
    }
    void startPlayTimer() { HAL_TIM_Base_Start_IT(&htim15); }
    void stopPlayTimer() { HAL_TIM_Base_Stop_IT(&htim15); }
    void startTextTimer() { HAL_TIM_Base_Start_IT(&htim16); }
    void stopTextTimer() {
        HAL_TIM_Base_Stop_IT(&htim16);
        __HAL_TIM_SET_COUNTER(&htim16, 0);
    }
    void startSdTimer() { HAL_TIM_Base_Start_IT(&htim17); }
    void stopSdTimer() {
        HAL_TIM_Base_Stop_IT(&htim17);
        __HAL_TIM_SET_COUNTER(&htim17, 0);
    }
    void startBeatSyncTimer() { HAL_TIM_Base_Start_IT(&htim23); }
    void stopBeatSyncTimer() {
        HAL_TIM_Base_Stop_IT(&htim23);
        __HAL_TIM_SET_COUNTER(&htim23, 0);
    }
    void startLimitAlertTimer() { HAL_TIM_Base_Start_IT(&htim24); }
    void stopLimitAlertTimer() {
        HAL_TIM_Base_Stop_IT(&htim24);
        __HAL_TIM_SET_COUNTER(&htim24, 0);
    }

    /* Dac Functions --------------------------------------------- */

    void dac_initialize();

    /* Sd Functions ---------------------------------------------- */

    SdResult sd_initialize();
    SdResult sd_reinitialize();
    SdResult sd_detect();
    SdResult sd_mount();
    SdResult sd_unmount();
    SdResult sd_getLabel();
    SdResult sd_setLabel();
    SdResult sd_getSpace();
    SdResult sd_checkFileExist(const char* fileAddress);
    SdResult sd_checkFolderExist(const char* folderAddress);
    SdResult sd_loadImage(const char* fileAddress, uint32_t paletteAddress, uint32_t dataAddress, uint16_t paletteSize, uint16_t width, uint16_t height, RGBMode mode);
    void sd_getLibraries();
    SdResult sd_getFileLibrary();
    SdResult sd_getDrumkitLibrary();
    void sd_getInstLibrary();
    void sd_getSampleLibrary();
    SdResult sd_checkFile(uint8_t fileNum_);
    SdResult sd_loadFile(uint8_t fileNum_);
    SdResult sd_saveFile(uint8_t fileNum_);
    SdResult sd_clearFile(uint8_t fileNum_);
    SdResult sd_checkDrumkit(uint8_t kitNum_);
    SdResult sd_loadDrumkit(uint8_t kitNum_);
    SdResult sd_saveDrumkit(uint8_t kitNum_);
    SdResult sd_clearDrumkit(uint8_t kitNum_);
    SdResult sd_checkSamplesInUse();
    FRESULT sd_createDirectory(const char* path);
    FRESULT sd_deleteDirectory(const char* path);
    void sd_debug(const char* msg, FRESULT res);
    FRESULT sd_write(const void* buffer, UINT size);  // f_write on sd.file; short write is an error
    FRESULT sd_read(void* buffer, UINT size);         // f_read on sd.file; short read is an error

    static inline int compareWords(const void* str1, const void* str2) { return strcmp(*(char**)str1, *(char**)str2); }
    static inline void sortWords(char* words[], int count) { qsort(words, count, sizeof(words[0]), compareWords); }

    /* Sdram Functions ------------------------------------------- */

    void sdram_fadeOutAudio(uint32_t ramAddress_, uint32_t sampleSize_, uint16_t fadeOutSize_);

    /* Lcd Functions --------------------------------------------- */

    void lcd_initialize();
    void lcd_test(TestMode mode_);
    void lcd_update();
    void lcd_drawLogo(bool animate_);
    void lcd_clearLogo(bool animate_);
    void lcd_drawPage(bool animate_);
    void lcd_clearPage();
    // Alert functions
    void lcd_drawAlert();
    void lcd_clearAlert();
    // Limit functions
    void lcd_drawLimitAlert();
    // Sd functions
    void lcd_drawSdDataIntro();
    void lcd_drawSdData();
    void lcd_drawMonitor();
    void lcd_drawMonitorBar(uint16_t textY_, uint16_t barY_, uint32_t permille_, uint32_t& drawn_);
    void lcd_clearSdData();
    void lcd_drawSdAlert(SdResult result_);
    void lcd_clearSdAlert();
    void lcd_drawInitSdAlert(SdResult result_);
    void lcd_clearInitSdAlert();
    // Song functions
    void lcd_drawEncoderBankTab();
    void lcd_drawSongBankTab(uint8_t bankNum_, bool mode_);
    void lcd_drawSongBankShift();
    // Menu functions
    void lcd_drawMenuIcon(Menu menu_);
    void lcd_drawEncoderBox(uint8_t encoderNum_);
    void lcd_drawFileBox(uint8_t menuTab_, FileStatus status_);
    void lcd_clearMenuTab(uint8_t menuTab_);
    void lcd_clearHeader(uint8_t menuTab_);
    void lcd_clearData(uint8_t menuTab_);
    void lcd_clearEncoderBox(uint8_t encoderNum_);
    void lcd_clearFileBox(uint8_t menuTab_);
    void lcd_clearTextBox(uint8_t menuTab_);
    void lcd_clearSignBox(uint8_t menuTab_);
    void lcd_transitionMenu();
    void lcd_transitionSelect();
    void lcd_setMenuHeaderState(RGB16Color color_);
    void lcd_setMenuDataState(RGB16Color color_);
    void lcd_setMenuNumState(RGB16Color color_);
    void lcd_setMenuTextState(RGB16Color color_);
    void lcd_setMenuSignState(RGB16Color color_);
    void lcd_setLayerTabState(uint8_t layerNum_);
    // Global menu functions
    void lcd_drawGlobalMenu();
    void lcd_drawGlobalMenuFunction(uint8_t encoderNum_);
    void lcd_drawGlobalMenuData(uint8_t encoderNum_);
    void lcd_updateGlobalMenuFunction(EncoderFunc function_);
    void lcd_updateGlobalMenuData(EncoderFunc function_);
    // File menu functions
    void lcd_drawFileMenu();
    void lcd_drawFile_NewData();
    void lcd_drawFile_LoadData();
    void lcd_drawFile_SaveData();
    void lcd_drawFile_ClearData();
    // Drumkit menu functions
    void lcd_drawDrumkitMenu();
    void lcd_drawDrumkit_NewData();
    void lcd_drawDrumkit_LoadData();
    void lcd_drawDrumkit_SaveData();
    void lcd_drawDrumkit_ClearData();
    // Mixer menu functions
    void lcd_drawMixerMenu();
    void lcd_drawMixer_MasterVolumeData();
    void lcd_drawMixer_HpVolumeData();
    void lcd_drawMixer_LoVolumeData();
    void lcd_drawMixer_LiVolumeData();
    void lcd_drawMixer_LiInData();
    void lcd_drawMixer_PanData();
    void lcd_drawMixer_LimiterData();
    void lcd_drawMixer_NormalizeData();
    // Rhythm menu functions
    void lcd_drawRhythmMenu();
    void lcd_drawRhythm_TempoData();
    void lcd_drawRhythm_MeasureData();
    void lcd_drawRhythm_BarData();
    void lcd_drawRhythm_QuantizeData();
    // Metronome menu functions
    void lcd_drawMetroMenu();
    void lcd_drawMetro_ActiveData();
    void lcd_drawMetro_PrecountData();
    void lcd_drawMetro_SampleData();
    void lcd_drawMetro_VolumeData();
    // Io menu functions
    void lcd_drawIoMenu();
    void lcd_drawIo_SyncInData();
    void lcd_drawIo_SyncOutData();
    void lcd_drawIo_SyncInPpqnData();
    void lcd_drawIo_SyncOutPpqnData();
    void lcd_drawIo_MidiInData();
    void lcd_drawIo_MidiOutData();
    // Eq menu functions
    void lcd_drawEqMenu();
    void lcd_drawEq_ActiveData();
    void lcd_drawEq_QData();
    void lcd_drawEq_LowShelfData();
    void lcd_drawEq_HighShelfData();
    void lcd_drawEq_PeakData(uint8_t peakNum_);
    // Filter menu functions
    void lcd_drawFilterMenu(uint8_t filterNum_);
    void lcd_drawFilter_ActiveData(uint8_t filterNum_);
    void lcd_drawFilter_TypeData(uint8_t filterNum_);
    void lcd_drawFilter_SlopeData(uint8_t filterNum_);
    void lcd_drawFilter_FrequencyData(uint8_t filterNum_);
    void lcd_drawFilter_ResonanceData(uint8_t filterNum_);
    void lcd_drawFilter_DriveData(uint8_t filterNum_);
    void lcd_drawFilter_DryData(uint8_t filterNum_);
    void lcd_drawFilter_WetData(uint8_t filterNum_);
    // Effect menu functions
    void lcd_drawEffectMenu(uint8_t effectNum_);
    void lcd_drawEffect_ActiveData(uint8_t effectNum_);
    void lcd_drawEffect_TypeData(uint8_t effectNum_);
    void lcd_drawEffect_AData(uint8_t effectNum_);
    void lcd_drawEffect_BData(uint8_t effectNum_);
    void lcd_drawEffect_CData(uint8_t effectNum_);
    void lcd_drawEffect_DData(uint8_t effectNum_);
    void lcd_drawEffect_EData(uint8_t effectNum_);
    void lcd_drawEffect_FData(uint8_t effectNum_);
    // Reverb menu functions
    void lcd_drawReverbMenu();
    void lcd_drawReverb_ActiveData();
    void lcd_drawReverb_SizeData();
    void lcd_drawReverb_DecayData();
    void lcd_drawReverb_DampingData();
    void lcd_drawReverb_PreDelayData();
    void lcd_drawReverb_SurroundData();
    void lcd_drawReverb_DryData();
    void lcd_drawReverb_WetData();
    // Lfo menu functions
    void lcd_drawLfoMenu(uint8_t lfoNum_);
    void lcd_drawLfo_ActiveData(uint8_t lfoNum_);
    void lcd_drawLfo_TypeData(uint8_t lfoNum_);
    void lcd_drawLfo_GraphData(uint8_t lfoNum_);
    void lcd_drawLfo_RateData(uint8_t lfoNum_);
    void lcd_drawLfo_PhaseData(uint8_t lfoNum_);
    void lcd_drawLfo_InvertData(uint8_t lfoNum_);
    void lcd_drawLfo_DepthData(uint8_t lfoNum_);
    // Layer inst menu functions
    void lcd_drawLayerInst0Menu(uint8_t layerNum_, bool graph_);
    void lcd_drawLayerInst1Menu(uint8_t layerNum_);
    void lcd_drawLayerInst2Menu(uint8_t layerNum_);
    void lcd_drawLayerInst_InstData(uint8_t layerNum_);
    void lcd_drawLayerInst_SampleData(uint8_t layerNum_);
    void lcd_drawLayerInst_GraphData(uint8_t layerNum_);
    void lcd_drawLayerInst_LevelData(uint8_t layerNum_);
    bool sd_formatSupported(const char* fileName_, uint16_t refSize_);
    void layerInst_stepPitch(bool up_, bool encoder_);
    bool layerInst_lfoShown(uint8_t layerNum_, LayerShown value_, int16_t& shown_);
    void lcd_drawLayerInstLfoValues();
    void lcd_drawLayerInst_PitchData(uint8_t layerNum_);
    void lcd_drawLayerInst_DirectionData(uint8_t layerNum_);
    void lcd_drawLayerInst_ResolutionData(uint8_t layerNum_);
    void lcd_drawLayerInst_StartData(uint8_t layerNum_);
    void lcd_drawLayerInst_EndData(uint8_t layerNum_);
    void lcd_drawLayerInst_TimingData(uint8_t layerNum_);
    void lcd_drawLayerInst_ProbabilityData(uint8_t layerNum_);
    void lcd_drawLayerInst_OutData(uint8_t layerNum_);
    // Layer song menu functions
    void lcd_drawLayerSongMenu(uint8_t layerNum_);
    void lcd_drawLayerSong_BeatFillTypeData(uint8_t layerNum_);
    void lcd_drawLayerSong_BeatFillPatternData(uint8_t layerNum_);
    void lcd_drawLayerSong_BeatFillTimeData(uint8_t layerNum_);
    void lcd_drawLayerSong_BeatFillLevelData(uint8_t layerNum_);
    void lcd_drawLayerSong_BeatFillGraphData(uint8_t layerNum_);
    void lcd_drawLayerSong_PositionData(uint8_t layerNum_);
    void lcd_drawLayerSong_ShiftData(uint8_t layerNum_);
    // Layer tab functions
    void lcd_drawLayerTab(uint8_t layerNum_);
    void lcd_drawLayerBox(uint8_t layerNum_);
    void lcd_drawLayerTab_NumData(uint8_t layerNum_);
    void lcd_drawLayerTab_InstData(uint8_t layerNum_);
    void lcd_drawLayerTab_Mute(uint8_t layerNum_);
    void lcd_drawLayerTab_Fill(uint8_t layerNum_);
    void lcd_drawLayerTab_Style(uint8_t layerNum_);
    // Transition functions
    void lcd_drawTransition();
    void checkMenuTransition(Menu menu_, bool mode_);
    void checkEncoderTransition(EncoderFunc function_, bool mode_);
    // Play functions
    void lcd_drawPlay();
    void lcd_drawIcon();
    void lcd_drawCopyPaste();
    void lcd_drawCountDown();
    void lcd_clearCountDown();
    void lcd_restartPlay();
    void lcd_resetPlay();
    void lcd_redrawPlay();
    void lcd_cleanEndPlay();
    void lcd_invertPlayColor();
    void lcd_resetPlayColor();
    // Song functions
    void lcd_calculateSongX();
    void lcd_drawMeasureBar();
    void lcd_drawBeat(uint8_t layerNum_, uint8_t bankNum_, uint8_t beatNum_, bool selected_);
    void lcd_clearBeat(uint8_t layerNum_, uint8_t bankNum_, uint8_t beatNum_);
    void lcd_drawSong(uint8_t layerNum_, uint8_t bankNum);
    void lcd_clearSong(uint8_t layerNum_, uint8_t bankNum);
    void lcd_clearInterval(uint8_t layerNum_, uint8_t bankNum, uint16_t startInterval_, uint16_t endInterval_);

    /* Menu Functions -------------------------------------------- */

    void preMenuLayerClear();

    /* Global Functions ------------------------------------------ */

    void global_select();

    /* File Functions -------------------------------------------- */

    void file_select();

    void file_menuRight();
    void file_menuLeft();
    void file_menuUp();
    void file_menuDown();

    void file_newSelect();
    void file_newAction();
    void file_loadSelect();
    void file_loadAction();
    void file_saveSelect();
    void file_saveAction();
    void file_clearSelect();
    void file_clearAction();

    /* Drumkit Functions ----------------------------------------- */

    void drumkit_select();

    void drumkit_menuRight();
    void drumkit_menuLeft();
    void drumkit_menuUp();
    void drumkit_menuDown();

    void drumkit_newSelect();
    void drumkit_newAction();
    void drumkit_loadSelect();
    void drumkit_loadAction();
    void drumkit_saveSelect();
    void drumkit_saveAction();
    void drumkit_clearSelect();
    void drumkit_clearAction();

    /* Rhythm Functions ------------------------------------------ */

    void rhythm_select();
    void rhythm_reset();

    void rhythm_menuRight();
    void rhythm_menuLeft();
    void rhythm_menuUp();
    void rhythm_menuDown();

    void rhythm_setTempo(uint8_t tempo_);
    void rhythm_setMeasure(uint8_t measure_);
    void rhythm_setBar(uint8_t bar_);
    void rhythm_setQuantize(uint8_t quantize_);

    /* Mixer Functions ------------------------------------------- */

    void mixer_select();
    void mixer_reset();

    void mixer_menuRight();
    void mixer_menuLeft();
    void mixer_menuUp();
    void mixer_menuDown();

    void mixer_setMasterVolume(uint8_t volume_);
    void mixer_setHpVolume(uint8_t volume_);
    void mixer_setLoVolume(uint8_t volume_);
    void mixer_setLiVolume(uint8_t volume_);
    void mixer_setLiIn(uint8_t mode_);
    void mixer_setPan(uint8_t pan_);
    void mixer_setLimiter(uint8_t limiter_);
    void mixer_setNormalize(bool active_);

    void mixer_setMasterMute(bool mute_);
    void mixer_setHpMute(bool mute_);
    void mixer_setLoMute(bool mute_);
    void mixer_setLiMute(bool mute_);

    void mixer_masterVolumeTransition(float volumeFloat_);
    void mixer_panTransition(float volumeLeftFloat_, float volumeRightFloat_);
    void mixer_calculateMasterVolumeTransition();
    void mixer_calculatePanTransition();

    /* Metronome Functions --------------------------------------- */

    void metro_select();
    void metro_reset();

    void metro_menuRight();
    void metro_menuLeft();
    void metro_menuUp();
    void metro_menuDown();

    void metro_setActive(bool active_);
    void metro_setPrecount(bool precount_);
    void metro_setSample(uint8_t sample_);
    void metro_setVolume(uint8_t volume_);

    void metro_volumeTransition(float volumeFloat);
    void metro_calculateVolumeTransition();

    /* Io Functions ---------------------------------------------- */

    void io_select();
    void io_reset();

    void io_menuRight();
    void io_menuLeft();
    void io_menuUp();
    void io_menuDown();

    void io_setSyncIn(uint8_t mode_);
    void io_setSyncOut(uint8_t mode_);
    void io_setSyncInPpqn(uint8_t ppqnIdx_);
    void io_setSyncOutPpqn(uint8_t ppqnIdx_);
    void io_setMidiIn(uint8_t mode_);
    void io_setMidiOut(uint8_t mode_);
    void io_setMidiInChannel(uint8_t channel_);
    void io_setMidiOutChannel(uint8_t channel_);

    /* Eq Functions ---------------------------------------------- */

    void eq_select();
    void eq_reset();

    void eq_menuRight();
    void eq_menuLeft();
    void eq_menuUp();
    void eq_menuDown();

    void eq_setActive(bool active_);
    void eq_setQ(uint8_t q_);
    void eq_setFreqLowShelf(uint8_t freq_);
    void eq_setGainLowShelf(uint8_t gain_);
    void eq_setFreqHighShelf(uint8_t freq_);
    void eq_setGainHighShelf(uint8_t gain_);
    void eq_setFreqPeak(uint8_t peakNum_, uint8_t freq_);
    void eq_setGainPeak(uint8_t peakNum_, uint8_t gain_);

    void eq_genTransition(EqTransitionMode mode_, bool activeActive_, bool targetActive_);
    void eq_calculateActiveTransition();

    /* Filter Functions ------------------------------------------ */

    void filter_select(uint8_t filterNum_);
    void filter_reset(uint8_t filterNum_);

    void filter_menuRight();
    void filter_menuLeft();
    void filter_menuUp();
    void filter_menuDown();

    void filter_setActive(uint8_t filterNum_, bool active_);
    void filter_setType(uint8_t filterNum_, uint8_t type_);
    void filter_setSlope(uint8_t filterNum_, uint8_t slope_);
    void filter_setFrequency(uint8_t filterNum_, uint8_t freq_);
    void filter_setResonance(uint8_t filterNum_, uint8_t res_);
    void filter_setDrive(uint8_t filterNum_, uint8_t drive_);
    void filter_setDry(uint8_t filterNum_, uint8_t dry_);
    void filter_setWet(uint8_t filterNum_, uint8_t wet_);

    void filter_genTransition(uint8_t filterNum_, FilterTransitionMode mode_, bool activeActive_, bool targetActive_, uint8_t activeType_, uint8_t targetType_);
    void filter_mixTransition(uint8_t filterNum_, float dryFloat, float wetFloat);
    void filter_calculateGenTransition(uint8_t filterNum_);
    void filter_calculateMixTransition(uint8_t filterNum_);

    /* Effect Functions ------------------------------------------ */

    void effect_select(uint8_t effectNum_);
    void effect_reset(uint8_t effectNum_);

    void effect_menuRight();
    void effect_menuLeft();
    void effect_menuUp();
    void effect_menuDown();

    void effect_setActive(uint8_t effectNum_, bool active_);
    void effect_setType(uint8_t effectNum_, uint8_t type_);
    void effect_setAData(uint8_t effectNum_, uint8_t subEffectNum_, uint8_t aData_);
    void effect_setBData(uint8_t effectNum_, uint8_t subEffectNum_, uint8_t bData_);
    void effect_setCData(uint8_t effectNum_, uint8_t subEffectNum_, uint8_t cData_);
    void effect_setDData(uint8_t effectNum_, uint8_t subEffectNum_, uint8_t dData_);
    void effect_setEData(uint8_t effectNum_, uint8_t subEffectNum_, uint8_t eData_);
    void effect_setFData(uint8_t effectNum_, uint8_t subEffectNum_, uint8_t fData_);

    void effect_genTransition(uint8_t effectNum_, EffectTransitionMode mode_, bool activeActive_, bool targetActive_, uint8_t activeType_, uint8_t targetType_);
    void effect_mixTransition(uint8_t effectNum_, float dryFloat_, float wetFloat_);
    void effect_calculateGenTransition(uint8_t effectNum_);
    void effect_calculateMixTransition(uint8_t effectNum_);
    void effect_updateMixFloats(uint8_t effectNum_);

    void effect_cleanMemory(uint8_t effectNum_, uint8_t type_);

    /* Reverb Functions ------------------------------------------ */

    void reverb_select();
    void reverb_reset();

    void reverb_menuRight();
    void reverb_menuLeft();
    void reverb_menuUp();
    void reverb_menuDown();

    void reverb_setActive(bool active_);
    void reverb_setSize(uint8_t size_);
    void reverb_setDecay(uint8_t decay_);
    void reverb_setDamping(uint8_t damping_);
    void reverb_setPreDelay(uint8_t preDelay_);
    void reverb_setSurround(uint8_t surround_);
    void reverb_setDry(uint8_t dry_);
    void reverb_setWet(uint8_t wet_);

    void reverb_genTransition(ReverbTransitionMode mode_, bool activeActive_, bool targetActive_);
    void reverb_mixTransition(float dryFloat, float wetFloat);
    void reverb_calculateGenTransition();
    void reverb_calculateMixTransition();

    /* Lfo Functions --------------------------------------------- */

    void lfo_select(uint8_t lfoNum_);
    void lfo_reset(uint8_t lfoNum_);

    void lfo_menuRight();
    void lfo_menuLeft();
    void lfo_menuUp();
    void lfo_menuDown();

    void lfo_setActive(uint8_t lfoNum_, uint8_t active_);
    void lfo_setType(uint8_t lfoNum_, uint8_t type_);
    void lfo_setRate(uint8_t lfoNum_, uint8_t rate_);
    void lfo_setPhase(uint8_t lfoNum_, uint8_t phase_);
    void lfo_setInvert(uint8_t lfoNum_, bool invert_);
    void lfo_setDepth(uint8_t lfoNum_, uint8_t depth_);

    void lfo_copy();
    void lfo_paste();

    /* Layer Functions ------------------------------------------- */

    void layer_playBeat(uint8_t layerNum_, float fillVolMultiplier_);

    /* Layer Inst Functions -------------------------------------- */

    void layerInst_select(uint8_t layerNum_);
    void layerInst_reset(uint8_t layerNum_);

    void layerInst_menuRight();
    void layerInst_menuLeft();
    void layerInst_menuUp();
    void layerInst_menuDown();

    void layerInst_setInstSelected(uint8_t layerNum_, int16_t inst_);
    void layerInst_setInstLoaded(uint8_t layerNum_);
    void layerInst_setSampleSelected(uint8_t layerNum_, int16_t sample_);
    void layerInst_setSampleLoaded(uint8_t layerNum_);
    void layerInst_clearSampleSector(uint8_t sectorNum_);

    void layerInst_setLevel(uint8_t layerNum_, uint8_t level_);
    void layerInst_setPitch(uint8_t layerNum_, uint8_t pitch_);
    void layerInst_setDirection(uint8_t layerNum_, uint8_t direction_);
    void layerInst_setResolution(uint8_t layerNum_, uint8_t resolution_);
    void layerInst_setStart(uint8_t layerNum_, uint8_t start_);
    void layerInst_setEnd(uint8_t layerNum_, uint8_t end_);
    void layerInst_setTiming(uint8_t layerNum_, uint8_t timing_);
    void layerInst_setProbability(uint8_t layerNum_, uint8_t probability_);

    void layerInst_setLfoLevel(uint8_t layerNum_, uint8_t lfo_);
    void layerInst_setLfoPitch(uint8_t layerNum_, uint8_t lfo_);
    void layerInst_setLfoDirection(uint8_t layerNum_, uint8_t lfo_);
    void layerInst_setLfoResolution(uint8_t layerNum_, uint8_t lfo_);
    void layerInst_setLfoStart(uint8_t layerNum_, uint8_t lfo_);
    void layerInst_setLfoEnd(uint8_t layerNum_, uint8_t lfo_);
    void layerInst_setLfoTiming(uint8_t layerNum_, uint8_t lfo_);
    void layerInst_setLfoProbability(uint8_t layerNum_, uint8_t lfo_);

    void layerInst_setEq(uint8_t layerNum_, uint8_t eq_);
    void layerInst_setFilter(uint8_t layerNum_, uint8_t filter_);
    void layerInst_setEffect(uint8_t layerNum_, uint8_t effect_);
    void layerInst_setReverb(uint8_t layerNum_, uint8_t reverb_);

    void layerInst_calculateMixMode(uint8_t layerNum_);
    void layerInst_setOut(uint8_t layerNum_, uint8_t out_);

    void routing_select();
    void euclid_select();
    void song_select();
    void song_step(EncoderFunc function_, bool up_);
    void song_setActive(bool active_);
    void song_manualBank(uint8_t bank_);
    void song_toStartBank();
    void song_copyBank();
    void song_pasteBank();
    void lcd_drawSongMenu();
    void lcd_drawSong_Data();
    void lcd_drawSongOverview();
    void preview_request(uint8_t layerNum_, bool now_);
    void preview_start(uint8_t layerNum_);
    void preview_service();
    void preview_update();
    bool preview_padReplay(uint8_t layerNum_);
    void preview_setOn(bool on_);
    void preview_setLevel(uint8_t level_);
    void lcd_drawMetro_PreviewData();
    void lcd_drawMetro_PreviewLevelData();
    void lcd_drawMetro_AutosaveData();
    bool layerPageMenu() const;
    void euclid_apply();
    void euclid_clear();
    void euclid_step(EncoderFunc function_, bool up_);
    void lcd_drawEuclidMenu();
    void lcd_drawEuclid_Data();
    void lcd_drawEuclidOverview();
    void lcd_drawRoutingMenu();
    void lcd_drawRouting_Data(uint8_t route_);
    void lcd_drawRoutingState();
    void lcd_drawRoutingOverview();
    void routing_stepModuleOut(uint8_t module_, bool up_);
    void routing_setModuleOut(uint8_t module_, uint8_t out_);
    void routing_setMetroOut(uint8_t out_);
    void routing_setPatch(const uint8_t* kit_);
    void routing_reset();

    void layerInst_setMute(uint8_t layerNum_, bool mute_);
    void layerInst_setFill(uint8_t layerNum_, bool fill_);
    void layerInst_setStyle(uint8_t layerNum_, bool style_);

    void layerInst_copy();
    void layerInst_paste();

    /* Layer Song Functions -------------------------------------- */

    void layerSong_select(uint8_t layerNum_, uint8_t bankNum_);
    void layerSong_reset(uint8_t layerNum_, uint8_t bankNum_);

    void layerSong_menuRight();
    void layerSong_menuLeft();
    void layerSong_menuUp(uint8_t tab_);
    void layerSong_menuDown(uint8_t tab_);

    void layerSong_beatRight();
    void layerSong_beatLeft();

    void layerSong_setBeat(uint8_t layerNum_, uint8_t bankNum_, uint16_t interval_, uint8_t fillType_, uint8_t fillPattern_, uint8_t fillTime_, uint8_t fillLevel_, bool selected_, uint8_t velocity_ = 0);
    void layerSong_setSelectedBeatFill(uint8_t fillType_, uint8_t fillPattern_, uint8_t fillTime_, uint8_t fillLevel_);
    void layerSong_setSelectedBeatFillTime(uint8_t fillTime_);
    void layerSong_setSelectedBeatFillLevel(uint8_t fillLevel_);
    void layerSong_generateBeat(uint8_t type);
    void layerSong_shiftSelectedBeat(bool direction_);
    void layerSong_shiftAll(bool forward_);
    void layerSong_insertBeat();
    void layerSong_commitBank(int16_t selectedBeatNum_);
    void layerSong_resetSelectedBeat();
    void layerSong_resetBeats(uint8_t layerNum_, uint8_t bankNum_, uint16_t startInterval_);
    void layerSong_resetAllBeats(uint8_t layerNum_, uint8_t bankNum_);
    void layerSong_quantizeActiveBeats(uint8_t layerNum_, uint8_t bankNum_);
    void layerSong_arrangeActiveBeats(uint8_t layerNum_, uint8_t bankNum_);
    void layerSong_calculateLastActiveBeatNum(uint8_t layerNum_, uint8_t bankNum_);
    void layerSong_calculatePlayBeatNum(uint8_t layerNum_, uint8_t bankNum_, uint16_t playInterval_);
    void layerSong_calculateBeatFill(uint8_t layerNum_, uint8_t bankNum_, uint8_t beatNum_, uint8_t fillType_, uint8_t fillPattern, uint8_t fillTime_, uint8_t fillLevel_);
    void layerSong_calculateBeatFillTime(uint8_t layerNum_, uint8_t bankTab_, uint8_t beatNum_, uint8_t fillTime_);
    void layerSong_calculateBeatFillLevel(uint8_t layerNum_, uint8_t bankTab_, uint8_t beatNum_, uint8_t fillLevel_);

    void layerSong_copy();
    void layerSong_paste();

    void layerSong_selectBank(uint8_t bankNum_);
    void layerSong_triggerBank(uint8_t bankNum_);

    /* Play Functions -------------------------------------------- */

    void record();
    void play();
    void stop();
    void reset();

    void triggerStop();
    void triggerReset();

    /* Audio Functions ------------------------------------------- */

    void processAudioBlock(uint32_t* inputBuffer_, uint32_t* outputBuffer_);

    void processAudioReceive(uint32_t* inputtBuffer);
    void processAudioMetronome();
    void processAudioLayer();
    void processAudioRouting();
    void processAudioEq();
    void processAudioFilter(uint8_t filterNum_);
    void processAudioEffect(uint8_t effectNum_);
    void processAudioReverb();
    void processAudioSend(uint32_t* outputBuffer);

    float processAudioFilterHelper(uint8_t filterNum_, uint8_t channel_, float audio_);
    float effectDryGain(uint8_t effectNum_) const;
    float processAudioEffectHelper(uint8_t effectNum_, float audio_);
    float processAudioLpfHelper(uint8_t lpfNum_, float audio_);

    /* Interrupt Functions --------------------------------------- */

    void interruptPlay();

    void interruptMidiTxClock();
    void startMidiTx();

    void interruptSyncInPulse();
    void interruptSyncInGate();

    void interruptTransition();

    void interruptSystemPreset();
    void interruptEncoderPreset();

    void interruptEncoderRead(uint8_t encoder_);

    void interruptFuncButtonRead();

    void interruptLayerKeypadRead();
    void interruptLeftKeypadRead();
    void interruptRightKeypadRead();

    void interruptUpKeyRead();
    void interruptDownKeyRead();
    void interruptLongKeyRead();

    void interruptText();
    void interruptLedEq();
    void interruptSd();
    void interruptBeatSync();
    void interruptLimitAlert();

    /* Debug Functions ------------------------------------------- */

    inline void check(int32_t num_, bool side_, uint8_t line_) {
        uint16_t xPos;
        uint16_t yPos = 30 + (line_ * 10);

        switch (side_) {
            case 0:
                lcd.setAlignment(LEFT);
                xPos = 30;
                break;

            case 1:
                lcd.setAlignment(RIGHT);
                xPos = 930;
                break;
        }

        lcd.setFont(FONT_05x07);
        lcd.setForeColor(WHITE);
        lcd.setBackColor(BLACK);

        lcd.drawNumber(num_, 8, xPos, yPos);
    }
};

// clang-format on

#endif
