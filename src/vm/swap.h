#ifndef VM_SWAP_H
#define VM_SWAP_H
#include <stdint.h>
#include <stdbool.h>

void swap_init (void);
bool swap_in (void *frame, int slot_idx);
int swap_out (void *frame);
void read_from_disk (uint8_t *frame, int slot_idx);
void write_to_disk (uint8_t *frame, int slot_idx);
void free_slot (int slot_idx);

#endif /* vm/swap.h */
