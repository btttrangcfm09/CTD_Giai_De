/* Code Generation - Implementation (Triển khai sinh mã)
 * @copyright (c) 2008, Hedspi, Hanoi University of Technology
 * @author Huu-Duc Nguyen
 * @version 1.0
 * 
 * Mô tả:
 *   Triển khai các hàm sinh mã máy ảo cho KPL.
 *   Quản lý code buffer và sinh các lệnh VM.
 */
// Stack-based: Hiểu lệnh nào push, lệnh nào pop
// - LA, LV, LC → push lên stack
// - ST → pop 2 giá trị (địa chỉ + giá trị) rồi gán
// - AD, SB → pop 2 số, push kết quả

// Thứ tự gọi emitXX():
// - LA rồi LV rồi ST → gán biến
// - Condition rồi FJ → if-else

#include <stdio.h>
#include "reader.h"
#include "codegen.h"  

// Kích thước tối đa của code buffer (10000 lệnh)
#define CODE_SIZE 10000

// ===== BIẾN TOÀN CỤC EXTERN =====
extern SymTab* symtab;              // Bảng ký hiệu (từ symtab.c)
extern Object* readiFunction;       // Hàm có sẵn READI
extern Object* readcFunction;       // Hàm có sẵn READC
extern Object* writeiProcedure;     // Thủ tục có sẵn WRITEI
extern Object* writecProcedure;     // Thủ tục có sẵn WRITEC
extern Object* writelnProcedure;    // Thủ tục có sẵn WRITELN

// ===== BIẾN TOÀN CỤC =====
CodeBlock* codeBlock;               // Bộ đệm chứa mã máy ảo

/**
 * @brief Tính mức lồng nhau từ scope hiện tại đến scope đích
 * 
 * Thuật toán:
 * 1. Bắt đầu từ scope hiện tại (symtab->currentScope)
 * 2. Đếm số lần nhảy qua outer cho đến khi gặp scope đích
 * 3. Số lần đó là nested level
 * 
 * Ví dụ:
 *   PROGRAM (scope0)
 *     PROCEDURE A (scope1) 
 *       PROCEDURE B (scope2)
 *         VAR x (scope2)
 *       END;
 *     END;
 * 
 * Nếu hiện tại ở scope2 và cần truy cập:
 * - Biến trong scope2: level = 0 (cùng scope)
 * - Biến trong scope1: level = 1 (nhảy 1 lần)
 * - Biến trong scope0: level = 2 (nhảy 2 lần)
 * 
 * @param scope Scope đích cần truy cập
 * @return Mức lồng nhau (0 = cùng scope, 1 = cha, 2 = ông, ...)
 */
int computeNestedLevel(Scope* scope) {
  int level = 0;                        // Khởi tạo level = 0
  Scope* tmp = symtab->currentScope;    // Bắt đầu từ scope hiện tại
  
  // Duyệt ngược từ scope hiện tại ra ngoài cho đến khi gặp scope đích
  while (tmp != scope) {
    tmp = tmp->outer;                   // Nhảy ra scope ngoài
    level ++;                           // Tăng level
  }
  return level;                         // Trả về số mức lồng nhau
}

/**
 * @brief Sinh mã lấy địa chỉ của biến
 * 
 * Sinh lệnh LA (Load Address) với:
 * - level: mức lồng nhau giữa scope hiện tại và scope chứa biến
 * - offset: vị trí của biến trong frame
 * 
 * Kết quả: Địa chỉ của biến được đẩy lên stack
 * Dùng khi cần gán giá trị cho biến (làm L-value)
 */
void genVariableAddress(Object* var) {
  int level = computeNestedLevel(VARIABLE_SCOPE(var));  // Tính nested level
  int offset = VARIABLE_OFFSET(var);                    // Lấy offset của biến
  genLA(level, offset);                                 // Sinh lệnh LA
}

/**
 * @brief Sinh mã lấy giá trị của biến
 * 
 * Sinh lệnh LV (Load Value) với:
 * - level: mức lồng nhau giữa scope hiện tại và scope chứa biến
 * - offset: vị trí của biến trong frame
 * 
 * Kết quả: Giá trị của biến được đẩy lên stack
 * Dùng khi đọc giá trị biến trong biểu thức (làm R-value)
 */
void genVariableValue(Object* var) {
  int level = computeNestedLevel(VARIABLE_SCOPE(var));  // Tính nested level
  int offset = VARIABLE_OFFSET(var);                    // Lấy offset của biến
  genLV(level, offset);                                 // Sinh lệnh LV
}

/**
 * @brief Sinh mã lấy địa chỉ của tham số
 * 
 * Tương tự genVariableAddress nhưng dùng cho tham số.
 * Tham số có offset âm (trước frame base).
 * Dùng cho tham biến (PARAM_REFERENCE).
 */
void genParameterAddress(Object* param) {
  int level = computeNestedLevel(PARAMETER_SCOPE(param));  // Tính nested level
  int offset = PARAMETER_OFFSET(param);                    // Lấy offset tham số (âm)
  genLA(level, offset);                                    // Sinh lệnh LA
}

/**
 * @brief Sinh mã lấy giá trị của tham số
 * 
 * Tương tự genVariableValue nhưng dùng cho tham số.
 * Dùng cho tham trị (PARAM_VALUE).
 */
void genParameterValue(Object* param) {
  int level = computeNestedLevel(PARAMETER_SCOPE(param));  // Tính nested level
  int offset = PARAMETER_OFFSET(param);                    // Lấy offset tham số (âm)
  genLV(level, offset);                                    // Sinh lệnh LV
}

/**
 * @brief Sinh mã lấy địa chỉ vị trí lưu giá trị trả về của hàm
 * 
 * Giá trị trả về được lưu ở offset 0 (RETURN_VALUE_OFFSET) tức base+0.
 * Base pointer (BP) của frame trỏ đúng vào return value.
 * Dùng khi gán giá trị trả về: func_name := value
 */
void genReturnValueAddress(Object* func) {
  int level = computeNestedLevel(FUNCTION_SCOPE(func));  // Tính nested level
  int offset = RETURN_VALUE_OFFSET;                      // Offset = 0 (tại base)
  genLA(level, offset);                                  // Sinh lệnh LA
}

/**
 * @brief Sinh mã lấy giá trị trả về của hàm
 * 
 * Dùng khi đọc giá trị trả về trong biểu thức.
 */
void genReturnValueValue(Object* func) {
  int level = computeNestedLevel(FUNCTION_SCOPE(func));  // Tính nested level
  int offset = RETURN_VALUE_OFFSET;                      // Offset = 0
  genLV(level, offset);                                  // Sinh lệnh LV
}

/**
 * @brief Sinh mã gọi thủ tục có sẵn (built-in)
 * 
 * Các thủ tục có sẵn không cần CALL, chỉ sinh lệnh đặc biệt:
 * - WRITEI: sinh lệnh WRI (Write Integer)
 * - WRITEC: sinh lệnh WRC (Write Char)
 * - WRITELN: sinh lệnh WLN (Write Line)
 */
void genPredefinedProcedureCall(Object* proc) {
  if (proc == writeiProcedure)        // Nếu là WRITEI
    genWRI();                         // Sinh lệnh WRI
  else if (proc == writecProcedure)   // Nếu là WRITEC
    genWRC();                         // Sinh lệnh WRC
  else if (proc == writelnProcedure)  // Nếu là WRITELN
    genWLN();                         // Sinh lệnh WLN
}

/**
 * @brief Sinh mã gọi thủ tục do người dùng định nghĩa
 * 
 * Sinh lệnh CALL với:
 * - level: mức lồng nhau (tính từ scope ngoài thủ tục)
 * - codeAddress: địa chỉ lệnh đầu tiên của thân thủ tục
 * 
 * Tại sao dùng scope->outer?
 * - proc->procAttrs->scope = scope của THÂN thủ tục (bên trong BEGIN...END)
 * - proc->procAttrs->scope->outer = scope NƠI KHAI BÁO thủ tục
 * 
 * Ví dụ:
 *   PROGRAM Main;           // scope0 (program)
 *   VAR x: INTEGER;
 *   
 *   PROCEDURE A;            // Khai báo ở scope0
 *   VAR y: INTEGER;         // scope1 (thân A)
 *   BEGIN
 *     ...
 *   END;
 *   
 *   PROCEDURE B;            // Khai báo ở scope0
 *   VAR z: INTEGER;         // scope2 (thân B)  
 *   BEGIN
 *     CALL A;  <- Gọi A từ đây
 *   END;
 * 
 * Khi sinh mã CALL A từ trong B:
 * - currentScope = scope2 (thân B)
 * - A.procAttrs->scope = scope1 (thân A)
 * - A.procAttrs->scope->outer = scope0 (nơi khai báo A)
 * - level = computeNestedLevel(scope0) = 1
 *   (vì từ scope2 -> scope0 chỉ nhảy 1 lần: scope2->outer = scope0)
 * 
 * Vì A và B cùng khai báo ở scope0, nên level = 1.
 * Level này dùng để VM tìm đúng static link khi tạo frame cho A.
 */
void genProcedureCall(Object* proc) {
  // Tính level từ scope ngoài thủ tục (nơi khai báo)
  int level = computeNestedLevel(proc->procAttrs->scope->outer);
  genCALL(level, proc->procAttrs->codeAddress);  // Sinh lệnh CALL
}

/**
 * @brief Sinh mã gọi hàm có sẵn (built-in)
 * 
 * Các hàm có sẵn:
 * - READI: sinh lệnh RI (Read Integer)
 * - READC: sinh lệnh RC (Read Char)
 * Kết quả được đẩy lên stack.
 */
void genPredefinedFunctionCall(Object* func) {
  if (func == readiFunction)          // Nếu là READI
    genRI();                          // Sinh lệnh RI
  else if (func == readcFunction)     // Nếu là READC
    genRC();                          // Sinh lệnh RC
}

/**
 * @brief Sinh mã gọi hàm do người dùng định nghĩa
 * 
 * Tương tự genProcedureCall, nhưng cho hàm.
 * Sau khi CALL, giá trị trả về sẽ nằm ở RETURN_VALUE_OFFSET.
 */
void genFunctionCall(Object* func) {
  // Tính level từ scope ngoài hàm (nơi khai báo)
  int level = computeNestedLevel(func->funcAttrs->scope->outer);
  genCALL(level, func->funcAttrs->codeAddress);  // Sinh lệnh CALL
}

// ===== CÁC HÀM WRAPPER SINH LỆNH MÁY ẢO =====
// Các hàm này là wrapper cho các hàm emit trong instructions.c
// Chúng tự động truyền codeBlock toàn cục vào các hàm emit

// --- Lệnh load địa chỉ ---
void genLA(int level, int offset) {
  emitLA(codeBlock, level, offset);   // Gọi emitLA với codeBlock
}

// --- Lệnh load giá trị ---
void genLV(int level, int offset) {
  emitLV(codeBlock, level, offset);   // Gọi emitLV với codeBlock
}

// --- Lệnh load hằng số ---
void genLC(WORD constant) {
  emitLC(codeBlock, constant);        // Đẩy hằng số lên stack
}

// --- Lệnh load gian tiếp (load indirect) ---
void genLI(void) {
  emitLI(codeBlock);                  // Lấy giá trị từ địa chỉ trên stack
}

// --- Lệnh cấp phát bộ nhớ ---
void genINT(int delta) {
  emitINT(codeBlock,delta);           // Tăng stack top lên delta ô
}

// --- Lệnh giải phóng bộ nhớ ---
void genDCT(int delta) {
  emitDCT(codeBlock,delta);           // Giảm stack top xuống delta ô
}

// --- Lệnh nhảy vô điều kiện ---
Instruction* genJ(CodeAddress label) {
  Instruction* inst = codeBlock->code + codeBlock->codeSize;  // Lưu con trỏ lệnh
  emitJ(codeBlock,label);                                     // Sinh lệnh J
  return inst;  // Trả về con trỏ để có thể cập nhật label sau (forward jump)
}

// --- Lệnh nhảy có điều kiện (false jump) ---
Instruction* genFJ(CodeAddress label) {
  Instruction* inst = codeBlock->code + codeBlock->codeSize;  // Lưu con trỏ lệnh
  emitFJ(codeBlock, label);                                   // Sinh lệnh FJ
  return inst;  // Trả về để có thể cập nhật label sau
}

// --- Lệnh dừng chương trình ---
void genHL(void) {
  emitHL(codeBlock);                  // Halt - kết thúc chương trình
}

// --- Lệnh lưu giá trị ---
void genST(void) {
  emitST(codeBlock);                  // Store - lưu giá trị vào địa chỉ
}

// --- Lệnh gọi hàm/thủ tục ---
void genCALL(int level, CodeAddress label) {
  emitCALL(codeBlock, level, label);  // Gọi CALL với level và địa chỉ
}

// --- Lệnh vào chương trình chính ---
void genEP(void) {
  emitEP(codeBlock);                  // Enter Program
}

// --- Lệnh vào hàm ---
void genEF(void) {
  emitEF(codeBlock);                  // Enter Function
}

// --- Lệnh đọc ký tự ---
void genRC(void) {
  emitRC(codeBlock);                  // Read Char - đọc 1 ký tự
}

// --- Lệnh đọc số nguyên ---
void genRI(void) {
  emitRI(codeBlock);                  // Read Integer - đọc 1 số nguyên
}

// --- Lệnh in ký tự ---
void genWRC(void) {
  emitWRC(codeBlock);                 // Write Char - in 1 ký tự
}

// --- Lệnh in số nguyên ---
void genWRI(void) {
  emitWRI(codeBlock);                 // Write Integer - in 1 số nguyên
}

// --- Lệnh xuống dòng ---
void genWLN(void) {
  emitWLN(codeBlock);                 // Write Line - in xuống dòng
}

// --- Lệnh cộng ---
void genAD(void) {
  emitAD(codeBlock);                  // Add - cộng 2 số trên stack
}

// --- Lệnh trừ ---
void genSB(void) {
  emitSB(codeBlock);                  // Subtract - trừ 2 số trên stack
}

// --- Lệnh nhân ---
void genML(void) {
  emitML(codeBlock);                  // Multiply - nhân 2 số trên stack
}

// --- Lệnh chia ---
void genDV(void) {
  emitDV(codeBlock);                  // Divide - chia 2 số trên stack
}

// --- Lệnh đổi dấu ---
void genNEG(void) {
  emitNEG(codeBlock);                 // Negate - đổi dấu số trên stack
}

// --- Lệnh chuyển CHAR sang INT ---
void genCV(void) {
  emitCV(codeBlock);                  // Convert - chuyển ký tự sang số
}

// --- Các lệnh so sánh ---
void genEQ(void) {
  emitEQ(codeBlock);                  // Equal - so sánh bằng
}

void genNE(void) {
  emitNE(codeBlock);                  // Not Equal - so sánh khác
}

void genGT(void) {
  emitGT(codeBlock);                  // Greater Than - so sánh lớn hơn
}

void genGE(void) {
  emitGE(codeBlock);                  // Greater or Equal - lớn hơn hoặc bằng
}

void genLT(void) {
  emitLT(codeBlock);                  // Less Than - so sánh nhỏ hơn
}

void genLE(void) {
  emitLE(codeBlock);                  // Less or Equal - nhỏ hơn hoặc bằng
}

// ===== HÀM HỖ TRỢ SINH MÃ =====

/**
 * @brief Cập nhật địa chỉ đích cho lệnh J (Jump)
 * 
 * Dùng cho forward jump - khi sinh lệnh nhảy nhưng chưa biết đích.
 * Ví dụ: IF-THEN-ELSE, WHILE, FOR...
 * 
 * @param jmp Con trỏ đến lệnh J cần cập nhật
 * @param label Địa chỉ đích mới (nhãn đích)
 */
void updateJ(Instruction* jmp, CodeAddress label) {
  jmp->q = label;  // Cập nhật trường q (destination) của lệnh J
}

/**
 * @brief Cập nhật địa chỉ đích cho lệnh FJ (False Jump)
 * Tương tự updateJ nhưng cho lệnh nhảy có điều kiện
 */
void updateFJ(Instruction* jmp, CodeAddress label) {
  jmp->q = label;  // Cập nhật trường q của lệnh FJ
}

/**
 * @brief Lấy địa chỉ lệnh hiện tại (vị trí lệnh tiếp theo sẽ được sinh)
 * 
 * Dùng để đặt nhãn (label) cho các lệnh nhảy.
 * Ví dụ: Lưu địa chỉ đầu vòng lặp, đầu ELSE...
 * 
 * @return Địa chỉ lệnh hiện tại (số lượng lệnh đã sinh)
 */
CodeAddress getCurrentCodeAddress(void) {
  return codeBlock->codeSize;  // Trả về số lượng lệnh hiện tại
}

/**
 * @brief Kiểm tra hàm có phải là hàm có sẵn (built-in) không
 * @return 1 nếu là READI hoặc READC, 0 nếu không
 */
int isPredefinedFunction(Object* func) {
  return ((func == readiFunction) || (func == readcFunction));
}

/**
 * @brief Kiểm tra thủ tục có phải là thủ tục có sẵn không
 * @return 1 nếu là WRITEI/WRITEC/WRITELN, 0 nếu không
 */
int isPredefinedProcedure(Object* proc) {
  return ((proc == writeiProcedure) || (proc == writecProcedure) || (proc == writelnProcedure));
}

// ===== HÀM QUẢN LÝ CODE BUFFER =====

/**
 * @brief Khởi tạo bộ đệm mã (code buffer)
 * 
 * Cấp phát bộ nhớ cho CODE_SIZE (10000) lệnh.
 * Gọi trước khi bắt đầu sinh mã.
 */
void initCodeBuffer(void) {
  codeBlock = createCodeBlock(CODE_SIZE);  // Tạo code block với kích thước cho trước
}

/**
 * @brief In toàn bộ mã máy ảo ra console
 * Dùng cho debug, xem mã đã sinh ra
 */
void printCodeBuffer(void) {
  printCodeBlock(codeBlock);  // Gọi hàm print từ instructions.c
}

/**
 * @brief Giải phóng bộ nhớ code buffer
 * Gọi khi kết thúc biên dịch
 */
void cleanCodeBuffer(void) {
  freeCodeBlock(codeBlock);  // Giải phóng bộ nhớ
}

/**
 * @brief Lưu mã máy ảo ra file nhị phân (.o)
 * 
 * File này có thể được chạy bằng interpreter (kplrun).
 * 
 * @param fileName Tên file output (ví dụ: "program.o")
 * @return IO_SUCCESS (0) nếu thành công, IO_ERROR (-1) nếu lỗi
 */
int serialize(char* fileName) {
  FILE* f;

  f = fopen(fileName, "wb");  // Mở file ở chế độ ghi nhị phân
  if (f == NULL) return IO_ERROR;  // Nếu không mở được -> trả về lỗi
  saveCode(codeBlock, f);  // Lưu code block vào file
  fclose(f);  // Đóng file
  return IO_SUCCESS;  // Trả về thành công
}
