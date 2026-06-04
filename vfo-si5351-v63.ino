/**
 * @file    vfo-si5351-v63.ino
 * @brief   Main sketch: Si5351-based HF receiver VFO for Arduino (ATmega328).
 *
 * Application entry point and real-time control loop. Initialises the hardware
 * (rotary encoder with push button, four function buttons, 16x2 LCD, Si5351
 * clock generator), builds the settings menu, restores the saved state from
 * EEPROM and then runs the main event loop.
 *
 * Responsibilities:
 *  - Hardware setup: button pull-ups and debouncers, encoder pin-change
 *    interrupt, LCD, and Si5351 output drive strengths.
 *  - Factory reset: holding button A during power-on wipes the EEPROM and
 *    boots with compiled-in defaults.
 *  - Menu construction: operating mode, conversion type, 1st-LO injection
 *    side, IF1/IF2 frequencies, BFO centre, "BFO follows IF2", "LO2 high-side",
 *    "Invert sidebands" and Si5351 calibration.
 *  - Input handling, LCD refresh (~25 fps) and debounced EEPROM autosave.
 *  - Frequency planning in setFrequencies(): maps the dial, IFs and operating
 *    mode onto the three Si5351 clocks (CLK0 1st LO, CLK1 2nd LO, CLK2 BFO)
 *    according to the selected conversion architecture, and computes the net
 *    sideband-inversion parity across the active mixers.
 *
 * @author  Jarosław Marek Niewiński (current version and modifications)
 * @author  Documentation and implementation assistance: Claude (Anthropic AI)
 * @note    Based on the VFO project by Jan Ciger (Janoc):
 *          https://janoc.rd-h.com/archives/649
 * @note    The bundled Si5351 driver (si5351.h / si5351.cpp) is the Etherkit
 *          library by Jason Milldrum and Dana H. Myers, used unmodified.
 * @license GNU General Public License v3.0 (GPLv3)
 */

// Include the library code
#include <avr/interrupt.h>

#include <LiquidCrystal.h>
#include <Bounce2.h>
#include <Rotary.h>
#include "si5351.h"
#include "Wire.h"

#include "definitions.h"
#include "lcdui/lcdui.h"
#include "vfo-screen.h"
#include "ArduinoLCDApi.h"
#include "eeprom_storage.h"

Rotary encoder = Rotary(2, 3);           // Sets the pins the rotary encoder uses.  Must be interrupt pins.
LiquidCrystal lcd(13, 12, 8, 9, 10, 11); // LCD pins (rs, enable, d4,d5,d6,d7)
Si5351 si5351(Defs::SI5351Addr);
Bounce btnA, btnB, btnC, btnTX, btnEnc;
volatile int8_t encoder_delta = 0;  // written in ISR, read in main loop - must be volatile
TVFOState VFOState;

bool memory_dirty = false;
uint_fast32_t last_memory_save = millis();

void clampFreq(TVFOState &VFOState);
void setFrequencies(TVFOState &VFOState);
void update_output(TVFOState &VFOState);

// Main code //////////////////////////////////////////////////////////////////
void setup()
{
    // Init hw //////////////////////////////////////////////////////////////////

    // button pin setup
    pinMode(Defs::BtnEncoder, INPUT_PULLUP); // Connect to a button that goes to GND on push
    pinMode(Defs::BtnA, INPUT_PULLUP);
    pinMode(Defs::BtnB, INPUT_PULLUP);
    pinMode(Defs::BtnC, INPUT_PULLUP);
    pinMode(Defs::BtnTX, INPUT_PULLUP);

    digitalWrite(Defs::BtnEncoder, HIGH); // enable pull-ups
    digitalWrite(Defs::BtnA, HIGH);
    digitalWrite(Defs::BtnB, HIGH);
    digitalWrite(Defs::BtnC, HIGH);
    digitalWrite(Defs::BtnTX, HIGH);

    // encoder
    encoder.begin(true);        // enable encoder pull-ups

    // configure debouncers
    btnA.attach(Defs::BtnA);
    btnB.attach(Defs::BtnB);
    btnC.attach(Defs::BtnC);
    btnTX.attach(Defs::BtnTX);
    btnEnc.attach(Defs::BtnEncoder);

    btnA.interval(Defs::BtnDebounceInterval);
    btnB.interval(Defs::BtnDebounceInterval);
    btnC.interval(Defs::BtnDebounceInterval);
    btnTX.interval(Defs::BtnTXDebounceInterval);  // PTT/TX - longer to avoid RF feedback
    btnEnc.interval(Defs::BtnEncDebounceInterval); // encoder push - shorter for snappy feel

    // lcd setup
    lcd.begin(Defs::LCDWidth, Defs::LCDHeight);

    // Factory reset: hold button A while powering on / resetting the board.
    // Pull-ups are already enabled above; A pressed reads LOW.
    if (digitalRead(Defs::BtnA) == LOW)
    {
        clearStorage();

        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(F("EEPROM cleared"));
        lcd.setCursor(0, 1);
        lcd.print(F("Release button A"));

        // Wait until A is released so we don't immediately re-trigger anything,
        // then give the user a moment to read the message.
        while (digitalRead(Defs::BtnA) == LOW)
            ;
        delay(1000);
        lcd.clear();
    }

    // Load state from EEPROM first so we can use saved calibration in Si5351 init
    loadState(VFOState);

    // Si5351 - init with saved correction value (units: 0.01 Hz = correction_Hz * 100)
    si5351.init(SI5351_CRYSTAL_LOAD_8PF, 0, (int32_t)VFOState.si3531_correction * 100L);
    si5351.drive_strength(SI5351_CLK0, SI5351_DRIVE_8MA); // 1st LO
    si5351.drive_strength(SI5351_CLK1, SI5351_DRIVE_8MA); // 2nd LO (double conversion)
    si5351.drive_strength(SI5351_CLK2, SI5351_DRIVE_8MA); // BFO

    // All three outputs stay < 100 MHz (RX <= 30 MHz, IF1 <= 45 MHz),
    // so the shared PLLA (fixed 800 MHz) drives CLK0/CLK1/CLK2 without
    // re-tuning the PLL when CLK0 changes -> no need to split PLLs.

    // encoder interrupts
    PCICR |= (1 << PCIE2);
    PCMSK2 |= (1 << PCINT18) | (1 << PCINT19);
    sei();

    // Init menu //////////////////////////////////////////////////////////////
    TArduinoLCDApi lcd_api(lcd, Defs::LCDWidth, Defs::LCDHeight);

    auto mode_list_box = LCDUI::make_listbox(lcd_api, "Op mode", Defs::OperatingModeStr,
                                             [](uint8_t selected, const char *) {
                                                 VFOState.bfo_dirty = true;
                                                 VFOState.opmode = static_cast<Defs::OperatingMode>(selected);
                                             },
                                             static_cast<uint8_t>(VFOState.opmode));

    auto bfo_center_item = LCDUI::make_number(lcd_api, "BFO Freq. [Hz]", 9, VFOState.bfo_center,
                                              [](int32_t x) {
                                                  VFOState.bfo_dirty  = true;
                                                  VFOState.bfo_center = x;
                                              },
                                              Defs::FreqLimitDown, Defs::FreqLimitUp);

    auto conversion_list_box = LCDUI::make_listbox(lcd_api, "Conversion", Defs::ConversionTypeStr,
                                           [](uint8_t selected, const char *) {
                                               // switches RX architecture: direct / single / double
                                               // -> retunes all LOs and toggles CLK1/CLK2 as needed
                                               VFOState.vfo_dirty = true;
                                               VFOState.if_dirty  = true;
                                               VFOState.bfo_dirty = true;
                                               VFOState.conversion = static_cast<Defs::ConversionType>(selected);
                                           },
                                           static_cast<uint8_t>(VFOState.conversion));

    auto if_list_box = LCDUI::make_listbox(lcd_api, "1st LO side", Defs::IFModeStr,
                                           [](uint8_t selected, const char *) {
                                               // 1st-mixer injection side (single/double conversion)
                                               VFOState.vfo_dirty = true;
                                               VFOState.ifmode    = static_cast<Defs::IFMode>(selected);
                                           },
                                           static_cast<uint8_t>(VFOState.ifmode));

    // IF1 / IF2 use the MenuNumber decade editor:
    //   OK -> enter edit, encoder selects the digit (decade),
    //   OK again -> edit that digit (1 Hz ... 1e8 step), OK on top digit -> exit.
    auto if1_item = LCDUI::make_number(lcd_api, "IF1 Freq. [Hz]", Defs::IFEditWidth, VFOState.if1_freq,
                                       [](int32_t x) {
                                           VFOState.if_dirty = true; // retunes 1st LO + 2nd LO
                                           VFOState.if1_freq = x;
                                       },
                                       Defs::IF1LimitDown, Defs::IF1LimitUp);

    auto if_item = LCDUI::make_number(lcd_api, "IF2 Freq. [Hz]", Defs::IFEditWidth, VFOState.if_freq,
                                      [](int32_t x) {
                                          VFOState.if_dirty = true; // retunes 2nd LO + BFO
                                          VFOState.if_freq  = x;
                                          // Auto-track: keep BFO at the new crystal-filter center.
                                          if (VFOState.bfo_follow_if2)
                                          {
                                              VFOState.bfo_center = x;
                                              VFOState.bfo_dirty  = true;
                                          }
                                      },
                                      Defs::IF2LimitDown, Defs::IF2LimitUp);

    auto si5351corr_item = LCDUI::make_number(lcd_api, "Si5351 Cal [Hz]", 9, VFOState.si3531_correction,
                                      [](int32_t x) {
                                          VFOState.cal_dirty = true;
                                          VFOState.si3531_correction  = x;
                                      },
                                      Defs::Si5351CorrDown, Defs::Si5351CorrUp);

    auto bfo_follow_box = LCDUI::make_checkbox(lcd_api, "BFO follows IF2", VFOState.bfo_follow_if2,
                                               [](bool checked) {
                                                   // On enable, snap BFO center to current IF2.
                                                   if (checked)
                                                   {
                                                       VFOState.bfo_center = VFOState.if_freq;
                                                       VFOState.bfo_dirty  = true;
                                                   }
                                               });

    auto lo2_box = LCDUI::make_checkbox(lcd_api, "LO2 high-side", VFOState.lo2_highside,
                                        [](bool checked) {
                                            VFOState.if_dirty = true; // retune 2nd LO + re-eval sideband
                                            (void) checked;
                                        });

    auto swap_sidebands_box
        = LCDUI::make_checkbox(lcd_api, "Invert sidebands", VFOState.sidebands_swap, [](bool checked) {
              VFOState.vfo_dirty      = true;
              VFOState.sidebands_swap = checked;
          });

    auto vfo_screen = make_vfo_screen(lcd_api, "VFO", VFOState);
    vfo_screen.set_focus(true);

    auto menu = LCDUI::make_menu(vfo_screen, mode_list_box, conversion_list_box, if_list_box,
                                 if1_item, if_item, bfo_center_item, bfo_follow_box,
                                 lo2_box, swap_sidebands_box, si5351corr_item);

    menu.render(true);

    // Main loop //////////////////////////////////////////////////////////////
    while (1)
    {
        btnA.update();
        btnB.update();
        btnC.update();
        btnTX.update();
        btnEnc.update();

        LCDUI::InputEvent ev;
        memset(&ev, 0, sizeof(LCDUI::InputEvent));

        ev.buttons = (!btnEnc.read()) | (!btnA.read() << 1) | (!btnB.read() << 2) | (!btnC.read() << 3)
                     | (!btnTX.read() << 4);

        if (btnC.fell())
        {
            // jump in/out of the settings
            if (menu.current_idx() == 0)
            {
                menu.set_current_idx(1);
                vfo_screen.set_focus(false);
            } else
            {
                menu.set_current_idx(0);
                vfo_screen.set_focus(true);
            }

            menu.render(true); // force redraw
        }

        // hack - force update on TX change
        // because we may have RIT on
        if (btnTX.rose() || btnTX.fell())
        {
            VFOState.tx_active = !btnTX.read();
            VFOState.vfo_dirty = true;
        }

        // Atomically snapshot encoder_delta from ISR to avoid read-modify-write race.
        if (encoder_delta != 0)
        {
            cli();
            int8_t delta  = encoder_delta;
            encoder_delta = 0;
            sei();
            ev.ax = 0;
            ev.ay = delta;
        }

        if (ev.ax != 0 || ev.ay != 0 || btnA.rose() || btnA.fell() || btnB.rose() || btnB.fell()
            || btnC.rose() || btnC.fell() || btnTX.rose() || btnTX.fell() || btnEnc.rose() || btnEnc.fell())
        {
            menu.handle_input(ev);
        }

        update_output(VFOState);
        menu.render();

        // Flush shadow buffer to LCD hardware (~25 fps).
        static uint_fast32_t last_lcd_flush = 0;
        uint_fast32_t now = millis();
        if (now - last_lcd_flush >= 40)
        {
            last_lcd_flush = now;
            lcd_api.flush();
        }

        // save to eeprom
        if(memory_dirty && last_memory_save + 10000 < millis())
        {
            memory_dirty = false;
            storeState(VFOState);
        }
    }
}

void loop() {}

/**
 * @brief Apply any pending state changes to the Si5351 outputs.
 *
 * Checks the per-field "dirty" flags. If any frequency-related field changed,
 * the dial/RIT/BFO values are clamped to their valid ranges and the three
 * clocks are reprogrammed via setFrequencies(). A separate flag handles
 * Si5351 calibration changes. Any change also arms the debounced EEPROM
 * autosave by setting @c memory_dirty.
 *
 * @param VFOState  Current receiver state (modified in place).
 */
void update_output(TVFOState &VFOState)
{
    if (VFOState.bfo_dirty || VFOState.vfo_dirty || VFOState.rit_dirty || VFOState.if_dirty)
    {
        clampFreq(VFOState);
        memory_dirty = true;
        last_memory_save = millis();

        VFOState.bfo_dirty = false;
        VFOState.vfo_dirty = false;
        VFOState.rit_dirty = false;
        VFOState.if_dirty  = false;

        setFrequencies(VFOState);
    }

    if(VFOState.cal_dirty)
    {
        memory_dirty = true;
        last_memory_save = millis();
        VFOState.cal_dirty = false;

        si5351.set_correction(VFOState.si3531_correction*100LL, SI5351_PLL_INPUT_XO);
    }
}

/**
 * @brief Clamp the dial, RIT and BFO frequencies to their valid ranges.
 *
 * Keeps the dial within the HF receive window (Defs::FreqLimitDown ..
 * Defs::FreqLimitUp), limits RIT so that dial + RIT stays in range, and
 * clamps the computed BFO frequency to the same window.
 *
 * @param VFOState  Current receiver state (modified in place).
 */
void clampFreq(TVFOState &VFOState)
{
    // Clamp dial to RX range (0.1 - 30 MHz)
    if (VFOState.vfo_freq >= Defs::FreqLimitUp)
        VFOState.vfo_freq = Defs::FreqLimitUp;
    if (VFOState.vfo_freq <= Defs::FreqLimitDown)
        VFOState.vfo_freq = Defs::FreqLimitDown;

    // Correct RIT so dial+rit stays within range
    if (VFOState.vfo_freq + VFOState.rit > Defs::FreqLimitUp)
        VFOState.rit = Defs::FreqLimitUp - VFOState.vfo_freq;
    if (VFOState.vfo_freq + VFOState.rit < Defs::FreqLimitDown)
        VFOState.rit = Defs::FreqLimitDown - VFOState.vfo_freq;

    // Clamp BFO
    if (VFOState.bfo_freq > Defs::FreqLimitUp)
        VFOState.bfo_freq = Defs::FreqLimitUp;
    if (VFOState.bfo_freq < Defs::FreqLimitDown)
        VFOState.bfo_freq = Defs::FreqLimitDown;
}

/**
 * @brief Program the three Si5351 clocks for the current state.
 *
 * Maps the dial frequency, intermediate frequencies and operating mode onto
 * the Si5351 outputs according to the selected conversion architecture:
 *
 *  - DIRECT: CLK0 = dial; CLK1 (2nd LO) and CLK2 (BFO) are disabled. Audio is
 *    recovered directly at the single mixer (zero IF).
 *  - SINGLE: one mixer. CLK0 = dial +/- IF2 (side from @c ifmode), CLK1 off,
 *    CLK2 = BFO near the crystal filter (IF2).
 *  - DOUBLE: two mixers. CLK0 = dial +/- IF1 (side from @c ifmode),
 *    CLK1 = 2nd LO (IF1 +/- IF2, side from @c lo2_highside), CLK2 = BFO.
 *
 * The 2nd LO (CLK1) is only rewritten when its value actually changes, to
 * avoid audible clicks and needless I2C traffic while tuning. For SSB/CW the
 * BFO offset direction is derived from the net sideband-inversion parity of
 * the active high-side mixers, XORed with the manual "Invert sidebands" flag.
 * In AM the BFO is switched off.
 *
 * @param VFOState  Current receiver state; @c bfo_freq is updated in place.
 */
void setFrequencies(TVFOState &VFOState)
{
    int32_t dial = VFOState.vfo_freq;

    // RIT only on RX
    if (!VFOState.tx_active)
        dial += (int32_t)VFOState.rit;

    const bool direct = (VFOState.conversion == Defs::ConversionType::DIRECT);
    const bool single = (VFOState.conversion == Defs::ConversionType::SINGLE);
    const bool dbl    = (VFOState.conversion == Defs::ConversionType::DOUBLE);

    // 1st-mixer injection side (ignored in DIRECT). OFF behaves as low-side.
    const bool inj1_high = (VFOState.ifmode == Defs::IFMode::F_PLUS_IF);

    // --- CLK0: 1st LO ---
    // Use labs() not abs() - on AVR int is 16-bit, abs() would overflow!
    int32_t lo1;
    if (direct)
        lo1 = dial;                                         // direct conversion: LO = dial
    else if (single)
        lo1 = inj1_high ? dial + (int32_t)VFOState.if_freq  // single, high-side
                        : labs(dial - (int32_t)VFOState.if_freq); // single, low-side
    else if (inj1_high)
        lo1 = dial + (int32_t)VFOState.if1_freq;            // double, 1st LO high-side
    else
        lo1 = labs(dial - (int32_t)VFOState.if1_freq);      // double, 1st LO low-side

    si5351.set_freq((uint64_t)lo1 * 100ULL, SI5351_CLK0);

    // --- CLK1: 2nd LO (FIXED) - only used in DOUBLE conversion ---
    // Only (re)write when it actually changes -> no clicks while tuning the dial,
    // minimal I2C traffic.
    static int32_t s_last_lo2  = -1;
    static int8_t  s_last_clk1 = -1; // -1 unknown, 0 off, 1 on
    if (dbl)
    {
        int32_t lo2 = VFOState.lo2_highside
                      ? (int32_t)VFOState.if1_freq + (int32_t)VFOState.if_freq   // IF1 + IF2
                      : labs((int32_t)VFOState.if1_freq - (int32_t)VFOState.if_freq); // IF1 - IF2

        if (lo2 != s_last_lo2 || s_last_clk1 != 1)
        {
            si5351.output_enable(SI5351_CLK1, 1);
            si5351.set_freq((uint64_t)lo2 * 100ULL, SI5351_CLK1);
            s_last_lo2  = lo2;
            s_last_clk1 = 1;
        }
    }
    else
    {
        if (s_last_clk1 != 0)
        {
            si5351.output_enable(SI5351_CLK1, 0);
            s_last_clk1 = 0;
        }
    }

    // --- DIRECT conversion: no IF, no BFO. Disable 2nd LO + BFO and return. ---
    if (direct)
    {
        si5351.output_enable(SI5351_CLK2, 0); // BFO off (audio comes from the mixer)
        return;
    }

    // --- Sideband inversion parity across the active mixers ---
    // Each high-side mixer inverts the sideband; parity decides net inversion.
    uint8_t high_count = 0;
    if (single)
    {
        if (inj1_high) ++high_count;             // the single mixer
    }
    else // double
    {
        if (inj1_high)              ++high_count; // 1st mixer
        if (VFOState.lo2_highside)  ++high_count; // 2nd mixer
    }
    bool net_invert = high_count & 1;
    bool eff_swap   = VFOState.sidebands_swap ^ net_invert;

    // --- CLK2: BFO ---
    switch (VFOState.opmode)
    {
        case Defs::OperatingMode::AM:
            // AM: disable BFO only. CLK0 (and CLK1 in double conv) stay on.
            si5351.output_enable(SI5351_CLK2, 0);
            break;

        case Defs::OperatingMode::LSB:
            VFOState.bfo_freq = eff_swap
                ? VFOState.bfo_center - Defs::BFOSSBOffset
                : VFOState.bfo_center + Defs::BFOSSBOffset;
            si5351.output_enable(SI5351_CLK2, 1);
            si5351.set_freq((uint64_t)VFOState.bfo_freq * 100ULL, SI5351_CLK2);
            break;

        case Defs::OperatingMode::USB:
            VFOState.bfo_freq = eff_swap
                ? VFOState.bfo_center + Defs::BFOSSBOffset
                : VFOState.bfo_center - Defs::BFOSSBOffset;
            si5351.output_enable(SI5351_CLK2, 1);
            si5351.set_freq((uint64_t)VFOState.bfo_freq * 100ULL, SI5351_CLK2);
            break;

        case Defs::OperatingMode::CW:
            VFOState.bfo_freq = eff_swap
                ? VFOState.bfo_center + Defs::BFOCWOffset
                : VFOState.bfo_center - Defs::BFOCWOffset;
            si5351.output_enable(SI5351_CLK2, 1);
            si5351.set_freq((uint64_t)VFOState.bfo_freq * 100ULL, SI5351_CLK2);
            break;
    }
}


/**
 * @brief Pin-change interrupt service routine for the rotary encoder.
 *
 * Decodes the quadrature transition and accumulates the step count into the
 * volatile @c encoder_delta, which the main loop reads and clears atomically.
 * CW/CCW are intentionally swapped here to match the physical wiring.
 */
ISR(PCINT2_vect)
{
    unsigned char dir = encoder.process();

    switch (dir)
    {
        case DIR_NONE:
            break;

        case DIR_CCW: ++encoder_delta; break;  // swapped: encoder wired in reverse

        case DIR_CW: --encoder_delta; break;   // swapped: encoder wired in reverse
    }
}

int main()
{
    init();
    setup();

    for (;;)
        loop();
}