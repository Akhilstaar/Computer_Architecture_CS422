#include "executor.h"

Exe::Exe(Mipc *mc)
{
   _mc = mc;
}

Exe::~Exe(void) {}

#ifdef BYPASS_ENABLED
void Exe::update_bypass(PipeReg &ID_EX_NXT)
{
   if (ID_EX_NXT._bypSRC1 == BYPASS_EX_EX)
   {
      ID_EX_NXT._decodedSRC1 = _mc->EX_MEM_CUR._opResultLo;
   }
#ifdef BYPASS_MEM_EX_ENABLED
   else if (ID_EX_NXT._bypSRC1 == BYPASS_MEM_EX)
   {
      ID_EX_NXT._decodedSRC1 = _mc->MEM_WB_CUR._opResultLo;
   }
#endif
   if (ID_EX_NXT._bypSRC2 == BYPASS_EX_EX)
   {
      ID_EX_NXT._decodedSRC2 = _mc->EX_MEM_CUR._opResultLo;
   }
#ifdef BYPASS_MEM_EX_ENABLED
   else if (ID_EX_NXT._bypSRC2 == BYPASS_MEM_EX)
   {
      ID_EX_NXT._decodedSRC2 = _mc->MEM_WB_CUR._opResultLo;
   }
#endif
}
#endif

void Exe::MainLoop(void)
{
   unsigned int ins;
   Bool isSyscall, isIllegalOp;

   while (1)
   {
      AWAIT_P_PHI0; // @posedge

      _mc->ID_EX_NXT = _mc->ID_EX_CUR;
      ins = _mc->ID_EX_NXT._ins;

      if (!_mc->ID_EX_NXT._isSyscall && !_mc->ID_EX_NXT._isIllegalOp)
      {
         if (_mc->ID_EX_NXT._opControl != NULL)
         {
#ifdef BYPASS_ENABLED
            update_bypass(_mc->ID_EX_NXT);
#endif
            _mc->ID_EX_NXT._opControl(_mc, ins);
         }
#ifdef MIPC_DEBUG
         fprintf(_mc->_debugLog, "<%llu> Executed ins %#x\n", SIM_TIME, ins);
#endif

         if (_mc->ID_EX_NXT._bdslot && _mc->ID_EX_NXT._btaken)
         {
            _mc->_pc = _mc->ID_EX_NXT._btgt;
         }
      }
      else if (_mc->ID_EX_NXT._isSyscall)
      {
#ifdef MIPC_DEBUG
         fprintf(_mc->_debugLog, "<%llu> Deferring execution of syscall ins %#x\n", SIM_TIME, ins);
#endif
      }
      else
      {
#ifdef MIPC_DEBUG
         fprintf(_mc->_debugLog, "<%llu> Illegal ins %#x in execution stage at PC %#x\n", SIM_TIME, ins, _mc->_pc);
#endif
      }

      AWAIT_P_PHI1; // @negedge -- copied to reg in negative cycle
      _mc->EX_MEM_CUR = _mc->ID_EX_NXT;
   }
}
