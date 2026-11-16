#pragma once
// Card artwork selector. Define CARD_ART_BULBASAUR before this include (or edit the line below)
// to build the Bulbasaur card; the default is the ME card.
#include <stdint.h>
#define CARD_BG_SIZE 3040
// #define CARD_ART_BULBASAUR
#ifdef CARD_ART_BULBASAUR
#include "card_art_bulbasaur.h"
#else
#include "card_art_me.h"
#endif
#include "card_font.h"
