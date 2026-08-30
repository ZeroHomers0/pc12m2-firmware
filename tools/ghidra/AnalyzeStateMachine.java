// Analyze state_machine: dump control-flow skeleton + full disassembly + variable access map
// @category LPC1765
// @menupath Tools.LPC1765.Analyze State Machine
// @description Decompose the giant state_machine function by writing flow/disasm to files

import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSetView;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceManager;
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.symbol.SymbolTable;
import ghidra.program.model.symbol.SourceType;
import ghidra.program.model.symbol.FlowType;
import java.io.FileWriter;
import java.io.PrintWriter;
import java.util.HashSet;
import java.util.Set;

public class AnalyzeStateMachine extends GhidraScript {

    @Override
    public void run() throws Exception {

        java.io.File outDir = askDirectory("选择状态机分析输出目录", "选择");

        Address fnAddr = toAddr("0x458c");
        Function fn = getFunctionAt(fnAddr);
        if (fn == null) {
            fn = getFunctionContaining(fnAddr);
            if (fn == null) {
                println("ERROR: no function at or containing 0x458c");
                return;
            }
            println("Note: 0x458c is inside function " + fn.getName() + " @ " + fn.getEntryPoint());
        }

        AddressSetView body = fn.getBody();
        println("Function " + fn.getName() + " entry=" + fn.getEntryPoint()
            + " bytes=" + body.getNumAddresses());

        PrintWriter flow = new PrintWriter(new FileWriter(new java.io.File(outDir, "state_machine_flow.txt")));
        PrintWriter disasm = new PrintWriter(new FileWriter(new java.io.File(outDir, "state_machine_disasm.txt")));

        flow.println("# state_machine control-flow skeleton");
        flow.println("# entry=" + fn.getEntryPoint() + "  bytes=" + body.getNumAddresses());
        flow.println("# format: <addr>  KIND  <detail>");
        flow.println();

        InstructionIterator it = currentProgram.getListing().getInstructions(body, true);
        int nIns = 0;
        while (it.hasNext()) {
            Instruction ins = it.next();
            Address a = ins.getAddress();
            nIns++;

            String var = resolveVar(ins);

            // ---- disasm line ----
            StringBuilder sb = new StringBuilder();
            sb.append(a).append("  ").append(ins.getMnemonicString());
            int nop = ins.getNumOperands();
            for (int i = 0; i < nop; i++) {
                sb.append(i == 0 ? "  " : ", ");
                sb.append(ins.getDefaultOperandRepresentation(i));
            }
            if (var != null) {
                sb.append("   ; VAR=").append(var);
            }
            disasm.println(sb.toString());

            // ---- flow: calls ----
            Reference[] refs = ins.getReferencesFrom();
            for (Reference r : refs) {
                if (r.getReferenceType().isCall()) {
                    Address t = r.getToAddress();
                    Function callee = getFunctionAt(t);
                    String nm = (callee != null) ? callee.getName() : ("sub_" + t);
                    flow.println(a + "  CALL  " + nm + "  (" + t + ")");
                }
            }

            // ---- flow: branches/jumps ----
            FlowType ft = ins.getFlowType();
            if (ft.isJump() || ft.isConditional()) {
                Address[] flows = ins.getFlows();
                Address fall = ins.getFallThrough();
                for (Address t : flows) {
                    if (fall == null || !t.equals(fall)) {
                        flow.println(a + "  ->  " + t + "   [" + ins.getMnemonicString() + "]");
                    }
                }
            }

            // ---- flow: variable access ----
            if (var != null) {
                flow.println(a + "  VAR  " + var + "   [" + ins.getMnemonicString() + "]");
            }
        }

        flow.println();
        flow.println("# total instructions = " + nIns);
        flow.close();
        disasm.close();
        println("Wrote state_machine_flow.txt (" + nIns + " instructions scanned)");
        println("Wrote state_machine_disasm.txt");
        println("Done.");
    }

    // Resolve whether this instruction loads the address of a USER_DEFINED RAM variable
    // (via a flash literal-pool slot typed as a pointer). Returns the variable name or null.
    private String resolveVar(Instruction ins) {
        ReferenceManager rm = currentProgram.getReferenceManager();
        for (Reference r : ins.getReferencesFrom()) {
            Address to = r.getToAddress();
            String nm = labelAt(to);
            if (nm != null) return nm;
            // one hop: literal slot -> RAM
            Reference[] slotRefs = rm.getReferencesFrom(to);
            if (slotRefs != null) {
                for (Reference sr : slotRefs) {
                    if (sr.getReferenceType().isData()) {
                        String nm2 = labelAt(sr.getToAddress());
                        if (nm2 != null) return nm2;
                    }
                }
            }
        }
        return null;
    }

    private String labelAt(Address a) {
        SymbolTable st = currentProgram.getSymbolTable();
        Symbol[] syms = st.getSymbols(a);
        for (Symbol s : syms) {
            if (s.getSource() == SourceType.USER_DEFINED) {
                return s.getName();
            }
        }
        return null;
    }
}
