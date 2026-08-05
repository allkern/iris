#pragma once

#include "scheduler.hpp"
#include "intc.hpp"
#include "dma.hpp"
#include "logger.hpp"

namespace iris::spu2 {

inline constexpr auto RAM_SIZE = 0x100000;// 2 MB

// SPU2 generation mode:
//   1 = synchronous - the voice mix is produced on the emulation thread, one
//       sample per 768 IOP cycles, into the ring buffer below which the audio
//       thread drains. Accurate envelope/IRQ timing, but underruns (crackles)
//       whenever the emulator can't sustain full speed.
//   0 = asynchronous - the audio thread generates the mix on demand. Cheaper
//       and gap-free under load, at the cost of envelope-timing accuracy.
#define SPU2_SYNC 0

inline constexpr auto OUT_BUFFER_SIZE = 4096;
inline constexpr auto ADMA_RING_FRAMES = 32768;

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

struct Sample {
    union {
        uint32_t u32;
        uint16_t u16[2];
        int16_t s16[2];
    };
};

struct Volume {
    uint16_t reg;       // Raw register value (for read-back)
    int16_t current;    // Current applied volume (signed 16-bit)
    int sweep;          // 1 = sweep/slide mode, 0 = fixed volume
    int exponential;
    int decrease;
    int negative;       // Sweep phase (0 = positive, 1 = negative)
    int shift;
    int step;
    int cycles;
};

struct Voice {
    uint16_t voll;
    uint16_t volr;
    uint16_t pitch;
    uint16_t adsr1;
    uint16_t adsr2;
    uint16_t volxl;
    uint16_t volxr;
    uint32_t ssa;
    uint32_t lsax;
    uint32_t nax;
    int32_t envx;
    
    // Internal stuff
    int playing;
    unsigned int counter;
    int32_t h[2];
    int16_t buf[28];
    int loop_start;
    int loop;
    int loop_end;
    int prev_sample_index;
    int loop_addr_specified;
    int16_t s[4];

    Volume vol_l;
    Volume vol_r;

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

    uint64_t env_cycle;
};

struct Core {
    Voice v[24];

    int words_written;
    uint32_t pmon;
    uint32_t non;
    uint16_t noise_level;
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

    Volume mvol_l;
    Volume mvol_r;
    Volume evol_l;
    Volume evol_r;

    int16_t reverb_out_l;
    int16_t reverb_out_r;

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

    // Capture buffers
    uint16_t cb_out1_addr;
    uint16_t cb_out3_addr;
    uint16_t cb_memout_addr;

    Sample* adma_buffer;

    uint32_t adma_buffer_max_size;
    uint32_t adma_write;
    uint32_t adma_read;
    uint32_t adma_fill;
};

struct Spu2 {
    // Wiring. Set by create/connect, preserved across reset.
    struct {
        iop::dma::Dma* dma;
        iop::intc::Intc* intc;
        scheduler::Scheduler* sched;
    } hw;

    // 2 MB
    uint16_t ram[0x100000];

    Core c[2];

    // CORE1 S/PDIF settings
    uint32_t spdif_out;
    uint32_t spdif_mode;
    uint32_t spdif_media;
    uint32_t spdif_copy;
    int spdif_irq;

    int sample_cycles;

    uint64_t emu_cycle;
    uint32_t reverb_cycles;

    Sample out_buffer[OUT_BUFFER_SIZE];
    volatile uint32_t out_write;
    volatile uint32_t out_read;

    logger::Logger* logger = nullptr;
    size_t logger_id = 0;
};

Spu2* create(logger::Logger* logger, iop::intc::Intc* intc, scheduler::Scheduler* sched);
void connect(Spu2* spu2, iop::dma::Dma* dma);
void reset(Spu2* spu2);
uint64_t read16(Spu2* spu2, uint32_t addr);
void write16(Spu2* spu2, uint32_t addr, uint64_t data);
void destroy(Spu2* spu2);
Sample get_sample(Spu2* spu, int adma_enable);
void tick(Spu2* spu2, int cycles);
int pop_sample(Spu2* spu2, Sample* out);
Sample get_voice_sample(Spu2* spu2, int c, int v);
Sample get_adma_sample(Spu2* spu2, int c);
void start_adma(Spu2* spu2, int c);
int adma_write(Spu2* spu2, int c, uint16_t* buf, uint32_t size);
int is_adma_active(Spu2* spu2, int c);
int adma_is_bitstream(Spu2* spu2);
uint16_t read_data(Spu2* spu2, int c);

}
