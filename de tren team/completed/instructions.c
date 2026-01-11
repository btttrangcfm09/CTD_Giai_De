/* 
 * @copyright (c) 2008, Hedspi, Hanoi University of Technology
 * @author Huu-Duc Nguyen
 * @version 1.0
 */

/**
 * ===== FILE: instructions.c =====
 * 
 * CHỨC NĂNG:
 *   - Cài đặt module quản lý lệnh máy ảo (VM instructions)
 *   - Cung cấp các hàm để tạo, phát sinh (emit), in, lưu/đọc mã lệnh
 * 
 * CẤU TRÚC:
 *   - createCodeBlock/freeCodeBlock: Quản lý bộ nhớ cho code block
 *   - emitCode: Hàm tổng quát để thêm lệnh vào code block
 *   - emitXX: 31 hàm wrapper cho từng loại lệnh cụ thể
 *   - printInstruction/printCodeBlock: In mã lệnh ra console (debug)
 *   - loadCode/saveCode: Đọc/ghi mã lệnh từ/vào file nhị phân
 */

#include <stdio.h>
#include <stdlib.h>
#include "instructions.h"

// Số lệnh tối đa đọc/ghi trong một lần từ file
#define MAX_BLOCK 50

/**
 * @brief Tạo một code block mới
 * 
 * CHỨC NĂNG:
 *   - Cấp phát bộ nhớ cho CodeBlock và mảng Instruction
 *   - Khởi tạo các giá trị ban đầu
 * 
 * THAM SỐ:
 *   - maxSize: Số lượng lệnh tối đa có thể chứa
 * 
 * TRẢ VỀ:
 *   - Con trỏ đến CodeBlock mới (cần giải phóng sau khi dùng xong)
 * 
 * VÍ DỤ:
 *   CodeBlock* cb = createCodeBlock(1000);
 *   // ... emit các lệnh
 *   freeCodeBlock(cb);
 */
CodeBlock* createCodeBlock(int maxSize) {
  // Cấp phát bộ nhớ cho struct CodeBlock
  CodeBlock* codeBlock = (CodeBlock*) malloc(sizeof(CodeBlock));

  // Cấp phát mảng chứa các lệnh (maxSize phần tử)
  codeBlock->code = (Instruction*) malloc(maxSize * sizeof(Instruction));
  codeBlock->codeSize = 0;        // Ban đầu chưa có lệnh nào
  codeBlock->maxSize = maxSize;   // Lưu kích thước tối đa
  return codeBlock;
}

/**
 * @brief Giải phóng bộ nhớ của code block
 * 
 * CHỨC NĂNG:
 *   - Giải phóng mảng lệnh
 *   - Giải phóng struct CodeBlock
 * 
 * THAM SỐ:
 *   - codeBlock: Code block cần giải phóng
 * 
 * LƯU Ý:
 *   - Phải gọi sau khi dùng xong code block để tránh memory leak
 */
void freeCodeBlock(CodeBlock* codeBlock) {
  free(codeBlock->code);   // Giải phóng mảng lệnh trước
  free(codeBlock);         // Giải phóng struct CodeBlock
}

/**
 * @brief Phát sinh một lệnh vào code block (hàm tổng quát)
 * 
 * CHỨC NĂNG:
 *   - Thêm một lệnh mới vào cuối code block
 *   - Kiểm tra overflow (tràn bộ nhớ)
 * 
 * THAM SỐ:
 *   - codeBlock: Code block đích
 *   - op: Mã lệnh (OpCode)
 *   - p: Tham số 1 (level hoặc DC_VALUE nếu không dùng)
 *   - q: Tham số 2 (offset/address/value hoặc DC_VALUE)
 * 
 * TRẢ VỀ:
 *   - 1: Thành công
 *   - 0: Thất bại (code block đầy)
 * 
 * VÍ DỤ:
 *   emitCode(cb, OP_LA, 2, 5);  // LA 2,5 - Load địa chỉ level=2, offset=5
 *   emitCode(cb, OP_LC, DC_VALUE, 100); // LC 100 - Load constant 100
 */
int emitCode(CodeBlock* codeBlock, enum OpCode op, WORD p, WORD q) {
  // Tính địa chỉ của lệnh tiếp theo (vị trí cuối mảng)
  Instruction* bottom = codeBlock->code + codeBlock->codeSize;

  // Kiểm tra overflow: nếu đã đầy thì không thêm được nữa
  if (codeBlock->codeSize >= codeBlock->maxSize) return 0;

  // Gán các trường của lệnh
  bottom->op = op;   // Mã lệnh
  bottom->p = p;     // Tham số 1
  bottom->q = q;     // Tham số 2
  
  codeBlock->codeSize ++;  // Tăng số lệnh hiện tại
  return 1;  // Thành công
}

// ===== CÁC HÀM EMIT CỤ THỂ =====
// Đây là các wrapper function, mỗi hàm phát sinh một loại lệnh cụ thể
// bằng cách gọi emitCode với OpCode tương ứng.
// DC_VALUE được dùng cho các tham số không sử dụng.

int emitLA(CodeBlock* codeBlock, WORD p, WORD q) { return emitCode(codeBlock, OP_LA, p, q); }  // Load Address: LA level,offset
int emitLV(CodeBlock* codeBlock, WORD p, WORD q) { return emitCode(codeBlock, OP_LV, p, q); }  // Load Value: LV level,offset
int emitLC(CodeBlock* codeBlock, WORD q) { return emitCode(codeBlock, OP_LC, DC_VALUE, q); }   // Load Constant: LC value
int emitLI(CodeBlock* codeBlock) { return emitCode(codeBlock, OP_LI, DC_VALUE, DC_VALUE); }    // Load Indirect: LI (load giá trị từ địa chỉ trên đỉnh stack)
int emitINT(CodeBlock* codeBlock, WORD q) { return emitCode(codeBlock, OP_INT, DC_VALUE, q); } // Increment Top: INT q (tăng sp thêm q)
int emitDCT(CodeBlock* codeBlock, WORD q) { return emitCode(codeBlock, OP_DCT, DC_VALUE, q); } // Decrement Top: DCT q (giảm sp đi q)
int emitJ(CodeBlock* codeBlock, WORD q) { return emitCode(codeBlock, OP_J, DC_VALUE, q); }     // Jump: J addr (nhảy vô điều kiện)
int emitFJ(CodeBlock* codeBlock, WORD q) { return emitCode(codeBlock, OP_FJ, DC_VALUE, q); }   // False Jump: FJ addr (nhảy nếu top=0)
int emitHL(CodeBlock* codeBlock) { return emitCode(codeBlock, OP_HL, DC_VALUE, DC_VALUE); }    // Halt: HL (dừng chương trình)
int emitST(CodeBlock* codeBlock) { return emitCode(codeBlock, OP_ST, DC_VALUE, DC_VALUE); }    // Store: ST (lưu giá trị vào biến)
int emitCALL(CodeBlock* codeBlock, WORD p, WORD q) { return emitCode(codeBlock, OP_CALL, p, q); } // Call: CALL level,addr (gọi hàm/thủ tục)
int emitEP(CodeBlock* codeBlock) { return emitCode(codeBlock, OP_EP, DC_VALUE, DC_VALUE); }    // Exit Procedure: EP (thoát thủ tục)
int emitEF(CodeBlock* codeBlock) { return emitCode(codeBlock, OP_EF, DC_VALUE, DC_VALUE); }    // Exit Function: EF (thoát hàm)
int emitRC(CodeBlock* codeBlock) { return emitCode(codeBlock, OP_RC, DC_VALUE, DC_VALUE); }    // Read Char: RC (đọc ký tự)
int emitRI(CodeBlock* codeBlock) { return emitCode(codeBlock, OP_RI, DC_VALUE, DC_VALUE); }    // Read Integer: RI (đọc số nguyên)
int emitWRC(CodeBlock* codeBlock) { return emitCode(codeBlock, OP_WRC, DC_VALUE, DC_VALUE); }  // Write Char: WRC (in ký tự)
int emitWRI(CodeBlock* codeBlock) { return emitCode(codeBlock, OP_WRI, DC_VALUE, DC_VALUE); }  // Write Integer: WRI (in số nguyên)
int emitWLN(CodeBlock* codeBlock) { return emitCode(codeBlock, OP_WLN, DC_VALUE, DC_VALUE); }  // Write Line: WLN (xuống dòng)
int emitAD(CodeBlock* codeBlock) { return emitCode(codeBlock, OP_AD, DC_VALUE, DC_VALUE); }    // Add: AD (cộng)
int emitSB(CodeBlock* codeBlock) { return emitCode(codeBlock, OP_SB, DC_VALUE, DC_VALUE); }    // Subtract: SB (trừ)
int emitML(CodeBlock* codeBlock) { return emitCode(codeBlock, OP_ML, DC_VALUE, DC_VALUE); }    // Multiply: ML (nhân)
int emitDV(CodeBlock* codeBlock) { return emitCode(codeBlock, OP_DV, DC_VALUE, DC_VALUE); }    // Divide: DV (chia)
int emitNEG(CodeBlock* codeBlock) { return emitCode(codeBlock, OP_NEG, DC_VALUE, DC_VALUE); }  // Negate: NEG (đổi dấu)
int emitCV(CodeBlock* codeBlock) { return emitCode(codeBlock, OP_CV, DC_VALUE, DC_VALUE); }    // Copy Top/Convert: CV (sao chép đỉnh stack)
int emitEQ(CodeBlock* codeBlock) { return emitCode(codeBlock, OP_EQ, DC_VALUE, DC_VALUE); }    // Equal: EQ (so sánh bằng)
int emitNE(CodeBlock* codeBlock) { return emitCode(codeBlock, OP_NE, DC_VALUE, DC_VALUE); }    // Not Equal: NE (so sánh khác)
int emitGT(CodeBlock* codeBlock) { return emitCode(codeBlock, OP_GT, DC_VALUE, DC_VALUE); }    // Greater Than: GT (so sánh lớn hơn)
int emitLT(CodeBlock* codeBlock) { return emitCode(codeBlock, OP_LT, DC_VALUE, DC_VALUE); }    // Less Than: LT (so sánh nhỏ hơn)
int emitGE(CodeBlock* codeBlock) { return emitCode(codeBlock, OP_GE, DC_VALUE, DC_VALUE); }    // Greater or Equal: GE (so sánh lớn hơn hoặc bằng)
int emitLE(CodeBlock* codeBlock) { return emitCode(codeBlock, OP_LE, DC_VALUE, DC_VALUE); }    // Less or Equal: LE (so sánh nhỏ hơn hoặc bằng)

int emitBP(CodeBlock* codeBlock) { return emitCode(codeBlock, OP_BP, DC_VALUE, DC_VALUE); }    // Breakpoint: BP (điểm dừng debug)


/**
 * @brief In một lệnh ra console (dạng assembly)
 * 
 * CHỨC NĂNG:
 *   - Chuyển đổi struct Instruction thành chuỗi assembly dễ đọc
 *   - In theo định dạng: OPCODE [p,q]
 * 
 * THAM SỐ:
 *   - inst: Con trỏ đến lệnh cần in
 * 
 * ĐỊNH DẠNG:
 *   - Lệnh có 2 tham số: "LA 2,5", "CALL 1,100"
 *   - Lệnh có 1 tham số: "LC 42", "INT 10"
 *   - Lệnh không tham số: "AD", "ST", "HL"
 * 
 * VÍ DỤ:
 *   Instruction inst = {OP_LA, 2, 5};
 *   printInstruction(&inst);  // In ra: LA 2,5
 */
void printInstruction(Instruction* inst) {
  switch (inst->op) {
  case OP_LA: printf("LA %d,%d", inst->p, inst->q); break;     // Load Address với level và offset
  case OP_LV: printf("LV %d,%d", inst->p, inst->q); break;     // Load Value với level và offset
  case OP_LC: printf("LC %d", inst->q); break;                 // Load Constant với giá trị q
  case OP_LI: printf("LI"); break;                              // Load Indirect (không tham số)
  case OP_INT: printf("INT %d", inst->q); break;               // Increment Top với số lượng q
  case OP_DCT: printf("DCT %d", inst->q); break;               // Decrement Top với số lượng q
  case OP_J: printf("J %d", inst->q); break;                   // Jump đến địa chỉ q
  case OP_FJ: printf("FJ %d", inst->q); break;                 // False Jump đến địa chỉ q
  case OP_HL: printf("HL"); break;                              // Halt (không tham số)
  case OP_ST: printf("ST"); break;                              // Store (không tham số)
  case OP_CALL: printf("CALL %d,%d", inst->p, inst->q); break; // Call với level và địa chỉ
  case OP_EP: printf("EP"); break;                              // Exit Procedure (không tham số)
  case OP_EF: printf("EF"); break;                              // Exit Function (không tham số)
  case OP_RC: printf("RC"); break;                              // Read Char (không tham số)
  case OP_RI: printf("RI"); break;                              // Read Integer (không tham số)
  case OP_WRC: printf("WRC"); break;                            // Write Char (không tham số)
  case OP_WRI: printf("WRI"); break;                            // Write Integer (không tham số)
  case OP_WLN: printf("WLN"); break;                            // Write Line (không tham số)
  case OP_AD: printf("AD"); break;                              // Add (không tham số)
  case OP_SB: printf("SB"); break;                              // Subtract (không tham số)
  case OP_ML: printf("ML"); break;                              // Multiply (không tham số)
  case OP_DV: printf("DV"); break;                              // Divide (không tham số)
  case OP_NEG: printf("NEG"); break;                            // Negate (không tham số)
  case OP_CV: printf("CV"); break;                              // Copy Top/Convert (không tham số)
  case OP_EQ: printf("EQ"); break;                              // Equal (không tham số)
  case OP_NE: printf("NE"); break;                              // Not Equal (không tham số)
  case OP_GT: printf("GT"); break;                              // Greater Than (không tham số)
  case OP_LT: printf("LT"); break;                              // Less Than (không tham số)
  case OP_GE: printf("GE"); break;                              // Greater or Equal (không tham số)
  case OP_LE: printf("LE"); break;                              // Less or Equal (không tham số)

  case OP_BP: printf("BP"); break;                              // Breakpoint (không tham số)
  default: break;  // Lệnh không hợp lệ - không làm gì
  }
}

/**
 * @brief In toàn bộ code block ra console
 * 
 * CHỨC NĂNG:
 *   - In tất cả các lệnh trong code block theo thứ tự
 *   - Mỗi lệnh có kèm số thứ tự (địa chỉ lệnh)
 * 
 * THAM SỐ:
 *   - codeBlock: Code block cần in
 * 
 * ĐỊNH DẠNG:
 *   0:  LA 1,5
 *   1:  LV 0,3
 *   2:  AD
 *   ...
 * 
 * SỬ DỤNG:
 *   - Debug: xem toàn bộ mã đã sinh ra
 *   - Kiểm tra đúng/sai của code generation
 */
void printCodeBlock(CodeBlock* codeBlock) {
  Instruction* pc = codeBlock->code;  // Con trỏ duyệt qua các lệnh
  int i;
  for (i = 0 ; i < codeBlock->codeSize; i ++) {
    printf("%d:  ",i);          // In địa chỉ lệnh (số thứ tự)
    printInstruction(pc);       // In nội dung lệnh
    printf("\n");               // Xuống dòng
    pc ++;                      // Chuyển sang lệnh tiếp theo
  }
}

/**
 * @brief Đọc mã lệnh từ file nhị phân vào code block
 * 
 * CHỨC NĂNG:
 *   - Đọc các lệnh từ file đã biên dịch (file .kpl compiled)
 *   - Load vào code block để thực thi bởi VM
 * 
 * THAM SỐ:
 *   - codeBlock: Code block đích (phải đã khởi tạo)
 *   - f: File handle (đã mở ở chế độ đọc nhị phân "rb")
 * 
 * THUẬT TOÁN:
 *   - Đọc từng khối MAX_BLOCK lệnh cho đến hết file
 *   - Tích lũy vào codeSize
 * 
 * VÍ DỤ:
 *   FILE* f = fopen("example.compiled", "rb");
 *   CodeBlock* cb = createCodeBlock(1000);
 *   loadCode(cb, f);
 *   fclose(f);
 */
void loadCode(CodeBlock* codeBlock, FILE* f) {
  Instruction* code = codeBlock->code;  // Con trỏ đến đầu mảng
  int n;  // Số lệnh đọc được trong mỗi lần fread

  codeBlock->codeSize = 0;  // Reset số lệnh về 0
  
  // Đọc cho đến khi hết file
  while (!feof(f)) {
    // Đọc tối đa MAX_BLOCK lệnh mỗi lần
    n = fread(code, sizeof(Instruction), MAX_BLOCK, f);
    code += n;                    // Dịch con trỏ đến vị trí tiếp theo
    codeBlock->codeSize += n;     // Cộng dồn số lệnh đã đọc
  }
}

/**
 * @brief Ghi mã lệnh từ code block ra file nhị phân
 * 
 * CHỨC NĂNG:
 *   - Lưu toàn bộ mã lệnh vào file để chạy sau
 *   - VM sẽ đọc lại file này để thực thi
 * 
 * THAM SỐ:
 *   - codeBlock: Code block nguồn (chứa mã đã sinh)
 *   - f: File handle (đã mở ở chế độ ghi nhị phân "wb")
 * 
 * ĐỊNH DẠNG FILE:
 *   - Ghi trực tiếp mảng Instruction dưới dạng nhị phân
 *   - Không có header, chỉ có raw data
 * 
 * VÍ DỤ:
 *   FILE* f = fopen("output.compiled", "wb");
 *   saveCode(cb, f);
 *   fclose(f);
 */
void saveCode(CodeBlock* codeBlock, FILE* f) {
  // Ghi toàn bộ mảng lệnh vào file một lần
  // fwrite(data, size_of_element, count, file)
  fwrite(codeBlock->code, sizeof(Instruction), codeBlock->codeSize, f);
}
