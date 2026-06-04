/**
 * @file    eeprom_storage.cpp
 * @brief   EEPROM persistence with wear levelling for the dial frequency.
 *
 * Implements loading, storing and clearing of TVFOState. The header fields sit
 * at fixed addresses (see the layout map below). The dial frequency, which
 * changes most often, is stored in a circular wear-levelled region: each save
 * advances to the next cell and a sentinel bit (the MSB of the stored value)
 * flips on wrap-around, so findLatestVFO() can identify the most recent entry.
 *
 * @author  Jarosław Marek Niewiński (current version and modifications)
 * @author  Documentation and implementation assistance: Claude (Anthropic AI)
 * @note    Based on the VFO project by Jan Ciger (Janoc):
 *          https://janoc.rd-h.com/archives/649
 * @license GNU General Public License v3.0 (GPLv3)
 */

#include <EEPROM.h>
#include "eeprom_storage.h"
#include "definitions.h"
#include "vfo-screen.h"

// keep 4 for avrdude's flash programming count
#define EEPROM_TOP (EEPROM.length() - 4)

// EEPROM layout (int_fast32_t = 4 bytes on AVR):
//   @0  bfo_center        (4)
//   @4  if_freq  / IF2    (4)
//   @8  si3531_correction (4)
//   @12 steps byte        (1)  vfo_step_idx<<4 | rit_step_idx
//   @13 modes byte        (1)  opmode[1:0] ifmode[3:2] bfoFollow[4] swap[5] lo2_high[6]
//   @14 if1_freq / IF1    (4)
//   @18 conversion byte   (1)  ConversionType (0=Direct,1=Single,2=Double)
//   @19 .. VFO wear-leveling region
static constexpr uint16_t VFO_START_ADDR = 19;
static uint16_t current_vfo_offset       = VFO_START_ADDR;
static int_fast32_t current_vfo_freq     = 0;
static bool current_sentinel             = false;

/// @brief Find the most recent dial frequency in the wear-levelled region.
/// @param[out] vfo   Latest stored dial frequency (sentinel bit cleared).
/// @param[out] next  Offset of the next free cell to write.
void findLatestVFO(int_fast32_t &vfo, uint16_t &next);

/// @brief Append a dial frequency to the wear-levelled region.
static void storeVFO(int_fast32_t vfo_freq);

void clearStorage()
{
    // Write 0xFF to the whole usable area. This leaves bfo_center (@0) as a
    // negative value (sign bit set), which loadState() treats as "EEPROM not
    // programmed yet" -> compiled-in defaults are kept on next boot.
    for(uint16_t addr = 0; addr < EEPROM_TOP; ++addr)
        EEPROM.update(addr, 0xFF);

    // Reset wear-leveling bookkeeping so a subsequent store starts cleanly.
    current_vfo_offset = VFO_START_ADDR;
    current_vfo_freq   = 0;
    current_sentinel   = false;
}

/// @brief Read the sentinel bit (MSB) used to mark the wear-levelling generation.
template<typename T> constexpr bool getSentinel(T value)
{
    return 0x01 & (value >> (8*sizeof(T) - 1));
}

/// @brief Set or clear the sentinel bit (MSB) of a stored value.
int_fast32_t setSentinel(int_fast32_t value, bool sentinel)
{
    uint_fast32_t mask = ((uint_fast32_t)1 << (8*sizeof(int_fast32_t) - 1));
    if(sentinel)
        return value | mask;
    else
        return value & (~mask);
}

void loadState(TVFOState &state)
{
    uint16_t addr     = 0;
    int_fast32_t freq = 0;
    uint8_t tmp       = 0;

    EEPROM.get(addr, freq); addr += sizeof(int_fast32_t);   // bfo_center @0
    // check that the EEPROM is actually programmed
    if(freq < 0)
        return; // EEPROM not programmed yet -> keep compiled-in defaults

    state.bfo_center = freq;
    state.bfo_dirty  = true;

    // IF2 (crystal filter)
    EEPROM.get(addr, state.if_freq); addr += sizeof(int_fast32_t);          // @4

    // Si5351 frequency correction
    EEPROM.get(addr, state.si3531_correction); addr += sizeof(int_fast32_t); // @8

    // Tuning steps
    EEPROM.get(addr++, tmp);                                                  // @12
    state.rit_step_idx = tmp & 0x0F;
    state.vfo_step_idx = (tmp & 0xF0) >> 4;

    // Opmodes
    EEPROM.get(addr++, tmp);                                                  // @13
    state.opmode         = (Defs::OperatingMode) (0x03 & tmp);
    state.ifmode         = (Defs::IFMode) (0x03 & (tmp >> 2));
    state.bfo_follow_if2 = ((1 << 4) & tmp);
    state.sidebands_swap = ((1 << 5) & tmp);
    state.lo2_highside   = ((1 << 6) & tmp);

    // IF1 (up-conversion)
    EEPROM.get(addr, state.if1_freq); addr += sizeof(int_fast32_t);           // @14

    // Conversion architecture (direct / single / double)            // @18
    EEPROM.get(addr++, tmp);
    if(tmp <= (uint8_t) Defs::ConversionType::DOUBLE)
        state.conversion = (Defs::ConversionType) tmp;
    // else: leave compiled-in default (handles 0xFF on a partly-written cell)

    // VFO (wear-leveled region)
    EEPROM.get(VFO_START_ADDR, current_vfo_freq);
    current_sentinel = getSentinel(current_vfo_freq);

    findLatestVFO(current_vfo_freq, current_vfo_offset);
    state.vfo_freq = current_vfo_freq;

    state.bfo_dirty = true;
    state.vfo_dirty = true;
    state.rit_dirty = true;
    state.if_dirty  = true;
}

void storeState(const TVFOState &state)
{
    uint16_t addr = 0;

    // put()/update() only write when the value actually differs, so no need
    // to guard against EEPROM wear here. The only exception is VFO freq
    // because of the wear leveling used.

    // BFO freq
    EEPROM.put(addr, state.bfo_center);
    addr += sizeof(int_fast32_t);

    // IF2 freq
    EEPROM.put(addr, state.if_freq);
    addr += sizeof(int_fast32_t);

    // Si5351 frequency correction
    EEPROM.put(addr, state.si3531_correction); addr += sizeof(int_fast32_t);

    // Tuning steps
    uint8_t steps = ((0x0F & state.vfo_step_idx) << 4) | (0x0F & state.rit_step_idx);
    EEPROM.update(addr++, steps);

    // Opmodes
    uint8_t modes = ((0x03 & (uint8_t) state.opmode) |
                     ((0x03 & (uint8_t) state.ifmode) << 2) |
                     ((state.bfo_follow_if2 ? 1 : 0) << 4) |
                     ((state.sidebands_swap ? 1 : 0) << 5) |
                     ((state.lo2_highside   ? 1 : 0) << 6));
    EEPROM.update(addr++, modes);

    // IF1 freq
    EEPROM.put(addr, state.if1_freq); addr += sizeof(int_fast32_t);

    // Conversion architecture (direct / single / double)
    EEPROM.update(addr++, (uint8_t) state.conversion);

    if(current_vfo_freq != state.vfo_freq)
        storeVFO(state.vfo_freq);
}

void storeVFO(int_fast32_t vfo_freq)
{
    int_fast32_t sentinel_val = setSentinel(vfo_freq, current_sentinel);
    EEPROM.put(current_vfo_offset, sentinel_val);

    if(current_vfo_offset + sizeof(int_fast32_t) < EEPROM_TOP) // 4 for avrdude flash counter
        current_vfo_offset += sizeof(int_fast32_t);
    else
    {
        current_vfo_offset = VFO_START_ADDR;
        current_sentinel = !current_sentinel;
    }
}

void findLatestVFO(int_fast32_t &vfo, uint16_t &next)
{
    int_fast32_t freq, tmp;
    uint16_t addr = VFO_START_ADDR;

    EEPROM.get(addr, freq);
    bool sentinel = getSentinel(freq);

    addr += sizeof(int_fast32_t);
    while(addr < EEPROM_TOP)
    {
        EEPROM.get(addr, tmp);
        if(getSentinel(tmp) != sentinel)
            break;
        else
            freq = tmp;

        addr += sizeof(int_fast32_t);
    }

    vfo  = freq & 0x7FFFFFFF;
    next = (addr < EEPROM_TOP) ? addr : VFO_START_ADDR;
}