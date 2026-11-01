#ifndef OPENICESTAR_KERNEL_WINDOW_SERVICES_H
#define OPENICESTAR_KERNEL_WINDOW_SERVICES_H
#include <stdint.h>

void kernel_ui_mailbox_put_802e64(volatile uint32_t text);
void kernel_ui_mailbox_get_802ec6(volatile uint32_t flag,volatile uint32_t target);
void kernel_ui_launch_path_802ef2(volatile uint32_t text);
void kernel_ui_launch_relative_802f42(volatile uint32_t text);
void kernel_ui_frame_807302(volatile uint32_t id,volatile uint32_t x,volatile uint32_t y,volatile uint32_t width,volatile uint32_t height,volatile uint32_t color);
void kernel_ui_caption_807540(volatile uint32_t id,volatile uint32_t active);
uint32_t kernel_ui_title_hit_80762e(volatile uint32_t id,volatile uint32_t x,volatile uint32_t y);
uint32_t kernel_ui_client_hit_8076d0(volatile uint32_t id,volatile uint32_t x,volatile uint32_t y);
uint32_t kernel_ui_surface_hit_80784a(volatile uint32_t id,volatile uint32_t x,volatile uint32_t y);
uint32_t kernel_ui_find_hit_8078f2(volatile uint32_t x,volatile uint32_t y);
uint32_t kernel_ui_object_slot_807990(void);
void kernel_ui_delete_807f72(volatile uint32_t id);
void kernel_ui_icon_file_807ffe(volatile uint32_t id,volatile uint32_t path,volatile uint32_t flags);
void kernel_ui_icon_pointer_808090(volatile uint32_t id,volatile uint32_t icon,volatile uint32_t flags);
void kernel_ui_restore_8080ea(volatile uint32_t id);
void kernel_ui_hide_8081ea(volatile uint32_t id);
void kernel_ui_show_8082c6(volatile uint32_t id);
void kernel_ui_remove_808378(volatile uint32_t id);
void kernel_ui_event4_809e24(volatile uint32_t id,volatile uint32_t a,volatile uint32_t b,volatile uint32_t c);
void kernel_ui_event3_80ac4e(volatile uint32_t id,volatile uint32_t a,volatile uint32_t b);
void kernel_ui_move_80ac9c(volatile uint32_t id,volatile uint32_t dx,volatile uint32_t dy);
void kernel_ui_event3b_80ecba(volatile uint32_t id,volatile uint32_t a,volatile uint32_t b);
void kernel_ui_event3c_80ed08(volatile uint32_t id,volatile uint32_t a,volatile uint32_t b);
void kernel_ui_redraw_all_80fb12(void);
void kernel_ui_minimize_81002e(volatile uint32_t id);
void kernel_ui_close_api_810168(uint32_t id);
void kernel_ui_dialog_click_811c46(volatile uint32_t id,volatile uint32_t x,volatile uint32_t y);
void kernel_ui_double_border_811e4c(volatile uint32_t id,volatile uint32_t x0,volatile uint32_t y0,volatile uint32_t x1,volatile uint32_t y1);
uint32_t kernel_ui_image_slot_81215a(void);
void kernel_ui_image_fill_812748(volatile uint32_t id,volatile uint32_t color);
void kernel_ui_image_release_api_812bfa(uint32_t id);

#endif
