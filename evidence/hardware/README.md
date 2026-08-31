# PC12M-2 硬件证据目录

本目录结构与 PC6M-10 的 `evidence/hardware/` 保持一致。当前文件均来自已成熟的六相项目，
用于理解 MCU 与共用外围、辅助形成十二相待验证假设；除共用显示面板资料外，不能直接作为
PC12M-2 板端接线、器件数量或十二路门极编号的证明。

PC12M-2 自身的原理图、BOM、板端连接表和测试点定义仍需另行收集或实测确认。

## 目录职责

- `board/`：PC6M-10 整板原理图、BOM、LPC1765/U38 引脚与接线参考。
- `display/`：CYW-B12864G 显示操作面板手册及其与 PC6M-10 的接口接线。
- `display/extracted/`：从显示面板资料提取的检索用纯文本，不替代原始 Office 文件。
- `reports/`：PC6M-10 综合硬件分析报告。

## 参考文件清单

| 分类 | SHA-256 | 文件 | 证据边界 |
|---|---|---|---|
| 六相整板 | `2338A4E05D74F33715410FCD07102448ECA4E005716F3B5ECFA7C87C07A5C1F1` | `board/PC6M-10_BOM.xlsx` | 仅作 PC6M-10 物料参考 |
| 六相整板 | `8538828ADDDCFCE9B6760B0B92A0E149F87577E11DECA9CD9F157AF808C3A5A6` | `board/PC6M-10_Schematic.pdf` | 仅作同构外围参考 |
| 六相整板 | `742EEC6F7F5AB89299FD05CA2670CBA7CEEFF7DBCBB316A7D3A0DF717482C279` | `board/LPC1765_U38_Pinout_Wiring.xlsx` | 引脚假设须由十二相固件或实测复核 |
| 显示面板 | `3305FAC09E85E793C92D65F8C73E97C0DF32BF88B6EEF6E13C024690B8DAF2D3` | `display/PC6M-10_CYW-B12864G_Interface_Wiring.xlsx` | 控制板侧接线仅适用于六相板 |
| 显示面板 | `F9838BF4E7D02BC815A60E753CD9F6E7AA7D780E72339B141F898F368790A765` | `display/CYW-B12864G_Operation_Manual.docx` | 共用面板操作与菜单参考 |
| 文本提取 | `42BFFD27BE9DF05C4004556285B672B69E7F9BD257B7AB9C15F0EDB75F26756A` | `display/extracted/PC6M-10_CYW-B12864G_Interface_Wiring.txt` | 六相接线表检索副本 |
| 文本提取 | `B01690EF062A27DADCF58256D910BB6402A0C68E45316CDAA428B28608D61AC5` | `display/extracted/CYW-B12864G_Operation_Manual.txt` | 面板手册检索副本 |
| 六相报告 | `DFCF7D475C96B7314E4E6F39C458F4AAF05B786FAACE983C4F675221393DAF02` | `reports/PC6M-10_Hardware_Analysis_Report_2026-07-14.docx` | 只作为六相分析参考 |
