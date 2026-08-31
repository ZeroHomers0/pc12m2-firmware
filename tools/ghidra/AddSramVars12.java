// AddSramVars12.java — label 12-phase (PC12M-2) SRAM variables
// @category LPC1765
// @menupath Tools.LPC1765.Add SRAM + 12-phase vars
// @description Add 64KB SRAM at 0x10000000 and apply labels derived from
//   real 12-phase code analysis (decompile FUN_0000e70c/00010a9c/00001f6c/
//   0000258c/0000abc0 + literal-pool translation + 6-phase globals.c compare).
//   Confidence tiers:
//     HIGH   — verified via 12-phase decompile/disasm access pattern
//     PLAUS  — referenced in 12-phase; role from neighbour/6-phase, not proven
//     NEW    — discovered 12-phase-only variables
//   Idempotent: stale USER_DEFINED labels are renamed to the current name.

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.symbol.SymbolTable;
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.symbol.SourceType;
import ghidra.program.model.listing.CodeUnit;

public class AddSramVars12 extends GhidraScript {

    // [addr, name, tier, note]
    private static final String[][] VARS = {
        // ---- HIGH: verified via 12-phase decompile / disasm ----
        {"0x10000000", "startup_word",        "PLAUS", "SRAM[0]; written by FUN_000000cc startup"},
        {"0x10000006", "tick_ready",          "HIGH",  "T0 ISR 1ms heartbeat flag"},
        {"0x10001fd8", "freq_hz",             "HIGH",  "50/60 select; MR0 gate & branch"},
        {"0x10001fd9", "phase_cnt",           "HIGH",  "T0 ISR counter, cap 0xc8"},
        {"0x10001fda", "debounce_count",      "HIGH",  "input debounce"},
        {"0x10001fdc", "out_setpoint",        "HIGH",  "trigger/PID/ramp-down output"},
        {"0x10001fe0", "input_locked",        "HIGH",  "run_state soft-start+lock; MR0 gate 2..7"},
        {"0x10001fe4", "softstart_angle",     "HIGH",  "=180-ulim_angle(0x1658); trigger_step 0x2c88-this*6333/100"},
        {"0x10001fe8", "softstop_angle",      "HIGH",  "=180-llim_angle(0x1648); stop ramp target; PID clamp"},
        {"0x10001ffc", "pid_feedback",        "HIGH",  "PID feedback compare"},
        {"0x10002000", "pid_state",           "HIGH",  "closed-loop PID state 0/1/2"},
        {"0x10002004", "pid_active",          "HIGH",  "closed-loop active flag (1=on)"},
        {"0x1000200c", "mode2_target",        "HIGH",  "gain_sel==2 mode target (from adc_ch3; %10000, clamp 10..1000)"},
        {"0x10002028", "mode2_scale",         "HIGH",  "gain_sel==2 scale = gain_c*1000/gain_a"},
        {"0x10002030", "startup_timer",       "HIGH",  "soft-start timer init 0x1771(6001) at start"},
        {"0x10002034", "out_div",             "HIGH",  "trigger divider (clamp+1-scale)*10/div; 60Hz MR0 subtract; 6p 0x205c"},
        {"0x10002040", "trigger_step",        "HIGH",  "soft-start/stop ramp step (0x2c88-ulim*6333/100)/50/div"},
        {"0x10002044", "trigger_accum",       "HIGH",  "soft-start/stop ramp accumulator"},
        {"0x10002048", "trigger_angle",       "HIGH",  "current trigger angle; *100 -> out_setpoint"},
        {"0x1000204c", "step_counter",        "HIGH",  "TMR1 step counter, %20 gate for MR0 update"},
        {"0x1000204d", "mode_byte",           "HIGH",  "1=fwd/2=rev; MR0 mode branch"},
        {"0x1000204e", "input_pending",       "HIGH",  "EINT2=2/EINT3=1 pending"},
        {"0x1000204f", "startup_state",       "HIGH",  "soft-start state machine (cleared at start)"},
        {"0x10002050", "trig_dir",            "HIGH",  "TMR1 direction 0/1 (invert phase)"},
        {"0x10002051", "startup_count",       "HIGH",  "10-tick startup countdown (>9 -> reset+phase calc)"},
        {"0x10002054", "alarm_timer",         "HIGH",  "alarm timer >1500 -> alarm_flag=1"},
        {"0x10002058", "alarm_flag",          "HIGH",  "alarm latch flag"},
        {"0x10002074", "pid_watchdog",        "HIGH",  "PID watchdog count (>100 -> flag)"},
        {"0x10002080", "ov_count_1",          "HIGH",  "over-range alarm counter 1 (x50 of ov_delay_1)"},
        {"0x10002084", "ov_count_2",          "HIGH",  "over-range alarm counter 2 -> out_param|=0x20"},
        {"0x10002094", "ov_count_3",          "HIGH",  "over-range alarm counter 3 -> out_param|=8/0x200"},
        {"0x10002098", "eint2_flag",          "HIGH",  ""},
        {"0x10002099", "eint3_flag",          "HIGH",  ""},
        {"0x1000209a", "adc_flag",            "HIGH",  ""},
        {"0x1000209c", "hold_count",          "HIGH",  ""},
        {"0x100020a4", "out_scale",           "HIGH",  "output scale (50Hz MR0 subtract); =setpoint*88/100; 6p 0x20cc"},
        {"0x100020c0", "auth_pass_flag",      "HIGH",  "auth result 1=fail/0=pass"},
        {"0x100020c8", "auth_retry",          "HIGH",  "auth retry counter <5 (FUN_00010a38)"},
        {"0x100020d0", "pid_target",          "HIGH",  "PID input target (FUN_00010a9c param1)"},
        {"0x100020d4", "pid_actual",          "HIGH",  "PID input actual (FUN_00010a9c param2)"},
        {"0x100020d8", "pid_kp",              "HIGH",  "P coefficient (x2 term)"},
        {"0x100020dc", "pid_kd",              "HIGH",  "D coefficient (x(e1-e2)*2 term)"},
        {"0x100020e0", "pid_ki",              "HIGH",  "I coefficient (x10 term)"},
        {"0x100020e4", "pid_err",             "HIGH",  "current PID error"},
        {"0x100020e8", "pid_err_prev",        "HIGH",  "previous PID error"},
        {"0x100020ec", "pid_err_prev2",       "HIGH",  "error 2 steps back"},
        {"0x100020f0", "pid_err_abs",         "HIGH",  "|error|"},
        {"0x100020f4", "pid_gain_sel",        "HIGH",  "PID gain level select"},
        {"0x100020f8", "pid_integral",        "HIGH",  "PID integral accumulator (FUN_000110f6 return)"},
        {"0x100020fc", "pid_const",           "HIGH",  "constant 1"},
        {"0x10002100", "pid_divisor",         "HIGH",  "level divisor (thresh 0xdc..0x1771 -> 8..0xb4)"},
        {"0x10002108", "pid_compute",         "HIGH",  "PID formula intermediate result"},
        {"0x10002110", "adc_ch0_raw",         "HIGH",  "ADC ch0 raw sample"},
        {"0x10002124", "adc_ch0_buf",         "HIGH",  "ADC ch0 average buffer"},
        {"0x1000214c", "adc_ch1_raw",         "HIGH",  "ADC ch1 raw sample"},
        {"0x10002160", "adc_ch1_buf",         "HIGH",  "ADC ch1 average buffer"},
        {"0x10002188", "adc_ch2_raw",         "HIGH",  "ADC ch2 raw sample"},
        {"0x1000219c", "adc_ch2_buf",         "HIGH",  "ADC ch2 average buffer"},
        {"0x1000223c", "adc_ch3_raw",         "HIGH",  "ADC ch3 raw sample"},
        {"0x10002250", "adc_ch3_buf",         "HIGH",  "ADC ch3 average buffer"},
        {"0x100021c4", "adc_ch4_raw",         "HIGH",  "ADC ch4 raw sample"},
        {"0x100021d8", "adc_ch4_buf",         "HIGH",  "ADC ch4 average buffer"},
        {"0x10002200", "adc_ch5_raw",         "HIGH",  "ADC ch5 raw sample"},
        {"0x10002214", "adc_ch5_buf",         "HIGH",  "ADC ch5 average buffer"},

        // ---- HIGH: control/trigger/protection (main loop FUN_0000e70c) ----
        {"0x100015c4", "adc_scan_idx",        "HIGH",  "ADC 6-ch scan index 0..5"},
        {"0x100015c5", "adc_avg_idx_0",       "HIGH",  "ADC ch0 average index"},
        {"0x100015c6", "adc_avg_idx_1",       "HIGH",  "ADC ch1 average index"},
        {"0x100015c7", "adc_avg_idx_2",       "HIGH",  "ADC ch2 average index"},
        {"0x100015c8", "adc_avg_idx_3",       "HIGH",  "ADC ch3 average index"},
        {"0x100015c9", "adc_avg_idx_4",       "HIGH",  "ADC ch4 average index"},
        {"0x100015ca", "adc_avg_idx_5",       "HIGH",  "ADC ch5 average index"},
        {"0x100015c0", "adc_avg_work",        "HIGH",  "ADC average working word"},
        {"0x100015d0", "pid_target_set",      "HIGH",  "PID target setpoint (menu/soft-start); FUN_000110f6 arg1"},
        {"0x100015a4", "adc_conv_ch2",        "PLAUS", "ADC ch2 converted"},
        {"0x10001590", "adc_conv_ch5",        "HIGH",  "ADC ch5 converted (over-range alarm input)"},
        {"0x10001594", "adc_conv_ch4",        "HIGH",  "ADC ch4 converted (PID mode1 input)"},
        {"0x100015a8", "adc_conv_ch3",        "HIGH",  "ADC ch3 converted (mode2 target source; run gate)"},
        {"0x100015b8", "adc_conv_fb",         "HIGH",  "ADC converted PID feedback (FUN_110f6 arg2)"},
        {"0x100015bc", "adc_conv_aux1",       "PLAUS", "ADC converted aux output"},
        {"0x100015b4", "adc_conv_aux2",       "PLAUS", "ADC converted aux output"},
        {"0x10001630", "gain_a",              "HIGH",  "gain sel==0 (pool eb2c/eb30 /15)"},
        {"0x10001634", "gain_b",              "HIGH",  "gain sel==1 (pool eb34 /15)"},
        {"0x1000163c", "gain_c",              "HIGH",  "gain sel==2 scale numerator (x1000/gain_a)"},
        {"0x10001638", "gain_coef",           "HIGH",  "gain coefficient (6p g_gain_b@0x1638 same addr)"},
        {"0x10001644", "startup_div",         "HIGH",  "soft-start trigger_step divisor (pool eba4)"},
        {"0x10001645", "stop_div",            "HIGH",  "stop-ramp trigger_step divisor (pool f924)"},
        {"0x10001648", "llim_angle",          "HIGH",  "lower angle limit param; 180-this -> softstop_angle"},
        {"0x10001658", "ulim_angle",          "HIGH",  "upper angle limit param; 180-this -> softstart_angle"},
        {"0x1000168c", "trig_phase",          "HIGH",  "TMR1 MR0 phase param; MR0=0xc8+this*4 (50Hz)"},

        // ---- HIGH: closed-loop gain params (FUN_00010a9c) ----
        {"0x1000171a", "cl_thresh_hi",        "HIGH",  "|err|>=0x10ec0 -> cl_gain_big"},
        {"0x1000171b", "cl_thresh_lo",        "HIGH",  "|err|<=0x10ecc -> cl_gain_small"},
        {"0x1000171c", "cl_gain_big",         "HIGH",  "closed-loop large gain"},
        {"0x1000171d", "cl_gain_mid",         "HIGH",  "closed-loop middle gain"},
        {"0x1000171e", "cl_gain_small",       "HIGH",  "closed-loop small gain"},
        {"0x10001706", "pid_kp2",             "HIGH",  "closed-loop PID P gain (FUN_000110f6 arg3)"},
        {"0x10001707", "pid_ki2",             "HIGH",  "closed-loop PID I gain (FUN_000110f6 arg4)"},

        // ---- HIGH: UART3 comm (FUN_0000abc0/ab7c/a994) ----
        {"0x100016f8", "comm_baud_idx",       "HIGH",  "baud table index (6p g_baud_idx 0x1700 -8)"},
        {"0x100016fc", "comm_div_sel",        "HIGH",  "baud divider select 0..3 (6p same addr uart_frame_sel)"},
        {"0x100016fd", "comm_rx_flag",        "PLAUS", "comm rx condition flag"},
        {"0x1000176c", "comm_quiet_timer",    "HIGH",  "comm quiet timer >30000 -> out_param|=0x8000"},
        {"0x10001770", "comm_state",          "HIGH",  "UART state machine 0/1/5/6"},
        {"0x10001771", "comm_tx_count",       "HIGH",  "tx count; state==1 +1, >10 -> state=5"},
        {"0x10001772", "comm_tx_len",         "PLAUS", "comm tx length"},
        {"0x10001773", "comm_tx_data",        "HIGH",  "comm tx data byte"},
        {"0x10001774", "comm_tx_param",       "HIGH",  "comm tx param (cleared)"},
        {"0x10001798", "comm_scan_timer",     "HIGH",  "comm scan timer >300 -> configure baud"},
        {"0x1000179c", "comm_baud_table",     "HIGH",  "baud divisor table (8 entries x4B)"},
        {"0x10002340", "comm_frame_buf",      "NEW",   "comm frame buffer (tx data)"},

        // ---- HIGH: menu / display (FUN_00004464 disasm) ----
        {"0x10001754", "disp_calc",           "HIGH",  "display calc = gain*menu/1000 -> pid_target"},
        {"0x10001758", "menu_tick",           "HIGH",  "menu tick 250 -> 251 triggers"},
        {"0x1000175d", "stop_req",            "HIGH",  "stop request ==1 (main loop stop condition)"},
        {"0x10001765", "menu_state2",         "PLAUS", "menu state secondary (cleared on reset)"},
        {"0x10001744", "menu_param_1",        "PLAUS", "menu param (cleared on reset)"},
        {"0x10001750", "menu_val",            "PLAUS", "menu current value"},
        {"0x1000175c", "menu_opt",            "PLAUS", "menu option counter"},
        {"0x1000175e", "menu_flag_1",         "PLAUS", "menu flag (reset-clear)"},
        {"0x1000175f", "menu_flag_2",         "PLAUS", "menu flag (reset-clear)"},
        {"0x10001760", "menu_flag_3",         "PLAUS", "menu flag (reset-clear)"},
        {"0x10001761", "menu_flag_4",         "PLAUS", "menu flag (reset-clear)"},
        {"0x10001762", "menu_flag_5",         "PLAUS", "menu flag"},
        {"0x10001768", "menu_param_2",        "PLAUS", "menu param"},
        {"0x10001778", "menu_param_3",        "PLAUS", "menu param"},
        {"0x1000177c", "menu_param_4",        "PLAUS", "menu param"},

        // ---- PLAUS: menu/param working vars (FUN_04464 R/W) ----
        {"0x100015cc", "menu_param_5",        "PLAUS", "menu/param working var"},
        {"0x100015cd", "menu_param_6",        "PLAUS", "menu/param working var"},
        {"0x100015cf", "menu_param_7",        "PLAUS", "menu/param working var"},
        {"0x100015d4", "menu_param_8",        "PLAUS", "menu/param working var"},
        {"0x100015e0", "menu_param_9",        "PLAUS", "menu/param working var"},
        {"0x100015e6", "menu_param_10",       "PLAUS", "menu/param working var"},
        {"0x100015ec", "menu_param_11",       "PLAUS", "menu/param working var"},
        {"0x100015f2", "menu_param_12",       "PLAUS", "menu/param working var"},
        {"0x10001600", "menu_param_13",       "PLAUS", "menu/param working var"},

        // ---- PLAUS: EEPROM param mirror (FUN_0258c/03534) ----
        {"0x1000164e", "eeprom_param_1",      "PLAUS", "EEPROM param mirror (FUN_0258c write)"},
        {"0x1000164f", "eeprom_param_2",      "PLAUS", "EEPROM param mirror"},
        {"0x10001651", "eeprom_param_3",      "PLAUS", "EEPROM param mirror"},
        {"0x10001652", "eeprom_adc_cfg",      "PLAUS", "EEPROM ADC config (read by FUN_01f6c ADC)"},
        {"0x10001653", "eeprom_param_4",      "PLAUS", "EEPROM param mirror"},
        {"0x10001675", "eeprom_param_5",      "PLAUS", "EEPROM param mirror"},
        {"0x1000167e", "eeprom_param_6",      "PLAUS", "EEPROM param mirror"},
        {"0x1000167f", "eeprom_param_7",      "PLAUS", "EEPROM param mirror"},
        {"0x10001681", "eeprom_param_8",      "PLAUS", "EEPROM param mirror"},
        {"0x10001682", "eeprom_param_9",      "PLAUS", "EEPROM param mirror"},
        {"0x10001683", "eeprom_param_10",     "PLAUS", "EEPROM param mirror"},
        {"0x1000168d", "eeprom_param_11",     "PLAUS", "EEPROM param mirror"},
        {"0x100016d5", "eeprom_param_12",     "PLAUS", "EEPROM param mirror"},
        {"0x100016d6", "eeprom_param_13",     "PLAUS", "EEPROM param mirror"},
        {"0x100016f5", "eeprom_param_14",     "PLAUS", "EEPROM param mirror (next to menu_state 0x16f7)"},
        {"0x100016f6", "eeprom_param_15",     "PLAUS", "EEPROM param mirror"},
        {"0x10001709", "comm_param_1",        "PLAUS", "comm protocol param (EEPROM mirror)"},
        {"0x1000170a", "comm_param_2",        "PLAUS", "comm protocol param"},
        {"0x1000170b", "comm_param_3",        "PLAUS", "comm protocol param"},

        // ---- PLAUS: protocol work area (FUN_0000b3b2) ----
        {"0x100017d0", "protocol_work_1",     "PLAUS", "FUN_0000b3b2 protocol work"},
        {"0x100017d4", "protocol_work_2",     "PLAUS", "FUN_0000b3b2 protocol work"},
        {"0x100017d8", "protocol_work_3",     "PLAUS", "FUN_0000b3b2 protocol work"},

        // ---- PLAUS: data buffers ----
        {"0x10002728", "buf_data_1",          "PLAUS", "data buffer"},
        {"0x1000273c", "buf_data_2",          "PLAUS", "data buffer"},
        {"0x100027a0", "buf_data_3",          "PLAUS", "data buffer"},
        {"0x100029a0", "buf_data_4",          "PLAUS", "data buffer"},

        // ---- existing: PLAUS / NEW ----
        {"0x10001654", "out_fine",            "PLAUS", "param var; NOT in 12p MR0 formula"},
        {"0x10001794", "menu_index",          "NEW",   ""},
        {"0x10001788", "disp_counter",        "NEW",   ""},
        {"0x10001780", "frame_count",         "NEW",   ""},
        {"0x100016f7", "menu_state",          "NEW",   ""},
        {"0x10001790", "disp_param",          "NEW",   ""},
        {"0x10002278", "lookup_table",        "NEW",   ""},
    };

    @Override
    public void run() throws Exception {
        Memory mem = currentProgram.getMemory();
        Address sramStart = toAddr("0x10000000");
        MemoryBlock sram = mem.getBlock(sramStart);
        if (sram == null) {
            sram = mem.createUninitializedBlock("SRAM", sramStart, 0x10000, false);
            println("Added SRAM block: " + sram.getName() + " @ " + sram.getStart());
        } else {
            println("SRAM block already exists @ " + sram.getStart());
        }

        SymbolTable st = currentProgram.getSymbolTable();
        int n = 0, nskip = 0, nrepl = 0, nkeep = 0;
        for (String[] v : VARS) {
            String addrStr = v[0], name = v[1], tier = v[2], note = v[3];
            Address a = toAddr(addrStr);
            if (!sram.contains(a)) {
                println("SKIP (outside SRAM): " + addrStr + " " + name);
                nskip++;
                continue;
            }
            boolean need = true;
            for (Symbol s : st.getSymbols(a)) {
                if (s.getSource() != SourceType.USER_DEFINED) continue;
                if (s.getName().equals(name)) { need = false; nkeep++; }
                else { s.delete(); nrepl++; }
            }
            if (need) {
                st.createLabel(a, name, SourceType.USER_DEFINED);
                String c = "12-phase [" + tier + "] " + name + (note.isEmpty() ? "" : " — " + note);
                currentProgram.getListing().setComment(a, CodeUnit.EOL_COMMENT, c);
                n++;
            }
        }
        println("Applied " + n + " labels (" + nrepl + " renamed), " + nkeep + " kept, "
                + nskip + " skipped. Tiers: HIGH=" + count("HIGH") + " PLAUS=" + count("PLAUS")
                + " NEW=" + count("NEW"));
    }

    private int count(String tier) {
        int c = 0;
        for (String[] v : VARS) if (v[2].equals(tier)) c++;
        return c;
    }
}
