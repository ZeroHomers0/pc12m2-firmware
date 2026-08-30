// Add SRAM segment for LPC1765 firmware (12-phase PC12M-2)
// @category LPC1765
// @menupath Tools.LPC1765.Add SRAM (no labels)
// @description Add 64KB SRAM at 0x10000000. NO variable labels — 12-phase
//   SRAM layout differs from 6-phase; labels must be derived from real disasm.
//   Copy of AddSramAndVars.java minus the (now-invalid) 6-phase labels.

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.mem.MemoryBlock;

public class AddSramOnly extends GhidraScript {

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

        println("Done: SRAM block present, labels deferred to 12-phase analysis");
    }
}
