; ModuleID = 'cminus'
source_filename = "/home/zhaokejian/complier/2025ustc-compiler/tests/testcases_general/1-return.cminus"

declare i32 @input()

declare void @output(i32)

declare void @outputFloat(float)

declare void @neg_idx_except()

define void @main() {
label_entry:
  ret void
}
