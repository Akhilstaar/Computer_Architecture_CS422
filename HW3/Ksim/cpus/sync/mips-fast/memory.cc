#include "memory.h"

Memory::Memory(Mipc *mc)
{
   _mc = mc;
}

Memory::~Memory(void) {}

void Memory::MainLoop(void)
{
   Bool memControl;

   while (1)
   {
      AWAIT_P_PHI0; // @posedge

      _mc->EX_MEM_NXT = _mc->EX_MEM_CUR;
      memControl = _mc->EX_MEM_NXT._memControl;

      AWAIT_P_PHI1; // @negedge

      if (memControl)
      {
         _mc->EX_MEM_NXT._memOp(_mc);
#ifdef MIPC_DEBUG
         fprintf(_mc->_debugLog, "<%llu> Accessing memory at address %#x for ins %#x\n", SIM_TIME, _mc->EX_MEM_NXT._memory_addr_reg, _mc->EX_MEM_NXT._ins);
#endif
      }
      else
      {
#ifdef MIPC_DEBUG
         fprintf(_mc->_debugLog, "<%llu> Memory has nothing to do for ins %#x\n", SIM_TIME, _mc->EX_MEM_NXT._ins);
#endif
      }

      _mc->MEM_WB_CUR = _mc->MEM_WB_NXT;
   }
}
