#include <stdio.h>
#include "pin.H"
#include <iostream>
#include <fstream>
#include <unordered_set>

using namespace std;

ofstream OutFile;

// counters
enum CounterType
{
    LOAD,
    STORE,
    NOP,
    DIRECT_CALL,
    INDIRECT_CALL,
    RETURN,
    UNCOND_BR,
    COND_BR,
    LOGICAL,
    ROTATE_SHIFT,
    FLAGOP,
    VECTOR,
    CMOV,
    MMX_SSE,
    SYSCALL,
    FLOATING_POINT,
    OTHER,
    NUM_COUNTERS // 17
};

UINT64 Counters[NUM_COUNTERS] = {0};
UINT64 icount = 0;
UINT64 fast_forward_count = 0;

UINT64 Lengths[16] = {0};           // inslength
UINT64 Operands[8] = {0};           // operands
UINT64 Read_reg[8] = {0};           // read registers
UINT64 Write_reg[8] = {0};          // write registers
UINT64 Mem_Operands[8] = {0};       // memory operands
UINT64 Mem_Read_Operands[8] = {0};  // memory read operands
UINT64 Mem_Write_Operands[8] = {0}; // memory write operands
UINT32 Max_Membytes = 0;            // max memory bytes
UINT32 Membytes = 0;                // total memorybytes
UINT64 Memins = 0;                  // no. of mem instructions having atleast one mem op
INT32 Max_imm = INT_MIN;            // max immediate
INT32 Min_imm = INT_MAX;            // min immediate
ADDRDELTA Max_Disp = -1 * 1e9;      // max displacement
ADDRDELTA Min_Disp = 1e9;           // min displacement

unordered_set<UINT32> insfootprints;
unordered_set<UINT32> datafootprints;

VOID INSmetric(UINT32 len, ADDRINT addr, UINT32 ops, UINT32 readregs, UINT32 wrregs, INT32 minimm, INT32 maximm)
{
    Lengths[len] += 1;
    Operands[ops] += 1;
    Read_reg[readregs] += 1;
    Write_reg[wrregs] += 1;
    if (minimm < Min_imm)
        Min_imm = minimm;
    if (maximm > Max_imm)
        Max_imm = maximm;
    ADDRINT start = (addr >> 5);
    ADDRINT end = ((addr + len - 1) >> 5);
    for (ADDRINT i = start; i <= end; i++)
    {
        insfootprints.insert(i);
    }
}

VOID MEMINSmetric(UINT32 memops, UINT32 readmemops, UINT32 writememops, ADDRDELTA mindisp, ADDRDELTA maxdisp, UINT32 membytes)
{
    Mem_Operands[memops] += 1;
    Mem_Read_Operands[readmemops] += 1;
    Mem_Write_Operands[writememops] += 1;
    if (memops > 0)
    {
        Memins += 1;
        if (mindisp < Min_Disp)
            Min_Disp = mindisp;
        if (maxdisp > Max_Disp)
            Max_Disp = maxdisp;
        Membytes += membytes;
        if (membytes > Max_Membytes)
            Max_Membytes = membytes;
    }
}

// This function is called on every instruction regardless of fast-forwarding.
// It updates the total instruction count and also switches on instrumentation when appropriate.
VOID InsCount(UINT32 nins)
{
    icount += nins;
}

ADDRINT Terminate(void)
{
    return (icount >= fast_forward_count + 1000000000);
}

ADDRINT FastForward(void)
{
    return (icount >= fast_forward_count && icount);
}

VOID AddToCounter(UINT64 *counter)
{
    *counter += 1;
}

VOID AddToMemCounter(ADDRINT addr, UINT32 size, UINT64 *counter, UINT32 delta)
{
    *counter += delta;

    ADDRINT start = (addr >> 5);
    ADDRINT end = ((addr + size - 1) >> 5);
    for (ADDRINT i = start; i <= end; i++)
    {
        datafootprints.insert(i);
    }
}

VOID PrintResults()
{
    OutFile << "\n===================PARTA==================\n";
    // Calculate total instructions counted in the instrumentation phase
    UINT64 total_instructions = 0;
    for (int i = 0; i < NUM_COUNTERS; i++)
    {
        total_instructions += Counters[i];
    }

    OutFile << "---- Instrumentation Results ----" << endl;
    OutFile << "Loads: " << Counters[LOAD] << std::setw(6) << "[ " << (100.0 * Counters[LOAD] / total_instructions) << "% ]\n";
    OutFile << "Stores: " << Counters[STORE] << std::setw(6) << "[ " << (100.0 * Counters[STORE] / total_instructions) << "% ]\n";
    OutFile << "NOPs: " << Counters[NOP] << std::setw(6) << "[ " << (100.0 * Counters[NOP] / total_instructions) << "% ]\n";
    OutFile << "Direct calls: " << Counters[DIRECT_CALL] << std::setw(6) << "[ " << (100.0 * Counters[DIRECT_CALL] / total_instructions) << "% ]\n";
    OutFile << "Indirect calls: " << Counters[INDIRECT_CALL] << std::setw(6) << "[ " << (100.0 * Counters[INDIRECT_CALL] / total_instructions) << "% ]\n";
    OutFile << "Returns: " << Counters[RETURN] << std::setw(6) << "[ " << (100.0 * Counters[RETURN] / total_instructions) << "% ]\n";
    OutFile << "Unconditional branches: " << Counters[UNCOND_BR] << std::setw(6) << "[ " << (100.0 * Counters[UNCOND_BR] / total_instructions) << "% ]\n";
    OutFile << "Conditional branches: " << Counters[COND_BR] << std::setw(6) << "[ " << (100.0 * Counters[COND_BR] / total_instructions) << "% ]\n";
    OutFile << "Logical operations: " << Counters[LOGICAL] << std::setw(6) << "[ " << (100.0 * Counters[LOGICAL] / total_instructions) << "% ]\n";
    OutFile << "Rotate and shift: " << Counters[ROTATE_SHIFT] << std::setw(6) << "[ " << (100.0 * Counters[ROTATE_SHIFT] / total_instructions) << "% ]\n";
    OutFile << "Flag operations: " << Counters[FLAGOP] << std::setw(6) << "[ " << (100.0 * Counters[FLAGOP] / total_instructions) << "% ]\n";
    OutFile << "Vector instructions: " << Counters[VECTOR] << std::setw(6) << "[ " << (100.0 * Counters[VECTOR] / total_instructions) << "% ]\n";
    OutFile << "Conditional moves: " << Counters[CMOV] << std::setw(6) << "[ " << (100.0 * Counters[CMOV] / total_instructions) << "% ]\n";
    OutFile << "MMX and SSE instructions: " << Counters[MMX_SSE] << std::setw(6) << "[ " << (100.0 * Counters[MMX_SSE] / total_instructions) << "% ]\n";
    OutFile << "System calls: " << Counters[SYSCALL] << std::setw(6) << "[ " << (100.0 * Counters[SYSCALL] / total_instructions) << "% ]\n";
    OutFile << "Floating-point: " << Counters[FLOATING_POINT] << std::setw(6) << "[ " << (100.0 * Counters[FLOATING_POINT] / total_instructions) << "% ]\n";
    OutFile << "Others: " << Counters[OTHER] << std::setw(6) << "[ " << (100.0 * Counters[OTHER] / total_instructions) << "% ]\n";

    OutFile << "\n=================== PART B ==================\n";
    // Calculate CPI
    double cpi = (total_instructions > 0) ? static_cast<double>(((Counters[LOAD] + Counters[STORE]) * 69 + total_instructions)) / total_instructions : 0;
    OutFile << "CPI: " << cpi << endl;

    OutFile << "\n=================== PART C ==================\n";
    OutFile << "Instruction Footprint: " << (insfootprints.size() * 32)
            << " bytes (" << insfootprints.size() << " unique chunks)\n";
    OutFile << "Data Footprint: " << (datafootprints.size() * 32)
            << " bytes (" << datafootprints.size() << " unique chunks)\n";

    OutFile << "\n=================== PART D ==================\n";
    OutFile << "1. Instruction Length Distribution:\n";
    for (int i = 1; i <= 15; i++)
        OutFile << i << " bytes: " << Lengths[i] << "\n";

    OutFile << "\n2. Operand Count Distribution:\n";
    for (int i = 0; i < 8; i++)
        OutFile << i << " operands: " << Operands[i] << "\n";

    OutFile << "\n3. Register Read Operands:\n";
    for (int i = 0; i < 8; i++)
        OutFile << i << " reads: " << Read_reg[i] << "\n";

    OutFile << "\n4. Register Write Operands:\n";
    for (int i = 0; i < 8; i++)
        OutFile << i << " writes: " << Write_reg[i] << "\n";

    OutFile << "\n5. Memory Operands per Instruction:\n";
    for (int i = 0; i < 8; i++)
        OutFile << i << " mem ops: " << Mem_Operands[i] << "\n";

    OutFile << "\n6. Memory Read Operands per Instruction:\n";
    for (int i = 0; i < 8; i++)
        OutFile << i << " read ops: " << Mem_Read_Operands[i] << "\n";

    OutFile << "\n7. Memory Write Operands per Instruction:\n";
    for (int i = 0; i < 8; i++)
        OutFile << i << " write ops: " << Mem_Write_Operands[i] << "\n";

    UINT64 mem_instr = 0;
    for (int i = 1; i < 8; i++)
        mem_instr += Mem_Operands[i];
    double avg_mem = (mem_instr > 0) ? 1.0 * Membytes / Memins : 0;
    OutFile << "\n8. Memory Bytes:\nMax: " << Max_Membytes << "\nAverage: " << avg_mem << "\n";

    OutFile << "\n9. Immediate Value Range:\n";
    if (Min_imm > Max_imm)
        OutFile << "None\n";
    else
        OutFile << "Min: " << Min_imm << "\nMax: " << Max_imm << "\n";

    OutFile << "\n10. Displacement Value Range:\n";
    if (Min_Disp > Max_Disp)
        OutFile << "None\n";
    else
        OutFile << "Min: " << Min_Disp << "\nMax: " << Max_Disp << "\n";
    OutFile.close();
    return;
}

VOID ExitRoutine()
{
    PrintResults();
    exit(0);
}

// Instrumentation function for instructions.
VOID BBTraceRoutine(TRACE trace, VOID *v)
{
    // For every basic block in the trace
    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl))
    {
        for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins))
        {
            // Categorize instructions based on XED categories.
            xed_category_enum_t cat = static_cast<xed_category_enum_t>(INS_Category(ins));

            if (cat == XED_CATEGORY_INVALID)
                continue;

            // mem operand instructions
            UINT32 memOperands = INS_MemoryOperandCount(ins);
            UINT32 memreadOperands = 0;
            UINT32 memwriteOperands = 0;
            UINT32 memops = 0;
            ADDRDELTA mindisp = 1e9, maxdisp = -1 * 1e9, disp;
            UINT32 membytes = 0;

            if (memOperands > 0)
            {
                for (UINT32 memOp = 0; memOp < memOperands; memOp++)
                {
                    membytes += INS_MemoryOperandSize(ins, memOp);
                    disp = INS_OperandMemoryDisplacement(ins, memOp);
                    if (maxdisp < disp)
                        maxdisp = disp;
                    if (mindisp > disp)
                        mindisp = disp;

                    if (INS_MemoryOperandIsRead(ins, memOp))
                    {
                        memreadOperands++;
                        memops++;
                        UINT32 size = INS_MemoryOperandSize(ins, memOp);
                        UINT32 chunks = (size + 3) / 4;
                        INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
                        INS_InsertThenPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)AddToMemCounter, IARG_MEMORYREAD_EA, IARG_UINT32, size, IARG_PTR, &Counters[LOAD], IARG_UINT32, chunks, IARG_END);
                    }
                    if (INS_MemoryOperandIsWritten(ins, memOp))
                    {
                        memwriteOperands++;
                        memops++;
                        UINT32 size = INS_MemoryOperandSize(ins, memOp);
                        UINT32 chunks = (size + 3) / 4;
                        INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
                        INS_InsertThenPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)AddToMemCounter, IARG_MEMORYWRITE_EA, IARG_UINT32, size, IARG_PTR, &Counters[STORE], IARG_UINT32, chunks, IARG_END);
                    }
                }
            }
            INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
            INS_InsertThenPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)MEMINSmetric, IARG_UINT32, memops, IARG_UINT32, memreadOperands, IARG_UINT32, memwriteOperands, IARG_ADDRINT, mindisp, IARG_ADDRINT, maxdisp, IARG_UINT32, membytes, IARG_END);

            if (cat == XED_CATEGORY_NOP)
            {
                INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
                INS_InsertThenPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)AddToCounter, IARG_PTR, &Counters[NOP], IARG_END);
            }
            else if (cat == XED_CATEGORY_CALL)
            {
                if (INS_IsDirectCall(ins))
                {
                    INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
                    INS_InsertThenPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)AddToCounter, IARG_PTR, &Counters[DIRECT_CALL], IARG_END);
                }
                else
                {
                    INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
                    INS_InsertThenPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)AddToCounter, IARG_PTR, &Counters[INDIRECT_CALL], IARG_END);
                }
            }
            else if (cat == XED_CATEGORY_RET)
            {
                INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
                INS_InsertThenPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)AddToCounter, IARG_PTR, &Counters[RETURN], IARG_END);
            }
            else if (cat == XED_CATEGORY_X87_ALU)
            {
                INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
                INS_InsertThenPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)AddToCounter, IARG_PTR, &Counters[FLOATING_POINT], IARG_END);
            }
            else if (cat == XED_CATEGORY_UNCOND_BR)
            {
                INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
                INS_InsertThenPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)AddToCounter, IARG_PTR, &Counters[UNCOND_BR], IARG_END);
            }
            else if (cat == XED_CATEGORY_COND_BR)
            {
                INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
                INS_InsertThenPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)AddToCounter, IARG_PTR, &Counters[COND_BR], IARG_END);
            }
            else if (cat == XED_CATEGORY_LOGICAL)
            {
                INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
                INS_InsertThenPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)AddToCounter, IARG_PTR, &Counters[LOGICAL], IARG_END);
            }
            else if (cat == XED_CATEGORY_ROTATE || cat == XED_CATEGORY_SHIFT)
            {
                INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
                INS_InsertThenPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)AddToCounter, IARG_PTR, &Counters[ROTATE_SHIFT], IARG_END);
            }
            else if (cat == XED_CATEGORY_FLAGOP)
            {
                INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
                INS_InsertThenPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)AddToCounter, IARG_PTR, &Counters[FLAGOP], IARG_END);
            }
            else if (cat == XED_CATEGORY_AVX || cat == XED_CATEGORY_AVX2 ||
                     cat == XED_CATEGORY_AVX2GATHER || cat == XED_CATEGORY_AVX512)
            {
                INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
                INS_InsertThenPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)AddToCounter, IARG_PTR, &Counters[VECTOR], IARG_END);
            }
            else if (cat == XED_CATEGORY_CMOV)
            {
                INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
                INS_InsertThenPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)AddToCounter, IARG_PTR, &Counters[CMOV], IARG_END);
            }
            else if (cat == XED_CATEGORY_MMX || cat == XED_CATEGORY_SSE)
            {
                INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
                INS_InsertThenPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)AddToCounter, IARG_PTR, &Counters[MMX_SSE], IARG_END);
            }
            else if (cat == XED_CATEGORY_SYSCALL)
            {
                INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
                INS_InsertThenPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)AddToCounter, IARG_PTR, &Counters[SYSCALL], IARG_END);
            }
            else
            {
                INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
                INS_InsertThenPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)AddToCounter, IARG_PTR, &Counters[OTHER], IARG_END);
            }

            UINT32 operands = INS_OperandCount(ins);

            INT32 minimm = INT_MAX, maximm = INT_MIN, imm;

            for (UINT32 op = 0; op < operands; op++)
            {
                if (INS_OperandIsImmediate(ins, op))
                {
                    imm = INS_OperandImmediate(ins, op);
                    if (imm < minimm)
                        minimm = imm;
                    if (imm > maximm)
                        maximm = imm;
                }
            }

            INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
            INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)INSmetric, IARG_UINT32, INS_Size(ins), IARG_ADDRINT, INS_Address(ins), IARG_UINT32, operands, IARG_UINT32, INS_MaxNumRRegs(ins), IARG_UINT32, INS_MaxNumWRegs(ins), IARG_ADDRINT, minimm, IARG_ADDRINT, maximm, IARG_END);
        }

        // Check for termination condition before any other instrumentation.
        BBL_InsertIfCall(bbl, IPOINT_BEFORE, (AFUNPTR)Terminate, IARG_END);
        BBL_InsertThenCall(bbl, IPOINT_BEFORE, (AFUNPTR)ExitRoutine, IARG_END);

        BBL_InsertCall(bbl, IPOINT_BEFORE, (AFUNPTR)InsCount, IARG_UINT32, BBL_NumIns(bbl), IARG_END);
    }
}

VOID Fini(INT32 code, VOID *v)
{
    OutFile << "Execution terminated before completing instrumentation on all specified instructions." << endl;
    PrintResults();
    return;
}

KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "o", "inscount.out", "specify output file name");
KNOB<UINT64> KnobFastForwardTill(KNOB_MODE_OVERWRITE, "pintool", "f", "0", "fast forward by this many instructions");

INT32 Usage()
{
    cerr << "This tool categorizes instructions, counts them dynamically, and calculates CPI" << '\n';
    cerr << '\n'
         << KNOB_BASE::StringKnobSummary() << endl;
    return -1;
}

int main(int argc, char *argv[])
{
    if (PIN_Init(argc, argv))
        return Usage();

    OutFile.open(KnobOutputFile.Value().c_str());
    fast_forward_count = KnobFastForwardTill.Value();

    // Add instrumentation functions.
    TRACE_AddInstrumentFunction(BBTraceRoutine, 0); // BB-level instrumentation

    PIN_AddFiniFunction(Fini, 0);

    PIN_StartProgram();

    return 0;
}