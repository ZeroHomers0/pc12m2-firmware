// Type flash literal-pool entries as pointers so the decompiler resolves variable names
// @category LPC1765
// @menupath Tools.LPC1765.Type Literal Pool Pointers
// @description Create pointer types at flash literal-pool entries pointing to RAM variables

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.data.PointerDataType;
import ghidra.program.model.listing.Data;
import ghidra.program.model.symbol.RefType;
import ghidra.program.model.symbol.SourceType;

public class TypeLiteralPointers extends GhidraScript {

    @Override
    public void run() throws Exception {

        String[] ptrs = {
            "0x244", "0x2dc", "0x2e0", "0x2e4",
            "0x10000", "0x10004", "0x10008", "0x1000c", "0x10010",
            "0x10014", "0x10018", "0x1001c", "0x10020", "0x10024",
            "0x10028", "0x1002c", "0x10030", "0x10034",
            "0x10454", "0x10458", "0x1045c",
            "0x10640", "0x10644", "0x10648",
            "0xe988",
            "0xfba4", "0xfba8", "0xfbb0", "0xfbb4", "0xfbb8", "0xfb98",
            "0xfbbc", "0xfbd0", "0xfbd4", "0xfbd8", "0xfbdc", "0xfbe0",
            "0xfbe4", "0xfbe8", "0xfbec", "0xfbf0", "0xfbf4", "0xfbf8",
            "0xfbfc", "0xfffc",
        };

        int ok = 0;
        for (String s : ptrs) {
            Address addr = toAddr(s);
            try {
                clearListing(addr, addr.add(3));
                Data d = currentProgram.getListing().createData(addr, new PointerDataType());
                if (d != null) {
                    try {
                        Object v = d.getValue();
                        if (v instanceof Address) {
                            currentProgram.getReferenceManager().addMemoryReference(
                                addr, (Address) v, RefType.DATA, SourceType.USER_DEFINED, 0);
                        }
                    } catch (Exception refEx) {
                        // reference may already exist; ignore
                    }
                    println("OK: ptr @ " + addr);
                    ok++;
                } else {
                    println("NULL: " + addr);
                }
            } catch (Exception e) {
                println("FAIL: " + addr + " : " + e.getMessage());
            }
        }
        println("Done: " + ok + "/" + ptrs.length + " pointers");
    }
}
