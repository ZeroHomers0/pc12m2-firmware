// ProbeProgram.java — check program language / memory blocks / instruction presence
// @category LPC1765
import ghidra.app.script.GhidraScript;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;

public class ProbeProgram extends GhidraScript {
    @Override
    public void run() throws Exception {
        println("language = " + currentProgram.getLanguage().getLanguageID());
        println("imageBase = " + currentProgram.getImageBase());
        for (MemoryBlock b : currentProgram.getMemory().getBlocks()) {
            println("block " + b.getName() + " " + b.getStart() + ".." + b.getEnd()
                + " size=" + b.getSize() + " init=" + b.isInitialized());
        }
        InstructionIterator it = currentProgram.getListing().getInstructions(true);
        int n = 0;
        while (it.hasNext() && n < 200000) { it.next(); n++; }
        println("instruction count (first 200k scan) = " + n);
        // check a known address
        Instruction i = currentProgram.getListing().getInstructionAt(toAddr("0x458c"));
        println("instruction at 0x458c = " + (i == null ? "NULL" : i.toString()));
    }
}
