# 数字格式化直接执行复核（2026-11-18）

原BIN与当前ELF各900例通过，业务源码和固件未改，未烧写。

直接执行10个入口：disp_digit、disp_number3、disp_uint4、disp_uint5、disp_number、disp_signed_angle、disp_offset、disp_uint2、disp_fixed_1dec、disp_decimal1。仅在disp_render_char8叶函数记录四个寄存器参数并返回；数字分支、符号、十进制运算与列位置均真实执行，所有调用必须正常返回。覆盖十进制进位边界、100/328偏移、低字节截断及32位符号边界，正反显与两组行列。

本测试验证字符调用契约，不独立验证字符GPIO写迹；字形授权定制继续保留。不以字符叶替身证明整条LCD硬件链路。

累计候选1983个，双出口1844，单出口126，未命中13，剩余139个。该统计只描述候选执行覆盖，不代表整体等价百分比。基线SHA链保存在台账中，本批未关闭状态机全路径、外设模型与授权差异分类等剩余任务。

入口：test/emulation/test_numeric_leaf_contract.py；证据：evidence/reverse/reports/numeric_leaf_audit_2026-11-18/。
