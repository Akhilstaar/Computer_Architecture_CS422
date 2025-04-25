#include "wb.h"

Writeback::Writeback(Mipc *mc)
{
   _mc = mc;
}

Writeback::~Writeback(void) {}

void Writeback::MainLoop(void)
{
   unsigned int ins;
   Bool writeReg;
   Bool writeFReg;
   Bool loWPort;
   Bool hiWPort;
   Bool isSyscall;
   Bool isIllegalOp;
   unsigned decodedDST;
   unsigned opResultLo, opResultHi;

   while (1)
   {
      AWAIT_P_PHI0; // @posedge

      _mc->MEM_WB_NXT = _mc->MEM_WB_CUR;

      // Sample the important signals
      writeReg = _mc->MEM_WB_NXT._writeREG;
      writeFReg = _mc->MEM_WB_NXT._writeFREG;
      loWPort = _mc->MEM_WB_NXT._loWPort;
      hiWPort = _mc->MEM_WB_NXT._hiWPort;
      decodedDST = _mc->MEM_WB_NXT._decodedDST;
      opResultLo = _mc->MEM_WB_NXT._opResultLo;
      opResultHi = _mc->MEM_WB_NXT._opResultHi;
      isSyscall = _mc->MEM_WB_NXT._isSyscall;
      isIllegalOp = _mc->MEM_WB_NXT._isIllegalOp;
      ins = _mc->MEM_WB_NXT._ins;

      if (isSyscall)
      {
#ifdef MIPC_DEBUG
         fprintf(_mc->_debugLog, "<%llu> SYSCALL! Trapping to emulation layer at PC %#x\n", SIM_TIME, _mc->_pc);
#endif
         _mc->MEM_WB_NXT._opControl(_mc, ins);
         _mc->_pc = _mc->MEM_WB_NXT._pc + 4;
         _mc->_waitForSyscall = FALSE;
      }
      else if (isIllegalOp)
      {
         printf("Illegal ins %#x at PC %#x. Terminating simulation!\n", ins, _mc->_pc);
#ifdef MIPC_DEBUG
         fclose(_mc->_debugLog);
#endif
         printf("Register state on termination:\n\n");
         _mc->dumpregs();
         exit(0);
      }
      else
      {
         if (writeReg)
         {
            _mc->_gpr[decodedDST] = opResultLo;
#ifdef MIPC_DEBUG
            fprintf(_mc->_debugLog, "<%llu> Writing to reg %u, value: %#x\n", SIM_TIME, decodedDST, opResultLo);
#endif
         }
         else if (writeFReg)
         {
            _mc->_fpr[(decodedDST) >> 1].l[FP_TWIDDLE ^ ((decodedDST) & 1)] = opResultLo;
#ifdef MIPC_DEBUG
            fprintf(_mc->_debugLog, "<%llu> Writing to freg %u, value: %#x\n", SIM_TIME, decodedDST >> 1, opResultLo);
#endif
         }
         else if (loWPort || hiWPort)
         {
            if (loWPort)
            {
               _mc->_lo = opResultLo;
#ifdef MIPC_DEBUG
               fprintf(_mc->_debugLog, "<%llu> Writing to Lo, value: %#x\n", SIM_TIME, opResultLo);
#endif
            }
            if (hiWPort)
            {
               _mc->_hi = opResultHi;
#ifdef MIPC_DEBUG
               fprintf(_mc->_debugLog, "<%llu> Writing to Hi, value: %#x\n", SIM_TIME, opResultHi);
#endif
            }
         }
      }
      _mc->_gpr[0] = 0;

      AWAIT_P_PHI1; // @negedge
   }
}
