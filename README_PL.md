# VFO Si5351 — odbiornik KF z wybieraną przemianą częstotliwości

Sterownik syntezera **Si5351** dla odbiornika/transceivera krótkofalowego,
zbudowany na Arduino (ATmega328). Obsługuje wyświetlacz LCD 16×2, enkoder
obrotowy z przyciskiem oraz cztery przyciski funkcyjne. Konfiguracja jest
zapisywana w pamięci EEPROM i odtwarzana po włączeniu.

Odbiornik pracuje w trzech architekturach przemiany — **bezpośredniej**,
**pojedynczej** i **podwójnej** — wybieranych z menu. Trzy wyjścia zegarowe
Si5351 pełnią role: `CLK0` pierwszy geterodyna (przestrajany), `CLK1` drugi
heterodyna (stały, przy przemianie podwójnej) oraz `CLK2` BFO.

> 🇬🇧 English version: [`README.en.md`](README.en.md)

---

## Spis treści
- [Funkcje](#funkcje)
- [Architektura RF](#architektura-rf)
- [Plan częstotliwości](#plan-częstotliwości)
- [Typy przemiany](#typy-przemiany)
- [Inwersja wstęgi](#inwersja-wstęgi)
- [Sterowanie](#sterowanie)
- [Menu](#menu)
- [Edycja IF1/IF2 (dekadowa)](#edycja-if1if2-dekadowa)
- [Tryb „BFO podąża za IF2”](#tryb-bfo-podąża-za-if2)
- [Reset do ustawień fabrycznych](#reset-do-ustawień-fabrycznych)
- [Sprzęt](#sprzęt)
- [Struktura projektu](#struktura-projektu)
- [Mapa EEPROM](#mapa-eeprom)
- [Kompilacja](#kompilacja)
- [Uwagi sprzętowe](#uwagi-sprzętowe)
- [Autorzy i licencja](#autorzy-i-licencja)

---

## Funkcje
- Zakres odbioru **0,1 – 30 MHz** (KF)
- Wybierana **architektura przemiany**: bezpośrednia / pojedyncza / podwójna
- Trzy zegary Si5351: `CLK0` 1. heterodyna (przestrajana), `CLK1` 2. heterodyna
  (stała), `CLK2` BFO
- Tryby **AM / LSB / USB / CW**, **RIT**, strojenie krokowe i dekadowe
- **Edytowalne IF1 (1–45 MHz) i IF2 (455 kHz–12 MHz)** z menu
- **Auto-BFO** — BFO podąża za IF2 (opcjonalnie)
- Automatyczne wyznaczanie inwersji wstęgi z możliwością ręcznego odwrócenia
- EEPROM z **równoważeniem zużycia** (wear leveling) dla częstotliwości VFO
- Reset do ustawień fabrycznych z poziomu firmware (przycisk A przy starcie)
- Bezmigotaniowe odświeżanie LCD (renderowanie różnicowe)

---

## Architektura RF
```
               CLK0 (1. LO)       CLK1 (2. LO)        CLK2 (BFO)
               przestrajany       stały               stały
                   │                  │                   │
Antena ─► [Mikser 1] ─► 1. p.cz. ─► [Mikser 2] ─► 2. p.cz. ─► [Detektor] ─► Audio
           dial±IF1            IF1±IF2        filtr      iloczynowy
                                              kwarcowy
```
W przemianie podwójnej `CLK1` (2. heterodyna) realizuje konwersję w górę na
wysoką 1. p.cz. (≤45 MHz), a następnie konwersję w dół na filtr kwarcowy
(2. p.cz., 455 kHz–12 MHz). W przemianie pojedynczej działa tylko jeden mikser,
a w bezpośredniej sygnał audio powstaje wprost na jednym mikserze (zerowa p.cz.).

---

## Plan częstotliwości
$f_d$ = częstotliwość strojenia (dial), IF1 = 1. p.cz., IF2 = 2. p.cz.

| Wyjście | Funkcja | Wzór |
|---|---|---|
| CLK0 | 1. LO (przestrajany) | dial+IF1 (high) / \|dial−IF1\| (low) |
| CLK1 | 2. LO (stały) | IF1+IF2 (high) / \|IF1−IF2\| (low) |
| CLK2 | BFO | bfo_center ± offset |

Przy odbiorze ≤ 30 MHz i IF1 ≤ 45 MHz maksymalna częstotliwość wyjściowa wynosi
75 MHz (<100 MHz), więc wszystkie wyjścia mogą bezpiecznie współdzielić pętlę
PLLA bez przestrajania PLL podczas zmiany CLK0.

---

## Typy przemiany
Pozycja menu **Conversion** wybiera architekturę odbiornika; wybór jest
zapisywany w EEPROM (@18) i odtwarzany po starcie:

- **Direct** (bezpośrednia, zerowa p.cz.) — `CLK0` stroi wprost na
  częstotliwość odbieraną, `CLK1` (2. LO) i `CLK2` (BFO) są wyłączone, a sygnał
  audio powstaje bezpośrednio na mikserze. Ustawienia IF1/IF2/BFO są pomijane.
  Na ekranie VFO sygnalizowane jako `DC`.
- **Single** (pojedyncza) — jeden mikser: `CLK0` = dial ± IF2 (strona zależna
  od **1st LO side**), `CLK1` wyłączony, `CLK2` = BFO przy filtrze kwarcowym (IF2).
- **Double** (podwójna, domyślna) — dwa miksery: `CLK0` = dial ± IF1,
  `CLK1` = 2. LO (IF1 ± IF2, strona zależna od **LO2 high-side**),
  `CLK2` = BFO przy IF2.

Pozycja **1st LO side** wybiera stronę injekcji pierwszego miksera dla
przemiany pojedynczej i podwójnej (`F+IF` = high-side, `F-IF`/`OFF` = low-side);
nie ma znaczenia w przemianie bezpośredniej. Ekran VFO pokazuje odpowiednio
`+IF`, `-IF` lub `DC`.

---

## Inwersja wstęgi
Liczy się **parzystość** liczby mikserów pracujących na górnej wstędze (high-side):

| Mikser 1 | Mikser 2 | Suma | Inwersja |
|---|---|---|---|
| low | low | 0 | nie |
| high | low | 1 | tak |
| low | high | 1 | tak |
| high | high | 2 | nie |

Parzystość jest wyznaczana automatycznie i łączona operacją XOR z ustawieniem
**Invert sidebands**. Przy obu mikserach na górnej wstędze nie ma inwersji.

---

## Sterowanie
| Element | Funkcja (ekran VFO) |
|---|---|
| Enkoder (obrót) | strojenie / zmiana wartości |
| Enkoder (OK) | zmiana kroku / zatwierdzenie |
| A | strojenie dekadowe |
| B | RIT (ponowne naciśnięcie = zerowanie) |
| C | wejście/wyjście z menu ustawień |
| TX | nadawanie PTT (RIT wyłączony podczas TX) |

---

## Menu
| Pozycja | Opis | Zakres |
|---|---|---|
| VFO | ekran główny | 0,1–30 MHz |
| Op mode | tryb pracy | AM/LSB/USB/CW |
| Conversion | architektura przemiany | Direct / Single / Double |
| 1st LO side | strona injekcji 1. miksera | OFF / F-IF / F+IF |
| IF1 Freq. [Hz] | 1. p.cz. | 1–45 MHz |
| IF2 Freq. [Hz] | 2. p.cz. (filtr) | 455 kHz–12 MHz |
| BFO Freq. [Hz] | środek BFO | 0,1–30 MHz |
| BFO follows IF2 | automatyczne śledzenie BFO | tak/nie |
| LO2 high-side | strona injekcji 2. LO | tak/nie |
| Invert sidebands | ręczna inwersja wstęgi | tak/nie |
| Si5351 Cal [Hz] | kalibracja | −100k…+100k |

---

## Edycja IF1/IF2 (dekadowa)
Pozycje IF1 i IF2 używają edytora **cyfra po cyfrze** (`MenuNumber`, szerokość
9 dekad, `Defs::IFEditWidth`):

1. **OK** — wejście w edycję pola
2. **Obrót enkodera** — wybór cyfry (dekady) do edycji
3. **OK** — edycja wybranej cyfry; obrót zmienia ją o 1, 10, …, 10⁸ Hz
4. **OK** na najwyższej cyfrze — wyjście z pola

Daje to rozdzielczość **1 Hz** w całym zakresie obu p.cz.

---

## Tryb „BFO podąża za IF2”
Po zaznaczeniu pola **BFO follows IF2**:
- każda zmiana IF2 w menu ustawia również `bfo_center` = IF2,
- przy włączeniu BFO zostaje ustawiony na bieżącą wartość IF2.

Utrzymuje to BFO w środku filtra kwarcowego automatycznie — wygodne przy
zmianie filtra/IF2 bez ręcznej korekty BFO. Gdy tryb jest aktywny, ręczne
zmiany `BFO Freq.` zostaną nadpisane przy kolejnej zmianie IF2.

---

## Reset do ustawień fabrycznych
Przytrzymanie **przycisku A** podczas włączania (lub resetu) płytki czyści całą
pamięć EEPROM (poza 4 bajtami zarezerwowanymi dla licznika programowania
avrdude) i powoduje start z wartościami domyślnymi wkompilowanymi w firmware.
Na wyświetlaczu pojawia się komunikat `EEPROM cleared / Release button A`.
Świeże wartości domyślne są zapisywane do EEPROM przy pierwszej zmianie stanu.

---

## Sprzęt
| Funkcja | Wyprowadzenie |
|---|---|
| Enkoder A/B | D2/D3 |
| Przycisk enkodera | A3 |
| Przyciski A/B/C | A0/A1/A2 |
| TX (PTT) | D4 |
| LCD (RS,E,D4–D7) | D13,D12,D8,D9,D10,D11 |
| Si5351 I²C | A4/A5 |

Wyjścia: CLK0→1. mikser, CLK1→2. mikser, CLK2→detektor. Zalecane są filtry
pasmowo-przepustowe i bufory między stopniami.

---

## Struktura projektu
```
├── README.md / README.en.md   dokumentacja (PL / EN)
├── vfo-si5351-v63.ino          program główny, plan częstotliwości
├── definitions.h               konfiguracja, limity, wyprowadzenia
├── vfo-screen.h                ekran VFO + struktura TVFOState
├── eeprom_storage.cpp/.h       zapis w EEPROM (wear leveling dla VFO)
├── ArduinoLCDApi.cpp/.h        sterownik LCD (render różnicowy)
├── si5351.cpp/.h               biblioteka Si5351 (Etherkit)
└── lcdui/                      framework UI (CRTP, constexpr)
```

---

## Mapa EEPROM
`int_fast32_t` = 4 bajty na AVR. Częstotliwość VFO przechowywana jest w
obszarze z równoważeniem zużycia, za nagłówkiem o stałym układzie.

| Adres | Pole | Bajty |
|---|---|---|
| @0 | `bfo_center` | 4 |
| @4 | `if_freq` (IF2) | 4 |
| @8 | `si3531_correction` | 4 |
| @12 | kroki: `vfo_step_idx`<<4 \| `rit_step_idx` | 1 |
| @13 | tryby: opmode[1:0] ifmode[3:2] bfoFollow[4] swap[5] lo2_high[6] | 1 |
| @14 | `if1_freq` (IF1) | 4 |
| @18 | typ przemiany (0=Direct, 1=Single, 2=Double) | 1 |
| @19… | `vfo_freq` (obszar wear leveling) | reszta |

---

## Kompilacja
Biblioteki: `LiquidCrystal`, `Bounce2`, `Rotary`, `Wire`, `EEPROM`,
`si5351` (Etherkit). Płytka: ATmega328 (UNO / Nano / Pro Mini).

---

## Uwagi sprzętowe
- **Granica 100 MHz Si5351** — tutaj wszystkie wyjścia pozostają <100 MHz, więc
  podział PLL nie jest potrzebny. Przy rozszerzeniu na VHF przenieś CLK1/CLK2
  na PLLB.
- **Harmoniczne / obrazy** — Si5351 daje przebieg prostokątny; między stopniami
  wymagane są filtry pasmowo-przepustowe.
- **Szum fazowy** przestrajanej 1. heterodyny w trybie ułamkowym — dla lepszego
  zakresu dynamiki rozważ niższą IF1.
- **Bezwzględna wstęga** zależy od położenia pasma przepustowego filtra; ustaw
  `Invert sidebands` raz na żywym sygnale.
- **Zweryfikuj** każdą heterodynę częstościomierzem.

---

## Autorzy i licencja
Projekt VFO oparty jest na pracy **Jana Cigera (Janoc)**:
<https://janoc.rd-h.com/archives/649>

- **Autor obecnej wersji i zmian:** Jarosław Marek Niewiński
- **Wsparcie przy implementacji i dokumentacji:** Claude (asystent AI Anthropic)

Biblioteka **Si5351** (pliki `si5351.h` / `si5351.cpp`) © 2015–2019
Jason Milldrum, Dana H. Myers, użyta bez zmian.

Całość rozpowszechniana na licencji **GNU General Public License v3.0 (GPLv3)** —
zob. plik [`LICENSE`](LICENSE). Zachowaj informacje o autorach zgodnie z licencją.
