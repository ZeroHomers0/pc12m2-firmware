// DumpFuncsFixed.java — dump all function entries to a fixed file (headless-safe)
// @category LPC1765
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;
import ghidra.program.model.listing.FunctionManager;
import java.io.FileWriter;
import java.io.PrintWriter;

public class DumpFuncsFixed extends GhidraScript {
    @Override
    public void run() throws Exception {
        FunctionManager fm = currentProgram.getFunctionManager();
        int n = 0;
        StringBuilder sb = new StringBuilder();
        FunctionIterator it = fm.getFunctions(true);
        while (it.hasNext()) {
            Function f = it.next();
            sb.append(f.getEntryPoint()).append("  ").append(f.getName())
              .append("  body=").append(f.getBody().getNumAddresses()).append("\n");
            n++;
        }
        String out = "D:/code/PC12M-2/tools/_ghidra_proj/_pc12m2_functions.txt";
        try (PrintWriter w = new PrintWriter(new FileWriter(out))) {
            w.write(sb.toString());
            w.println("# total = " + n);
        }
        println("DUMPED " + n + " functions -> " + out);
    }
}
