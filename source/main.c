#include <tonc.h>
#include "../VirtualPet48x32/src/vpet.h"
// https://www.coranac.com/man/tonclib/

// --------------------------------------------------------
// Vpet imports

void vpet_init();
void vpet_run();

uint16_t key_down = 0, key_new = 0, key_last = 0, key_new_or_repeat = 0;
int vpet_key_repeat = 0;

void init_game() {
	RandomSeed();

	vpet_init();
}
extern uint8_t vpet_screen_buffer[PET_SCREEN_W / 8][PET_SCREEN_H];
uint8_t vpet_screen_buffer_old[PET_SCREEN_W / 8][PET_SCREEN_H];

// --------------------------------------------------------

#include "lcd_grid_mask.h"
#include "background.h"

OBJ_ATTR obj_buffer[128];
OBJ_AFFINE *obj_aff_buffer = (OBJ_AFFINE*)obj_buffer;

#define FOREGROUND_SB_NUM 31
#define BACKGROUND_SB_NUM 30

#define FOREGROUND_CB_NUM 2
#define BACKGROUND_CB_NUM 0
#define FOREGROUND_CB_STARTING_TILE 176

void set_pixel(int x, int y) {
	x *= 5; y *= 5;
	int tile_number = FOREGROUND_CB_STARTING_TILE + (x/8) + (y/8)*30;

	for(int i=0; i<8; i++)
		tile_mem[FOREGROUND_CB_NUM][tile_number].data[y&7] |= 1<< ((x&7)*4);
}

void res_pixel(int x, int y) {
	x *= 5; y *= 5;
	int tile_number = FOREGROUND_CB_STARTING_TILE + (x/8) + (y/8)*30;

	for(int i=0; i<8; i++)
		tile_mem[FOREGROUND_CB_NUM][tile_number].data[y&7] &= ~(1<< ((x&7)*4));
}

void load_video(void) {
	// ----------------------------------------------------
	// Sprites

	// Load tiles and palette of sprite into video and palete RAM
	memcpy32(&tile_mem[4][0], lcd_grid_maskTiles, lcd_grid_maskTilesLen / 4);
	pal_obj_mem[1] = CLR_MONEYGREEN;
	//memcpy32(pal_obj_mem, metrPal, metrPalLen / 4);

	oam_init(obj_buffer, 128);

	for(int y=0; y<3; y++) {
		for(int x=0; x<4; x++) {
			OBJ_ATTR *metr = &obj_buffer[x+y*4];
			obj_set_attr(metr,
				ATTR0_SQUARE | ATTR0_WINDOW,  // Square, OBJ window sprite
				ATTR1_SIZE_64, // 64x64 pixels,
				ATTR2_PALBANK(0) | 0); // palette index 0, tile index 0

			// Set position
			obj_set_pos(metr, x*65, y*65);
		}
	}

	oam_copy(oam_mem, obj_buffer, 4*3);

	// ----------------------------------------------------
	// Backgrounds

	memcpy32(&tile_mem[BACKGROUND_CB_NUM][0], backgroundTiles, backgroundTilesLen/4);
	memcpy32(pal_bg_mem, backgroundPal, backgroundPalLen/4);

	for(int y=0; y<20; y++) {
		for(int x=0; x<30; x++) {
			se_mat[BACKGROUND_SB_NUM][y][x] = y*30+x;
			se_mat[FOREGROUND_SB_NUM][y][x] = y*30+x + FOREGROUND_CB_STARTING_TILE;
		}
	}

	memset32(&tile_mem[FOREGROUND_CB_NUM][FOREGROUND_CB_STARTING_TILE], 0x00000000, 30*20*32 / 4);

	// ----------------------------------------------------
	// Registers

	REG_DISPCNT = DCNT_MODE0 | DCNT_BG0 | DCNT_BG1 | DCNT_OBJ | DCNT_OBJ_1D | DCNT_WINOBJ;
	REG_BG0CNT = BG_PRIO(1) | BG_CBB(BACKGROUND_CB_NUM) | BG_SBB(BACKGROUND_SB_NUM) | BG_8BPP | BG_REG_32x32;
	REG_BG1CNT = BG_PRIO(0) | BG_CBB(FOREGROUND_CB_NUM) | BG_SBB(FOREGROUND_SB_NUM) | BG_4BPP | BG_REG_32x32 | BG_MOSAIC;
	REG_WINOUT = WINOUT_BUILD(WIN_BG0, WIN_BG0|WIN_BG1|WIN_BLD);
	REG_BLDCNT = BLD_BG0 | BLD_BLACK;
	REG_MOSAIC = MOS_BH(4) | MOS_BV(4);
	REG_BLDY   = 1;
}



int main(void)
{
	oam_init(obj_buffer, 128);

	irq_init(NULL);
	irq_enable(II_VBLANK);

	memset(vpet_screen_buffer_old, 0, sizeof(vpet_screen_buffer_old));
	init_game();

	load_video();

	while (1) {
		VBlankIntrWait();

		// Update sceen
		for(int y=0; y<PET_SCREEN_H; y++) {
			for(int x=0; x<PET_SCREEN_W/8; x++) {
				uint8_t pixels = vpet_screen_buffer[x][y];
				if(pixels != vpet_screen_buffer_old[x][y]) {
					vpet_screen_buffer_old[x][y] = pixels;

					for(int i=7; i>=0; i--) {
						if(pixels & (128>>i)) {
							set_pixel(x*8+i, y);
						} else {
							res_pixel(x*8+i, y);
						}
					}
				}
			}
		}

		// Update keys
		uint32_t gba_keys = ~REG_KEYINPUT;

		key_last = key_down;

		key_down = 0;
		if(gba_keys & (1<<KI_LEFT))
			key_down |= VPET_KEY_LEFT;
		if(gba_keys & (1<<KI_RIGHT))
			key_down |= VPET_KEY_RIGHT;
		if(gba_keys & (1<<KI_UP))
			key_down |= VPET_KEY_UP;
		if(gba_keys & (1<<KI_DOWN))
			key_down |= VPET_KEY_DOWN;
		if(gba_keys & (1<<KI_A))
			key_down |= VPET_KEY_A;
		if(gba_keys & (1<<KI_B))
			key_down |= VPET_KEY_B;
		if(gba_keys & (1<<KI_R))
			key_down |= VPET_KEY_C;
		if(gba_keys & (1<<KI_SELECT))
			key_down |= VPET_KEY_RESET;
		key_new = key_down & (~key_last);
		key_new_or_repeat = key_new;

		if(key_new & VPET_KEY_RESET)
			init_game();

		if((key_down&(VPET_KEY_LEFT|VPET_KEY_RIGHT|VPET_KEY_UP|VPET_KEY_DOWN)) ==
		   (key_last&(VPET_KEY_LEFT|VPET_KEY_RIGHT|VPET_KEY_UP|VPET_KEY_DOWN)) ) {
			vpet_key_repeat++;
			if(vpet_key_repeat > 15) {
				vpet_key_repeat = 12;
				key_new_or_repeat |= key_down & (VPET_KEY_LEFT|VPET_KEY_RIGHT|VPET_KEY_UP|VPET_KEY_DOWN);
			}
		} else {
			vpet_key_repeat = 0;
		}

		// Run for a frame
		vpet_run();
	}
}
