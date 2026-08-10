// "Wayfarer" (working title) - a new, separate homebrew game from the
// sibling prism/ project (a match-3 puzzle game): a top-down
// action-adventure in the tradition of the original Zelda (1986) -
// free pixel-level movement around bordered rooms, viewed from above.
// See docs/GAMEBOY_ROADMAP.md's own entry for the whole milestone
// roadmap; this is Milestone 1 only - a player sprite that walks
// around one static bordered room with wall collision. No room
// transitions, combat, items, or HUD yet - all explicitly deferred to
// later milestones, not silently missing.
//
// See room.h/room.c for the room's tile/palette data and wall bounds,
// player.h/player.c for the directional sprite and movement/collision.

#include <gb/gb.h>
#include <gb/cgb.h>
#include <stdint.h>
#include <rand.h>

#include "player.h"
#include "room.h"

void main(void) {
    // Seeded now even though nothing's randomized yet this milestone -
    // matches prism/'s own initrand(DIV_REG)-as-first-line convention,
    // already in place for whenever a later milestone needs real
    // randomness (e.g. enemy spawn/behavior).
    initrand(DIV_REG);

    // Real CGB hardware's boot ROM leaves the LCD *on* (LCDC=$91) with
    // every background palette color white before handing off control
    // (pandocs' Power_Up_Sequence.md) - disabling the display before
    // any bulk VRAM/tilemap/palette write below avoids a real,
    // well-known GB homebrew gotcha (a live LCD catching a write
    // mid-flight, producing a torn frame) that the sibling prism/
    // project's own title.c hit and documented
    // (docs/GAMEBOY_ROADMAP.md's corrected "PPU rendering-catch-up
    // quirk" entry) - same real-hardware-safe (VBlank-gated)
    // LCD-disable pattern applied here from the very start.
    wait_vbl_done();
    DISPLAY_OFF;

    room_init();
    player_init(); // also enables SHOW_SPRITES

    SHOW_BKG;
    DISPLAY_ON;

    while (1) {
        uint8_t joy = joypad();
        player_update(joy);
        vsync();
    }
}
