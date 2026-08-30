// Create and name the 8 interrupt handlers for LPC1765 firmware
// @category LPC1765
// @menupath Tools.LPC1765.Create ISR Functions
// @description Create and name the Cortex-M3 ISR functions from the vector table

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.SourceType;

public class CreateIsrFunctions extends GhidraScript {

    @Override
    public void run() throws Exception {

        String[][] isrs = {
            { "0x1e4",  "WDT_IRQHandler"    },
            { "0x29a",  "TIMER0_IRQHandler" },
            { "0xff48", "TIMER2_IRQHandler" },
            { "0xff6c", "TIMER1_IRQHandler" },
            { "0xaf08", "UART3_IRQHandler"  },
            { "0xf9e8", "EINT1_IRQHandler"  },
            { "0xfa0a", "EINT2_IRQHandler"  },
            { "0xfa2c", "EINT3_IRQHandler"  },
        };

        for (String[] isr : isrs) {
            Address addr = toAddr(isr[0]);
            Function func = getFunctionAt(addr);
            if (func == null) {
                func = createFunction(addr, isr[1]);
            } else {
                func.setName(isr[1], SourceType.USER_DEFINED);
            }
            if (func == null) {
                println("FAILED: " + isr[1] + " @ " + addr);
            } else {
                println("OK: " + isr[1] + " @ " + addr);
            }
        }

        println("Done: 8 ISR functions");
    }
}
