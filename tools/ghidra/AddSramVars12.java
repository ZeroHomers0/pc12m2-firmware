// AddSramVars12.java — label 12-phase (PC12M-2) SRAM variables
// @category LPC1765
// @menupath Tools.LPC1765.Add SRAM + 12-phase vars
// @description Add 64KB SRAM at 0x10000000 and apply labels derived from
//   real 12-phase code analysis (ISR disassembly + access-pattern matching
//   vs 6-phase reference). Confidence tiers:
//     HIGH   — verified via code access pattern in 12-phase disasm
//     PLAUS  — same SRAM address as 6-phase AND referenced in 12-phase
//     NEW    — discovered new 12-phase variables
//   Names use 6-phase semantics where the role is proven; moved variables
//   carry their 6-phase name (they relocated in the 12-phase build).

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.symbol.SymbolTable;
import ghidra.program.model.symbol.SourceType;

public class AddSramVars12 extends GhidraScript {

    // [addr, name, tier]
    private static final String[][] VARS = {
        // ---- HIGH: verified via code access pattern ----
        {"0x10000006", "tick_ready",        "HIGH"},
        {"0x10001fd8", "freq_hz",           "HIGH"},
        {"0x10001fd9", "phase_cnt",         "HIGH"},
        {"0x10001fda", "debounce_count",    "HIGH"},
        {"0x1000204d", "mode_byte",         "HIGH"},
        {"0x1000204e", "input_pending",     "HIGH"},
        {"0x10002098", "eint2_flag",        "HIGH"},
        {"0x10002099", "eint3_flag",        "HIGH"},
        {"0x1000209a", "adc_flag",          "HIGH"},
        {"0x1000209c", "hold_count",        "HIGH"},
        {"0x1000161c", "out_param",         "HIGH"},
        {"0x10001fe0", "trigger_seq_state", "HIGH"},
        {"0x100020a4", "out_scale_work",    "HIGH"},
        {"0x10002034", "m0_angle",          "HIGH"},
        {"0x10001624", "adj_trim",          "HIGH"},
        {"0x10001764", "tick_countdown",    "HIGH"},
        {"0x10001fdc", "phase_in_raw",      "HIGH"},

        // ---- PLAUS: same address as 6-phase AND referenced in 12-phase ----
        {"0x1000162c", "out_freq_adj",      "PLAUS"},
        {"0x10001628", "cfg_word",          "PLAUS"},
        {"0x10001ffc", "out_setpoint",      "PLAUS"},
        {"0x10002074", "disp_scan",         "PLAUS"},
        {"0x10002000", "input_locked",      "PLAUS"},
        {"0x10002004", "setpoint2",         "PLAUS"},
        {"0x100020c0", "eint1_flag",        "PLAUS"},
        {"0x10001654", "out_fine",          "PLAUS"},

        // ---- NEW: discovered 12-phase variables ----
        {"0x10001794", "menu_index",        "NEW"},
        {"0x10001788", "disp_counter",      "NEW"},
        {"0x10001780", "frame_count",       "NEW"},
        {"0x10002340", "comm_frame_buf",    "NEW"},
        {"0x100016f7", "menu_state",        "NEW"},
        {"0x10001790", "disp_param",        "NEW"},
        {"0x10002278", "lookup_table",      "NEW"},
    };

    @Override
    public void run() throws Exception {
        // Add SRAM block (idempotent)
        Memory mem = currentProgram.getMemory();
        Address sramStart = toAddr("0x10000000");
        MemoryBlock sram = mem.getBlock(sramStart);
        if (sram == null) {
            sram = mem.createUninitializedBlock("SRAM", sramStart, 0x10000, false);
            println("Added SRAM block: " + sram.getName() + " @ " + sram.getStart());
        } else {
            println("SRAM block already exists @ " + sram.getStart());
        }

        // Apply labels
        SymbolTable st = currentProgram.getSymbolTable();
        int n = 0, nskip = 0;
        for (String[] v : VARS) {
            String addrStr = v[0], name = v[1], tier = v[2];
            Address a = toAddr(addrStr);
            if (!sram.contains(a)) {
                println("SKIP (outside SRAM): " + addrStr + " " + name);
                nskip++;
                continue;
            }
            // keep tier in the comment so the source of the name is visible
            st.createLabel(a, name, SourceType.USER_DEFINED);
            setLabelComment(a, "12-phase [" + tier + "] " + name);
            n++;
        }
        println("Applied " + n + " labels (" + nskip + " skipped). Tiers: HIGH=" + count("HIGH")
                + " PLAUS=" + count("PLAUS") + " NEW=" + count("NEW"));
    }

    private int count(String tier) {
        int c = 0;
        for (String[] v : VARS) if (v[2].equals(tier)) c++;
        return c;
    }

    private void setLabelComment(Address a, String comment) throws Exception {
        ghidra.program.model.symbol.Symbol[] syms = currentProgram.getSymbolTable().getSymbols(a);
        if (syms != null && syms.length > 0) {
            syms[0].setDescription(comment);
        }
    }
}
