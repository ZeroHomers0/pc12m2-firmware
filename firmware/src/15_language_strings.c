#include <stdint.h>
#include "inc/firmware_language.h"

typedef struct { uint32_t address; const char *text; } EnglishText;

/* Every complete line is at most 16 LCD columns; labels which share a line with
 * a numeric value are at most 11 columns. */
static const EnglishText english_texts[] = {
  {0x0754,"FAULT STATUS"},{0x0760,"CALIB. ERROR"},{0x076c,"CALIBRATION"},
  {0x0778,"OUT VOLT 50%"},{0x0784,"VALUE:"},{0x0790,"STATUS: STOP"},
  {0x4370,"INPUT:        %"},{0x4384,"OUTPUT:       V"},{0x4398,"CURRENT:      A"},{0x43ac,"    MODE:"},
  {0x47dc,"FAULT "},{0x47e8,"RUN   "},{0x47f0,"STOP  "},{0x47fc,"CONST VOLT"},{0x4804,"CONST CURR"},{0x480c,"OPEN LOOP"},
  {0x4814,"1.BASIC SET     "},{0x4824,"2.PROTECT SET   "},{0x4834,"3.COMM SET      "},{0x4844,"4.FACTORY RESET "},
  {0x4854,"1.U CURR:"},{0x4868,"2.V CURR:"},{0x487c,"3.W CURR:"},{0x4890,"4.OUT CURR:"},
  {0x4d58,"1.CUR RUN TIME"},{0x4d6c,"  60000H30M"},{0x4d80,"2.TOTAL RUN TIME"},{0x4d9c," PASS: ------"},
  {0x4dac,"60 S"},{0x4db4,"  INITIAL SET"},{0x4dc8," PASS: ------"},{0x522c,"RESET"},{0x523c,"RSTART"},{0x56dc,"PWD ERROR"},
  {0x5b18,"5.OUTPUT V:"},{0x5b2c,"6.E-STOP:"},{0x5b38,"    "},{0x5b40,"7.RESET:"},{0x5b54,"8.FEEDBACK:"},
  {0x5b68,"9.INPUT:"},{0x5b7c,"10.CTRL:"},{0x5b90,"11.START:"},{0x5ba4,"                "},
  {0x6018,"ESTOP"},{0x6020,"EXT  "},{0x6028,"LIMIT"},{0x6030,"RESET"},{0x6038,"OFF  "},{0x6040,"ON   "},
  {0x6048,"ANA  "},{0x6050,"DIG  "},{0x6058,"FULL "},{0x6060,"HALF "},{0x6474,"     "},{0x647c,"   "},
  {0x6488,"1.BASIC SET     "},{0x649c,"2.PROTECT SET   "},{0x64b0,"3.COMM SET      "},{0x64c4,"4.FACTORY RESET "},
  {0x64d8,"5.PID SET       "},{0x64ec,"6.PHASE CALIB   "},{0x6500,"7.RUN TIME      "},{0x6514,"8.PRODUCT INFO  "},{0x6528,"9.CURR BALANCE  "},
  {0x6540,"1.RUN MODE:"},{0x6554,"2.V RNG:       V"},{0x6568,"3.I RNG:       A"},{0x657c,"4.CT RAT:      A"},
  {0x6594,"VOLT "},{0x659c,"CURR "},{0x65a4,"OPEN "},
  {0x65bc,"1.OV V:        V"},{0x65d0,"2.OV T:        S"},{0x65e4,"3.UN V:        V"},{0x65f8,"4.UN T:        S"},
  {0x6a18,"1.ADDR:"},{0x6a2c,"2.BAUD:"},{0x6a40,"3.PARITY:"},{0x6a54,"4.COM CHK:"},
  {0x6a78,"EVEN  "},{0x6a80,"ODD   "},{0x6a88,"NONE  "},{0x6a94,"ON   "},
  {0x6aa4,"1.PID PROF:"},{0x6ab8,"2.P VAL:"},{0x6acc,"3.I VAL:"},{0x6ae0,"4.D VAL:   AUTO"},
  {0x6af8,"FAST "},{0x6b08,"MID  "},{0x6b14,"SLOW "},{0x6b24,"USER "},
  {0x6b34,"PHASE CALIB"},{0x6b40,"OUT VOLT 50%"},{0x6b4c,"VALUE:"},{0x6b58,"STATUS: STOP"},
  {0x6b78,"MODEL:PC12M-2"},{0x6b84,"VERSION:V2.0"},{0x6b94,"MAKER:XIANPOWER"},{0x6ba4,"TEL:02984205750"},{0x6bb8,"CURR BALANCE"},
  {0x6fe4,"5.V LIM:       V"},{0x6ff8,"6.I LIM:       A"},{0x700c,"7.START:       S"},{0x7020,"8.STOP:        S"},
  {0x7034,"9.PH LIM:"},{0x7048,"10.M/S OFF:"},{0x705c,"11.CTRL:"},{0x7068,"     "},
  {0x7070,"12.START:"},{0x7084,"13.E-STOP:"},{0x7098,"14.FDBK:"},{0x70ac,"15.INPUT:"},{0x70c0,"16.ST ANG:"},
  {0x7974,"V"},{0x7980,"A"},{0x7998,"COMM "},{0x79a0,"LOC  "},{0x79a8,"FIX  "},{0x79b4,"JOG  "},{0x79bc,"LAT  "},
  {0x7e10,"5.IF LIM:      A"},{0x7e24,"6.IF TIME:     S"},{0x7e38,"7.CT LIM:      A"},{0x7e4c,"8.CT TIME:     S"},
  {0x7e60,"9.PH LOSS:"},{0x7e6c,"     "},{0x7e74,"10.IMBAL:      %"},{0x86e0,"%"},{0x8b2c,"1 STOP"},
  {0x8f44,"      "},{0x8f6c,"RESET "},{0x8f78,"DONE  "},{0x8f88,"RSTART"},{0x8f9c,"INIT  "},
  {0x9400,"5.HI GAIN:"},{0x9414,"6.MID GAIN:"},{0x9428,"7.FAST SH:"},{0x943c,"8.MID SH:"},{0x9450,"9.SLOW SH:"},
  {0xa130,"9.CURR BALANCE"},{0xa140,"               "},{0xa158,"FAULT STATUS"},{0xa164,"      NONE"},
  {0xa178,"  PHASE LOSS"},{0xa578,"  IF OVERLOAD"},{0xa590,"  OVER VOLT"},{0xa5a4,"  UNDER VOLT"},{0xa5b8,"  CT OVERLOAD"},
  {0xa5cc,"  PHASE ORDER"},{0xa5e0,"PHASE IMBALANCE"},{0xa5f4,"  IF OVERCURR"},{0xa608,"  CT OVERCURR"},
  {0xa61c,"  FEEDBACK ERR"},{0xa630,"  BATTERY REV"},{0xa644,"  OVERHEAT"},{0xa658,"  COMM ERROR"},{0xa66c,"  FREQ ERROR"},
  {0xac1c,"STOP"},{0x0710,"CPU ERROR"},{0x4334,"FAULT"},{0x6a8c,"OUTPUT VOLT 50%"},{0x6a98,"VALUE:"},{0xa070,"9.CURR BALANCE"},{0xa080,"              "},{0xa0b0,"FAULT STATUS"},
  {UI_TEXT_MENU_LANGUAGE,"10.LANGUAGE     "},{UI_TEXT_LANGUAGE_TITLE,"SELECT LANGUAGE"},
  {UI_TEXT_LANGUAGE_ZH,"1.CHINESE"},{UI_TEXT_LANGUAGE_EN,"2.ENGLISH"}
};

uint32_t ui_language_translate(uint32_t canonical_address, uint32_t chinese_address)
{
  uint32_t i;
  if (ui_language_get() != UI_LANGUAGE_ENGLISH) return chinese_address;
  for (i = 0; i < sizeof(english_texts) / sizeof(english_texts[0]); ++i)
    if (english_texts[i].address == canonical_address) return (uint32_t)english_texts[i].text;
  return chinese_address;
}
