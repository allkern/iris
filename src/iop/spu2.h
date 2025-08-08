#ifndef SPU2_H
#define SPU2_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "scheduler.h"
#include "intc.h"
#include "dma.h"

#define SPU2_RAM_SIZE 0x100000 // 2 MB

/* Memory ranges:
    1f900000-1f90017f CORE0 Voice settings
    1f900180-1f9001b1 CORE0 Common settings
    1f9001c0-1f9002df CORE0 Voice address settings
    1f9002e0-1f900347 CORE0 Misc. settings
    1f900400-1f90057f CORE1 Voice settings
    1f900580-1f9005b1 CORE1 Common settings
    1f9005c0-1f9006df CORE1 Voice address settings
    1f9006e0-1f900747 CORE1 Reverb/Misc. settings
    1f900760-1f900787 CORE0 Volume/Reverb settings
    1f900788-1f9007af CORE1 Volume/Reverb settings
    1f9007c0-1f9007cf S/PDIF settings

   Breakdown:
    1f900000-1f90017f CORE0 Voice settings:
        1f900XX0 Voice X (0-23) VOLL                  (S) voice volume (left)
        1f900XX2 Voice X (0-23) VOLR                  (S) voice volume (right)
        1f900XX4 Voice X (0-23) PITCH                 (S) voice pitch
        1f900XX6 Voice X (0-23) ADSR1                 (S) voice envelope (AR, DR, SL)
        1f900XX8 Voice X (0-23) ADSR2                 (S) voice envelope (SR, RR)
        1f900XXa Voice X (0-23) ENVX                  (S) voice envelope (current value)
        1f900XXc Voice X (0-23) VOLXL                 (S) voice volume (current value left)
        1f900XXe Voice X (0-23) VOLXR                 (S) voice volume (current value right)
    1f900180-1f9001b1 CORE0 Common settings
        1f900180 PMON                                 (P) pitch modulation on
        1f900184 NON                                  (P) noise generator on
        1f900188 VMIXL                                (P) voice output mixing (dry left)
        1f90018C VMIXEL                               (P) voice output mixing (wet left)
        1f900190 VMIXR                                (P) voice output mixing (dry right)
        1f900194 VMIXER                               (P) voice output mixing (wet right)
        1f900198 MMIX                                 (P) output type after voice mixing
        1f90019A ATTR                                 (P) core attributes
        1f90019C IRQA                                 (P) IRQ address 
        1f9001A0 KON                                  (P) key on (start voice sound generation)
        1f9001A4 KOFF                                 (P) key off (end voice sound generation)
        1f9001A8 TSA                                  (P) DMA transfer start address
        1f9001AC DATA                                 (S) DMA data register
        1f9001AE CTRL                                 (S) DMA control register
        1f9001B0 ADMAS                                (S) AutoDMA (ADMA) status
    1f9001c0-1f9002df CORE0 Voice address settings
    1f9002e0-1f900347 CORE0 Misc. settings
    1f900400-1f90057f CORE1 Voice settings
    1f900580-1f9005b1 CORE1 Common settings
    1f9005c0-1f9006df CORE1 Voice address settings
    1f9006e0-1f900747 CORE1 Reverb/Misc. settings
    1f900760-1f900787 CORE0 Volume/Reverb settings
    1f900788-1f9007af CORE1 Volume/Reverb settings
    1f9007c0-1f9007cf S/PDIF settings
*/

struct spu2_voice {
    uint16_t voll;
    uint16_t volr;
    uint16_t pitch;
    uint16_t adsr1;
    uint16_t adsr2;
    uint16_t envx;
    uint16_t volxl;
    uint16_t volxr;
    uint32_t ssa;
    uint32_t lsax;
    uint32_t nax;

    // Internal stuff
    int playing;
    unsigned int counter;
    int32_t h[2];
    int16_t buf[28];
    int loop_start;
    int loop;
    int loop_end;
    int prev_sample_index;
    int16_t s[4];

    // Envelope
    int adsr_cycles_left;
    int adsr_phase;
    int adsr_cycles_reload;
    int adsr_cycles;
    int adsr_mode;
    int adsr_dir;
    int adsr_shift;
    int adsr_step;
    int adsr_pending_step;
    int adsr_sustain_level;
};

struct spu2_core {
    struct spu2_voice v[24];

    int words_written;
    uint32_t pmon;
    uint32_t non;
    uint32_t vmixl;
    uint32_t vmixel;
    uint32_t vmixr;
    uint32_t vmixer;
    uint16_t mmix;
    uint16_t attr;
    uint32_t irqa;
    uint32_t kon;
    uint32_t koff;
    uint32_t tsa;
    uint16_t data;
    uint16_t ctrl;
    uint16_t admas;
    uint32_t esa;
    uint32_t fb_src_a;
    uint32_t fb_src_b;
    uint32_t iir_dest_a0;
    uint32_t iir_dest_a1;
    uint32_t acc_src_a0;
    uint32_t acc_src_a1;
    uint32_t acc_src_b0;
    uint32_t acc_src_b1;
    uint32_t iir_src_a0;
    uint32_t iir_src_a1;
    uint32_t iir_dest_b0;
    uint32_t iir_dest_b1;
    uint32_t acc_src_c0;
    uint32_t acc_src_c1;
    uint32_t acc_src_d0;
    uint32_t acc_src_d1;
    uint32_t iir_src_b1;
    uint32_t iir_src_b0;
    uint32_t mix_dest_a0;
    uint32_t mix_dest_a1;
    uint32_t mix_dest_b0;
    uint32_t mix_dest_b1;
    uint32_t eea;
    uint32_t endx;
    uint16_t stat;
    uint16_t ends;
    uint16_t mvoll;
    uint16_t mvolr;
    uint16_t evoll;
    uint16_t evolr;
    uint16_t avoll;
    uint16_t avolr;
    uint16_t bvoll;
    uint16_t bvolr;
    uint16_t mvolxl;
    uint16_t mvolxr;
    uint16_t iir_alpha;
    uint16_t acc_coef_a;
    uint16_t acc_coef_b;
    uint16_t acc_coef_c;
    uint16_t acc_coef_d;
    uint16_t iir_coef;
    uint16_t fb_alpha;
    uint16_t fb_x;
    uint16_t in_coef_l;
    uint16_t in_coef_r;

    // ADMA
    uint32_t memin_write_addr;
    uint32_t memin_read_addr;

    int adma_playing;

    struct spu2_sample* adma_buf[2];
    unsigned int adma_buf_size;
    int adma_buf_w_index;
    int adma_buf_r_index;
    unsigned int adma_buf_w;
    unsigned int adma_buf_r;
};

struct ps2_spu2 {
    // 2 MB
    uint16_t ram[0x100000];

    struct spu2_core c[2];

    // CORE1 S/PDIF settings
    uint32_t spdif_out;
    uint32_t spdif_mode;
    uint32_t spdif_media;
    uint32_t spdif_copy;
    int spdif_irq;

    struct ps2_iop_dma* dma;
    struct ps2_iop_intc* intc;
    struct sched_state* sched;
};

struct spu2_sample {
    union {
        uint32_t u32;
        uint16_t u16[2];
        int16_t s16[2];
    };
};

struct ps2_spu2* ps2_spu2_create(void);
void ps2_spu2_init(struct ps2_spu2* spu2, struct ps2_iop_dma* dma, struct ps2_iop_intc* intc, struct sched_state* sched);
uint64_t ps2_spu2_read16(struct ps2_spu2* spu2, uint32_t addr);
void ps2_spu2_write16(struct ps2_spu2* spu2, uint32_t addr, uint64_t data);
void ps2_spu2_destroy(struct ps2_spu2* spu2);
struct spu2_sample ps2_spu2_get_sample(struct ps2_spu2* spu);
struct spu2_sample ps2_spu2_get_voice_sample(struct ps2_spu2* spu2, int c, int v);
struct spu2_sample ps2_spu2_get_adma_sample(struct ps2_spu2* spu2, int c);
void spu2_start_adma(struct ps2_spu2* spu2, int c);
int spu2_is_adma_active(struct ps2_spu2* spu2, int c);

#ifdef __cplusplus
}
#endif

#endif