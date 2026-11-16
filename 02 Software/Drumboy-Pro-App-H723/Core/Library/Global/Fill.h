// Fill engine: fill type/pattern tables, beat and fill storage, and the code that turns a
// fill setting into timed, levelled micro-hits. Kept free of hardware headers so it can be
// compiled and tested on a PC (see 02 Software/tests).
#ifndef __FILL_H
#define __FILL_H

#include <stdint.h>
#include <stdlib.h>

#include "Util.h"

// clang-format off

const uint16_t kBeatLibrarySize                     = 64;
const uint16_t kBeatFillLibrarySize                 = 32;
const uint16_t kBeatMicroLibrarySize                = 16 + 1;

const uint8_t kFillTimeLibrarySize                  = 3;
const uint8_t kFillLevelLibrarySize                 = 6;

// Fill library dimensions. Beat packs fillType into 4 bits and fillPattern into 3 bits,
// so at most 16 types x 8 patterns can be addressed. Append new types at the end to keep
// the indices stored in existing song files valid.
const uint8_t kFillTypeLibrarySize                 = 15;
const uint8_t kFillPatternLibrarySize              = 8;

const uint8_t kMinFillType                          = 0;
const uint8_t kMaxFillType                          = kFillTypeLibrarySize - 1;

const uint8_t kMinFillPattern                       = 0;
const uint8_t kMaxFillPattern                       = kFillPatternLibrarySize - 1;

static_assert(kFillTypeLibrarySize <= 16, "fillType is stored in 4 bits of Beat::data");
static_assert(kFillPatternLibrarySize <= 8, "fillPattern is stored in 3 bits of Beat::data");

const uint8_t kMinFillTime                          = 0;
const uint8_t kMaxFillTime                          = 2;

const uint8_t kMinFillLevel                         = 0;
const uint8_t kMaxFillLevel                         = 4;

struct FillCopyData {
    bool active                                     = false;
    int8_t type;
    int8_t pattern;
    int8_t time;
    int8_t level;
};

struct FillTypeData {
    char nameLong[11];
};

const struct FillTypeData kFillTypeDataLibrary[kFillTypeLibrarySize] = {
    {"     BASIC"},
    {"      WAVE"},
    {"    TRAP-A"},
    {"    TRAP-B"},
    {"    TRAP-C"},
    {"    TRAP-D"},
    {"     SWING"},
    {"      ROLL"},
    {"    TECHNO"},
    {"    BREAKS"},
    {"    GARAGE"},
    {"   TRIPLET"},
    {"    EUCLID"},
    {"   RATCHET"},
    {"  FOOTWORK"}};

struct FillPatternData {
    char nameLong[11];
    uint8_t stepSize;
    uint8_t stepMid;
    uint8_t levelLo;
    uint8_t levelHi;
    static const uint8_t levelStep                  = 75;
    float timeLibrary[kBeatMicroLibrarySize];
};

const struct FillPatternData kFillPatternDataLibrary[kFillTypeLibrarySize][kFillPatternLibrarySize] = {
    // BASIC
    {
     // 10000000
     {"      FLAT", 1, 1, 100, 100, {0.000000}},
     // 10000000
     {"     GHOST", 1, 1, 100, 100, {0.000000}},
     // 10000000
     {"     MUTED", 1, 1, 100, 100, {0.000000}},
     // 10001000
     {"   BASIC_2", 2, 1, 40, 100, {0.000000, 0.500000, 1.000000}},
     // 1000 1000 1000
     {"   BASIC_3", 3, 1, 40, 100, {0.000000, 0.333333, 0.666666, 1.000000}},
     // 10001000 10001000
     {"   BASIC_4", 4, 2, 40, 100, {0.000000, 0.250000, 0.500000, 0.750000, 1.000000}},
     // 1010 1010 1010
     {"   BASIC_6", 6, 3, 40, 100, {0.000000, 0.166666, 0.333333, 0.500000, 0.666666, 0.833333, 1.000000}},
     // 10101010 10101010
     {"   BASIC_8", 8, 4, 40, 100, {0.000000, 0.125000, 0.250000, 0.375000, 0.500000, 0.625000, 0.750000, 0.875000, 1.000000}}},
        
     // WAVE
    {
     // 11111111 10101010
     {"    WAVE_A", 12, 8, 40, 100, {0.000000, 0.062500, 0.125000, 0.187500, 0.250000, 0.312500, 0.375000, 0.437500, 0.500000, 0.625000, 0.750000, 0.875000, 1.000000}},
     // 10101010 11111111
     {"    WAVE_B", 12, 4, 40, 100, {0.000000, 0.125000, 0.250000, 0.375000, 0.500000, 0.562500, 0.625000, 0.687500, 0.750000, 0.812500, 0.875000, 0.937500, 1.000000}},
     // 111111 101010
     {"    WAVE_C", 9, 6, 40, 100, {0.000000, 0.083333, 0.166666, 0.250000, 0.333333, 0.416666, 0.500000, 0.666666, 0.833333, 1.000000}},
     // 101010 111111
     {"    WAVE_D", 9, 3, 40, 100, {0.000000, 0.166666, 0.333333, 0.500000, 0.583333, 0.666666, 0.750000, 0.833333, 0.916666, 1.000000}},
     // 11111111 101010
     {"    WAVE_E", 11, 8, 40, 100, {0.000000, 0.062500, 0.125000, 0.187500, 0.250000, 0.312500, 0.375000, 0.437500, 0.500000, 0.666666, 0.833333, 1.000000}},
     // 10101010 111111
     {"    WAVE_F", 10, 4, 40, 100, {0.000000, 0.125000, 0.250000, 0.375000, 0.500000, 0.583333, 0.666666, 0.750000, 0.833333, 0.916666, 1.000000}},
     // 111111 10101010
     {"    WAVE_G", 10, 6, 40, 100, {0.000000, 0.083333, 0.166666, 0.250000, 0.333333, 0.416666, 0.500000, 0.625000, 0.750000, 0.875000, 1.000000}},
     // 101010 11111111
     {"    WAVE_H", 11, 3, 40, 100, {0.000000, 0.166666, 0.333333, 0.500000, 0.562500, 0.625000, 0.687500, 0.750000, 0.812500, 0.875000, 0.937500, 1.000000}}},
    
     // TRAP-A
    {
     // 00000000 00000000
     {"   TRAP_A1", 5, 2, 40, 100, {0.000000, 0.250000, 0.500000, 0.625000, 0.750000, 1.000000}},
     // 10101000 10001000
     {"   TRAP_A2", 5, 3, 40, 100, {0.000000, 0.125000, 0.250000, 0.500000, 0.750000, 1.000000}},
     // 11111000 10001000
     {"   TRAP_A3", 7, 5, 40, 100, {0.000000, 0.062500, 0.125000, 0.187500, 0.250000, 0.500000, 0.750000, 1.000000}},
     // 10001111 10001000
     {"   TRAP_A4", 7, 5, 40, 100, {0.000000, 0.250000, 0.312500, 0.375000, 0.437500, 0.500000, 0.750000, 1.000000}},
     // 10001110 10001010
     {"   TRAP_A5", 7, 4, 40, 100, {0.000000, 0.250000, 0.312500, 0.375000, 0.500000, 0.750000, 0.875000, 1.000000}},
     // 10001000 11111000
     {"   TRAP_A6", 7, 2, 40, 100, {0.000000, 0.250000, 0.500000, 0.562500, 0.625000, 0.687500, 0.750000, 1.000000}},
     // 11111000 101010
     {"   TRAP_A7", 8, 5, 40, 100, {0.000000, 0.062500, 0.125000, 0.187500, 0.250000, 0.500000, 0.666666, 0.833333, 1.000000}},
     // 11111000 100111
     {"   TRAP_A8", 9, 5, 40, 100, {0.000000, 0.062500, 0.125000, 0.187500, 0.250000, 0.500000, 0.750000, 0.833333, 0.916666, 1.000000}}},

     // TRAP-B
    {   
     // 10001111 10001010
     {"   TRAP_B1", 8, 5, 40, 100, {0.000000, 0.250000, 0.312500, 0.375000, 0.437500, 0.500000, 0.750000, 0.875000, 1.000000}},
     // 10001000 11111010
     {"   TRAP_B2", 8, 2, 40, 100, {0.000000, 0.250000, 0.500000, 0.562500, 0.625000, 0.687500, 0.750000, 0.875000, 1.000000}},
     // 10001000 10001111
     {"   TRAP_B3", 7, 2, 40, 100, {0.000000, 0.250000, 0.500000, 0.750000, 0.812500, 0.875000, 0.937500, 1.000000}},
     // 10001111 10101010
     {"   TRAP_B4", 9, 5, 40, 100, {0.000000, 0.250000, 0.312500, 0.375000, 0.437500, 0.500000, 0.625000, 0.750000, 0.875000, 1.000000}},
     // 10101111 10101010
     {"   TRAP_B5", 10, 6, 40, 100, {0.000000, 0.125000, 0.250000, 0.312500, 0.375000, 0.437500, 0.500000, 0.625000, 0.750000, 0.875000, 1.000000}},
     // 221110 101010
     {"   TRAP_B6", 10, 7, 40, 100, {0.000000, 0.041666, 0.083333, 0.125000, 0.166666, 0.250000, 0.333333, 0.500000, 0.666666, 0.833333, 1.000000}},
     // 101010 221110
     {"   TRAP_B7", 10, 3, 40, 100, {0.000000, 0.166667, 0.333333, 0.500000, 0.541666, 0.583333, 0.625000, 0.666666, 0.750000, 0.833333, 1.000000}},
     // 11111000 10101111
     {"   TRAP_B8", 11, 5, 40, 100, {0.000000, 0.062500, 0.125000, 0.187500, 0.250000, 0.500000, 0.625000, 0.750000, 0.812500, 0.875000, 0.937500, 1.000000}}},

     // TRAP-C
    {
     // 10001010 11111111
     {"   TRAP_C1", 11, 3, 40, 100, {0.000000, 0.250000, 0.375000, 0.500000, 0.562500, 0.625000, 0.687500, 0.750000, 0.812500, 0.875000, 0.937500, 1.000000}},
     // 11111010 10101111
     {"   TRAP_C2", 12, 6, 40, 100, {0.000000, 0.062500, 0.125000, 0.187500, 0.250000, 0.375000, 0.500000, 0.625000, 0.750000, 0.812500, 0.875000, 0.937500, 1.000000}},
     // 11101110 10101110
     {"   TRAP_C3", 11, 6, 40, 100, {0.000000, 0.062500, 0.125000, 0.250000, 0.312500, 0.375000, 0.500000, 0.625000, 0.750000, 0.812500, 0.875000, 1.000000}},
     // 22221000 101010
     {"   TRAP_C4", 12, 9, 40, 100, {0.000000, 0.031250, 0.062500, 0.093750, 0.125000, 0.156250, 0.187500, 0.218750, 0.250000, 0.500000, 0.666666, 0.833333, 1.000000}},
     // 101010 22221110
     {"   TRAP_C5", 14, 3, 40, 100, {0.000000, 0.166666, 0.333333, 0.500000, 0.531250, 0.562500, 0.593750, 0.625000, 0.656250, 0.687500, 0.718750, 0.750000, 0.812500, 0.875000, 1.000000}},
     // 11111000 10001111
     {"   TRAP_C6", 10, 5, 40, 100, {0.000000, 0.062500, 0.125000, 0.187500, 0.250000, 0.500000, 0.750000, 0.812500, 0.875000, 0.937500, 1.000000}},
     // 10101111 11101010
     {"   TRAP_C7", 11, 6, 40, 100, {0.000000, 0.125000, 0.250000, 0.312500, 0.375000, 0.437500, 0.500000, 0.562500, 0.625000, 0.750000, 0.875000, 1.000000}},
     // 22221010 10001010
     {"   TRAP_C8", 13, 10, 40, 100, {0.000000, 0.031250, 0.062500, 0.093750, 0.125000, 0.156250, 0.187500, 0.218750, 0.250000, 0.375000, 0.500000, 0.750000, 0.875000, 1.000000}}},

     // TRAP-D
    {
     // 10102222 10101000
     {"   TRAP_D1", 13, 10, 40, 100, {0.000000, 0.125000, 0.250000, 0.281250, 0.312500, 0.343750, 0.375000, 0.406250, 0.437500, 0.468750, 0.500000, 0.625000, 0.750000, 1.000000}},
     // 10111022 10101110
     {"   TRAP_D2", 13, 8, 40, 100, {0.000000, 0.125000, 0.187500, 0.250000, 0.375000, 0.406250, 0.437500, 0.468750, 0.500000, 0.625000, 0.750000, 0.812500, 0.875000, 1.000000}},
     // 10101110 10102222
     {"   TRAP_D3", 15, 5, 40, 100, {0.000000, 0.125000, 0.250000, 0.312500, 0.375000, 0.500000, 0.625000, 0.750000, 0.781250, 0.812500, 0.843750, 0.875000, 0.906250, 0.937500, 0.968750, 1.000000}},
     // 22221010 11111010
     {"   TRAP_D4", 16, 10, 40, 100, {0.000000, 0.031250, 0.062500, 0.093750, 0.125000, 0.156250, 0.187500, 0.218750, 0.250000, 0.375000, 0.500000, 0.562500, 0.625000, 0.687500, 0.750000, 0.875000, 1.000000}},
     // 10102210 22101110
     {"   TRAP_D5", 15, 7, 40, 100, {0.000000, 0.125000, 0.250000, 0.281250, 0.312500, 0.343750, 0.375000, 0.500000, 0.531250, 0.562500, 0.593750, 0.625000, 0.750000, 0.812500, 0.875000, 1.000000}},
     // 10102222 20101010
     {"   TRAP_D6", 15, 10, 40, 100, {0.000000, 0.125000, 0.250000, 0.281250, 0.312500, 0.343750, 0.375000, 0.406250, 0.437500, 0.468750, 0.500000, 0.531250, 0.625000, 0.750000, 0.875000, 1.000000}},
     // 22221111 10101010
     {"   TRAP_D7", 16, 12, 40, 100, {0.000000, 0.031250, 0.062500, 0.093750, 0.125000, 0.156250, 0.187500, 0.218750, 0.250000, 0.312500, 0.375000, 0.437500, 0.500000, 0.625000, 0.750000, 0.875000, 1.000000}},
     // 22221010 11101010
     {"   TRAP_D8", 15, 10, 40, 100, {0.000000, 0.031250, 0.062500, 0.093750, 0.125000, 0.156250, 0.187500, 0.218750, 0.250000, 0.375000, 0.500000, 0.562500, 0.625000, 0.750000, 0.875000, 1.000000}}},
    
    // SWING
    {
     // swing A
     {"   SWING_A", 3, 1, 40, 100, {0.000000, 0.500000, 0.875000, 1.000000}},
     // swing B   
     {"   SWING_B", 4, 2, 40, 100, {0.000000, 0.250000, 0.500000, 0.875000, 1.000000}},
     // swing C
     {"   SWING_C", 7, 5, 40, 100, {0.000000, 0.250000, 0.312500, 0.375000, 0.437500, 0.500000, 0.875000, 1.000000}},
     // swing D
     {"   SWING_D", 5, 3, 40, 100, {0.000000, 0.166666, 0.333333, 0.500000, 0.875000, 1.000000}},
     // swing E
     {"   SWING_E", 6, 4, 40, 100, {0.000000, 0.125000, 0.250000, 0.375000, 0.500000, 0.875000, 1.000000}},
     // swing F   
     {"   SWING_F", 4, 2, 40, 100, {0.000000, 0.375000, 0.500000, 0.875000, 1.000000}},
     // swing G
     {"   SWING_G", 4, 2, 40, 100, {0.000000, 0.062500, 0.125000, 0.875000, 1.000000}},
     // swing H
     {"   SWING_H", 5, 2, 40, 100, {0.000000, 0.333333, 0.666666, 0.750000, 0.875000, 1.000000}}},

    // ROLL
    {
     {"  ROLL_DBL", 15, 3, 40, 100, {0.000000, 0.250000, 0.375000, 0.500000, 0.562500, 0.625000, 0.687500, 0.750000, 0.781250, 0.812500, 0.843750, 0.875000, 0.906250, 0.937500, 0.968750, 1.000000}},
     {" ROLL_TRIP", 12, 3, 40, 100, {0.000000, 0.250000, 0.375000, 0.500000, 0.583333, 0.666667, 0.750000, 0.791667, 0.833333, 0.875000, 0.916667, 0.958333, 1.000000}},
     {" ROLL_EVEN", 15, 5, 40, 100, {0.000000, 0.125000, 0.250000, 0.333333, 0.416667, 0.500000, 0.562500, 0.625000, 0.687500, 0.750000, 0.791667, 0.833333, 0.875000, 0.916667, 0.958333, 1.000000}},
     {" ROLL_LONG", 15, 4, 40, 100, {0.000000, 0.125000, 0.250000, 0.375000, 0.500000, 0.562500, 0.625000, 0.687500, 0.750000, 0.791667, 0.833333, 0.875000, 0.906250, 0.937500, 0.968750, 1.000000}},
     {" ROLL_STEP", 16, 4, 40, 100, {0.000000, 0.125000, 0.250000, 0.375000, 0.500000, 0.562500, 0.625000, 0.687500, 0.750000, 0.781250, 0.812500, 0.843750, 0.875000, 0.906250, 0.937500, 0.968750, 1.000000}},
     {" ROLL_LATE", 13, 1, 40, 100, {0.000000, 0.500000, 0.562500, 0.625000, 0.687500, 0.750000, 0.781250, 0.812500, 0.843750, 0.875000, 0.906250, 0.937500, 0.968750, 1.000000}},
     {"ROLL_BRAKE", 15, 12, 40, 100, {0.000000, 0.031250, 0.062500, 0.093750, 0.125000, 0.156250, 0.187500, 0.218750, 0.250000, 0.312500, 0.375000, 0.437500, 0.500000, 0.625000, 0.750000, 1.000000}},
     {"   ROLL_32", 16, 8, 40, 100, {0.000000, 0.062500, 0.125000, 0.187500, 0.250000, 0.312500, 0.375000, 0.437500, 0.500000, 0.562500, 0.625000, 0.687500, 0.750000, 0.812500, 0.875000, 0.937500, 1.000000}}},

    // TECHNO
    {
     {"    GALLOP", 12, 6, 40, 100, {0.000000, 0.125000, 0.187500, 0.250000, 0.375000, 0.437500, 0.500000, 0.625000, 0.687500, 0.750000, 0.875000, 0.937500, 1.000000}},
     {"  GALLOP_R", 12, 6, 40, 100, {0.000000, 0.062500, 0.187500, 0.250000, 0.312500, 0.437500, 0.500000, 0.562500, 0.687500, 0.750000, 0.812500, 0.937500, 1.000000}},
     {"   ROLLING", 12, 6, 40, 100, {0.000000, 0.062500, 0.125000, 0.250000, 0.312500, 0.375000, 0.500000, 0.562500, 0.625000, 0.750000, 0.812500, 0.875000, 1.000000}},
     {"  TRESILLO", 6, 3, 40, 100, {0.000000, 0.187500, 0.375000, 0.500000, 0.687500, 0.875000, 1.000000}},
     {"  DOTTED_8", 6, 3, 40, 100, {0.000000, 0.187500, 0.375000, 0.562500, 0.750000, 0.937500, 1.000000}},
     {"   EVERY_5", 4, 2, 40, 100, {0.000000, 0.312500, 0.625000, 0.937500, 1.000000}},
     {"    RUMBLE", 8, 4, 40, 100, {0.000000, 0.250000, 0.375000, 0.437500, 0.500000, 0.750000, 0.875000, 0.937500, 1.000000}},
     {"    PICKUP", 6, 1, 40, 100, {0.000000, 0.812500, 0.875000, 0.906250, 0.937500, 0.968750, 1.000000}}},

    // BREAKS
    {
     {"   BREAK_A", 7, 4, 40, 100, {0.000000, 0.125000, 0.312500, 0.375000, 0.625000, 0.812500, 0.875000, 1.000000}},
     {"   BREAK_B", 5, 2, 40, 100, {0.000000, 0.375000, 0.562500, 0.812500, 0.937500, 1.000000}},
     {"   SHUFFLE", 10, 5, 40, 100, {0.000000, 0.125000, 0.187500, 0.312500, 0.437500, 0.500000, 0.625000, 0.687500, 0.812500, 0.937500, 1.000000}},
     {"      CHOP", 10, 4, 40, 100, {0.000000, 0.125000, 0.312500, 0.437500, 0.500000, 0.625000, 0.812500, 0.875000, 0.937500, 0.968750, 1.000000}},
     {"    JUNGLE", 10, 5, 40, 100, {0.000000, 0.375000, 0.406250, 0.437500, 0.468750, 0.500000, 0.875000, 0.906250, 0.937500, 0.968750, 1.000000}},
     {"   STUTTER", 9, 1, 40, 100, {0.000000, 0.500000, 0.531250, 0.562500, 0.593750, 0.625000, 0.656250, 0.687500, 0.718750, 1.000000}},
     {"  HALFTIME", 4, 1, 40, 100, {0.000000, 0.625000, 0.875000, 0.937500, 1.000000}},
     {"   EDIT_32", 16, 8, 40, 100, {0.000000, 0.031250, 0.125000, 0.156250, 0.312500, 0.343750, 0.437500, 0.468750, 0.500000, 0.531250, 0.625000, 0.656250, 0.875000, 0.906250, 0.937500, 0.968750, 1.000000}}},

    // GARAGE
    {
     {"   SHUF_58", 16, 8, 40, 100, {0.000000, 0.072500, 0.125000, 0.197500, 0.250000, 0.322500, 0.375000, 0.447500, 0.500000, 0.572500, 0.625000, 0.697500, 0.750000, 0.822500, 0.875000, 0.947500, 1.000000}},
     {"   SHUF_66", 16, 8, 40, 100, {0.000000, 0.082500, 0.125000, 0.207500, 0.250000, 0.332500, 0.375000, 0.457500, 0.500000, 0.582500, 0.625000, 0.707500, 0.750000, 0.832500, 0.875000, 0.957500, 1.000000}},
     {"   2STEP_A", 4, 2, 40, 100, {0.000000, 0.375000, 0.625000, 0.952500, 1.000000}},
     {"   2STEP_B", 6, 3, 40, 100, {0.000000, 0.202500, 0.375000, 0.500000, 0.827500, 0.875000, 1.000000}},
     {"      SKIP", 10, 5, 40, 100, {0.000000, 0.125000, 0.200000, 0.325000, 0.375000, 0.500000, 0.625000, 0.700000, 0.825000, 0.875000, 1.000000}},
     {"   SWING_8", 8, 4, 40, 100, {0.000000, 0.165000, 0.250000, 0.415000, 0.500000, 0.665000, 0.750000, 0.915000, 1.000000}},
     {"    BOUNCE", 12, 6, 40, 100, {0.000000, 0.125000, 0.200000, 0.250000, 0.325000, 0.450000, 0.500000, 0.625000, 0.700000, 0.750000, 0.825000, 0.950000, 1.000000}},
     {"      LAZY", 6, 3, 40, 100, {0.000000, 0.250000, 0.455000, 0.625000, 0.830000, 0.955000, 1.000000}}},

    // TRIPLET
    {
     {"   TRIP_12", 12, 6, 40, 100, {0.000000, 0.083333, 0.166667, 0.250000, 0.333333, 0.416667, 0.500000, 0.583333, 0.666667, 0.750000, 0.833333, 0.916667, 1.000000}},
     {"  TRIP_GAL", 8, 4, 40, 100, {0.000000, 0.083333, 0.166667, 0.250000, 0.500000, 0.583333, 0.666667, 0.750000, 1.000000}},
     {" TRIP_ROLL", 10, 1, 40, 100, {0.000000, 0.500000, 0.583333, 0.666667, 0.750000, 0.791667, 0.833333, 0.875000, 0.916667, 0.958333, 1.000000}},
     {"   QUINT_5", 5, 3, 40, 100, {0.000000, 0.200000, 0.400000, 0.600000, 0.800000, 1.000000}},
     {"    SEPT_7", 7, 4, 40, 100, {0.000000, 0.142857, 0.285714, 0.428571, 0.571429, 0.714286, 0.857143, 1.000000}},
     {"   NONUP_9", 9, 5, 40, 100, {0.000000, 0.111111, 0.222222, 0.333333, 0.444444, 0.555556, 0.666667, 0.777778, 0.888889, 1.000000}},
     {"  POLY_3V4", 6, 3, 40, 100, {0.000000, 0.250000, 0.333333, 0.500000, 0.666667, 0.750000, 1.000000}},
     {"  POLY_3V8", 10, 5, 40, 100, {0.000000, 0.125000, 0.250000, 0.333333, 0.375000, 0.500000, 0.625000, 0.666667, 0.750000, 0.875000, 1.000000}}},

    // EUCLID
    {
     {"     E3_16", 3, 2, 40, 100, {0.000000, 0.375000, 0.687500, 1.000000}},
     {"     E5_16", 5, 3, 40, 100, {0.000000, 0.250000, 0.437500, 0.625000, 0.812500, 1.000000}},
     {"     E7_16", 7, 4, 40, 100, {0.000000, 0.187500, 0.312500, 0.437500, 0.625000, 0.750000, 0.875000, 1.000000}},
     {"     E9_16", 9, 4, 40, 100, {0.000000, 0.125000, 0.250000, 0.375000, 0.500000, 0.562500, 0.687500, 0.812500, 0.937500, 1.000000}},
     {"    E11_16", 11, 5, 40, 100, {0.000000, 0.125000, 0.187500, 0.312500, 0.375000, 0.500000, 0.562500, 0.687500, 0.750000, 0.875000, 0.937500, 1.000000}},
     {"    E13_16", 13, 6, 40, 100, {0.000000, 0.125000, 0.187500, 0.250000, 0.312500, 0.437500, 0.500000, 0.562500, 0.625000, 0.750000, 0.812500, 0.875000, 0.937500, 1.000000}},
     {"     E5_12", 5, 3, 40, 100, {0.000000, 0.250000, 0.416667, 0.666667, 0.833333, 1.000000}},
     {"     E7_12", 7, 3, 40, 100, {0.000000, 0.166667, 0.333333, 0.500000, 0.583333, 0.750000, 0.916667, 1.000000}}},

    // RATCHET
    {
     {"   ACCEL_8", 8, 3, 40, 100, {0.000000, 0.192369, 0.368900, 0.528580, 0.670123, 0.791815, 0.891181, 0.964103, 1.000000}},
     {"  ACCEL_12", 12, 5, 40, 100, {0.000000, 0.122358, 0.239274, 0.350481, 0.455669, 0.554472, 0.646447, 0.731043, 0.807550, 0.875000, 0.931959, 0.975944, 1.000000}},
     {"  ACCEL_16", 16, 7, 40, 100, {0.000000, 0.080477, 0.159359, 0.236568, 0.312014, 0.385595, 0.457195, 0.526675, 0.593874, 0.658594, 0.720591, 0.779553, 0.835062, 0.886525, 0.933014, 0.972795, 1.000000}},
     {"   DECEL_8", 8, 6, 40, 100, {0.000000, 0.035897, 0.108819, 0.208185, 0.329877, 0.471420, 0.631100, 0.807631, 1.000000}},
     {"  DECEL_12", 12, 8, 40, 100, {0.000000, 0.024056, 0.068041, 0.125000, 0.192450, 0.268957, 0.353553, 0.445528, 0.544331, 0.649519, 0.760726, 0.877642, 1.000000}},
     {" BOUNCE_BL", 12, 4, 40, 100, {0.000000, 0.174874, 0.323518, 0.449865, 0.557259, 0.648545, 0.726138, 0.792091, 0.848152, 0.895804, 0.936308, 0.970736, 1.000000}},
     {"    GOLDEN", 8, 5, 40, 100, {0.000000, 0.090170, 0.236068, 0.326238, 0.472136, 0.618034, 0.708204, 0.854102, 1.000000}},
     {"   CLUSTER", 10, 5, 40, 100, {0.000000, 0.170534, 0.314127, 0.414127, 0.470534, 0.500000, 0.529466, 0.585873, 0.685873, 0.829466, 1.000000}}},

    // FOOTWORK
    {
     {"    JUKE_A", 6, 4, 40, 100, {0.000000, 0.125000, 0.250000, 0.375000, 0.500000, 0.750000, 1.000000}},
     {"    JUKE_B", 7, 3, 40, 100, {0.000000, 0.166667, 0.333333, 0.500000, 0.666667, 0.833333, 0.916667, 1.000000}},
     {"    JUKE_C", 9, 4, 40, 100, {0.000000, 0.083333, 0.250000, 0.416667, 0.500000, 0.583333, 0.750000, 0.875000, 0.916667, 1.000000}},
     {" TRIP_KICK", 11, 4, 40, 100, {0.000000, 0.125000, 0.250000, 0.375000, 0.500000, 0.625000, 0.750000, 0.791667, 0.875000, 0.916667, 0.958333, 1.000000}},
     {"    STUT_2", 14, 5, 40, 100, {0.000000, 0.250000, 0.281250, 0.312500, 0.343750, 0.500000, 0.750000, 0.781250, 0.812500, 0.843750, 0.875000, 0.906250, 0.937500, 0.968750, 1.000000}},
     {" HALF_JUKE", 7, 3, 40, 100, {0.000000, 0.250000, 0.375000, 0.750000, 0.875000, 0.916667, 0.958333, 1.000000}},
     {"     BLAST", 6, 6, 40, 100, {0.000000, 0.041667, 0.083333, 0.125000, 0.166667, 0.208333, 1.000000}},
     {" CALL_RESP", 9, 3, 40, 100, {0.000000, 0.083333, 0.166667, 0.500000, 0.583333, 0.666667, 0.750000, 0.791667, 0.833333, 1.000000}}}};

/*
1/6     {0.000000, 0.166667, 0.333333, 0.500000, 0.666666, 0.833333}
1/12    {0.000000, 0.083333, 0.166666, 0.250000, 0.333333, 0.416666, 0.500000, 0.583333, 0.666666, 0.750000, 0.833333, 0.916666}
1/24    {0.000000, 0.041666, 0.083333, 0.125000, 0.166666, 0.208333, 0.250000, 0.291666, 0.333333, 0.375000, 0.416666, 0.458333, 0.500000, 0.541666, 0.583333, 0.625000, 0.666666, 0.708333, 0.750000, 0.791666, 0.833333, 0.875000, 0.916666, 0.958333}

1/8     {0.000000, 0.125000, 0.250000, 0.375000, 0.500000, 0.625000, 0.750000, 0.875000}
1/16    {0.000000, 0.062500, 0.125000, 0.187500, 0.250000, 0.312500, 0.375000, 0.437500, 0.500000, 0.562500, 0.625000, 0.687500, 0.750000, 0.812500, 0.875000, 0.937500}
1/32    {0.000000, 0.031250, 0.062500, 0.093750, 0.125000, 0.156250, 0.187500, 0.218750, 0.250000, 0.281250, 0.312500, 0.343750, 0.375000, 0.406250, 0.437500, 0.468750, 0.5000000, 0.531250, 0.562500, 0.593750, 0.625000, 0.656250, 0.687500, 0.718750, 0.750000, 0.781250, 0.812500, 0.843750, 0.875000, 0.906250, 0.937500, 0.968750}
*/

struct FillTimeData {
    char nameLongR[11];
};

const struct FillTimeData kFillTimeDataLibrary[] = {
    {"       STD"},
    {"      LEFT"},
    {"     RIGHT"}};

struct FillLevelData {
    char nameLongR[11];
};

const struct FillLevelData kFillLevelDataLibrary[] = {
    {"      FLAT"},
    {"      STEP"},
    {"   DESCEND"},
    {"    ASCEND"},
    {"    RANDOM"}};

/*----------------------------------------------------------------------------*/

#define NULL_FILL_TARGET                            255

struct BeatMicro {
    // [7]      : active
    // [6:0]    : level

    uint8_t data                                    = 0x00;
    uint16_t interval                               = 0x00;

    void set(uint16_t interval_, uint8_t level_) {
        setActive(true);
        setLevel(level_);
        interval = interval_;
    }

    void reset() {
        data = 0x00;
        interval = 0x00;
    }

    void setActive(bool active) {
        data &= 0x7F;                               // Clear bit 7
        data |= (active << 7);                      // Write bit 7
    }

    bool getActive() const { return (data >> 7); }

    void setLevel(uint8_t level_) {
        data &= 0x80;                               // Keep only bit 7
        data |= (level_ & 0x7F);                    // Write lower 7 bits
    }

    uint8_t getLevel() const { return (data & 0x7F); }

    void setInterval(uint16_t interval_) { interval = interval_; }

    uint16_t getInterval() const { return interval; }
};

/*----------------------------------------------------------------------------*/

#define FILL_TIME_STD                               0x00
#define FILL_TIME_L_ALIGNED                         0x01
#define FILL_TIME_R_ALIGNED                         0x02

struct BeatFill {
    // [7]   : active
    // [6:0] : stepSize

    uint8_t data                                    = 0x00;
    static constexpr uint8_t ACTIVE_MASK            = 0x80;
    static constexpr uint8_t STEPSIZE_MASK          = 0x7F;

    BeatMicro beatMicroLibrary[kBeatMicroLibrarySize];

    void setActive(bool active_) { (active_) ? data |= ACTIVE_MASK : data &= static_cast<uint8_t>(~ACTIVE_MASK); }

    bool getActive() const { return (data & ACTIVE_MASK) != 0; }

    void setStepSize(uint8_t stepSize_) {
        data &= ACTIVE_MASK;
        data |= (stepSize_ & STEPSIZE_MASK);
    }

    uint8_t getStepSize() const { return data & STEPSIZE_MASK; }

    void set(uint16_t startInterval_, uint16_t endInterval_, uint8_t fillType_, uint8_t fillPattern_, uint8_t fillTime_, uint8_t fillLevel_) {
        setActive(true);

        // Reset beat micro library
        for (uint8_t i = 0; i < kBeatMicroLibrarySize; i++) {
            beatMicroLibrary[i].reset();
        }

        // Set beat micro library
        const FillPatternData& refFill = kFillPatternDataLibrary[fillType_][fillPattern_];
        uint16_t totalInterval = static_cast<uint16_t>(endInterval_ - startInterval_);

        uint8_t stepSize;
        uint16_t interval;
        uint8_t level;
        float levelInc;
        float levelDec;
        int levelHigh = refFill.levelHi;
        int levelLow = refFill.levelLo;
        int levelStep = refFill.levelStep;
        float levelDiff = levelHigh - levelLow;
        uint8_t levelLibrary[refFill.stepSize + 1];
        uint8_t i;

        switch (fillTime_) {
            case FILL_TIME_STD:
                stepSize = refFill.stepSize;
                setStepSize(stepSize);
                switch (fillLevel_) {
                    case 0:  // Flat
                        for (i = 0; i < stepSize; i++) levelLibrary[i] = levelHigh;
                        break;

                    case 1:  // Step
                        for (i = 0; i < stepSize; i++) {
                            (i % 2) ? levelLibrary[i] = levelStep : levelLibrary[i] = levelHigh;
                        }
                        break;

                    case 2:  // Hi-Lo
                        levelDec = levelDiff / (stepSize - 1);
                        for (i = 0; i < stepSize; i++) {
                            levelLibrary[i] = clamp((int)(levelHigh - (levelDec * i)), levelLow, levelHigh);
                        }
                        break;

                    case 3:  // Lo-Hi
                        levelInc = levelDiff / (stepSize - 1);
                        for (i = 0; i < stepSize; i++) {
                            levelLibrary[i] = clamp((int)(levelLow + (levelInc * i)), levelLow, levelHigh);
                        }
                        break;

                    case 4:  // Random
                        for (i = 0; i < stepSize; i++) {
                            levelLibrary[i] = clamp((int)(levelLow + (rand() % (uint8_t)levelDiff)), levelLow, levelHigh);
                            ;
                        }
                        break;
                }

                for (i = 0; i < getStepSize(); i++) {
                    interval = static_cast<uint16_t>(startInterval_ + static_cast<uint16_t>(totalInterval * refFill.timeLibrary[i]));
                    level = levelLibrary[i];
                    beatMicroLibrary[i].set(interval, level);
                }
                break;

            case FILL_TIME_L_ALIGNED:
                stepSize = refFill.stepSize + 1;
                setStepSize(stepSize);
                switch (fillLevel_) {
                    case 0:  // Flat
                        for (i = 0; i < stepSize; i++) levelLibrary[i] = levelHigh;
                        break;

                    case 1:  // Step
                        for (i = 0; i < stepSize; i++) {
                            (i % 2) ? levelLibrary[i] = levelStep : levelLibrary[i] = levelHigh;
                        }
                        break;

                    case 2:  // Hi-Lo
                        levelDec = (float)levelDiff / (stepSize - 1);
                        for (i = 0; i < stepSize; i++) {
                            levelLibrary[i] = clamp((int)(levelHigh - (levelDec * i)), levelLow, levelHigh);
                        }
                        break;

                    case 3:  // Lo-Hi
                        levelInc = (float)levelDiff / (stepSize - 1);
                        for (i = 0; i < stepSize; i++) {
                            levelLibrary[i] = clamp((int)(levelLow + (levelInc * i)), levelLow, levelHigh);
                        }
                        break;

                    case 4:  // Random
                        for (i = 0; i < stepSize; i++) {
                            levelLibrary[i] = clamp((int)(levelLow + (rand() % (uint8_t)levelDiff)), levelLow, levelHigh);
                            ;
                        }
                        break;
                }

                for (i = 0; i < getStepSize(); i++) {
                    interval = static_cast<uint16_t>(startInterval_ + static_cast<uint16_t>(totalInterval * refFill.timeLibrary[i] * 0.5));
                    level = levelLibrary[i];
                    beatMicroLibrary[i].set(interval, level);
                }
                break;

            case FILL_TIME_R_ALIGNED:
                stepSize = refFill.stepSize + 1;
                setStepSize(stepSize);
                switch (fillLevel_) {
                    case 0:  // Flat
                        for (i = 0; i < stepSize; i++) levelLibrary[i] = levelHigh;
                        break;

                    case 1:  // Step
                        levelLibrary[0] = levelHigh;
                        for (i = 0; i < (stepSize - 1); i++) {
                            (i % 2) ? levelLibrary[i + 1] = levelStep : levelLibrary[i + 1] = levelHigh;
                        }
                        break;

                    case 2:  // Hi-Lo
                        stepSize -= 1;
                        levelDec = (float)levelDiff / (stepSize - 1);
                        levelLibrary[0] = levelHigh;
                        for (i = 0; i < stepSize; i++) {
                            levelLibrary[i + 1] = clamp((int)(levelHigh - (levelDec * i)), levelLow, levelHigh);
                        }
                        break;

                    case 3:  // Lo-Hi
                        stepSize -= 1;
                        levelInc = (float)levelDiff / (stepSize - 1);
                        levelLibrary[0] = levelHigh;
                        for (i = 0; i < stepSize; i++) {
                            levelLibrary[i + 1] = clamp((int)(levelLow + (levelInc * i)), levelLow, levelHigh);
                        }
                        break;

                    case 4:  // Random
                        levelLibrary[0] = levelHigh;
                        for (i = 1; i < stepSize; i++) {
                            levelLibrary[i] = clamp((int)(levelLow + (rand() % (uint8_t)levelDiff)), levelLow, levelHigh);
                            ;
                        }
                        break;
                }

                for (i = 0; i < getStepSize(); i++) {
                    (i == 0) ? interval = startInterval_ : interval = static_cast<uint16_t>(startInterval_ + static_cast<uint16_t>(totalInterval * ((refFill.timeLibrary[i - 1] * 0.5) + 0.5f)));
                    level = levelLibrary[i];
                    beatMicroLibrary[i].set(interval, level);
                }
                break;
        }
    }

    void reset() {
        setActive(false);
        setStepSize(0);
        for (uint8_t i = 0; i < kBeatMicroLibrarySize; i++) {
            beatMicroLibrary[i].reset();
        }
    }
};

/*----------------------------------------------------------------------------*/

struct Beat {
    // [15]     : active
    // [14:11]  : fillType
    // [10:8]   : fillPattern
    // [7:4]    : fillTime
    // [3:0]    : fillLevel

    uint16_t data                                   = 0x0000;
    uint8_t fillTarget                              = NULL_FILL_TARGET;
    uint8_t velocity                                = 0;                        // BeatNote.h: 0 = from the pattern
    uint16_t startInterval                          = 0;
    uint16_t endInterval                            = 0;
    uint16_t note                                   = 0;                        // BeatNote.h: chance, condition, nudge

    // Bit masks for precise field extraction

    static constexpr uint16_t ACTIVE_MASK           = 0x8000;
    static constexpr uint16_t TYPE_MASK             = 0x7800;                   // Bits 14:11
    static constexpr uint16_t PATTERN_MASK          = 0x0700;                   // Bits 10:8
    static constexpr uint16_t TIME_MASK             = 0x00F0;                   // Bits 7:4
    static constexpr uint16_t LEVEL_MASK            = 0x000F;                   // Bits 3:0

    // Bit shifts for fast field alignment

    static constexpr unsigned TYPE_SHIFT            = 11;
    static constexpr unsigned PATTERN_SHIFT         = 8;
    static constexpr unsigned TIME_SHIFT            = 4;
    static constexpr unsigned LEVEL_SHIFT           = 0;

    void reset() noexcept {
        data = 0x0000;
        startInterval = 0;
        endInterval = 0;
        fillTarget = NULL_FILL_TARGET;
        velocity = 0;
        note = 0;
    }

    // Per-note settings (BeatNote.h); all zero = plays as before V1.14.
    void setVelocity(uint8_t velocity_) noexcept { velocity = (velocity_ > 127) ? 127 : velocity_; }
    uint8_t getVelocity() const noexcept { return velocity; }
    void setNote(uint16_t note_) noexcept { note = note_; }
    uint16_t getNote() const noexcept { return note; }
    void resetNote() noexcept {
        velocity = 0;
        note = 0;
    }

    // Active bit logic
    void setActive(bool active) noexcept {
        data = active ? (data | ACTIVE_MASK) : (data & ~ACTIVE_MASK);
    }

    bool getActive() const noexcept { return (data & ACTIVE_MASK) != 0; }

    // Fill group logic (0-15)
    void setFillType(uint8_t type_) noexcept {
        data = (data & ~TYPE_MASK) | (static_cast<uint16_t>(type_ & 0x0F) << TYPE_SHIFT);
    }

    uint8_t getFillType() const noexcept { return static_cast<uint8_t>((data & TYPE_MASK) >> TYPE_SHIFT); }

    // Pattern variation logic (0-7)
    void setFillPattern(uint8_t pattern_) noexcept {
        data = (data & ~PATTERN_MASK) | (static_cast<uint16_t>(pattern_ & 0x07) << PATTERN_SHIFT);
    }

    uint8_t getFillPattern() const noexcept { return static_cast<uint8_t>((data & PATTERN_MASK) >> PATTERN_SHIFT); }

    // Timing mode logic (0-15)
    void setFillTime(uint8_t mode_) noexcept {
        data = (data & ~TIME_MASK) | (static_cast<uint16_t>(mode_ & 0x0F) << TIME_SHIFT);
    }

    uint8_t getFillTime() const noexcept { return static_cast<uint8_t>((data & TIME_MASK) >> TIME_SHIFT); }

    // Level/velocity logic (0-15)
    void setFillLevel(uint8_t mode_) noexcept {
        data = (data & ~LEVEL_MASK) | (static_cast<uint16_t>(mode_ & 0x0F) << LEVEL_SHIFT);
    }

    uint8_t getFillLevel() const noexcept { return static_cast<uint8_t>(data & LEVEL_MASK); }

    // Utility getters and setters
    void setFillTarget(uint8_t target_) noexcept { fillTarget = target_; }

    uint8_t getFillTarget() const noexcept { return fillTarget; }

    void setStartInterval(uint16_t interval_) noexcept { startInterval = interval_; }

    uint16_t getStartInterval() const noexcept { return startInterval; }

    void setEndInterval(uint16_t interval_) noexcept { endInterval = interval_; }

    uint16_t getEndInterval() const noexcept { return endInterval; }

    uint16_t getData() const noexcept { return data; }
};

/*----------------------------------------------------------------------------*/

struct Bank {
    Beat beatLibrary[kBeatLibrarySize];
    BeatFill beatFillLibrary[kBeatFillLibrarySize];

    int8_t lastActiveBeatNum                        = -1;
    int8_t playBeatNum                              = -1;
    int8_t playBeatMicroNum                         = -1;

    int16_t ownerOfFillTarget(uint8_t fillIdx) const {
        for (uint8_t i = 0; i < kBeatLibrarySize; i++) {
            if (beatLibrary[i].getActive() && beatLibrary[i].getFillTarget() == fillIdx) {
                return static_cast<int16_t>(i);
            }
        }
        return -1;
    }

    uint8_t findFreeFillSlot() const {
        for (uint8_t i = 0; i < kBeatFillLibrarySize; i++) {
            if (!beatFillLibrary[i].getActive() && ownerOfFillTarget(i) == -1) {
                return i;
            }
        }
        return NULL_FILL_TARGET;
    }

    void setBeat(uint8_t beatNum_, uint16_t startInterval_, uint16_t endInterval_) {
        Beat& beat = beatLibrary[beatNum_];
        beat.resetNote();  // a new beat: the slot may still hold another beat's note settings
        beat.setActive(true);
        beat.setStartInterval(startInterval_);
        beat.setEndInterval(endInterval_);
        beat.setFillType(0);

        uint8_t oldTarget = beat.getFillTarget();
        if (oldTarget != NULL_FILL_TARGET) {
            if (ownerOfFillTarget(oldTarget) == beatNum_) {
                beatFillLibrary[oldTarget].reset();
            }
        }

        beat.setFillTarget(NULL_FILL_TARGET);
        beat.setFillTime(0);
        beat.setFillLevel(0);
    }

    void resetBeat(uint8_t beatNum_) {
        Beat& beat = beatLibrary[beatNum_];
        uint8_t oldTarget = beat.getFillTarget();
        beat.resetNote();

        // ownerOfFillTarget only matches an active beat. Free the slot first.
        if (oldTarget != NULL_FILL_TARGET) {
            if (ownerOfFillTarget(oldTarget) == beatNum_) {
                beatFillLibrary[oldTarget].reset();
            }
        }

        beat.setActive(false);
        beat.setStartInterval(0);
        beat.setEndInterval(0);
        beat.setFillType(0);
        beat.setFillTarget(NULL_FILL_TARGET);
        beat.setFillTime(0);
        beat.setFillLevel(0);
    }

    uint8_t setBeatFill(uint8_t beatNum_, uint8_t fillType_, uint8_t fillPattern_, uint8_t fillTime_, uint8_t fillLevel_) {
        Beat& beat = beatLibrary[beatNum_];

        // Out-of-range values (e.g. a song saved by newer firmware with more fill types)
        // fall back to a plain beat instead of indexing past the fill tables.
        if ((fillType_ > kMaxFillType) || (fillPattern_ > kMaxFillPattern) || (fillTime_ > kMaxFillTime) || (fillLevel_ > kMaxFillLevel)) {
            fillType_ = 0;
            fillPattern_ = 0;
            fillTime_ = 0;
            fillLevel_ = 0;
        }

        if ((fillType_ == 0) && (fillPattern_ <= 2)) {
            uint8_t oldTarget = beat.getFillTarget();
            if (oldTarget != NULL_FILL_TARGET) {
                if (ownerOfFillTarget(oldTarget) == beatNum_) {
                    beatFillLibrary[oldTarget].reset();
                }
            }

            beat.setFillType(fillType_);
            beat.setFillPattern(fillPattern_);
            beat.setFillTarget(NULL_FILL_TARGET);
            beat.setFillTime(fillTime_);
            beat.setFillLevel(fillLevel_);
            return NULL_FILL_TARGET;
        }

        // Complex fill: ensure a beatFill slot exists
        uint8_t fillTarget_ = beat.getFillTarget();

        if (fillTarget_ == NULL_FILL_TARGET) {
            fillTarget_ = findFreeFillSlot();
        }

        if (fillTarget_ == NULL_FILL_TARGET) {
            // No free slot -> fall back to basic state
            beat.setFillType(0);
            beat.setFillPattern(0);
            beat.setFillTarget(NULL_FILL_TARGET);
            beat.setFillTime(0);
            beat.setFillLevel(0);
            return NULL_FILL_TARGET;
        }

        // Assign state to beat
        beat.setFillType(fillType_);
        beat.setFillPattern(fillPattern_);
        beat.setFillTarget(fillTarget_);
        beat.setFillTime(fillTime_);
        beat.setFillLevel(fillLevel_);

        // Initialize/refresh beatFill micro pattern
        beatFillLibrary[fillTarget_].set(beat.getStartInterval(), beat.getEndInterval(), fillType_, fillPattern_, fillTime_, fillLevel_);
        return fillTarget_;
    }

    void resetBeatFill(uint8_t beatNum_) {
        Beat& beat = beatLibrary[beatNum_];
        uint8_t tgt = beat.getFillTarget();

        if (tgt != NULL_FILL_TARGET) {
            if (ownerOfFillTarget(tgt) == beatNum_) {
                beatFillLibrary[tgt].reset();
            }
        }

        beat.setFillType(0);
        beat.setFillPattern(0);
        beat.setFillTime(0);
        beat.setFillLevel(0);
        beat.setFillTarget(NULL_FILL_TARGET);
    }
};

// clang-format on

#endif
