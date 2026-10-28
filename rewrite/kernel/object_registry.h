#ifndef ICESTAR_KERNEL_OBJECT_REGISTRY_H
#define ICESTAR_KERNEL_OBJECT_REGISTRY_H
#include <stdint.h>

/* Original object names live at offset 0x10. Zero means no name match. */
uint32_t kernel_object_find_name(uint32_t name);
/* Original 0x127 field; returns the full, unvalidated state word. */
uint32_t kernel_object_set_state(uint32_t object, uint32_t state);
/* Signed live count/capacity; failure returns 0xffffffff. */
uint32_t kernel_object_registry_add(uint32_t object);
uint32_t kernel_object_registry_remove(uint32_t object);

#endif
