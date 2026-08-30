// ListFunctions.java — dump all function entries + body sizes for module-level W7 check
// @category LPC1765
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;
import ghidra.program.model.listing.FunctionManager;
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.symbol.SymbolTable;
import ghidra.program.model.symbol.SymbolIterator;
import ghidra.program.model.symbol.SymbolType;
import java.io.FileWriter;
import java.io.PrintWriter;

public class ListFunctions extends GhidraScript {
    @Override
    public void run() throws Exception {
        java.io.File outDir = askDirectory("选择函数清单输出目录", "选择");
        FunctionManager fm = currentProgram.getFunctionManager();
        println("getFunctionCount = " + fm.getFunctionCount());
        println("getFunctionAt 0x458c = " + fm.getFunctionAt(toAddr("0x458c")));
        println("getFunctionContaining 0x458c = " + fm.getFunctionContaining(toAddr("0x458c")));

        PrintWriter w = new PrintWriter(new FileWriter(new java.io.File(outDir, "_all_functions.txt")));
        FunctionIterator it = fm.getFunctions(true);
        int n = 0;
        while (it.hasNext()) {
            Function f = it.next();
            w.println(f.getEntryPoint() + "  " + f.getName() + "  body=" + f.getBody().getNumAddresses());
            n++;
        }
        w.println("# total(funcManager) = " + n);
        w.close();

        // fallback: symbols of type FUNCTION
        PrintWriter w2 = new PrintWriter(new FileWriter(new java.io.File(outDir, "_all_func_symbols.txt")));
        SymbolTable st = currentProgram.getSymbolTable();
        SymbolIterator sit = st.getSymbolIterator();
        int ns = 0;
        while (sit.hasNext()) {
            Symbol s = sit.next();
            if (s.getSymbolType() == SymbolType.FUNCTION) {
                w2.println(s.getAddress() + "  " + s.getName());
                ns++;
            }
        }
        w2.println("# total(symbol FUNCTION) = " + ns);
        w2.close();
        println("Wrote funcManager=" + n + " symbolFUNC=" + ns);
    }
}
