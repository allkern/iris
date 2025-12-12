#ifndef IOP_H
#define IOP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdio.h>

#define COP0_BPC      3
#define COP0_BDA      5
#define COP0_JUMPDEST 6
#define COP0_DCIC     7
#define COP0_BADVADDR 8
#define COP0_BDAM     9
#define COP0_BPCM     11
#define COP0_SR       12
#define COP0_CAUSE    13
#define COP0_EPC      14
#define COP0_PRID     15

/*
  Name       Alias    Common Usage
  R0         zero     Constant (always 0)
  R1         at       Assembler temporary (destroyed by some assembler pseudoinstructions!)
  R2-R3      v0-v1    Subroutine return values, may be changed by subroutines
  R4-R7      a0-a3    Subroutine arguments, may be changed by subroutines
  R8-R15     t0-t7    Temporaries, may be changed by subroutines
  R16-R23    s0-s7    Static variables, must be saved by subs
  R24-R25    t8-t9    Temporaries, may be changed by subroutines
  R26-R27    k0-k1    Reserved for kernel (destroyed by some IRQ handlers!)
  R28        gp       Global pointer (rarely used)
  R29        sp       Stack pointer
  R30        fp(s8)   Frame Pointer, or 9th Static variable, must be saved
  R31        ra       Return address (used so by JAL,BLTZAL,BGEZAL opcodes)
  -          pc       Program counter
  -          hi,lo    Multiply/divide results, may be changed by subroutines
*/

struct iop_bus_s {
    void* udata;
    uint32_t (*read8)(void* udata, uint32_t addr);
    uint32_t (*read16)(void* udata, uint32_t addr);
    uint32_t (*read32)(void* udata, uint32_t addr);
    void (*write8)(void* udata, uint32_t addr, uint32_t data);
    void (*write16)(void* udata, uint32_t addr, uint32_t data);
    void (*write32)(void* udata, uint32_t addr, uint32_t data);
};

struct iop_state {
    struct iop_bus_s bus;

    uint32_t r[32];
    uint32_t opcode;
    uint32_t pc, next_pc, saved_pc;
    uint32_t hi, lo;
    uint32_t load_d, load_v;
    uint32_t last_cycles;
    uint32_t total_cycles;
    uint32_t biu_config;
    int branch, delay_slot, branch_taken;

    void (*kputchar)(void*, char);
    void* kputchar_udata;
    void (*sm_putchar)(void*, char);
    void* sm_putchar_udata;

    uint32_t cop0_r[16];

    int p;

    uint32_t module_list_addr;

    /* cache module list */
    int module_count;
    struct iop_module *module_list;
};

/*
  0     IEc Current Interrupt Enable  (0=Disable, 1=Enable) ;rfe pops IUp here
  1     KUc Current Kernel/User Mode  (0=Kernel, 1=User)    ;rfe pops KUp here
  2     IEp Previous Interrupt Disable                      ;rfe pops IUo here
  3     KUp Previous Kernel/User Mode                       ;rfe pops KUo here
  4     IEo Old Interrupt Disable                       ;left unchanged by rfe
  5     KUo Old Kernel/User Mode                        ;left unchanged by rfe
  6-7   -   Not used (zero)
  8-15  Im  8 bit interrupt mask fields. When set the corresponding
            interrupts are allowed to cause an exception.
  16    Isc Isolate Cache (0=No, 1=Isolate)
              When isolated, all load and store operations are targetted
              to the Data cache, and never the main memory.
              (Used by PSX Kernel, in combination with Port FFFE0130h)
  17    Swc Swapped cache mode (0=Normal, 1=Swapped)
              Instruction cache will act as Data cache and vice versa.
              Use only with Isc to access & invalidate Instr. cache entries.
              (Not used by PSX Kernel)
  18    PZ  When set cache parity bits are written as 0.
  19    CM  Shows the result of the last load operation with the D-cache
            isolated. It gets set if the cache really contained data
            for the addressed memory location.
  20    PE  Cache parity error (Does not cause exception)
  21    TS  TLB shutdown. Gets set if a programm address simultaneously
            matches 2 TLB entries.
            (initial value on reset allows to detect extended CPU version?)
  22    BEV Boot exception vectors in RAM/ROM (0=RAM/KSEG0, 1=ROM/KSEG1)
  23-24 -   Not used (zero)
  25    RE  Reverse endianness   (0=Normal endianness, 1=Reverse endianness)
              Reverses the byte order in which data is stored in
              memory. (lo-hi -> hi-lo)
              (Affects only user mode, not kernel mode) (?)
              (The bit doesn't exist in PSX ?)
  26-27 -   Not used (zero)
  28    CU0 COP0 Enable (0=Enable only in Kernel Mode, 1=Kernel and User Mode)
  29    CU1 COP1 Enable (0=Disable, 1=Enable) (none in PSX)
  30    CU2 COP2 Enable (0=Disable, 1=Enable) (GTE in PSX)
  31    CU3 COP3 Enable (0=Disable, 1=Enable) (none in PSX)
*/

#define SR_IEC 0x00000001
#define SR_KUC 0x00000002
#define SR_IEP 0x00000004
#define SR_KUP 0x00000008
#define SR_IEO 0x00000010
#define SR_KUO 0x00000020
#define SR_IM  0x0000ff00
#define SR_IM0 0x00000100
#define SR_IM1 0x00000200
#define SR_IM2 0x00000400
#define SR_IM3 0x00000800
#define SR_IM4 0x00001000
#define SR_IM5 0x00002000
#define SR_IM6 0x00004000
#define SR_IM7 0x00008000
#define SR_ISC 0x00010000
#define SR_SWC 0x00020000
#define SR_PZ  0x00040000
#define SR_CM  0x00080000
#define SR_PE  0x00100000
#define SR_TS  0x00200000
#define SR_BEV 0x00400000
#define SR_RE  0x02000000
#define SR_CU0 0x10000000
#define SR_CU1 0x20000000
#define SR_CU2 0x40000000
#define SR_CU3 0x80000000

struct iop_state* iop_create(void);
void iop_init(struct iop_state* iop, struct iop_bus_s bus);
void iop_init_kputchar(struct iop_state* iop, void (*kputchar)(void*, char), void* udata);
void iop_init_sm_putchar(struct iop_state* iop, void (*sm_putchar)(void*, char), void* udata);
void iop_destroy(struct iop_state* iop);
void iop_cycle(struct iop_state* iop);
void iop_reset(struct iop_state* iop);
void iop_set_irq_pending(struct iop_state* iop);
void iop_fetch(struct iop_state* iop);
int iop_execute(struct iop_state* iop);

// External bus access functions
uint32_t iop_read8(struct iop_state* iop, uint32_t addr);
uint32_t iop_read16(struct iop_state* iop, uint32_t addr);
uint32_t iop_read32(struct iop_state* iop, uint32_t addr);
void iop_write8(struct iop_state* iop, uint32_t addr, uint32_t data);
void iop_write16(struct iop_state* iop, uint32_t addr, uint32_t data);
void iop_write32(struct iop_state* iop, uint32_t addr, uint32_t data);

/*
    00h INT     Interrupt
    01h MOD     TLB modification (none such in PSX)
    02h TLBL    TLB load         (none such in PSX)
    03h TLBS    TLB store        (none such in PSX)
    04h AdEL    Address error, Data load or Instruction fetch
    05h AdES    Address error, Data store
                The address errors occur when attempting to read
                outside of KUseg in user mode and when the address
                is misaligned. (See also: BadVaddr register)
    06h IBE     Bus error on Instruction fetch
    07h DBE     Bus error on Data load/store
    08h Syscall Generated unconditionally by syscall instruction
    09h BP      Breakpoint - break instruction
    0Ah RI      Reserved instruction
    0Bh CpU     Coprocessor unusable
    0Ch Ov      Arithmetic overflow
*/

#define CAUSE_INT       (0x00 << 2)
#define CAUSE_MOD       (0x01 << 2)
#define CAUSE_TLBL      (0x02 << 2)
#define CAUSE_TLBS      (0x03 << 2)
#define CAUSE_ADEL      (0x04 << 2)
#define CAUSE_ADES      (0x05 << 2)
#define CAUSE_IBE       (0x06 << 2)
#define CAUSE_DBE       (0x07 << 2)
#define CAUSE_SYSCALL   (0x08 << 2)
#define CAUSE_BP        (0x09 << 2)
#define CAUSE_RI        (0x0a << 2)
#define CAUSE_CPU       (0x0b << 2)
#define CAUSE_OV        (0x0c << 2)

#ifdef __cplusplus
}
#endif

#endif
