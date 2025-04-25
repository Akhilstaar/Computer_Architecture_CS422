#ifndef __MIPS_H__
#define __MIPS_H__

#include "sim.h"

class Mipc;
class MipcSysCall;
class SysCall;

typedef unsigned Bool;
#define TRUE 1
#define FALSE 0

#if BYTE_ORDER == LITTLE_ENDIAN
#define FP_TWIDDLE 0
#else
#define FP_TWIDDLE 1
#endif

// #define BYPASS_ENABLED 1        // Enable bypass paths
#define BYPASS_MEM_EX_ENABLED 1 // Enable MEM->EX bypass path specifically
#define REG_DEFAULT 10000 // Sentinel value for unused register source

#define BYPASS_NONE 0x01
#define BYPASS_EX_EX 0x02  // Forward from EX stage (result available end of EX)
#define BYPASS_MEM_EX 0x04 // Forward from MEM stage (result available end of MEM)

#include "mem.h"
#include "../../common/syscall.h"
#include "queue.h"

// #define MIPC_DEBUG 1

typedef struct
{
   unsigned int _pc;  // Program Counter
   unsigned int _ins; // Instruction register
   Bool _isSyscall;   // System call detected
   Bool _isIllegalOp; // Illegal opcode
   Bool _isNOP;       // NOP

   // Decode (ID) stage
   unsigned _decodedDST;                  // Destination register
   signed int _decodedSRC1, _decodedSRC2; // Source operands
   unsigned _regSRC1, _regSRC2;
   unsigned _decodedShiftAmt; // Shift amount

   Bool _requiresFP;
   Bool _hiWrite, _loWrite;
   Bool _memControl;         // Memory instruction?
   Bool _hiWPort, _loWPort;  // HI/LO write flags
   unsigned _subregOperand;  // for lwl, lwr
   signed int _branchOffset; // Branch offset

#ifdef BYPASS_ENABLED
   unsigned _bypSRC1, _bypSRC2;
#endif

   // Execute (EX) stage
   int _bdslot;
   unsigned int _btgt;                // branch target
   int _btaken;                       // taken branch (1 if taken, 0 if fall-through)
   unsigned _opResultLo, _opResultHi; // ALU results
   unsigned _memory_addr_reg;         // Memory address

   Bool _writeREG, _writeFREG; // WB control

   // Control signals
   void (*_opControl)(Mipc *, unsigned);
   void (*_memOp)(Mipc *);

   void clear();
} PipeReg;

void PipeReg::clear()
{
   _pc = 0;
   _ins = 0; // NOP instruction
   _isSyscall = FALSE;
   _isIllegalOp = FALSE;
   _isNOP = TRUE; // Default to NOP

   _decodedDST = 0;
   _decodedSRC1 = 0;
   _decodedSRC2 = 0;
   _regSRC1 = REG_DEFAULT;
   _regSRC2 = REG_DEFAULT;
   _decodedShiftAmt = 0;
   _requiresFP = FALSE;
   _hiWrite = FALSE;
   _loWrite = FALSE;
   _memControl = FALSE;
   _subregOperand = 0;
   _branchOffset = 0;
#ifdef BYPASS_ENABLED
   _bypSRC1 = BYPASS_NONE;
   _bypSRC2 = BYPASS_NONE;
#endif

   _btgt = 0xdeadbeef; // Use a recognizable invalid address
   _btaken = 0;
   _bdslot = 0; // Not a branch/jump by default
   _opResultLo = 0;
   _opResultHi = 0;
   _memory_addr_reg = 0;

   _writeREG = FALSE;
   _writeFREG = FALSE;
   _hiWPort = FALSE;
   _loWPort = FALSE;

   _opControl = NULL;
   _memOp = NULL;
}

class Mipc : public SimObject
{
public:
   Mipc(Mem *m);
   ~Mipc();

   FAKE_SIM_TEMPLATE;

   MipcSysCall *_sys; // Emulated system call layer

   void dumpregs(void); // Dumps current register state

   void Reboot(char *image = NULL);
   // Restart processor.
   // "image" = file name for new memory
   // image if any.

   void MipcDumpstats();                // Prints simulation statistics
   void Dec(unsigned int ins, Bool real);          // Decoder function
   void fake_syscall(unsigned int ins); // System call interface

   PipeReg IF_ID_CUR, ID_EX_CUR, EX_MEM_CUR, MEM_WB_CUR;
   PipeReg IF_ID_NXT, ID_EX_NXT, EX_MEM_NXT, MEM_WB_NXT;

   unsigned int _gpr[32]; // general-purpose integer registers

   union
   {
      unsigned int l[2];
      float f[2];
      double d;
   } _fpr[16]; // floating-point registers (paired)

   unsigned int _hi, _lo; // mult, div destination
   unsigned int _pc;
   // unsigned int _lastbdslot;			// branch delay state
   unsigned int _boot; // boot code loaded?

   Bool _waitForSyscall;
   Bool _toStall;
   Bool is_subreg;
#ifdef BRANCH_INTERLOCK
   Bool _branchInterlock;
#endif

   // Simulation statistics counters

   LL _nfetched;
   LL _num_cond_br;
   LL _num_jal;
   LL _num_jr;
   LL _num_load;
   LL _num_store;
   LL _fpinst;
   // LL _load_interlock_cycles;

   Mem *_mem; // attached memory (not a cache)

   Log _l;
   int _sim_exit; // 1 on normal termination

   FILE *_debugLog;

   // EXE stage definitions

   static void func_add_addu(Mipc *, unsigned);
   static void func_and(Mipc *, unsigned);
   static void func_nor(Mipc *, unsigned);
   static void func_or(Mipc *, unsigned);
   static void func_sll(Mipc *, unsigned);
   static void func_sllv(Mipc *, unsigned);
   static void func_slt(Mipc *, unsigned);
   static void func_sltu(Mipc *, unsigned);
   static void func_sra(Mipc *, unsigned);
   static void func_srav(Mipc *, unsigned);
   static void func_srl(Mipc *, unsigned);
   static void func_srlv(Mipc *, unsigned);
   static void func_sub_subu(Mipc *, unsigned);
   static void func_xor(Mipc *, unsigned);
   static void func_div(Mipc *, unsigned);
   static void func_divu(Mipc *, unsigned);
   static void func_mfhi(Mipc *, unsigned);
   static void func_mflo(Mipc *, unsigned);
   static void func_mthi(Mipc *, unsigned);
   static void func_mtlo(Mipc *, unsigned);
   static void func_mult(Mipc *, unsigned);
   static void func_multu(Mipc *, unsigned);
   static void func_jalr(Mipc *, unsigned);
   static void func_jr(Mipc *, unsigned);
   static void func_await_break(Mipc *, unsigned);
   static void func_syscall(Mipc *, unsigned);
   static void func_addi_addiu(Mipc *, unsigned);
   static void func_andi(Mipc *, unsigned);
   static void func_lui(Mipc *, unsigned);
   static void func_ori(Mipc *, unsigned);
   static void func_slti(Mipc *, unsigned);
   static void func_sltiu(Mipc *, unsigned);
   static void func_xori(Mipc *, unsigned);
   static void func_beq(Mipc *, unsigned);
   static void func_bgez(Mipc *, unsigned);
   static void func_bgezal(Mipc *, unsigned);
   static void func_bltzal(Mipc *, unsigned);
   static void func_bltz(Mipc *, unsigned);
   static void func_bgtz(Mipc *, unsigned);
   static void func_blez(Mipc *, unsigned);
   static void func_bne(Mipc *, unsigned);
   static void func_j(Mipc *, unsigned);
   static void func_jal(Mipc *, unsigned);
   static void func_lb(Mipc *, unsigned);
   static void func_lbu(Mipc *, unsigned);
   static void func_lh(Mipc *, unsigned);
   static void func_lhu(Mipc *, unsigned);
   static void func_lwl(Mipc *, unsigned);
   static void func_lw(Mipc *, unsigned);
   static void func_lwr(Mipc *, unsigned);
   static void func_lwc1(Mipc *, unsigned);
   static void func_swc1(Mipc *, unsigned);
   static void func_sb(Mipc *, unsigned);
   static void func_sh(Mipc *, unsigned);
   static void func_swl(Mipc *, unsigned);
   static void func_sw(Mipc *, unsigned);
   static void func_swr(Mipc *, unsigned);
   static void func_mtc1(Mipc *, unsigned);
   static void func_mfc1(Mipc *, unsigned);

   // MEM stage definitions

   static void mem_lb(Mipc *);
   static void mem_lbu(Mipc *);
   static void mem_lh(Mipc *);
   static void mem_lhu(Mipc *);
   static void mem_lwl(Mipc *);
   static void mem_lw(Mipc *);
   static void mem_lwr(Mipc *);
   static void mem_lwc1(Mipc *);
   static void mem_swc1(Mipc *);
   static void mem_sb(Mipc *);
   static void mem_sh(Mipc *);
   static void mem_swl(Mipc *);
   static void mem_sw(Mipc *);
   static void mem_swr(Mipc *);
};

// Emulated system call interface

class MipcSysCall : public SysCall
{
public:
   MipcSysCall(Mipc *ms)
   {

      char buf[1024];
      m = ms->_mem;
      _ms = ms;
      _num_load = 0;
      _num_store = 0;
   };

   ~MipcSysCall() {};

   LL GetDWord(LL addr);
   void SetDWord(LL addr, LL data);

   Word GetWord(LL addr);
   void SetWord(LL addr, Word data);

   void SetReg(int reg, LL val);
   LL GetReg(int reg);
   LL GetTime(void);

private:
   Mipc *_ms;
};
#endif /* __MIPS_H__ */
