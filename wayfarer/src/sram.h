// Persists the 9 permanent, one-way progress flags (key collected,
// heart pickup collected, enemy defeated, game won, brute defeated,
// sword collected, shield collected, boss defeated, treasure chest
// collected) to real battery-backed cartridge RAM - not transient state
// (player position, current room, current hearts, current max hearts -
// the last of which world.c instead re-derives at boot by calling
// player_increase_max_hearts() once if the chest flag below is set).
// A fresh boot always starts at room (0,0) with full hearts and no
// sword/shield; a loaded save just means already-collected items stay
// collected and an already-won game shows the win screen again
// immediately. The original 8 flags fill _SRAM[1] completely; the
// chest flag (Milestone 17) is the first to need a second state byte,
// _SRAM[2]. See sram.c.

#ifndef WAYFARER_SRAM_H
#define WAYFARER_SRAM_H

#include <stdint.h>

void sram_init(void);

uint8_t sram_get_key_collected(void);
uint8_t sram_get_pickup_collected(void);
uint8_t sram_get_enemy_defeated(void);
uint8_t sram_get_won(void);
uint8_t sram_get_brute_defeated(void);
uint8_t sram_get_sword_collected(void);
uint8_t sram_get_shield_collected(void);
uint8_t sram_get_boss_defeated(void);
uint8_t sram_get_chest_collected(void);

// Each sets the corresponding flag and writes it to SRAM immediately -
// saved as soon as it happens, more crash-resilient than waiting for a
// designated checkpoint (same reasoning as the sibling prism/
// project's own highscore.c).
void sram_set_key_collected(void);
void sram_set_pickup_collected(void);
void sram_set_enemy_defeated(void);
void sram_set_won(void);
void sram_set_brute_defeated(void);
void sram_set_sword_collected(void);
void sram_set_shield_collected(void);
void sram_set_boss_defeated(void);
void sram_set_chest_collected(void);

// Wipes the persisted save back to "nothing collected, enemy alive,
// not won" - the same state a genuinely fresh cartridge already
// starts from. Used by world.c's restart_game() to escape a `won`
// save's own "always shows the win screen" behavior.
void sram_reset(void);

#endif
