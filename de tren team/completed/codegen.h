/* Code Generation (Sinh mã)
 * @copyright (c) 2008, Hedspi, Hanoi University of Technology
 * @author Huu-Duc Nguyen
 * @version 1.0
 * 
 * Mô tả:
 *   Module này thực hiện sinh mã máy ảo (VM bytecode) từ cây cú pháp.
 *   Bao gồm:
 *   - Sinh mã cho biến, tham số, hàm, thủ tục
 *   - Sinh mã cho biểu thức, câu lệnh
 *   - Quản lý địa chỉ và offset trong stack frame
 *   - Tính toán nested level (mức lồng nhau)
 */

#ifndef __CODEGEN_H__
#define __CODEGEN_H__

#include "symtab.h"
#include "instructions.h"

// ===== CÁC MACRO TIỆN ÍCH =====

/**
 * RESERVED_WORDS = Số ô nhớ dành riêng ĐẦU TIÊN trong mỗi stack frame
 * 
 * Khi gọi hàm/thủ tục, frame base (BP) trỏ đến return value.
 * 4 ô đầu tiên luôn cố định:
 * [base+0] Return Value   - Giá trị trả về (chỉ cho hàm)
 * [base+1] Dynamic Link   - Con trỏ đến frame của hàm gọi (caller)
 * [base+2] Return Address - Địa chỉ lệnh quay về sau khi kết thúc  
 * [base+3] Static Link    - Con trỏ đến frame của scope cha (theo static scope)
 * [base+4] Tham số 1 / Biến cục bộ đầu tiên
 * [base+5] Tham số 2 / Biến cục bộ thứ hai
 * ...
 * 
 * Lưu ý: Các macro *_OFFSET bên dưới là offset DƯƠNG tính từ base!
 */
#define RESERVED_WORDS 4

// ===== MACRO TRUY CẬP THUỘC TÍNH THỦ TỤC =====
// Lấy số lượng tham số của thủ tục
#define PROCEDURE_PARAM_COUNT(proc) (proc->procAttrs->numOfParams)
// Lấy scope (phạm vi) của thủ tục
#define PROCEDURE_SCOPE(proc) (proc->procAttrs->scope)
// Lấy kích thước frame (số ô nhớ cần cấp phát) của thủ tục
#define PROCEDURE_FRAME_SIZE(proc) (proc->procAttrs->scope->frameSize)

// ===== MACRO TRUY CẬP THUỘC TÍNH HÀM =====
// Lấy số lượng tham số của hàm
#define FUNCTION_PARAM_COUNT(func) (func->funcAttrs->numOfParams)
// Lấy scope (phạm vi) của hàm
#define FUNCTION_SCOPE(func) (func->funcAttrs->scope)
// Lấy kích thước frame của hàm
#define FUNCTION_FRAME_SIZE(func) (func->funcAttrs->scope->frameSize)

// ===== MACRO TRUY CẬP THUỘC TÍNH CHƯƠNG TRÌNH =====
// Lấy scope toàn cục của chương trình
#define PROGRAM_SCOPE(prog) (prog->progAttrs->scope)
// Lấy kích thước frame của chương trình
#define PROGRAM_FRAME_SIZE(prog) (prog->progAttrs->scope->frameSize)

// ===== MACRO TRUY CẬP THUỘC TÍNH BIẾN =====
// Lấy offset (vị trí tương đối) của biến trong stack frame
#define VARIABLE_OFFSET(var) (var->varAttrs->localOffset)
// Lấy scope chứa biến
#define VARIABLE_SCOPE(var) (var->varAttrs->scope)

// ===== MACRO TRUY CẬP THUỘC TÍNH THAM SỐ =====
// Lấy offset của tham số trong stack frame (offset âm)
#define PARAMETER_OFFSET(param) (param->paramAttrs->localOffset)
// Lấy scope chứa tham số
#define PARAMETER_SCOPE(param) (param->paramAttrs->scope)

// ===== VỊ TRÍ CÁC Ô NHỚ ĐẶC BIỆT TRONG STACK FRAME =====
/**
 * Cấu trúc stack frame (base pointer trỏ đến return value):
 * 
 * [base+0] Return Value   ← Frame Base (BP) trỏ vào đây
 * [base+1] Dynamic Link   (con trỏ đến frame của caller)
 * [base+2] Return Address (địa chỉ lệnh quay về)
 * [base+3] Static Link    (con trỏ đến frame của scope cha)
 * ├─────────────────────
 * [base+4] Tham số 1 / Biến cục bộ 1
 * [base+5] Tham số 2 / Biến cục bộ 2
 * [base+6] Tham số 3 / Biến cục bộ 3
 * ...
 * 
 * Các offset dưới đây là vị trí TƯƠNG ĐỐI từ base:
 * - RETURN_VALUE_OFFSET = 0: tại base+0
 * - DYNAMIC_LINK_OFFSET = 1: tại base+1  
 * - RETURN_ADDRESS_OFFSET = 2: tại base+2
 * - STATIC_LINK_OFFSET = 3: tại base+3
 */
#define RETURN_VALUE_OFFSET 0   // Vị trí: base+0 (return value)
#define DYNAMIC_LINK_OFFSET 1   // Vị trí: base+1 (dynamic link)
#define RETURN_ADDRESS_OFFSET 2 // Vị trí: base+2 (return address)
#define STATIC_LINK_OFFSET 3    // Vị trí: base+3 (static link)

// ===== HÀM TÍNH TOÁN NESTED LEVEL =====

/**
 * @brief Tính mức lồng nhau giữa scope hiện tại và scope đích
 * 
 * Nested level = số lần phải nhảy qua static link để đến scope đích.
 * Dùng để sinh lệnh LA, LV, CALL với tham số level đúng.
 * 
 * Ví dụ: 
 * - Truy cập biến cùng scope: level = 0
 * - Truy cập biến scope cha: level = 1
 * - Truy cập biến scope ông: level = 2
 * 
 * @param scope Scope đích cần truy cập
 * @return Số mức lồng nhau (0, 1, 2, ...)
 */
int computeNestedLevel(Scope* scope);

// ===== HÀM SINH MÃ CHO BIẾN =====

/**
 * @brief Sinh mã lấy địa chỉ của biến
 * Sinh lệnh LA (Load Address) với level và offset phù hợp
 * @param var Con trỏ đến đối tượng biến
 */
void genVariableAddress(Object* var);

/**
 * @brief Sinh mã lấy giá trị của biến
 * Sinh lệnh LV (Load Value) với level và offset phù hợp
 * @param var Con trỏ đến đối tượng biến
 */
void genVariableValue(Object* var);

// ===== HÀM SINH MÃ CHO THAM SỐ =====

/**
 * @brief Sinh mã lấy địa chỉ của tham số
 * Dùng cho tham biến (PARAM_REFERENCE)
 * @param param Con trỏ đến đối tượng tham số
 */
void genParameterAddress(Object* param);

/**
 * @brief Sinh mã lấy giá trị của tham số
 * Dùng cho tham trị (PARAM_VALUE)
 * @param param Con trỏ đến đối tượng tham số
 */
void genParameterValue(Object* param);

// ===== HÀM SINH MÃ CHO GIÁ TRỊ TRẢ VỀ CỦA HÀM =====

/**
 * @brief Sinh mã lấy địa chỉ vị trí lưu giá trị trả về
 * Dùng khi gán giá trị trả về: func_name := value
 * @param func Con trỏ đến đối tượng hàm
 */
void genReturnValueAddress(Object* func);

/**
 * @brief Sinh mã lấy giá trị trả về của hàm
 * Dùng khi đọc giá trị trả về trong biểu thức
 * @param func Con trỏ đến đối tượng hàm
 */
void genReturnValueValue(Object* func);

// ===== HÀM SINH MÃ CHO PHẦN TỬ MẢNG =====

/**
 * @brief Sinh mã lấy địa chỉ phần tử mảng
 * Tính địa chỉ: base_address + index * element_size
 * @param arrayType Kiểu mảng
 */
void genArrayElementAddress(Type* arrayType);

/**
 * @brief Sinh mã lấy giá trị phần tử mảng
 * @param arrayType Kiểu mảng
 */
void genArrayElementValue(Type* arrayType);

// ===== HÀM SINH MÃ GỌI THỦ TỤC/HÀM =====

/**
 * @brief Sinh mã gọi thủ tục có sẵn (predefined)
 * Các thủ tục: WRITEI, WRITEC, WRITELN
 * Sinh lệnh WRI, WRC, WLN tương ứng
 * @param proc Con trỏ đến đối tượng thủ tục có sẵn
 */
void genPredefinedProcedureCall(Object* proc);

/**
 * @brief Sinh mã gọi thủ tục do người dùng định nghĩa
 * Sinh lệnh CALL với level và địa chỉ code
 * @param proc Con trỏ đến đối tượng thủ tục
 */
void genProcedureCall(Object* proc);

/**
 * @brief Sinh mã gọi hàm có sẵn (predefined)
 * Các hàm: READI, READC
 * Sinh lệnh RI, RC tương ứng
 * @param func Con trỏ đến đối tượng hàm có sẵn
 */
void genPredefinedFunctionCall(Object* func);

/**
 * @brief Sinh mã gọi hàm do người dùng định nghĩa
 * Sinh lệnh CALL với level và địa chỉ code
 * @param func Con trỏ đến đối tượng hàm
 */
void genFunctionCall(Object* func);

// ===== HÀM SINH CÁC LỆNH MÁY ẢO =====
// Các hàm này là wrapper cho các hàm emit trong instructions.c

// --- Lệnh load/store ---
void genLA(int level, int offset);    // Load Address: đẩy địa chỉ lên stack
void genLV(int level, int offset);    // Load Value: đẩy giá trị lên stack
void genLC(WORD constant);            // Load Constant: đẩy hằng số lên stack
void genLI(void);                     // Load Indirect: lấy giá trị từ địa chỉ trên stack
void genINT(int delta);               // INcrement Top: cấp phát delta ô nhớ
void genDCT(int delta);               // DeCrement Top: giải phóng delta ô nhớ
Instruction* genJ(CodeAddress label); // Jump: nhảy vô điều kiện đến label
Instruction* genFJ(CodeAddress label);// False Jump: nhảy nếu TOS = false
void genHL(void);                     // HaLt: dừng chương trình
void genST(void);                     // STore: lưu giá trị vào địa chỉ
void genCALL(int level, CodeAddress label); // CALL: gọi hàm/thủ tục
void genEP(void);                     // Enter Program: vào chương trình chính
void genEF(void);                     // Enter Function: vào hàm

// --- Lệnh I/O ---
void genRC(void);                     // Read Char: đọc 1 ký tự
void genRI(void);                     // Read Integer: đọc 1 số nguyên
void genWRC(void);                    // Write Char: in 1 ký tự
void genWRI(void);                    // Write Integer: in 1 số nguyên
void genWLN(void);                    // Write Line: in xuống dòng

// --- Lệnh số học ---
void genAD(void);                     // ADd: cộng 2 số trên stack
void genSB(void);                     // SuBtract: trừ 2 số trên stack
void genML(void);                     // MuLtiply: nhân 2 số trên stack
void genDV(void);                     // DiVide: chia 2 số trên stack
void genNEG(void);                    // NEGate: đổi dấu số trên stack

// --- Lệnh chuyển đổi và so sánh ---
void genCV(void);                     // Character to Value: chuyển CHAR sang INT
void genEQ(void);                     // EQual: so sánh bằng
void genNE(void);                     // Not Equal: so sánh khác
void genGT(void);                     // Greater Than: so sánh lớn hơn
void genGE(void);                     // Greater or Equal: so sánh lớn hơn hoặc bằng
void genLT(void);                     // Less Than: so sánh nhỏ hơn
void genLE(void);                     // Less or Equal: so sánh nhỏ hơn hoặc bằng

// ===== HÀM HỖ TRỢ SINH MÃ =====

/**
 * @brief Cập nhật đích nhảy cho lệnh J
 * Dùng cho forward jump (nhảy về phía trước chưa biết địa chỉ)
 * @param jmp Con trỏ đến lệnh J cần cập nhật
 * @param label Địa chỉ đích mới
 */
void updateJ(Instruction* jmp, CodeAddress label);

/**
 * @brief Cập nhật đích nhảy cho lệnh FJ
 * Dùng cho forward jump có điều kiện
 * @param jmp Con trỏ đến lệnh FJ cần cập nhật
 * @param label Địa chỉ đích mới
 */
void updateFJ(Instruction* jmp, CodeAddress label);

/**
 * @brief Lấy địa chỉ lệnh hiện tại (để đặt nhãn)
 * @return Địa chỉ lệnh tiếp theo sẽ được sinh ra
 */
CodeAddress getCurrentCodeAddress(void);

/**
 * @brief Kiểm tra có phải thủ tục có sẵn không
 * @param proc Đối tượng thủ tục cần kiểm tra
 * @return 1 nếu là WRITEI/WRITEC/WRITELN, 0 nếu không
 */
int isPredefinedProcedure(Object* proc);

/**
 * @brief Kiểm tra có phải hàm có sẵn không
 * @param func Đối tượng hàm cần kiểm tra
 * @return 1 nếu là READI/READC, 0 nếu không
 */
int isPredefinedFunction(Object* func);

// ===== HÀM QUẢN LÝ CODE BUFFER =====

/**
 * @brief Khởi tạo bộ đệm chứa mã (code buffer)
 * Cấp phát bộ nhớ để lưu các lệnh máy ảo sinh ra
 */
void initCodeBuffer(void);

/**
 * @brief In toàn bộ mã đã sinh ra ra console
 * Dùng để debug, xem mã máy ảo
 */
void printCodeBuffer(void);

/**
 * @brief Giải phóng bộ nhớ code buffer
 * Gọi khi kết thúc biên dịch
 */
void cleanCodeBuffer(void);

/**
 * @brief Lưu mã máy ảo ra file nhị phân
 * File này có thể chạy bằng interpreter
 * @param fileName Tên file output (.o)
 * @return IO_SUCCESS nếu thành công, IO_ERROR nếu lỗi
 */
int serialize(char* fileName);

#endif
