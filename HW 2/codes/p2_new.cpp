#include <iostream>
#include <fstream>
#include <iomanip>
#include "pin.H"
#include <cstdlib>
#include <cstring>

#define BILLION 1000000000

// Configuration Constants
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

/** Enumerations and Structures **/

enum BP_TYPE
{
    BP_FNBT,
    BP_BIMODAL,
    BP_SAG,
    BP_GAG,
    BP_GSHARE,
    BP_HYBRID_SAG_GAG,
    BP_HYBRID_MAJORITY,
    BP_HYBRID_TOURNAMENT,
    BP_COUNT
};

struct BRANCH_STATS
{
    UINT64 total = 0;
    UINT64 forward = 0;
    UINT64 backward = 0;
    UINT64 mispredictions = 0;
    UINT64 forward_misp = 0;
    UINT64 backward_misp = 0;
};

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

/** Global Variables **/

ostream *out = &cerr;
UINT64 icount = 0;
UINT64 fastForwardIns, maxIns;
bool fastForwardDone = false;

// Part A: Direction Predictors
UINT8 bimodal_pht[BIMODAL_SIZE];                // 2-bit counters
UINT16 sag_bht[SAG_BHT_SIZE];                   // 9-bit branch history
UINT8 sag_pht[SAG_PHT_SIZE];                    // 2-bit counters
UINT16 gag_ghr = 0;                             // 9-bit global history
UINT8 gag_pht[GAG_PHT_SIZE];                    // 3-bit counters
UINT16 gshare_ghr = 0;                          // 9-bit global history
UINT8 gshare_pht[GSHARE_PHT_SIZE];              // 3-bit counters
UINT8 hybrid_meta_sag_gag[HYBRID_META_SIZE];    // 2-bit meta-predictors
UINT8 hybrid_meta_gag_gshare[HYBRID_META_SIZE]; // 2-bit meta-predictors
UINT8 hybrid_meta_gshare_sag[HYBRID_META_SIZE]; // 2-bit meta-predictors
BRANCH_STATS bp_stats[BP_COUNT];

// Part B: Target Predictors
BTB_ENTRY btb_pc[BTB_SETS][BTB_WAYS];   // PC-indexed BTB
BTB_ENTRY btb_hash[BTB_SETS][BTB_WAYS]; // PC+history hashed BTB
UINT8 path_history = 0;                 // 7-bit path history
TARGET_STATS target_stats[2];

/** Knobs for Command-Line Options **/

KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "o", "", "output file");
KNOB<UINT64> KnobFastForward(KNOB_MODE_WRITEONCE, "pintool", "f", "0", "fast forward (billions)");

/** Utility Macro **/

#define CLAMP(value, min, max) ((value) < (min) ? (min) : ((value) > (max) ? (max) : (value)))

/** Function Definitions **/

INT32 Usage()
{
    cerr << "CS422 Branch Predictor Analysis Tool\n";
    cerr << KNOB_BASE::StringKnobSummary() << endl;
    return -1;
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
}

VOID InsCount(UINT32 num_ins)
{
    icount += num_ins;
    if (!fastForwardDone && icount >= fastForwardIns)
    {
        fastForwardDone = true;
    }
}

ADDRINT IsFastForwardDone()
{
    return fastForwardDone;
}

ADDRINT Terminate()
{
    return icount >= maxIns;
}

/** Direction Prediction Functions **/

BOOL PredictBimodal(ADDRINT pc)
{
    return bimodal_pht[pc % BIMODAL_SIZE] >= 2;
}

BOOL PredictSAg(ADDRINT pc)
{
    UINT32 bht_idx = pc % SAG_BHT_SIZE;
    UINT32 pht_idx = sag_bht[bht_idx] % SAG_PHT_SIZE;
    return sag_pht[pht_idx] >= 2;
}

BOOL PredictGAg()
{
    return gag_pht[gag_ghr % GAG_PHT_SIZE] >= 4;
}

BOOL PredictGshare(ADDRINT pc)
{
    UINT32 idx = (pc ^ gshare_ghr) % GSHARE_PHT_SIZE;
    return gshare_pht[idx] >= 4;
}

BOOL PredictHybridSAgGAg(ADDRINT pc)
{
    UINT32 meta_idx = gag_ghr % HYBRID_META_SIZE;
    return (hybrid_meta_sag_gag[meta_idx] >= 2) ? PredictGAg() : PredictSAg(pc);
}

BOOL PredictHybridMajority(ADDRINT pc)
{
    int cnt = PredictSAg(pc) + PredictGAg() + PredictGshare(pc);
    return cnt >= 2;
}

BOOL PredictHybridTournament(ADDRINT pc)
{
    UINT32 meta_idx = gag_ghr % HYBRID_META_SIZE;
    BOOL choose_GAg = (hybrid_meta_sag_gag[meta_idx] >= 2);
    if (!choose_GAg)
    { // W is SAg
        return (hybrid_meta_gshare_sag[meta_idx] >= 2) ? PredictGshare(pc) : PredictSAg(pc);
    }
    else
    { // W is GAg
        return (hybrid_meta_gag_gshare[meta_idx] >= 2) ? PredictGshare(pc) : PredictGAg();
    }
}

VOID UpdatePredictors(ADDRINT pc, BOOL taken)
{
    UINT32 idx;
    // Bimodal: 2-bit saturating counter
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
        hybrid_meta_sag_gag[meta_idx] = CLAMP(
            hybrid_meta_sag_gag[meta_idx] + (sag_correct ? -1 : 1), 0, 3);
    }
    if (gag_correct != gshare_correct)
    {
        hybrid_meta_gag_gshare[meta_idx] = CLAMP(
            hybrid_meta_gag_gshare[meta_idx] + (gag_correct ? -1 : 1), 0, 3);
    }
    if (gshare_correct != sag_correct)
    {
        hybrid_meta_gshare_sag[meta_idx] = CLAMP(
            hybrid_meta_gshare_sag[meta_idx] + (gshare_correct ? -1 : 1), 0, 3);
    }
}

/** Target Prediction Functions **/

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

/** Analysis Functions **/

VOID AnalyzeBranch(ADDRINT pc, BOOL taken, BOOL isForward)
{
    for (INT32 i = 0; i < BP_COUNT; i++)
    {
        BOOL prediction = false;
        switch (i)
        {
        case BP_FNBT:
            prediction = !isForward;
            break; // Forward not-taken, backward taken
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
        if (prediction != taken)
        {
            bp_stats[i].mispredictions++;
            if (isForward)
                bp_stats[i].forward_misp++;
            else
                bp_stats[i].backward_misp++;
        }
        bp_stats[i].total++;
        if (isForward)
            bp_stats[i].forward++;
        else
            bp_stats[i].backward++;
    }
    UpdatePredictors(pc, taken);
    path_history = ((path_history << 1) | taken) & 0x7F; // Update only for conditional branches
}

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

/** Instrumentation Function **/

VOID Trace(TRACE trace, VOID *v)
{
    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl))
    {
        BBL_InsertCall(bbl, IPOINT_BEFORE, (AFUNPTR)InsCount,
                       IARG_UINT32, BBL_NumIns(bbl), IARG_END);
        BBL_InsertIfCall(bbl, IPOINT_BEFORE, (AFUNPTR)Terminate, IARG_END);
        BBL_InsertThenCall(bbl, IPOINT_BEFORE, (AFUNPTR)ExitRoutine, IARG_END);
        for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins))
        {
            if (INS_IsBranch(ins))
            {
                if (INS_IsIndirectControlFlow(ins))
                {
                    INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)IsFastForwardDone, IARG_END);
                    INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)AnalyzeIndirectBranch,
                                       IARG_INST_PTR, IARG_BRANCH_TARGET_ADDR, IARG_UINT32, INS_Size(ins), IARG_END);
                }
                else if (INS_HasFallThrough(ins))
                {
                    ADDRINT target = INS_DirectControlFlowTargetAddress(ins);
                    BOOL isForward = (target > INS_Address(ins));
                    INS_InsertIfCall(ins, IPOINT_BEFORE, (AFUNPTR)IsFastForwardDone, IARG_END);
                    INS_InsertThenCall(ins, IPOINT_BEFORE, (AFUNPTR)AnalyzeBranch,
                                       IARG_INST_PTR, IARG_BRANCH_TAKEN, IARG_BOOL, isForward, IARG_END);
                }
            }
        }
    }
}

/** Output Function **/

VOID PrintResults()
{
    *out << "\nBranch Direction Predictor Accuracy:\n";
    *out << "======================================================================\n";
    *out << left << setw(20) << "Predictor"
         << setw(20) << "Forward Miss Rate"
         << setw(20) << "Backward Miss Rate"
         << "Overall Miss Rate\n";
    *out << "----------------------------------------------------------------------\n";
    const char *dir_names[] = {"FNBT", "Bimodal", "SAg", "GAg", "gshare",
                               "Hybrid SAg-GAg", "Hybrid Majority", "Hybrid Tournament"};
    for (INT32 i = 0; i < BP_COUNT; i++)
    {
        double fwd = bp_stats[i].forward ? (double)bp_stats[i].forward_misp / bp_stats[i].forward : 0;
        double bwd = bp_stats[i].backward ? (double)bp_stats[i].backward_misp / bp_stats[i].backward : 0;
        double overall = bp_stats[i].total ? (double)bp_stats[i].mispredictions / bp_stats[i].total : 0;
        *out << setw(20) << dir_names[i] << fixed << setprecision(4)
             << setw(20) << fwd << setw(20) << bwd << overall << endl;
    }
    *out << "\nIndirect Target Predictor Accuracy:\n";
    *out << "==============================================================\n";
    *out << left << setw(15) << "Predictor"
         << setw(15) << "Miss Rate"
         << setw(15) << "BTB Miss Rate" << endl;
    *out << "--------------------------------------------------------------\n";
    const char *target_names[] = {"PC-indexed", "PC+History"};
    for (INT32 i = 0; i < 2; i++)
    {
        double miss_rate = target_stats[i].accesses ? (double)target_stats[i].mispredictions / target_stats[i].accesses : 0;
        double btb_miss_rate = target_stats[i].accesses ? (double)target_stats[i].misses / target_stats[i].accesses : 0;
        *out << setw(15) << target_names[i] << fixed << setprecision(4)
             << setw(15) << miss_rate << setw(15) << btb_miss_rate << endl;
    }
}

VOID ExitRoutine()
{
    PrintResults();
    if (out != &cerr)
        delete out;
    exit(0);
}

/** Main Function **/

int main(int argc, char *argv[])
{
    if (PIN_Init(argc, argv))
        return Usage();
    InitPredictors();
    fastForwardIns = KnobFastForward.Value() * BILLION;
    maxIns = fastForwardIns + BILLION;
    if (!KnobOutputFile.Value().empty())
    {
        out = new ofstream(KnobOutputFile.Value().c_str());
        if (!out->good())
        {
            cerr << "Cannot open output file: " << KnobOutputFile.Value() << endl;
            return -1;
        }
    }
    TRACE_AddInstrumentFunction(Trace, 0);
    PIN_StartProgram();
    return 0;
}
