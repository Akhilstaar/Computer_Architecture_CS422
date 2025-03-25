#include <iostream>
#include <fstream>
#include <set>
#include <iomanip>
#include "pin.H"
#include <cstdlib>
#include <cstring>

/* Macro and type definitions */
#define BILLION 1000000000
#define CLAMP(value, min, max) ((value) < (min) ? (min) : ((value) > (max) ? (max) : (value)))

/* Branch Predictor Constants */
#define BIMODAL_SIZE 512
#define SAG_BHT_SIZE 1024
#define SAG_PHT_SIZE 512
#define GAG_PHT_SIZE 512
#define GSHARE_PHT_SIZE 512
#define HYBRID_META_SIZE 512
#define GLOBAL_HISTORY_BITS 9
#define BTB_SETS 128
#define BTB_WAYS 4
#define PATH_HISTORY_BITS 7

using namespace std;

/* Branch Predictor Types */
typedef enum
{
    BP_FNBT = 0,          // Static Forward not-taken, Backward taken
    BP_BIMODAL,           // Bimodal predictor
    BP_SAG,               // SAg predictor
    BP_GAG,               // GAg predictor
    BP_GSHARE,            // gshare predictor
    BP_HYBRID_SAG_GAG,    // Hybrid of SAg and GAg
    BP_HYBRID_MAJORITY,   // Hybrid with majority vote
    BP_HYBRID_TOURNAMENT, // Hybrid with tournament predictor
    BP_COUNT
} BP_TYPE;

/* Branch Statistics */
typedef struct
{
    UINT64 total;          // Total number of branches
    UINT64 forward;        // Forward branches
    UINT64 backward;       // Backward branches
    UINT64 mispredictions; // Total mispredictions
    UINT64 forward_misp;   // Forward branch mispredictions
    UINT64 backward_misp;  // Backward branch mispredictions
} BRANCH_STATS;

struct TARGET_STATS
{
    UINT64 accesses = 0;
    UINT64 misses = 0;
    UINT64 mispredictions = 0;
};

struct BTB_ENTRY
{
    bool valid = false;
    ADDRINT tag = 0;
    ADDRINT target = 0;
    UINT32 lru = 0;
};

/* Global variables */
std::ostream *out = &cerr;

ADDRINT fastForwardDone = 0;
UINT64 icount = 0; // number of dynamically executed instructions

UINT64 fastForwardIns; // number of instruction to fast forward
UINT64 maxIns;         // maximum number of instructions to simulate

/* Global variables for branch predictors */
// Branch direction predictors
UINT8 bimodal_pht[BIMODAL_SIZE]; // 2-bit saturating counters for bimodal
UINT16 sag_bht[SAG_BHT_SIZE];    // Branch history table for SAg (9-bit per entry)
UINT8 sag_pht[SAG_PHT_SIZE];     // Pattern history table for SAg (2-bit counters)
UINT16 gag_ghr = 0;              // Global history register for GAg
UINT8 gag_pht[GAG_PHT_SIZE];     // Pattern history table for GAg (3-bit counters)
UINT16 gshare_ghr = 0;
UINT8 gshare_pht[GSHARE_PHT_SIZE];              // Pattern history table for gshare (3-bit counters)
UINT8 hybrid_meta_sag_gag[HYBRID_META_SIZE];    // Meta-predictor for SAg vs GAg
UINT8 hybrid_meta_gag_gshare[HYBRID_META_SIZE]; // Meta-predictor for GAg vs gshare
UINT8 hybrid_meta_gshare_sag[HYBRID_META_SIZE]; // Meta-predictor for gshare vs SAg

// Statistics
BRANCH_STATS bp_stats[BP_COUNT]; // Stats for branch predictors

// Part B: Target Predictors
BTB_ENTRY btb_pc[BTB_SETS][BTB_WAYS];   // PC-indexed BTB
BTB_ENTRY btb_hash[BTB_SETS][BTB_WAYS]; // PC+history hashed BTB
UINT8 path_history = 0;                 // 7-bit path history
TARGET_STATS target_stats[2];

/* Command line switches */
KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "o", "", "specify file name for HW1 output");
KNOB<UINT64> KnobFastForward(KNOB_MODE_WRITEONCE, "pintool", "f", "0", "number of instructions to fast forward in billions");

/* Utilities */

/* Print out help message. */
INT32 Usage()
{
    cerr << "CS422 Homework 2" << endl;
    cerr << KNOB_BASE::StringKnobSummary() << endl;
    return -1;
}

VOID InsCount(UINT32 c)
{
    icount += c;
}

ADDRINT FastForward(void)
{
    return (icount >= fastForwardIns && icount < maxIns);
}

VOID FastForwardDone(void)
{
    fastForwardDone = 1;
}

ADDRINT IsFastForwardDone(void)
{
    return fastForwardDone;
}

ADDRINT Terminate(void)
{
    return (icount >= maxIns);
}

VOID PrintResults(void)
{
    *out << "===============================================" << endl;
    *out << endl;
    *out << "Branch Predictor Statistics:" << endl;
    
    const char *bp_names[] = {"FNBT", "Bimodal", "SAg", "GAg", "gshare", "Hybrid SAg-GAg", "Hybrid Majority", "Hybrid Tournament"};
    
    for (UINT32 i = 0; i < BP_COUNT; i++)
    {
        double forward_rate = bp_stats[i].forward > 0 ? (double)bp_stats[i].forward_misp / bp_stats[i].forward : 0;
        double backward_rate = bp_stats[i].backward > 0 ? (double)bp_stats[i].backward_misp / bp_stats[i].backward : 0;
        double overall_rate = bp_stats[i].total > 0 ? (double)bp_stats[i].mispredictions / bp_stats[i].total : 0;
        
        *out << bp_names[i] << " : Accesses " << bp_stats[i].total << ", Mispredictions " << bp_stats[i].mispredictions << "(" << overall_rate << ") , " << "Forward branches " << bp_stats[i].forward << " , Forward mispredictions " << bp_stats[i].forward_misp << " (" << forward_rate << "), Backward branches " << bp_stats[i].backward << ", Backward mispredictions " << bp_stats[i].backward_misp << "(" << backward_rate << ")" << endl;
    }
    
    *out << endl;
    *out << "Branch Target Predictor";
    *out << "\n===============================================\n";
    
    const char *target_names[] = {"BTB1", "BTB2"};
    for (INT32 i = 0; i < 2; i++)
    {
        double miss_rate = target_stats[i].accesses ? (double)target_stats[i].mispredictions / target_stats[i].accesses : 0;
        double btb_miss_rate = target_stats[i].accesses ? (double)target_stats[i].misses / target_stats[i].accesses : 0;
        *out << target_names[i] << " : Accesses " << target_stats[i].accesses << ", Missses " << target_stats[i].misses << "(" << btb_miss_rate << ") , Mispredictions " << target_stats[i].mispredictions << " (" << miss_rate << ")" << endl;
    }

    *out << "===============================================" << endl;

    return;
}

VOID InitPredictors()
{
    // Bimodal: 512 entries, 2-bit counters initialized to 2 (weakly taken)
    for (int i = 0; i < BIMODAL_SIZE; i++)
        bimodal_pht[i] = 2;
    // SAg: BHT 1024 entries, 9-bit history initialized to 0
    memset(sag_bht, 0, sizeof(sag_bht));
    // SAg: PHT 512 entries, 2-bit counters initialized to 2
    for (int i = 0; i < SAG_PHT_SIZE; i++)
        sag_pht[i] = 2;
    // GAg: PHT 512 entries, 3-bit counters initialized to 4 (weakly taken)
    for (int i = 0; i < GAG_PHT_SIZE; i++)
        gag_pht[i] = 4;
    // gshare: PHT 512 entries, 3-bit counters initialized to 4 (weakly taken)
    for (int i = 0; i < GSHARE_PHT_SIZE; i++)
        gshare_pht[i] = 4;
    // Hybrid meta-predictors: 512 entries each, 2-bit counters initialized to 1 (neutral)
    memset(hybrid_meta_sag_gag, 1, sizeof(hybrid_meta_sag_gag));
    memset(hybrid_meta_gag_gshare, 1, sizeof(hybrid_meta_gag_gshare));
    memset(hybrid_meta_gshare_sag, 1, sizeof(hybrid_meta_gshare_sag));
    // BTBs are zero-initialized by default (valid = false)

    return;
}

/* Branch prediction functions */
BOOL PredictBimodal(ADDRINT pc)
{
    return bimodal_pht[pc % BIMODAL_SIZE] >= 2; // Predict taken if counter >= 2
}

BOOL PredictSAg(ADDRINT pc)
{
    UINT32 bht_index = pc % SAG_BHT_SIZE;
    UINT32 pht_index = sag_bht[bht_index] % SAG_PHT_SIZE;
    return sag_pht[pht_index] >= 2; // Predict taken if counter >= 2
}

BOOL PredictGAg()
{
    return gag_pht[gag_ghr % GAG_PHT_SIZE] >= 4; // Predict taken if counter >= 4 (3-bit counter midpoint)
}

BOOL PredictGshare(ADDRINT pc)
{
    UINT32 index = (pc ^ gag_ghr) % GSHARE_PHT_SIZE;
    return gshare_pht[index] >= 4; // Predict taken if counter >= 4 (3-bit counter midpoint)
}

BOOL PredictHybridSAgGAg(ADDRINT pc)
{
    UINT32 meta_index = gag_ghr % HYBRID_META_SIZE;
    return (hybrid_meta_sag_gag[meta_index] >= 2) ? PredictGAg() : PredictSAg(pc);
}

BOOL PredictHybridMajority(ADDRINT pc)
{
    int cnt = PredictSAg(pc) + PredictGAg() + PredictGshare(pc);
    return cnt >= 2;
}

BOOL PredictHybridTournament(ADDRINT pc)
{
    UINT32 meta_index = gag_ghr % HYBRID_META_SIZE;
    BOOL choose_GAg = (hybrid_meta_sag_gag[meta_index] >= 2);
    if (!choose_GAg)
    { // W is SAg
        return (hybrid_meta_gshare_sag[meta_index] >= 2) ? PredictGshare(pc) : PredictSAg(pc);
    }
    else
    { // W is GAg
        return (hybrid_meta_gag_gshare[meta_index] >= 2) ? PredictGshare(pc) : PredictGAg();
    }
}

/* Update branch predictor state */
VOID UpdateBranchPredictors(ADDRINT pc, BOOL taken)
{
    // Update Bimodal
    UINT32 idx;
    idx = pc % BIMODAL_SIZE;
    bimodal_pht[idx] = CLAMP(bimodal_pht[idx] + (taken ? 1 : -1), 0, 3);

    // SAg: Update PHT (2-bit) and BHT (9-bit history)
    UINT32 bht_idx = pc % SAG_BHT_SIZE;
    idx = sag_bht[bht_idx] % SAG_PHT_SIZE;
    sag_pht[idx] = CLAMP(sag_pht[idx] + (taken ? 1 : -1), 0, 3);
    sag_bht[bht_idx] = ((sag_bht[bht_idx] << 1) | taken) & 0x1FF;

    // GAg: Update PHT (3-bit) and GHR (9-bit)
    idx = gag_ghr % GAG_PHT_SIZE;
    gag_pht[idx] = CLAMP(gag_pht[idx] + (taken ? 1 : -1), 0, 7);
    gag_ghr = ((gag_ghr << 1) | taken) & 0x1FF;

    // gshare: Update PHT (3-bit) and GHR (9-bit)
    idx = (pc ^ gshare_ghr) % GSHARE_PHT_SIZE;
    gshare_pht[idx] = CLAMP(gshare_pht[idx] + (taken ? 1 : -1), 0, 7);
    gshare_ghr = ((gshare_ghr << 1) | taken) & 0x1FF;

    // Update Meta-Predictors based on correctness
    BOOL sag_correct = (PredictSAg(pc) == taken);
    BOOL gag_correct = (PredictGAg() == taken);
    BOOL gshare_correct = (PredictGshare(pc) == taken);
    UINT32 meta_idx = gag_ghr % HYBRID_META_SIZE;
    if (sag_correct != gag_correct)
    {
        hybrid_meta_sag_gag[meta_idx] = CLAMP(hybrid_meta_sag_gag[meta_idx] + (sag_correct ? -1 : 1), 0, 3);
    }
    if (gag_correct != gshare_correct)
    {
        hybrid_meta_gag_gshare[meta_idx] = CLAMP(hybrid_meta_gag_gshare[meta_idx] + (gag_correct ? -1 : 1), 0, 3);
    }
    if (gshare_correct != sag_correct)
    {
        hybrid_meta_gshare_sag[meta_idx] = CLAMP(hybrid_meta_gshare_sag[meta_idx] + (gshare_correct ? -1 : 1), 0, 3);
    }

    return;
}

/* Analysis function for conditional branches */
VOID AnalyzeConditionalBranch(ADDRINT pc, BOOL taken, BOOL isForward)
{
    // Update stats for each predictor
    for (INT32 i = 0; i < BP_COUNT; i++)
    {
        bp_stats[i].total++;
        if (isForward)
        {
            bp_stats[i].forward++;
        }
        else
        {
            bp_stats[i].backward++;
        }

        BOOL prediction = false;

        // Get prediction based on predictor type
        switch (i)
        {
        case BP_FNBT:
            prediction = !isForward; // Forward not-taken, backward taken
            break;
        case BP_BIMODAL:
            prediction = PredictBimodal(pc);
            break;
        case BP_SAG:
            prediction = PredictSAg(pc);
            break;
        case BP_GAG:
            prediction = PredictGAg();
            break;
        case BP_GSHARE:
            prediction = PredictGshare(pc);
            break;
        case BP_HYBRID_SAG_GAG:
            prediction = PredictHybridSAgGAg(pc);
            break;
        case BP_HYBRID_MAJORITY:
            prediction = PredictHybridMajority(pc);
            break;
        case BP_HYBRID_TOURNAMENT:
            prediction = PredictHybridTournament(pc);
            break;
        }

        // Check for misprediction
        if (prediction != taken)
        {
            bp_stats[i].mispredictions++;
            if (isForward)
            {
                bp_stats[i].forward_misp++;
            }
            else
            {
                bp_stats[i].backward_misp++;
            }
        }
    }

    // Update predictor state with actual outcome
    UpdateBranchPredictors(pc, taken);
    path_history = ((path_history << 1) | taken) & 0x7F; // Update only for conditional branches
}

/* BTB Access functions */
INT32 FindBTBEntry(BTB_ENTRY btb[BTB_WAYS], ADDRINT pc)
{
    for (INT32 i = 0; i < BTB_WAYS; i++)
    {
        if (btb[i].valid && btb[i].tag == pc)
            return i;
    }
    return -1;
}

VOID UpdateLRU(BTB_ENTRY btb[BTB_WAYS], INT32 used_way)
{
    for (INT32 i = 0; i < BTB_WAYS; i++)
    {
        if (i != used_way)
            btb[i].lru++;
    }
    if (used_way != -1)
        btb[used_way].lru = 0;
}

ADDRINT PredictTarget(ADDRINT pc, INT32 predictor, ADDRINT fall_through)
{
    UINT32 set = (predictor == 1) ? (pc ^ path_history) % BTB_SETS : pc % BTB_SETS;
    BTB_ENTRY *btb = (predictor == 0) ? btb_pc[set] : btb_hash[set];
    INT32 way = FindBTBEntry(btb, pc);
    if (way != -1)
    {
        UpdateLRU(btb, way);
        return btb[way].target;
    }
    else
    {
        target_stats[predictor].misses++;
        return fall_through;
    }
}

VOID UpdateBTB(ADDRINT pc, ADDRINT target, INT32 predictor)
{
    UINT32 set = (predictor == 1) ? (pc ^ path_history) % BTB_SETS : pc % BTB_SETS;
    BTB_ENTRY *btb = (predictor == 0) ? btb_pc[set] : btb_hash[set];
    INT32 way = FindBTBEntry(btb, pc);
    if (way != -1)
    {
        // Update existing entry
        btb[way].target = target;
        UpdateLRU(btb, way);
    }
    else
    {
        // Allocate new entry using LRU
        INT32 lru_way = 0;
        UINT32 max_lru = 0;
        for (INT32 i = 0; i < BTB_WAYS; i++)
        {
            if (!btb[i].valid)
            {
                lru_way = i;
                break;
            }
            if (btb[i].lru > max_lru)
            {
                max_lru = btb[i].lru;
                lru_way = i;
            }
        }
        btb[lru_way].valid = true;
        btb[lru_way].tag = pc;
        btb[lru_way].target = target;
        UpdateLRU(btb, lru_way);
    }
}

/* Analysis function for indirect control transfers */
VOID AnalyzeIndirectBranch(ADDRINT pc, ADDRINT target, UINT32 size)
{
    ADDRINT fall_through = pc + size;
    for (INT32 i = 0; i < 2; i++)
    {
        target_stats[i].accesses++;
        ADDRINT predicted = PredictTarget(pc, i, fall_through);
        if (predicted != target)
        {
            target_stats[i].mispredictions++;
        }
        UpdateBTB(pc, target, i);
    }
}

VOID ExitRoutine()
{
    PrintResults();
    exit(0);
}

/* Instruction instrumentation routine */
VOID Trace(TRACE trace, VOID *v)
{
    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl))
    {
        BBL_InsertIfCall(bbl, IPOINT_BEFORE, (AFUNPTR)Terminate, IARG_END);
        BBL_InsertThenCall(bbl, IPOINT_BEFORE, (AFUNPTR)ExitRoutine, IARG_END);

        BBL_InsertIfCall(bbl, IPOINT_BEFORE, (AFUNPTR)FastForward, IARG_END);
        BBL_InsertThenCall(bbl, IPOINT_BEFORE, (AFUNPTR)FastForwardDone, IARG_END);

        for (INS ins = BBL_InsHead(bbl);; ins = INS_Next(ins))
        {
            
            /* For conditional branches */
            if (INS_Category(ins) == XED_CATEGORY_COND_BR)
            // if (INS_HasFallThrough(ins))
            {
                ADDRINT target = INS_DirectControlFlowTargetAddress(ins);
                BOOL isForward = (target > INS_Address(ins));
                
                INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)IsFastForwardDone, IARG_END);
                INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)AnalyzeConditionalBranch,
                IARG_INST_PTR,        // PC
                IARG_BRANCH_TAKEN,    // Whether branch is taken
                IARG_BOOL, isForward, // Whether branch is forward
                IARG_END);
            }
            
            /* For indirect control transfers */
            else if (INS_IsIndirectControlFlow(ins))
            {
                INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)IsFastForwardDone, IARG_END);
                INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)AnalyzeIndirectBranch,
                IARG_INST_PTR,           // PC
                IARG_BRANCH_TARGET_ADDR, // Actual target
                IARG_UINT32, INS_Size(ins),
                IARG_END);
            }

            if (ins == BBL_InsTail(bbl))
                break;
        }

        /* Called for each BBL */
        BBL_InsertCall(bbl, IPOINT_BEFORE, (AFUNPTR)InsCount, IARG_UINT32, BBL_NumIns(bbl), IARG_END);
    }
}

/* Fini routine */
VOID Fini(INT32 code, VOID *v)
{
    *out << "Execution terminated before completing instrumentation on all specified instructions." << endl;
    PrintResults();
    return;
}

int main(int argc, char *argv[])
{
    // Initialize PIN library. Print help message if -h(elp) is specified
    // in the command line or the command line is invalid
    if (PIN_Init(argc, argv))
        return Usage();

    /* Set number of instructions to fast forward and simulate */
    fastForwardIns = KnobFastForward.Value() * BILLION;
    maxIns = fastForwardIns + BILLION;

    string fileName = KnobOutputFile.Value();

    if (!fileName.empty())
        out = new std::ofstream(fileName.c_str());

    InitPredictors();

    // Register function to be called to instrument instructions
    TRACE_AddInstrumentFunction(Trace, 0);

    // Register function to be called when the application exits
    PIN_AddFiniFunction(Fini, 0);

    cerr << "===============================================" << endl;
    cerr << "This application is instrumented by HW2" << endl;
    if (!KnobOutputFile.Value().empty())
        cerr << "See file " << KnobOutputFile.Value() << " for analysis results" << endl;
    cerr << "===============================================" << endl;

    // Start the program, never returns
    PIN_StartProgram();

    return 0;
}