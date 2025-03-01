#include <stdio.h>
#include "pin.H"
#include <iostream>
#include <fstream>

using namespace std;

ofstream OutFile;

// counters
enum CounterType {
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
UINT64 total_cycles = 0; // Counter for total cycles

// Analysis function to increment counters by a delta
VOID AddToCounter(UINT64 *counter) {
    *counter += 1;
    total_cycles += 1;
}

VOID AddToMemCounter(UINT64 *counter, UINT32 delta) {
    *counter += delta;
    total_cycles += delta * 70;
}

// Instrumentation function
VOID Instruction(INS ins, VOID *v) {

    // Check if it's a Type B (has memory operands)
    UINT32 memOperands = INS_MemoryOperandCount(ins);
    if (memOperands > 0) {
        for (UINT32 memOp = 0; memOp < memOperands; memOp++) {
            if (INS_MemoryOperandIsRead(ins, memOp)) {
                UINT32 size = INS_MemoryOperandSize(ins, memOp);
                UINT32 chunks = (size + 3) / 4;
                INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)AddToMemCounter, IARG_PTR, &Counters[LOAD],IARG_UINT32, chunks, IARG_END);
            }
            if (INS_MemoryOperandIsWritten(ins, memOp)) {
                UINT32 size = INS_MemoryOperandSize(ins, memOp);
                UINT32 chunks = (size + 3) / 4;
                INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)AddToMemCounter, IARG_PTR, &Counters[STORE],IARG_UINT32, chunks, IARG_END);
            }
        }
    }

    // Categorize into Type A
    xed_category_enum_t cat = static_cast<xed_category_enum_t>(INS_Category(ins));
    if (cat == XED_CATEGORY_NOP) {
        INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)AddToCounter, IARG_PTR, &Counters[NOP], IARG_UINT32, 1, IARG_END);
    } 
    else if (cat == XED_CATEGORY_CALL) {
        if (INS_IsDirectCall(ins)) {
            INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)AddToCounter, IARG_PTR, &Counters[DIRECT_CALL], IARG_UINT32, 1, IARG_END);
        } 
        else {
            INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)AddToCounter, IARG_PTR, &Counters[INDIRECT_CALL], IARG_UINT32, 1, IARG_END);
        }
    } 
    else if (cat == XED_CATEGORY_RET) {
        INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)AddToCounter, IARG_PTR, &Counters[RETURN], IARG_UINT32, 1, IARG_END);
    } 
    else if (cat == XED_CATEGORY_X87_ALU) {
        INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)AddToCounter, IARG_PTR, &Counters[FLOATING_POINT], IARG_UINT32, 1, IARG_END);
    } 
    else if (cat == XED_CATEGORY_UNCOND_BR) {
        INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)AddToCounter, IARG_PTR, &Counters[UNCOND_BR], IARG_UINT32, 1, IARG_END);
    } 
    else if (cat == XED_CATEGORY_COND_BR) {
        INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)AddToCounter, IARG_PTR, &Counters[COND_BR], IARG_UINT32, 1, IARG_END);
    } 
    else if (cat == XED_CATEGORY_LOGICAL) {
        INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)AddToCounter, IARG_PTR, &Counters[LOGICAL], IARG_UINT32, 1, IARG_END);
    } 
    else if (cat == XED_CATEGORY_ROTATE || cat == XED_CATEGORY_SHIFT) {
        INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)AddToCounter, IARG_PTR, &Counters[ROTATE_SHIFT], IARG_UINT32, 1, IARG_END);
    } 
    else if (cat == XED_CATEGORY_FLAGOP) {
        INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)AddToCounter, IARG_PTR, &Counters[FLAGOP], IARG_UINT32, 1, IARG_END);
    } 
    else if (cat == XED_CATEGORY_AVX || cat == XED_CATEGORY_AVX2 || cat == XED_CATEGORY_AVX2GATHER || cat == XED_CATEGORY_AVX512) {
        INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)AddToCounter, IARG_PTR, &Counters[VECTOR], IARG_UINT32, 1, IARG_END);
    } 
    else if (cat == XED_CATEGORY_CMOV) {
        INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)AddToCounter, IARG_PTR, &Counters[CMOV], IARG_UINT32, 1, IARG_END);
    } 
    else if (cat == XED_CATEGORY_MMX || cat == XED_CATEGORY_SSE) {
        INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)AddToCounter, IARG_PTR, &Counters[MMX_SSE], IARG_UINT32, 1, IARG_END);
    } 
    else if (cat == XED_CATEGORY_SYSCALL) {
        INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)AddToCounter, IARG_PTR, &Counters[SYSCALL], IARG_UINT32, 1, IARG_END);
    }
    else {
        INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)AddToCounter, IARG_PTR, &Counters[OTHER], IARG_UINT32, 1, IARG_END);
    }
}

// Fini function to write results
VOID Fini(INT32 code, VOID *v) {
    OutFile << "Loads: " << Counters[LOAD] << '\n';
    OutFile << "Stores: " << Counters[STORE] << '\n';
    OutFile << "NOPs: " << Counters[NOP] << '\n';
    OutFile << "Direct calls: " << Counters[DIRECT_CALL] << '\n';
    OutFile << "Indirect calls: " << Counters[INDIRECT_CALL] << '\n';
    OutFile << "Returns: " << Counters[RETURN] << '\n';
    OutFile << "Unconditional branches: " << Counters[UNCOND_BR] << '\n';
    OutFile << "Conditional branches: " << Counters[COND_BR] << '\n';
    OutFile << "Logical operations: " << Counters[LOGICAL] << '\n';
    OutFile << "Rotate and shift: " << Counters[ROTATE_SHIFT] << '\n';
    OutFile << "Flag operations: " << Counters[FLAGOP] << '\n';
    OutFile << "Vector instructions: " << Counters[VECTOR] << '\n';
    OutFile << "Conditional moves: " << Counters[CMOV] << '\n';
    OutFile << "MMX and SSE instructions: " << Counters[MMX_SSE] << '\n';
    OutFile << "System calls: " << Counters[SYSCALL] << '\n';
    OutFile << "Floating-point: " << Counters[FLOATING_POINT] << '\n';
    OutFile << "Others: " << Counters[OTHER] << '\n';

    // Calculate total instructions
    UINT64 total_instructions = 0;
    for (int i = 0; i < NUM_COUNTERS; i++) {
        total_instructions += Counters[i];
    }
    OutFile << "Total instructions: " << total_instructions << '\n';
    OutFile << "Total cycles: " << total_cycles << '\n';

    // Calculate CPI
    double cpi = static_cast<double>(total_cycles) / total_instructions;
    OutFile << "CPI: " << cpi << endl;

    OutFile.close();
}

KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "o", "inscount.out", "specify output file name");

INT32 Usage() {
    cerr << "This tool categorizes instructions, counts them dynamically, and calculates CPI" << '\n';
    cerr << '\n' << KNOB_BASE::StringKnobSummary() << endl;
    return -1;
}

int main(int argc, char *argv[]) {
    if (PIN_Init(argc, argv)) return Usage();

    OutFile.open(KnobOutputFile.Value().c_str());

    INS_AddInstrumentFunction(Instruction, 0);
    PIN_AddFiniFunction(Fini, 0);

    PIN_StartProgram();
    
    return 0;
}
