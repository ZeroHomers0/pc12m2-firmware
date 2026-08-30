# Create and name the 8 interrupt handlers for LPC1765 firmware
# @category LPC1765
# @menupath Tools.LPC1765.Create ISR Functions
# @description Create and name the Cortex-M3 ISR functions from the vector table

from ghidra.program.model.symbol import SourceType
from ghidra.program.model.listing import CodeUnit

program = getCurrentProgram()
listing = program.getListing()
fm = program.getFunctionManager()
space = program.getAddressFactory().getDefaultAddressSpace()

# (offset, name, comment)  -- offsets have the Thumb bit already cleared
ISRS = [
    (0x0001E4, "WDT_IRQHandler",    "Watchdog timeout: clear WDTOF + increment timeout counter"),
    (0x00029A, "TIMER0_IRQHandler", "System tick: set 0x7A0 flag + increment counter (T0MR2=1999)"),
    (0x00FF48, "TIMER2_IRQHandler", "Output/PWM update (sine wave generation)"),
    (0x00FF6C, "TIMER1_IRQHandler", "Counter + divide-by-40 (frequency/period calc)"),
    (0x00AF08, "UART3_IRQHandler",  "Serial RX handler (parse IIR RX interrupts)"),
    (0x00F9E8, "EINT1_IRQHandler",  "External interrupt 1: clear EXTINT1 + set flags"),
    (0x00FA0A, "EINT2_IRQHandler",  "External interrupt 2: clear EXTINT2 + set flags"),
    (0x00FA2C, "EINT3_IRQHandler",  "External interrupt 3: clear EXTINT3 + set flags"),
]


def make_function(offset, name, comment):
    addr = space.getAddress(offset)
    # Ensure Thumb disassembly exists first (Cortex language => Thumb-2)
    disassemble(addr)
    func = fm.getFunctionAt(addr)
    if func is None:
        func = fm.createFunction(name, addr, None, SourceType.USER_DEFINED)
        if func is None:
            print("FAILED: %s @ %s" % (name, addr))
            return
    else:
        func.setName(name, SourceType.USER_DEFINED)
    print("OK: %s @ %s" % (name, addr))
    if comment:
        listing.setComment(addr, CodeUnit.PLATE_COMMENT, comment)


for off, name, cmt in ISRS:
    make_function(off, name, cmt)

print("Done: %d ISR functions" % len(ISRS))
