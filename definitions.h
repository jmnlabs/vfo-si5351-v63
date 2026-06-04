/**
 * @file    definitions.h
 * @brief   Compile-time configuration: enums, default values, limits and pins.
 *
 * Central place for the radio's configuration. Defines the operating modes,
 * the receiver conversion architecture, the first-mixer injection side, the
 * default frequencies (dial, IF1, IF2, BFO), the tuning/RIT step tables, the
 * editing limits for the menu, and the hardware pin assignment. All values
 * live in the @c Defs namespace and are used as power-on defaults; the running
 * values are held in TVFOState and persisted in EEPROM.
 *
 * @author  Jarosław Marek Niewiński (current version and modifications)
 * @author  Documentation and implementation assistance: Claude (Anthropic AI)
 * @note    Based on the VFO project by Jan Ciger (Janoc):
 *          https://janoc.rd-h.com/archives/649
 * @license GNU General Public License v3.0 (GPLv3)
 */

#pragma once

#include <stdint.h>
#include <Arduino.h>

#include "lcdui/utilities.h"

namespace Defs
{
// Radio config ///////////////////////////////////////////////////////////////
enum class OperatingMode
{
    AM  = 0,
    LSB = 1,
    USB = 2,
    CW  = 3
};

// IFMode selects the first-mixer injection side (used by SINGLE and DOUBLE
// conversion). It has no effect in DIRECT conversion (no IF, no mixer LO).
//   OFF        -> 1st LO low-side  (CLK0 = |dial - IF|), same as F_MINUS_IF
//   F_MINUS_IF -> 1st LO low-side  (CLK0 = |dial - IF1|)
//   F_PLUS_IF  -> 1st LO high-side (CLK0 =  dial + IF1)
enum class IFMode
{
    OFF        = 0,
    F_MINUS_IF = 1,
    F_PLUS_IF  = 2
};

// ConversionType selects the receiver architecture:
//   DIRECT -> direct conversion (no IF). CLK0 tunes the dial directly,
//             CLK1 (2nd LO) and CLK2 (BFO) are off. Audio comes straight
//             out of the single mixer (zero IF).
//   SINGLE -> single conversion. One mixer: CLK0 = dial +/- IF2,
//             CLK1 off, CLK2 = BFO near the crystal filter (IF2).
//   DOUBLE -> double (dual) conversion. Two mixers: CLK0 = dial +/- IF1,
//             CLK1 = 2nd LO (IF1 +/- IF2), CLK2 = BFO near IF2.
enum class ConversionType
{
    DIRECT = 0,
    SINGLE = 1,
    DOUBLE = 2
};

constexpr auto OperatingModeStr  = Utilities::make_array("AM", "LSB", "USB", "CW");
constexpr auto IFModeStr         = Utilities::make_array("OFF", "F-IF", "F+IF");
constexpr auto ConversionTypeStr = Utilities::make_array("Direct", "Single", "Double");

// Default receiver architecture on a fresh EEPROM.
constexpr auto DefaultConversion = ConversionType::DOUBLE;

constexpr auto VFOFreq          = 7100000; // VFO starting frequency (dial)

// 1st IF (up-conversion roofing filter), e.g. 45 MHz
constexpr auto IF1Freq          = 45000000;

// 2nd IF = crystal / SSB filter centre frequency
// Yaesu filter
constexpr auto IFFreq           = 8987500; // 2nd IF starting frequency
constexpr auto BFOFreq          = 8987500; // BFO starting frequency (near 2nd IF)

// Crystal ladder filter
// constexpr auto IFFreq           = 11997450; // 2nd IF starting frequency
// constexpr auto BFOFreq          = 11997450; // BFO starting frequency

// Default 2nd-LO injection side (CLK1 = IF1 +/- IF2)
//   true  -> high-side: LO2 = IF1 + IF2
//   false -> low-side : LO2 = IF1 - IF2
// With both mixers high-side there is no net sideband inversion.
constexpr bool LO2HighSide      = true;

// When true, BFO center automatically tracks the 2nd IF (crystal filter)
// frequency: editing IF2 in the menu also moves bfo_center to the new IF2.
constexpr bool BfoFollowIF2     = false;

constexpr auto BFOSSBOffset     = 1250;
constexpr auto BFOCWOffset      = 600;

constexpr auto TuningIncrements = Utilities::make_array(100000, 10, 50,
                                                        100, 500, 1000,
                                                        2500, 5000, 10000); // need the 100000 first to set the type
constexpr auto RitIncrements = Utilities::make_array(10, 100, 1000, 1);

constexpr auto TuningIncrementIdx = 4;
constexpr auto RitIncrementIdx    = 0;
constexpr auto Rit                = 0;

constexpr auto SidebandsSwap      = false;

// RX dial limits: 0.1 - 30 MHz (HF receiver)
constexpr int_fast32_t FreqLimitUp   = 30e6;
constexpr int_fast32_t FreqLimitDown = 1e5;

// Menu editing limits for the two IFs
constexpr int_fast32_t IF1LimitDown =  1000000; // 1   MHz
constexpr int_fast32_t IF1LimitUp   = 45000000; // 45  MHz  (max 1st IF)
constexpr int_fast32_t IF2LimitDown =   455000; // 455 kHz
constexpr int_fast32_t IF2LimitUp   = 12000000; // 12  MHz

// Decade-editor field width (digits) for IF1 / IF2 menu items.
// 9 digits covers up to 999,999,999 Hz with 1 Hz resolution per decade.
constexpr uint8_t IFEditWidth     = 9;

const int_fast32_t Si5351CorrUp   =  100000;
const int_fast32_t Si5351CorrDown = -100000;

// HW config //////////////////////////////////////////////////////////////////
constexpr auto BtnA       = A0;
constexpr auto BtnB       = A1;
constexpr auto BtnC       = A2;
constexpr auto BtnTX      = 4;
constexpr auto BtnEncoder = A3;

constexpr auto BtnDebounceInterval    = 25;   // A/B/C buttons [ms]
constexpr auto BtnEncDebounceInterval = 20;   // encoder push - snappier [ms]
constexpr auto BtnTXDebounceInterval  = 50;   // PTT/TX - longer, RF environment [ms]

// Base I2C address for the Si5351
constexpr auto SI5351Addr       = 0x60;
constexpr auto SI5351Correction = 455;

// globals
constexpr uint8_t LCDWidth  = 16;
constexpr uint8_t LCDHeight =  2;
};