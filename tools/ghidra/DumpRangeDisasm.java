// DumpRangeDisasm.java — dump raw disassembly (with literal-pool refs) for fixed address ranges
// Used for ISR handlers that Ghidra's function manager does NOT mark as functions
// (they are reached only via the vector table, not BL).
// Output: user-selected evidence/reverse/disassembly/functions directory
// @category LPC1765
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.listing.Listing;
import ghidra.program.model.symbol.Reference;
import ghidra.program.disassemble.Disassembler;
import java.io.FileWriter;
import java.io.PrintWriter;

public class DumpRangeDisasm extends GhidraScript {

    // {name, start, end}  — end is exclusive
    static final String[][] RANGES = {
        {"wdt_init_wd_feed", "0x200", "0x248"},
        {"timer0_init", "0x248", "0x298"},
        {"main_auth", "0x5cc", "0x6a0"},
    };

    @Override
    public void run() throws Exception {
        java.io.File outDir = askDirectory("选择反汇编输出目录", "选择");
        Listing listing = currentProgram.getListing();
        Disassembler dis = Disassembler.getDisassembler(currentProgram, monitor, null);

        for (String[] r : RANGES) {
            String name = r[0];
            Address start = toAddr(r[1]);
            Address end = toAddr(r[2]);
            AddressSet range = new AddressSet(start, end);

            // force disassembly of the range (follow flows to skip literal pools)
            dis.disassemble(start, range, true);
            // sweep any remaining undefined 2-byte-aligned slots (jump-table /
            // skipped basic-block entries not reached by flow following)
            for (long a = start.getOffset(); a < end.getOffset(); a += 2) {
                Address ad = toAddr(a);
                if (listing.getInstructionAt(ad) == null
                        && listing.getDefinedDataAt(ad) == null) {
                    dis.disassemble(ad, new AddressSet(ad, ad), true);
                }
            }

            String path = new java.io.File(outDir, r[1].substring(2) + "_" + name + ".txt").getPath();
            PrintWriter w = new PrintWriter(new FileWriter(path));
            w.println("# " + name + " range=" + r[1] + ".." + r[2]);

            InstructionIterator it = listing.getInstructions(range, true);
            int n = 0;
            while (it.hasNext()) {
                Instruction ins = it.next();
                StringBuilder sb = new StringBuilder();
                sb.append(ins.getAddress()).append("  ").append(ins.getMnemonicString());
                int nop = ins.getNumOperands();
                for (int i = 0; i < nop; i++) {
                    sb.append(i == 0 ? "  " : ", ");
                    sb.append(ins.getDefaultOperandRepresentation(i));
                }
                w.println(sb.toString());

                Reference[] refs = ins.getReferencesFrom();
                for (Reference ref : refs) {
                    Address to = ref.getToAddress();
                    if (ref.getReferenceType().isData()) {
                        try {
                            long val = currentProgram.getMemory().getInt(to) & 0xffffffffL;
                            w.println("      ; ref 0x" + String.format("%08x", to.getOffset())
                                + " -> 0x" + String.format("%08x", val));
                        } catch (Exception e) {
                            w.println("      ; ref 0x" + String.format("%08x", to.getOffset()) + " (no mem)");
                        }
                    } else if (ref.getReferenceType().isCall()) {
                        w.println("      ; call -> 0x" + String.format("%08x", to.getOffset()));
                    } else if (ref.getReferenceType().isJump()) {
                        w.println("      ; jump -> 0x" + String.format("%08x", to.getOffset()));
                    }
                }
                n++;
            }
            w.close();
            println("Dumped " + name + " (" + n + " insns) -> " + path);
        }
    }
}
