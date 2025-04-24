#include "decode.h"

Decode::Decode(Mipc *mc)
{
   _mc = mc;
}

Decode::~Decode(void) {}

// TODO: Implement bypass logic
#ifdef BYPASS_ENABLED
Bool Decode::computeBypass()
{
}
#else
Bool Decode::detectStalls()
{
   Bool stall = FALSE;

   return stall;
}
#endif

void Decode::MainLoop(void)
{
   unsigned int ins;
   while (1)
   {
      AWAIT_P_PHI0; // @posedge -- copy input and detect hazard

      _mc->IF_ID_NXT = _mc->IF_ID_CUR;
      ins = _mc->IF_ID_NXT._ins;

#ifdef BYPASS_ENABLED
      _mc->IF_ID_NXT._bypassSRC1 = BYPASS_NONE;
      _mc->IF_ID_NXT._bypassSRC2 = BYPASS_NONE;
#endif
      // Call Dec with actual=FALSE
      _mc->Dec(ins, FALSE);
      if (!_mc->IF_ID_NXT._isIllegalOp)
      {
#ifdef BYPASS_ENABLED
         _mc->_toStall = computeBypass();
#else
         _mc->_toStall = detectStalls();
#endif
      }

      if (!_mc->_toStall && _mc->IF_ID_C._isSyscall)
      {
         _mc->_isSyscallInPipe = TRUE;
      }

      AWAIT_P_PHI1; // @negedge

      if (_mc->_toStall)
         _mc->ID_EX_CUR.clear();
      else
      {
         _mc->Dec(ins, TRUE);
#ifdef MIPC_DEBUG
         fprintf(_mc->_debugLog, "<%llu> Decoded ins %#x\n", SIM_TIME, ins);
#endif
         _mc->ID_EX_CUR = _mc->ID_EX_NXT;
      }
   }
}