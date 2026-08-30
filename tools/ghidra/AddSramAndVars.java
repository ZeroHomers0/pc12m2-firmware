// Add SRAM segment and create variable labels for LPC1765 firmware
// @category LPC1765
// @menupath Tools.LPC1765.Add SRAM + Variables
// @description Add 64KB SRAM at 0x10000000 and label the firmware global variables

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.mem.MemoryBlock;

public class AddSramAndVars extends GhidraScript {

    @Override
    public void run() throws Exception {

        // 1. Add SRAM block (idempotent)
        Memory mem = currentProgram.getMemory();
        Address sramStart = toAddr("0x10000000");
        MemoryBlock sram = mem.getBlock(sramStart);
        if (sram == null) {
            sram = mem.createUninitializedBlock("SRAM", sramStart, 0x10000, false);
            println("Added SRAM block: " + sram.getName() + " @ " + sram.getStart());
        } else {
            println("SRAM block already exists @ " + sram.getStart());
        }

        // 2. Create variable labels: { address, type, name }
        //    type: "b" = byte (uint8), "d" = dword (uint32)
        String[][] vars = {
            { "0x10002134", "d", "wdt_timeout_count" },
            { "0x10000006", "b", "tick_ready" },
            { "0x10001ff9", "b", "phase_cnt" },
            { "0x10001784", "b", "tick_countdown" },
            { "0x10001ffa", "b", "debounce_count" },
            { "0x10001ff8", "b", "freq_hz" },
            { "0x10002000", "b", "input_locked" },
            { "0x10002076", "b", "input_state" },
            { "0x100020c0", "b", "eint1_flag" },
            { "0x100020c1", "b", "eint2_flag" },
            { "0x100020c2", "b", "eint3_flag" },
            { "0x100020c4", "b", "hold_count" },
            { "0x10002074", "b", "disp_scan" },
            { "0x10002075", "b", "mode_byte" },
            { "0x1000165b", "b", "out_phase" },
            { "0x10001654", "b", "out_fine" },
            { "0x10001624", "d", "out_param" },
            { "0x1000162c", "d", "out_freq_adj" },
            { "0x10001628", "d", "cfg_word" },
            { "0x10001ffc", "d", "out_setpoint" },
            { "0x100020cc", "d", "out_scale" },
            { "0x1000205c", "d", "out_div" },
            { "0x10002068", "b", "flag_68" },
            { "0x1000206c", "b", "flag_6c" },
            { "0x10002070", "b", "flag_70" },
            { "0x1000203c", "b", "flag_3c" },
        };

        int ok = 0;
        for (String[] v : vars) {
            Address addr = toAddr(v[0]);
            try {
                if (v[1].equals("b")) {
                    createByte(addr);
                } else {
                    createDWord(addr);
                }
                createLabel(addr, v[2], true);
                println("OK: " + v[2] + " @ " + addr);
                ok++;
            } catch (Exception e) {
                println("FAIL: " + v[2] + " @ " + addr + " : " + e.getMessage());
            }
        }

        println("Done: " + ok + "/" + vars.length + " variables labeled");
    }
}
