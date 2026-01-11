/* Instructions - Lệnh máy ảo (VM Bytecode)
 * @copyright (c) 2008, Hedspi, Hanoi University of Technology
 * @author Huu-Duc Nguyen
 * @version 1.0
 * 
 * Mô tả:
 *   Module này định nghĩa tập lệnh máy ảo cho KPL.
 *   Bao gồm:
 *   - Định nghĩa các opcode (mã lệnh)
 *   - Cấu trúc lệnh và code block
 *   - Hàm emit lệnh vào code buffer
 *   - Hàm load/save mã nhị phân
 */

#ifndef __INSTRUCTIONS_H__
#define __INSTRUCTIONS_H__

#include <stdio.h>

// ===== CÁC DEFINE CƠ BẢN =====
#define TRUE 1                    // Giá trị logic đúng
#define FALSE 0                   // Giá trị logic sai
#define DC_VALUE 0                // Don't Care - giá trị không quan tâm (khi p hoặc q không dùng)
#define INT_SIZE 1                // Kích thước INTEGER trên stack (1 word)
#define CHAR_SIZE 1               // Kích thước CHAR trên stack (1 word)

typedef int WORD;                 // Một từ (word) trên stack = 1 số nguyên             

// ===== TẬP LỆNH MÁY ẢO =====
/**
 * enum OpCode - Mã lệnh của máy ảo
 * 
 * Quy ước ký hiệu:
 * - t: top of stack (chỉ số đỉnh stack)
 * - b: base pointer (con trỏ frame hiện tại)
 * - pc: program counter (địa chỉ lệnh tiếp theo)
 * - s[i]: ô nhớ thứ i trên stack
 * - p: tham số level (mức lồng nhau)
 * - q: tham số offset hoặc địa chỉ/giá trị
 * - base(p): tính địa chỉ frame cách p mức từ frame hiện tại
 */
enum OpCode {
  // --- Lệnh load/store ---
  OP_LA,   // Load Address:    t := t + 1; s[t] := base(p) + q;
           //                  Đẩy địa chỉ (base+offset) lên stack
  
  OP_LV,   // Load Value:      t := t + 1; s[t] := s[base(p) + q];
           //                  Đẩy giá trị tại (base+offset) lên stack
  
  OP_LC,   // Load Constant:   t := t + 1; s[t] := q;
           //                  Đẩy hằng số q lên stack
  
  OP_LI,   // Load Indirect:   s[t] := s[s[t]];
           //                  Lấy giá trị tại địa chỉ đang nằm trên đỉnh stack
  
  // --- Lệnh quản lý stack ---
  OP_INT,  // INcrement Top:   t := t + q;
           //                  Cấp phát q ô nhớ trên stack
  
  OP_DCT,  // DeCrement Top:   t := t - q;
           //                  Giải phóng q ô nhớ trên stack
  
  // --- Lệnh nhảy ---
  OP_J,    // Jump:            pc := q;
           //                  Nhảy vô điều kiện đến địa chỉ q
  
  OP_FJ,   // False Jump:      if s[t] = 0 then pc := q; t := t - 1;
           //                  Nhảy đến q nếu đỉnh stack = 0 (false)
  
  OP_HL,   // Halt:            Dừng chương trình
  
  OP_ST,   // Store:           s[s[t-1]] := s[t]; t := t - 2;
           //                  Lưu giá trị s[t] vào địa chỉ s[t-1]
  
  // --- Lệnh gọi hàm/thủ tục ---
  OP_CALL, // Call:            s[t+2] := b; s[t+3] := pc; s[t+4]:= base(p);
           //                  b := t+1; pc := q;
           //                  Lưu dynamic link, return address, static link
           //                  Nhảy đến địa chỉ q (thân hàm/thủ tục)
  
  OP_EP,   // Exit Procedure:  t := b - 1; pc := s[b+2]; b := s[b+1];
           //                  Thoát thủ tục, khôi phục frame cũ
  
  OP_EF,   // Exit Function:   t := b; pc := s[b+2]; b := s[b+1];
           //                  Thoát hàm, giữ return value trên stack
  
  // --- Lệnh I/O ---
  OP_RC,   // Read Char:       read one character into s[s[t]]; t := t - 1;
           //                  Đọc 1 ký tự vào địa chỉ s[t]
  
  OP_RI,   // Read Integer:    read integer to s[s[t]]; t := t-1;
           //                  Đọc 1 số nguyên vào địa chỉ s[t]
  
  OP_WRC,  // Write Char:      write one character from s[t]; t := t-1;
           //                  In ký tự s[t]
  
  OP_WRI,  // Write Integer:   write integer from s[t]; t := t-1;
           //                  In số nguyên s[t]
  
  OP_WLN,  // WriteLN:         CR/LF
           //                  Xuống dòng
  
  // --- Lệnh số học ---
  OP_AD,   // Add:             t := t-1; s[t] := s[t] + s[t+1];
           //                  Cộng: s[t] + s[t+1]
  
  OP_SB,   // Subtract:        t := t-1; s[t] := s[t] - s[t+1];
           //                  Trừ: s[t] - s[t+1]
  
  OP_ML,   // Multiply:        t := t-1; s[t] := s[t] * s[t+1];
           //                  Nhân: s[t] * s[t+1]
  
  OP_DV,   // Divide:          t := t-1; s[t] := s[t] / s[t+1];
           //                  Chia: s[t] / s[t+1]
  
  OP_NEG,  // Negative:        s[t] := -s[t];
           //                  Đổi dấu: -s[t]
  
  OP_CV,   // Copy Top:        s[t+1] := s[t]; t := t + 1;
           //                  Sao chép đỉnh stack (dùng cho chuyển kiểu)
  
  // --- Lệnh so sánh (kết quả: 1=true, 0=false) ---
  OP_EQ,   // Equal:           t := t - 1; if s[t] = s[t+1] then s[t] := 1 else s[t] := 0;
  OP_NE,   // Not Equal:       t := t - 1; if s[t] != s[t+1] then s[t] := 1 else s[t] := 0;
  OP_GT,   // Greater:         t := t - 1; if s[t] > s[t+1] then s[t] := 1 else s[t] := 0;
  OP_LT,   // Less:            t := t - 1; if s[t] < s[t+1] then s[t] := 1 else s[t] := 0;
  OP_GE,   // Greater or Equal:t := t - 1; if s[t] >= s[t+1] then s[t] := 1 else s[t] := 0;
  OP_LE,   // Less or Equal:   t := t - 1; if s[t] <= s[t+1] then s[t] := 1 else s[t] := 0;

  OP_BP    // Break Point:     Chỉ dùng cho debug
};

// ===== CẤU TRÚC LỆNH =====
/**
 * struct Instruction - Một lệnh máy ảo
 * 
 * Mỗi lệnh gồm 3 phần:
 * - op: mã lệnh (opcode)
 * - p: tham số 1 (thường là level cho LA/LV/CALL)
 * - q: tham số 2 (thường là offset hoặc địa chỉ/giá trị)
 */
struct Instruction_ {
  enum OpCode op;  // Mã lệnh
  WORD p;          // Tham số 1 (level hoặc DC_VALUE)
  WORD q;          // Tham số 2 (offset/address/value hoặc DC_VALUE)
};

typedef struct Instruction_ Instruction;
typedef int CodeAddress;  // Địa chỉ lệnh (số thứ tự lệnh)

// ===== CẤU TRÚC CODE BLOCK =====
/**
 * struct CodeBlock - Bộ đệm chứa mã máy ảo
 * 
 * Chứa mảng các lệnh đã sinh ra trong quá trình biên dịch.
 */
struct CodeBlock_ {
  Instruction* code;  // Mảng các lệnh
  int codeSize;       // Số lượng lệnh hiện tại
  int maxSize;        // Kích thước tối đa của mảng
};

typedef struct CodeBlock_ CodeBlock;

// ===== HÀM QUẢN LÝ CODE BLOCK =====

/**
 * @brief Tạo một code block mới
 * @param maxSize Số lượng lệnh tối đa
 * @return Con trỏ đến code block mới
 */
CodeBlock* createCodeBlock(int maxSize);

/**
 * @brief Giải phóng bộ nhớ code block
 * @param codeBlock Code block cần giải phóng
 */
void freeCodeBlock(CodeBlock* codeBlock);

// ===== HÀM EMIT LỆNH =====

/**
 * @brief Emit một lệnh tổng quát vào code block
 * @param codeBlock Code block đích
 * @param op Mã lệnh
 * @param p Tham số 1
 * @param q Tham số 2
 * @return 1 nếu thành công, 0 nếu code block đầy
 */
int emitCode(CodeBlock* codeBlock, enum OpCode op, WORD p, WORD q);

// --- Các hàm emit cụ thể cho từng lệnh ---
// (Wrapper cho emitCode với opcode tương ứng)

int emitLA(CodeBlock* codeBlock, WORD p, WORD q);   // Emit Load Address
int emitLV(CodeBlock* codeBlock, WORD p, WORD q);   // Emit Load Value
int emitLC(CodeBlock* codeBlock, WORD q);           // Emit Load Constant
int emitLI(CodeBlock* codeBlock);                   // Emit Load Indirect
int emitINT(CodeBlock* codeBlock, WORD q);          // Emit Increment Top
int emitDCT(CodeBlock* codeBlock, WORD q);          // Emit Decrement Top
int emitJ(CodeBlock* codeBlock, WORD q);            // Emit Jump
int emitFJ(CodeBlock* codeBlock, WORD q);           // Emit False Jump
int emitHL(CodeBlock* codeBlock);                   // Emit Halt
int emitST(CodeBlock* codeBlock);                   // Emit Store
int emitCALL(CodeBlock* codeBlock, WORD p, WORD q); // Emit Call
int emitEP(CodeBlock* codeBlock);                   // Emit Exit Procedure
int emitEF(CodeBlock* codeBlock);                   // Emit Exit Function
int emitRC(CodeBlock* codeBlock);                   // Emit Read Char
int emitRI(CodeBlock* codeBlock);                   // Emit Read Integer
int emitWRC(CodeBlock* codeBlock);                  // Emit Write Char
int emitWRI(CodeBlock* codeBlock);                  // Emit Write Integer
int emitWLN(CodeBlock* codeBlock);                  // Emit Write Line
int emitAD(CodeBlock* codeBlock);                   // Emit Add
int emitSB(CodeBlock* codeBlock);                   // Emit Subtract
int emitML(CodeBlock* codeBlock);                   // Emit Multiply
int emitDV(CodeBlock* codeBlock);                   // Emit Divide
int emitNEG(CodeBlock* codeBlock);                  // Emit Negate
int emitCV(CodeBlock* codeBlock);                   // Emit Copy Top (Convert)
int emitEQ(CodeBlock* codeBlock);                   // Emit Equal
int emitNE(CodeBlock* codeBlock);                   // Emit Not Equal
int emitGT(CodeBlock* codeBlock);                   // Emit Greater Than
int emitLT(CodeBlock* codeBlock);                   // Emit Less Than
int emitGE(CodeBlock* codeBlock);                   // Emit Greater or Equal
int emitLE(CodeBlock* codeBlock);                   // Emit Less or Equal
int emitBP(CodeBlock* codeBlock);                   // Emit Breakpoint

// ===== HÀM TIỆN ÍCH =====

/**
 * @brief In một lệnh ra console (dùng cho debug)
 * @param instruction Lệnh cần in
 */
void printInstruction(Instruction* instruction);

/**
 * @brief In toàn bộ code block ra console
 * @param codeBlock Code block cần in
 */
void printCodeBlock(CodeBlock* codeBlock);

/**
 * @brief Đọc mã từ file nhị phân vào code block
 * @param codeBlock Code block đích
 * @param f File handle
 */
void loadCode(CodeBlock* codeBlock, FILE* f);

/**
 * @brief Ghi mã từ code block ra file nhị phân
 * @param codeBlock Code block nguồn
 * @param f File handle
 */
void saveCode(CodeBlock* codeBlock, FILE* f);

#endif
