/********************************************************************
 * VMZip v1.0.0 (Aug/2005)
 * vmzip.cpp - coded by El Bucanero
 *
 * Copyright (C) 2005 Damian Parrino <bucanero@fibertel.com.ar>
 * http://www.bucanero.com.ar/
 *
 * Greetz to:
 * Marcus Comstedt for his great info & resources					<http://mc.pp.se/dc/>
 * Dan Potter for KallistiOS and other great libs like Tsunami		<http://gamedev.allusion.net/>
 * AndrewK / Napalm 2001 for the lovely DC load-ip/tool				<http://adk.napalm-x.com/>
 * Lawrence Sebald for the MinGW/msys cross compiler tutorial		<http://ljsdcdev.sunsite.dk/>
 *
 * and last, but not least, thanks to SEGA for my ALWAYS LOVED Dreamcast! =)
 *
 ********************************************************************/

#include <kos.h>
#include <png/png.h>
#include <zlib/zlib.h>
#include <bzlib/bzlib.h>
#include "lists.h"
#include "vmu_icon.h"
#include "buk_icon.h"
#include "s3mplay.h"
#include "screen.h"
#include "vmzip_icon.h"

// GUI constants
#define VMU_MAX_PAGE 2
#define GAMES_LINE_LENGTH 4
#define GAMES_MAX_PAGE 1
#define SAVES_MAX_PAGE 4
#define SELECTED_RGB "�\xFF\x01\x01"

int file_select(interface_t inter, menu_pos_t *menu);
void update_lists(interface_t inter);
void vm_zip(interface_t inter);
void vm_unzip(interface_t inter);

extern uint8 romdisk[];

KOS_INIT_FLAGS(INIT_DEFAULT | INIT_MALLOCSTATS);
KOS_INIT_ROMDISK(romdisk);

int file_select(interface_t inter, menu_pos_t *menu) {
	int done=0;
	uint64	timer = timer_ms_gettime64();

	while (!done) {
		update_lists(inter);
		draw_frame();
		MAPLE_FOREACH_BEGIN(MAPLE_FUNC_CONTROLLER, cont_state_t, t)
			if (((t->buttons & CONT_DPAD_UP) || (t->joyy < 0)) && (menu->pos >= menu->top) && (timer + 200 < timer_ms_gettime64())) {
				if (menu->pos > menu->top) {
					menu->pos--;
				} else {
					if ((menu->pos == menu->top) && (menu->top != 0)) {
						menu->top=menu->top - menu->maxpage;
						menu->pos--;
					}
				}
				timer = timer_ms_gettime64();
			}
			if (((t->buttons & CONT_DPAD_DOWN) || (t->joyy > 0)) && (menu->pos <= menu->top + menu->maxpage) && (timer + 200 < timer_ms_gettime64())) {
				if ((menu->pos < menu->top + menu->maxpage - 1) && (menu->pos < menu->total)) {
					menu->pos++;
				} else {
					if ((menu->pos == menu->top + menu->maxpage - 1) && (menu->top + menu->maxpage - 1 < menu->total)) {
						menu->top=menu->top + menu->maxpage;
						menu->pos++;
					}
				}
				timer = timer_ms_gettime64();
			}
			if ((t->ltrig > 0) && (menu->top != 0) && (timer + 200 < timer_ms_gettime64())) {
				menu->top=menu->top - menu->maxpage;
				menu->pos=menu->top;
				timer = timer_ms_gettime64();
			}
			if ((t->rtrig > 0) && (menu->top + menu->maxpage - 1 < menu->total) && (timer + 200 < timer_ms_gettime64())) {
				menu->top=menu->top + menu->maxpage;
				menu->pos=menu->top;
				timer = timer_ms_gettime64();
			}
			if ((t->buttons & CONT_A) && (timer + 200 < timer_ms_gettime64())) {
				if (inter.saves == NULL) {
					inter.save_pos->top=0;
					inter.save_pos->pos=0;
					inter.saves=(save_node_t *)malloc(sizeof(save_node_t));
					inter.save_pos->total=load_save_list(inter.saves, get_vmu_ptr(inter.src_vmu, inter.src_pos->pos));
					file_select(inter, inter.save_pos);
					free_save_list(inter.saves);
					inter.saves=NULL;
				} else {
					if (inter.dst_vmu == NULL) {
						inter.dst_pos->top=0;
						inter.dst_pos->pos=0;
						inter.dst_vmu=(vmu_node_t *)malloc(sizeof(vmu_node_t));
						inter.dst_pos->total=load_vmu_list(inter.dst_vmu);
						file_select(inter, inter.dst_pos);
						free_vmu_list(inter.dst_vmu);
						inter.dst_vmu=NULL;
					} else {
						if (get_save_ptr(inter.saves, inter.save_pos->pos)->vmzipped) {
							vm_unzip(inter);
						} else {
							vm_zip(inter);
						}
						done=1;
					}
				}
				timer = timer_ms_gettime64();
			}
			if ((t->buttons & CONT_B) && (timer + 200 < timer_ms_gettime64())) {
				done=1;
// DEBUG
//				menu->pos=-1;
// DEBUG
				timer = timer_ms_gettime64();
			}
			if ((t->buttons & CONT_Y) && (timer + 200 < timer_ms_gettime64())) {
				credits_scroll("/rd/cred_bg.png", "/rd/main_bg.png");
				timer = timer_ms_gettime64();
			}
			if ((t->buttons & CONT_X) && (timer + 200 < timer_ms_gettime64())) {
				if ((inter.saves != NULL) && (inter.dst_vmu == NULL)) {
					if (get_save_ptr(inter.saves, inter.save_pos->pos)->vmzipped) {
						vm_unzip(inter);
					} else {
						vm_zip(inter);
					}
				}
				timer = timer_ms_gettime64();
			}
		MAPLE_FOREACH_END()
	}
	return(menu->pos);
}

void update_lists(interface_t inter) {
	int		i=0;
	vmu_node_t *vaux=inter.src_vmu;
	vmu_node_t *daux=inter.dst_vmu;
	save_node_t	*saux=inter.saves;

	strcpy(games_lst, "");
	while ((vaux->next != NULL) && (i < inter.src_pos->top + inter.src_pos->maxpage)) {
		if (i >= inter.src_pos->top) {
			if (i == inter.src_pos->pos) strcat(games_lst, SELECTED_RGB);
			sprintf(vmu_info, "%*s\n", (GAMES_LINE_LENGTH + strlen(vaux->name))/2, vaux->name);
			strcat(games_lst, vmu_info);
		}
		i++;
		vaux=vaux->next;
	}
	strcpy(vmus_lst, "");
	if (daux != NULL) {
		i=0;
		while ((daux->next != NULL) && (i < inter.dst_pos->top + inter.dst_pos->maxpage)) {
			if (i >= inter.dst_pos->top) {
				if (i == inter.dst_pos->pos) strcat(vmus_lst, SELECTED_RGB);
				sprintf(vmu_info, "%c%c(%d)\n", daux->name[0]-32, daux->name[1], daux->free_blocks);
				strcat(vmus_lst, vmu_info);
			}
			i++;
			daux=daux->next;
		}
	}
	strcpy(saves_lst, "");
	strcpy(vmu_info, "");
	vmu_icon=NULL;
	if (saux != NULL) {
		i=0;
		strcpy(games_lst, "");
		while ((saux->next != NULL) && (i < inter.save_pos->top + inter.save_pos->maxpage)) {
			if (i >= inter.save_pos->top) {
				if (i == inter.save_pos->pos) {
					strcat(saves_lst, SELECTED_RGB);
					sprintf(vmu_info, "     File: %s - %d Block(s)\n     %s\n%s", saux->name, (saux->size / 512), saux->desc_short, saux->desc_long);
					vmu_icon=saux;
				}
				strcat(saves_lst, saux->name);
				strcat(saves_lst, "\n");
			}
			i++;
			saux=saux->next;
		}
	}
}

void vm_zip(interface_t inter) {
	save_node_t *sptr;
	vmu_node_t *vptr;
	uint64	timer;
	char tmp[64];
	char *dstbuf;
	void *srcbuf;
	unsigned int dstlen;
	vmu_pkg_t pkg;
	uint8 *pkg_out;
	uint8 zip_blocks;
	int	pkg_size, ext;
	file_t f;

	vmu_set_icon(buk_icon_xpm);
	sptr=get_save_ptr(inter.saves, inter.save_pos->pos);
	vptr=get_vmu_ptr(inter.src_vmu, inter.src_pos->pos);
	sprintf(tmp, "/vmu/%s/%s", vptr->name, sptr->name);
	fs_load(tmp, &srcbuf);
	dstlen=(sptr->size) * 2;
	dstbuf=(char *)malloc(dstlen);
	if (BZ2_bzBuffToBuffCompress(dstbuf, &dstlen, (char *)srcbuf, sptr->size, 1, 0, 0) == BZ_OK) {
		printf("Compression OK! %s - SRC: %d - DST: %d\n", tmp, sptr->size, dstlen);
	};

	strcpy(pkg.desc_short, sptr->name);
	strcpy(pkg.desc_long, "Bucanero's VMZip Compressed Save");
	sprintf(pkg.app_id, "%c Bucanero VMZip", sptr->size/512);
	pkg.icon_cnt = 1;
	pkg.icon_anim_speed = 0;
	memcpy(pkg.icon_pal, vmzip_icon_pal, 16*2);
	pkg.icon_data=(uint8 *)vmzip_icon_data;
	pkg.eyecatch_type = VMUPKG_EC_NONE;
	pkg.data_len = dstlen;
	pkg.data = (uint8 *)dstbuf;
	vmu_pkg_build(&pkg, &pkg_out, &pkg_size);
	zip_blocks=(pkg_size+511)/512;

	if (inter.dst_vmu != NULL) {
		vptr=get_vmu_ptr(inter.dst_vmu, inter.dst_pos->pos);
		if (vptr->free_blocks < zip_blocks) {
			sprintf(vmu_info, "     Error! Not enough free blocks!\n     Required: %d blocks", zip_blocks);
		} else {
			ext=get_free_vmzip_ext(vptr);
			sprintf(tmp, "/vmu/%s/VMZIPDAT.%03d", vptr->name, ext);
			f = fs_open(tmp, O_WRONLY);
			if (fs_write(f, pkg_out, pkg_size) == pkg_size) {
					sprintf(vmu_info, "     VMZIPDAT.%03d: %d blocks saved to VMU %c%c.\n", ext, zip_blocks, vptr->name[0]-32, vptr->name[1]);
				} else {
					sprintf(vmu_info, "     Copy Error!\n");
				}
			fs_close(f);
		}
	} else {
		sprintf(vmu_info, "     VMZip Compressed size: %d blocks", zip_blocks);
	}
	free(pkg_out);
	free(srcbuf);
	free(dstbuf);

	timer = timer_ms_gettime64();
	while (timer + 2000 > timer_ms_gettime64()) {
		draw_frame();
	}
	vmu_set_icon(vmu_icon_xpm);
}

void vm_unzip(interface_t inter) {
	save_node_t *sptr;
	vmu_node_t *vptr;
	uint64	timer;
	uint8 save_blocks;
	char tmp[64];
	char *dstbuf;
	void *srcbuf;
	unsigned int dstlen;
	vmu_pkg_t pkg;
	file_t f;

	vmu_set_icon(buk_icon_xpm);
	sptr=get_save_ptr(inter.saves, inter.save_pos->pos);
	vptr=get_vmu_ptr(inter.src_vmu, inter.src_pos->pos);
	sprintf(tmp, "/vmu/%s/%s", vptr->name, sptr->name);
	fs_load(tmp, &srcbuf);
	vmu_pkg_parse((uint8*)srcbuf, &pkg);
	save_blocks=pkg.app_id[0];
	dstlen=save_blocks*512;
	dstbuf=(char *)malloc(dstlen);
	if (BZ2_bzBuffToBuffDecompress(dstbuf, &dstlen, (char *)pkg.data, pkg.data_len, 1, 0) == BZ_OK) {
		printf("Decompression OK! %s - SRC: %d - DST: %d\n", tmp, pkg.data_len, dstlen);
	};

	if (inter.dst_vmu != NULL) {
		vptr=get_vmu_ptr(inter.dst_vmu, inter.dst_pos->pos);
		if (vptr->free_blocks < save_blocks) {
			sprintf(vmu_info, "     Error! Not enough free blocks!\n     Required: %d blocks", save_blocks);
		} else {
			sprintf(tmp, "/vmu/%s/%.12s", vptr->name, pkg.desc_short);
			fs_unlink(tmp);
			f = fs_open(tmp, O_WRONLY);
			if (fs_write(f, dstbuf, dstlen) == dstlen) {
				sprintf(vmu_info, "     %.12s: %d blocks saved to VMU %c%c.\n", pkg.desc_short, save_blocks, vptr->name[0]-32, vptr->name[1]);
			} else {
				sprintf(vmu_info, "     Copy Error!\n");
			}
			fs_close(f);
		}
	} else {
		sprintf(vmu_info, "     VMZip Decompressed size: %d blocks", save_blocks);
	}
	free(srcbuf);
	free(dstbuf);

	timer = timer_ms_gettime64();
	while (timer + 2000 > timer_ms_gettime64()) {
		draw_frame();
	}
	vmu_set_icon(vmu_icon_xpm);
}

int main(int argc, char **argv) {
	int done=0;
	interface_t interface;

	cont_btn_callback(0, CONT_START | CONT_A | CONT_B | CONT_X | CONT_Y,
		(void (*)(unsigned char, long  unsigned int))arch_exit);

	vmu_set_icon(buk_icon_xpm);
	splash_screen("/rd/logo_scr.gz", 270, 270);
	vmu_set_icon(vmu_icon_xpm);

	pvr_init_defaults();

    font_init("/rd/font.gz");
    back_init("/rd/main_bg.png");
	icon_init();

	play_s3m("/rd/haiku.s3m");

	interface.src_vmu=(vmu_node_t *)malloc(sizeof(vmu_node_t));
	interface.saves=NULL;
	interface.dst_vmu=NULL;

	interface.src_pos=(menu_pos_t *)malloc(sizeof(menu_pos_t));
	interface.src_pos->top=0;
	interface.src_pos->pos=0;
	interface.src_pos->total=load_vmu_list(interface.src_vmu);
	interface.src_pos->maxpage=GAMES_MAX_PAGE;

	interface.save_pos=(menu_pos_t *)malloc(sizeof(menu_pos_t));
	interface.save_pos->maxpage=SAVES_MAX_PAGE;

	interface.dst_pos=(menu_pos_t *)malloc(sizeof(menu_pos_t));
	interface.dst_pos->maxpage=VMU_MAX_PAGE;

	while (done != -1) {
		done=file_select(interface, interface.src_pos);
	}

	free_vmu_list(interface.src_vmu);
	free(interface.src_pos);
	free(interface.save_pos);
	free(interface.dst_pos);

	pvr_mem_free(back_tex);
	pvr_mem_free(font_tex);
	pvr_mem_free(icon_tex);

	return(0);
}
