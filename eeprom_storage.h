/**
 * @file    eeprom_storage.h
 * @brief   Persistence of the receiver state in the on-chip EEPROM.
 *
 * Public interface for saving and restoring TVFOState. The fixed-layout header
 * (BFO centre, IF2, calibration, packed mode bits, IF1 and conversion type) is
 * written with EEPROM.update()/put() so unchanged bytes are not rewritten. The
 * frequently-changing dial frequency lives in a separate wear-levelled region
 * that cycles through the remaining cells to spread write wear.
 *
 * @author  Jarosław Marek Niewiński (current version and modifications)
 * @author  Documentation and implementation assistance: Claude (Anthropic AI)
 * @note    Based on the VFO project by Jan Ciger (Janoc):
 *          https://janoc.rd-h.com/archives/649
 * @license GNU General Public License v3.0 (GPLv3)
 */

#pragma once

#include <stdint.h>
struct TVFOState;

/**
 * @brief Persist the receiver state to EEPROM.
 *
 * Writes the fixed header and, if the dial frequency changed, appends a new
 * entry in the wear-levelled VFO region.
 *
 * @param state  State to store.
 */
void storeState(const TVFOState &state);

/**
 * @brief Restore the receiver state from EEPROM.
 *
 * If the EEPROM has never been programmed, the compiled-in defaults in
 * @p state are left untouched. Otherwise all persisted fields are loaded and
 * the latest dial frequency is recovered from the wear-levelled region.
 *
 * @param state  State to populate (modified in place).
 */
void loadState(TVFOState &state);

/**
 * @brief Erase the EEPROM so the next boot uses compiled-in defaults.
 *
 * Fills the usable area with 0xFF (leaving the 4 bytes reserved for avrdude's
 * flash programming counter) and resets the wear-levelling bookkeeping. The
 * all-ones pattern marks the EEPROM as "not programmed" for loadState().
 */
void clearStorage();
