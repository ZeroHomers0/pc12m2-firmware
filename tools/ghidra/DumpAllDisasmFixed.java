// DumpAllDisasmFixed.java — dump disasm of every function to a fixed file (headless-safe)
// @category LPC1765
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;
import ghidra.program.model.listing.FunctionManager;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import java.io.FileWriter;
import java.io.PrintWriter;

public class DumpAllDisasmFixed extends GhidraScript {
    @Override
    public void run() throws Exception {
        FunctionManager fm = currentProgram.getFunctionManager();
        String out = "D:/code/PC12M-2/tools/_ghidra_proj/_pc12m2_all_disasm.txt";
        int nfunc = 0, nins = 0;
        try (PrintWriter w = new PrintWriter(new FileWriter(out))) {
            FunctionIterator it = fm.getFunctions(true);
            while (it.hasNext()) {
                Function f = it.next();
                w.println("=== " + f.getEntryPoint() + " " + f.getName() + " body=" + f.getBody().getNumAddresses());
                InstructionIterator ii = currentProgram.getListing().getInstructions(f.getBody(), true);
                while (ii.hasNext()) {
                    Instruction i = ii.next();
                    w.println(i.getAddress() + ": " + i.toString());
                    nins++;
                }
                nfunc++;
            }
            w.println("# FUNCS=" + nfunc + " INSTRUCTIONS=" + nins);
        }
        println("DUMPED " + nfunc + " funcs / " + nins + " insns -> " + out);
    }
}
