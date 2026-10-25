#pragma once

#include <cstdint>
#include <iterator>
#include <string_view>

// The game's text language, settings.toml `language`. The Europe disc holds the game's text in
// every language below, indexed by one internal id the boot code stores at [0x35D900] from the
// console's language setting; that setting can only name English, French, German, Italian and
// Spanish, and the console build has no language screen of its own, so the rest were never
// reachable. src/game/hooks.cpp writes the chosen id where the game stores its own.
namespace language {

struct Choice {
    const char* key;   // settings.toml value
    const char* name;  // F1 panel, UTF-8 (the overlay font covers Latin-1)
    int id;            // the game's internal id, -1 the console's language (English)
};
inline constexpr Choice kChoices[] = {
    {"auto", "English (the console's language)", -1},
    {"en-gb", "English (UK)", 2},
    {"fr", "Fran\xC3\xA7" "ais", 3},
    {"de", "Deutsch", 4},
    {"it", "Italiano", 5},
    {"es", "Espa\xC3\xB1ol", 6},
    {"nl", "Nederlands", 7},
    {"da", "Dansk", 8},
    {"sv", "Svenska", 9},
    {"no", "Norsk", 10},
    {"fi", "Suomi", 11},
};

// index into kChoices; read once at boot by the hook, so a change takes a restart
inline uint32_t choice = 0;

inline int Id() { return choice < std::size(kChoices) ? kChoices[choice].id : -1; }

inline uint32_t Find(std::string_view key) {
    for (uint32_t i = 0; i < std::size(kChoices); i++)
        if (key == kChoices[i].key) return i;
    return 0;
}

}  // namespace language
