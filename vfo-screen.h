/**
 * @file    vfo-screen.h
 * @brief   Receiver state (TVFOState) and the main VFO tuning screen.
 *
 * Declares TVFOState, the single structure holding the complete runtime state
 * of the receiver (frequencies, operating mode, conversion settings, tuning
 * steps, calibration and the per-field "dirty" flags that drive lazy updates),
 * and the VFOScreen menu item that renders the frequency display and handles
 * tuning input.
 *
 * The VFO screen shows the dial frequency with thousands separators, the
 * operating mode, and a short status tag: "TX" while transmitting, "DC" in
 * direct conversion, or "+IF"/"-IF" for the 1st-LO injection side in
 * single/double conversion. It supports step tuning, per-digit (decade)
 * tuning and RIT, selected with the encoder push and the A/B function buttons.
 *
 * @author  Jarosław Marek Niewiński (current version and modifications)
 * @author  Documentation and implementation assistance: Claude (Anthropic AI)
 * @note    Based on the VFO project by Jan Ciger (Janoc):
 *          https://janoc.rd-h.com/archives/649
 * @license GNU General Public License v3.0 (GPLv3)
 */

#pragma once

#include "Arduino.h"
#include "definitions.h"
#include "lcdui/lcdui.h"

/**
 * @brief Complete runtime state of the receiver.
 *
 * Holds every user-adjustable value plus a set of "dirty" flags. Setting a
 * dirty flag signals update_output() to recompute the affected clocks on the
 * next loop iteration, so the Si5351 is only reprogrammed when something
 * actually changes. The structure is serialised to EEPROM by the storage
 * module.
 */
struct TVFOState
{
    Defs::OperatingMode opmode = Defs::OperatingMode::LSB;
    // Receiver architecture: direct / single / double conversion.
    Defs::ConversionType conversion = Defs::DefaultConversion;
    // 1st-mixer injection side (used by SINGLE and DOUBLE conversion).
    Defs::IFMode ifmode        = Defs::IFMode::F_PLUS_IF;

    int_fast32_t vfo_freq = Defs::VFOFreq;   // dial frequency

    int_fast32_t if1_freq   = Defs::IF1Freq; // 1st IF (up-conversion), <= 45 MHz
    int_fast32_t if_freq    = Defs::IFFreq;  // 2nd IF = crystal filter
    int_fast32_t bfo_freq   = Defs::BFOFreq;
    int_fast32_t bfo_center = Defs::BFOFreq;

    int_fast16_t rit = Defs::Rit;

    uint_fast8_t vfo_step_idx = Defs::TuningIncrementIdx;
    uint_fast8_t rit_step_idx = Defs::RitIncrementIdx;

    int_fast32_t si3531_correction = Defs::SI5351Correction;

    bool sidebands_swap = Defs::SidebandsSwap;
    bool lo2_highside   = Defs::LO2HighSide;   // 2nd LO injection side
    bool bfo_follow_if2 = Defs::BfoFollowIF2;  // BFO center tracks IF2
    bool tx_active      = false;

    // ensures the first update
    bool vfo_dirty = true;
    bool bfo_dirty = true;
    bool if_dirty  = true;
    bool rit_dirty = true;
    bool cal_dirty = true;
};

/**
 * @brief Main tuning screen menu item.
 *
 * Renders the dial frequency, operating mode and status tag, and translates
 * encoder/button input into frequency changes. Three tuning sub-modes are
 * available — whole-step tuning, per-digit (decade) tuning and RIT — each with
 * its own adjustable increment.
 *
 * @tparam LCDApi  LCD abstraction type (see LCDUI::LCDApiBase).
 */
template <typename LCDApi> class VFOScreen : public LCDUI::MenuItemBase<LCDApi, VFOScreen<LCDApi>>
{
    using base = LCDUI::MenuItemBase<LCDApi, VFOScreen<LCDApi>>;

public:
    enum VFOScreenMode
    {
        StepTuning,
        ChangingStep,
        DecadeTuning,
        ChangingDecade,
        RitTuning,
        ChangingRitStep
    };

public:
    VFOScreen(LCDApi &api, const char *title, TVFOState &state)
        : base(api, title), m_state(state), m_mode(StepTuning), m_selected_decade(0)
    {
    }

    void render_impl()
    {
        if (!this->is_dirty())
            return;

        constexpr int BUF_LEN = 11;
        char buf[BUF_LEN];
        char disp_buf[BUF_LEN];

        memset(buf, 0, BUF_LEN);
        memset(disp_buf, 0, BUF_LEN);

        this->m_lcd_api.int2string(buf, BUF_LEN, m_state.vfo_freq);

        auto s         = buf + BUF_LEN - 2;
        auto t         = disp_buf + BUF_LEN - 2;
        int8_t d_count = 0;

        while (t >= disp_buf)
        {
            if (d_count == 3)
            {
                *(t--)  = '.';
                d_count = 0;
            }

            *(t--) = *(s--);
            ++d_count;
        }

        // first line
        this->m_lcd_api.clear();
        this->m_lcd_api.setCursorPosition(0, 0);

        this->m_lcd_api.drawText(disp_buf);
        this->m_lcd_api.drawText("Hz");

        if (m_state.tx_active)
            this->m_lcd_api.drawText(" TX");
        else
        {
            // Show the conversion architecture; for single/double also show the
            // 1st-mixer injection side. Direct conversion has no IF/LO side.
            switch (m_state.conversion)
            {
                case Defs::ConversionType::DIRECT:
                    this->m_lcd_api.drawText(" DC");
                    break;

                case Defs::ConversionType::SINGLE:
                case Defs::ConversionType::DOUBLE:
                    switch (m_state.ifmode)
                    {
                        case Defs::IFMode::F_PLUS_IF: this->m_lcd_api.drawText("+IF"); break;
                        case Defs::IFMode::F_MINUS_IF: this->m_lcd_api.drawText("-IF"); break;
                        case Defs::IFMode::OFF: this->m_lcd_api.drawText("-IF"); break;
                    }
                    break;
            }
        }

        // second line
        uint8_t p = this->m_lcd_api.width() - 4;
        this->m_lcd_api.setCursorPosition(p, 1);
        this->m_lcd_api.drawText(Defs::OperatingModeStr[static_cast<uint8_t>(m_state.opmode)]);

        this->m_lcd_api.setCursorPosition(0, 1);
        this->m_lcd_api.setCursorVisible(false);

        switch (m_mode)
        {
            case ChangingStep:
            {
                this->m_lcd_api.setCursorVisible(true);
                this->m_lcd_api.drawText("STP: ");
                this->m_lcd_api.drawText(Defs::TuningIncrements[m_state.vfo_step_idx]);
                break;
            }

            case StepTuning:
            {
                this->m_lcd_api.drawText("VFO: ");
                this->m_lcd_api.drawText(Defs::TuningIncrements[m_state.vfo_step_idx]);
                break;
            }

            case DecadeTuning:
            case ChangingDecade:
            {
                this->m_lcd_api.drawText("CUR: ");

                uint8_t c = 9 - m_selected_decade;
                if (m_selected_decade >= 3)
                    --c;

                if (m_selected_decade >= 6)
                    --c;

                this->m_lcd_api.setCursorPosition(c, 0);
                this->m_lcd_api.setCursorVisible(true);

                break;
            }

            case RitTuning:
            {
                this->m_lcd_api.drawText("RIT: ");
                this->m_lcd_api.drawText(m_state.rit);
                break;
            }

            case ChangingRitStep:
            {
                this->m_lcd_api.setCursorVisible(true);
                this->m_lcd_api.drawText("RST: ");
                this->m_lcd_api.drawText(Defs::RitIncrements[m_state.rit_step_idx]);
                break;
            }
        }
    }

    void handle_input_impl(const LCDUI::InputEvent &ev)
    {
        switch (m_mode)
        {
            case StepTuning:
            {
                if (ev.ay > 0)
                {
                    m_state.vfo_freq += Defs::TuningIncrements[m_state.vfo_step_idx];
                    m_state.vfo_dirty = true;
                }

                else if (ev.ay < 0)
                {
                    m_state.vfo_freq -= Defs::TuningIncrements[m_state.vfo_step_idx];
                    m_state.vfo_dirty = true;
                }

                if (ev.b_OK)
                    m_mode = ChangingStep;

                else if (ev.b_A)
                    m_mode = ChangingDecade;

                else if (ev.b_B)
                    m_mode = RitTuning;

                break;
            }

            case ChangingStep:
            {
                if (ev.ay > 0 && m_state.vfo_step_idx < Defs::TuningIncrements.size() - 1)
                    ++m_state.vfo_step_idx;

                else if (ev.ay < 0 && m_state.vfo_step_idx > 0)
                    --m_state.vfo_step_idx;

                if (ev.b_OK)
                    m_mode = StepTuning;

                break;
            }

            case DecadeTuning:
            {
                int_fast32_t delta = 1;
                for (uint8_t i = 0; i < m_selected_decade; ++i)
                    delta *= 10;

                if (ev.ay > 0)
                {
                    m_state.vfo_freq += delta;
                    m_state.vfo_dirty = true;
                } else if (ev.ay < 0)
                {
                    m_state.vfo_freq -= delta;
                    m_state.vfo_dirty = true;
                }

                if (ev.b_A)
                    m_mode = ChangingDecade;
                else if (ev.b_OK)
                    m_mode = StepTuning;

                break;
            }

            case ChangingDecade:
            {
                if (ev.ay < 0 && m_selected_decade < m_max_decade)
                    ++m_selected_decade;
                else if (ev.ay > 0 && m_selected_decade > 0)
                    --m_selected_decade;

                if (!ev.b_A)
                    m_mode = DecadeTuning;

                break;
            }

            case RitTuning:
            {
                if (ev.ay > 0)
                {
                    m_state.rit += Defs::RitIncrements[m_state.rit_step_idx];
                    m_state.rit_dirty = true;
                }

                else if (ev.ay < 0)
                {
                    m_state.rit -= Defs::RitIncrements[m_state.rit_step_idx];
                    m_state.rit_dirty = true;
                }

                if (ev.b_OK)
                    m_mode = ChangingRitStep;

                else if (ev.b_B)
                {
                    m_state.rit       = 0;
                    m_state.rit_dirty = true;
                    m_mode            = StepTuning;
                }

                break;
            }

            case ChangingRitStep:
            {
                if (ev.ay > 0 && m_state.rit_step_idx < Defs::RitIncrements.size() - 1)
                    ++m_state.rit_step_idx;

                else if (ev.ay < 0 && m_state.rit_step_idx > 0)
                    --m_state.rit_step_idx;

                if (ev.b_OK)
                    m_mode = RitTuning;

                break;
            }
        }

        this->repaint();
    }

protected:
    TVFOState &m_state;
    VFOScreenMode m_mode;
    uint8_t m_selected_decade;
    static constexpr uint8_t m_max_decade = 6;
};

template <typename LCDApi>
constexpr VFOScreen<LCDApi> make_vfo_screen(LCDApi &api, const char *title, TVFOState &state)
{
    return VFOScreen<LCDApi>(api, title, state);
}