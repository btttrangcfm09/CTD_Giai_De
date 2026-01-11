/* Semantics (Ngữ nghĩa)
 * @copyright (c) 2008, Hedspi, Hanoi University of Technology
 * @author Huu-Duc Nguyen
 * @version 1.0
 * 
 * Mô tả:
 *   Module này thực hiện kiểm tra ngữ nghĩa cho ngôn ngữ KPL.
 *   Bao gồm:
 *   - Kiểm tra định danh đã khai báo
 *   - Kiểm tra tên chưa bị trùng
 *   - Kiểm tra kiểu dữ liệu khớp nhau
 *   - Tìm kiếm đối tượng trong các scope
 */

#include <stdlib.h>     // Thư viện chuẩn (malloc, free...)
#include <string.h>     // Xử lý chuỗi (strcmp, strcpy...)
#include "debug.h"      // Module debug
#include "semantics.h"  // Định nghĩa các hàm kiểm tra ngữ nghĩa
#include "error.h"      // Xử lý lỗi

// ===== BIẾN EXTERN =====
extern SymTab* symtab;          // Bảng ký hiệu toàn cục (từ symtab.c)
extern Token* currentToken;     // Token hiện tại (từ parser.c)

/**
 * @brief Tìm kiếm đối tượng theo tên trong bảng ký hiệu
 * 
 * Thuật toán tìm kiếm:
 * 1. Bắt đầu từ scope hiện tại
 * 2. Tìm trong danh sách đối tượng của scope đó
 * 3. Nếu không thấy, di chuyển ra scope bên ngoài (outer)
 * 4. Lặp lại cho đến khi hết scope
 * 5. Cuối cùng tìm trong danh sách đối tượng toàn cục
 * 
 * Quy tắc: Scope trong cùng có độ ưu tiên cao hơn (shadowing)
 * 
 * @param name Tên định danh cần tìm
 * @return Con trỏ đến Object nếu tìm thấy, NULL nếu không tìm thấy
 */
Object* lookupObject(char *name) {
  Scope* scope = symtab->currentScope;  // Bắt đầu từ scope hiện tại
  Object* obj;                          // Đối tượng kết quả

  // Bước 1-4: Tìm kiếm từ scope trong ra scope ngoài
  while (scope != NULL) {
    obj = findObject(scope->objList, name);  // Tìm trong danh sách đối tượng của scope
    if (obj != NULL) return obj;             // Nếu tìm thấy, trả về ngay
    scope = scope->outer;                    // Di chuyển ra scope ngoài
  }
  
  // Bước 5: Tìm trong danh sách toàn cục (các hàm/thủ tục có sẵn)
  obj = findObject(symtab->globalObjectList, name);
  if (obj != NULL) return obj;  // Tìm thấy trong toàn cục
  
  return NULL;  // Không tìm thấy ở đâu cả
}

/**
 * @brief Kiểm tra tên định danh chưa được sử dụng trong scope hiện tại
 * 
 * Hàm này đảm bảo không khai báo trùng tên trong cùng một scope.
 * Ví dụ: Không được khai báo 2 biến cùng tên "x" trong cùng một block.
 * 
 * Lưu ý: Chỉ kiểm tra trong scope hiện tại, không kiểm tra scope ngoài.
 * (Cho phép shadowing - che khuất biến ở scope ngoài)
 * 
 * @param name Tên định danh cần kiểm tra
 * @throws ERR_DUPLICATE_IDENT nếu tên đã tồn tại trong scope hiện tại
 */
void checkFreshIdent(char *name) {
  // Tìm kiếm CHỈ trong scope hiện tại (không tìm scope ngoài)
  if (findObject(symtab->currentScope->objList, name) != NULL)
    // Tên đã tồn tại -> báo lỗi trùng định danh
    error(ERR_DUPLICATE_IDENT, currentToken->lineNo, currentToken->colNo);
  // Nếu không tìm thấy -> OK, tên chưa dùng
}

/**
 * @brief Kiểm tra định danh đã được khai báo
 * 
 * Tìm kiếm định danh trong tất cả các scope (từ trong ra ngoài).
 * Không quan tâm loại đối tượng (có thể là biến, hằng, hàm, thủ tục...).
 * 
 * @param name Tên định danh cần kiểm tra
 * @return Con trỏ đến Object nếu tìm thấy
 * @throws ERR_UNDECLARED_IDENT nếu không tìm thấy
 */
Object* checkDeclaredIdent(char* name) {
  Object* obj = lookupObject(name);  // Tìm kiếm trong tất cả scope
  if (obj == NULL) {
    // Không tìm thấy -> báo lỗi chưa khai báo
    error(ERR_UNDECLARED_IDENT,currentToken->lineNo, currentToken->colNo);
  }
  return obj;  // Trả về đối tượng tìm được
}

/**
 * @brief Kiểm tra hằng số đã được khai báo
 * 
 * Không chỉ kiểm tra tồn tại, mà còn kiểm tra đúng loại OBJ_CONSTANT.
 * Ví dụ: Nếu "x" là biến thì không thể dùng như hằng số.
 * 
 * @param name Tên hằng số cần kiểm tra
 * @return Con trỏ đến Object (loại OBJ_CONSTANT)
 * @throws ERR_UNDECLARED_CONSTANT nếu không tìm thấy
 * @throws ERR_INVALID_CONSTANT nếu tìm thấy nhưng không phải hằng số
 */
Object* checkDeclaredConstant(char* name) {
  Object* obj = lookupObject(name);  // Tìm kiếm đối tượng
  
  if (obj == NULL)
    // Không tìm thấy -> báo lỗi chưa khai báo hằng
    error(ERR_UNDECLARED_CONSTANT,currentToken->lineNo, currentToken->colNo);
  
  if (obj->kind != OBJ_CONSTANT)
    // Tìm thấy nhưng không phải hằng số (có thể là biến, hàm...) -> báo lỗi
    error(ERR_INVALID_CONSTANT,currentToken->lineNo, currentToken->colNo);

  return obj;  // Trả về đối tượng hằng số  // Trả về đối tượng hằng số
}

/**
 * @brief Kiểm tra kiểu dữ liệu đã được khai báo
 * 
 * Kiểm tra tên kiểu do người dùng định nghĩa (user-defined type).
 * Ví dụ: TYPE MyArray = ARRAY [10] OF INTEGER;
 * 
 * @param name Tên kiểu cần kiểm tra
 * @return Con trỏ đến Object (loại OBJ_TYPE)
 * @throws ERR_UNDECLARED_TYPE nếu không tìm thấy
 * @throws ERR_INVALID_TYPE nếu tìm thấy nhưng không phải kiểu
 */
Object* checkDeclaredType(char* name) {
  Object* obj = lookupObject(name);  // Tìm kiếm đối tượng
  
  if (obj == NULL)
    // Không tìm thấy -> báo lỗi chưa khai báo kiểu
    error(ERR_UNDECLARED_TYPE,currentToken->lineNo, currentToken->colNo);
  
  if (obj->kind != OBJ_TYPE)
    // Tìm thấy nhưng không phải định nghĩa kiểu -> báo lỗi
    error(ERR_INVALID_TYPE,currentToken->lineNo, currentToken->colNo);

  return obj;  // Trả về đối tượng kiểu  
}

/**
 * @brief Kiểm tra biến đã được khai báo
 * 
 * Kiểm tra đối tượng là biến (OBJ_VARIABLE).
 * 
 * @param name Tên biến cần kiểm tra
 * @return Con trỏ đến Object (loại OBJ_VARIABLE)
 * @throws ERR_UNDECLARED_VARIABLE nếu không tìm thấy
 * @throws ERR_INVALID_VARIABLE nếu tìm thấy nhưng không phải biến
 */
Object* checkDeclaredVariable(char* name) {
  Object* obj = lookupObject(name);  // Tìm kiếm đối tượng
  
  if (obj == NULL)
    // Không tìm thấy -> báo lỗi chưa khai báo biến
    error(ERR_UNDECLARED_VARIABLE,currentToken->lineNo, currentToken->colNo);
  
  if (obj->kind != OBJ_VARIABLE)
    // Tìm thấy nhưng không phải biến -> báo lỗi
    error(ERR_INVALID_VARIABLE,currentToken->lineNo, currentToken->colNo);

  return obj;  // Trả về đối tượng biến  
}

/**
 * @brief Kiểm tra hàm đã được khai báo
 * 
 * Kiểm tra đối tượng là hàm (OBJ_FUNCTION).
 * Hàm khác thủ tục ở chỗ có giá trị trả về.
 * 
 * @param name Tên hàm cần kiểm tra
 * @return Con trỏ đến Object (loại OBJ_FUNCTION)
 * @throws ERR_UNDECLARED_FUNCTION nếu không tìm thấy
 * @throws ERR_INVALID_FUNCTION nếu tìm thấy nhưng không phải hàm
 */
Object* checkDeclaredFunction(char* name) {
  Object* obj = lookupObject(name);  // Tìm kiếm đối tượng
  
  if (obj == NULL)
    // Không tìm thấy -> báo lỗi chưa khai báo hàm
    error(ERR_UNDECLARED_FUNCTION,currentToken->lineNo, currentToken->colNo);
  
  if (obj->kind != OBJ_FUNCTION)
    // Tìm thấy nhưng không phải hàm -> báo lỗi
    error(ERR_INVALID_FUNCTION,currentToken->lineNo, currentToken->colNo);

  return obj;  // Trả về đối tượng hàm 
}

/**
 * @brief Kiểm tra thủ tục đã được khai báo
 * 
 * Kiểm tra đối tượng là thủ tục (OBJ_PROCEDURE).
 * Thủ tục khác hàm ở chỗ không có giá trị trả về.
 * 
 * @param name Tên thủ tục cần kiểm tra
 * @return Con trỏ đến Object (loại OBJ_PROCEDURE)
 * @throws ERR_UNDECLARED_PROCEDURE nếu không tìm thấy
 * @throws ERR_INVALID_PROCEDURE nếu tìm thấy nhưng không phải thủ tục
 */
Object* checkDeclaredProcedure(char* name) {
  Object* obj = lookupObject(name);  // Tìm kiếm đối tượng
  
  if (obj == NULL) 
    // Không tìm thấy -> báo lỗi chưa khai báo thủ tục
    error(ERR_UNDECLARED_PROCEDURE,currentToken->lineNo, currentToken->colNo);
  
  if (obj->kind != OBJ_PROCEDURE)
    // Tìm thấy nhưng không phải thủ tục -> báo lỗi
    error(ERR_INVALID_PROCEDURE,currentToken->lineNo, currentToken->colNo);

  return obj;  // Trả về đối tượng thủ tục  
}

/**
 * @brief Kiểm tra định danh có thể làm L-value (giá trị bên trái)
 * 
 * L-value là giá trị có thể được gán (xuất hiện bên trái dấu :=).
 * Có 3 loại L-value hợp lệ:
 * 1. Biến (OBJ_VARIABLE)
 * 2. Tham số (OBJ_PARAMETER)
 * 3. Tên hàm (OBJ_FUNCTION) - CHỈ trong thân hàm đó (để gán giá trị trả về)
 * 
 * Quy tắc đặc biệt cho hàm:
 * - Chỉ được gán giá trị cho tên hàm bên trong thân hàm đó
 * - Ví dụ: FUNCTION sum(a, b): INTEGER;
 *          BEGIN sum := a + b; END; <- OK
 *          Nhưng không được gán ở hàm khác
 * 
 * @param name Tên định danh cần kiểm tra
 * @return Con trỏ đến Object hợp lệ
 * @throws ERR_UNDECLARED_IDENT nếu không tìm thấy
 * @throws ERR_INVALID_IDENT nếu không phải L-value hợp lệ
 */
Object* checkDeclaredLValueIdent(char* name) {
  Object* obj = lookupObject(name);  // Tìm kiếm đối tượng
  Scope* scope;  // Dùng để kiểm tra scope của hàm

  if (obj == NULL)
    // Không tìm thấy -> báo lỗi chưa khai báo
    error(ERR_UNDECLARED_IDENT,currentToken->lineNo, currentToken->colNo);

  // Kiểm tra loại đối tượng
  switch (obj->kind) {
  case OBJ_VARIABLE:   // Biến -> OK, luôn là L-value
  case OBJ_PARAMETER:  // Tham số -> OK, luôn là L-value
    break;  // Không cần kiểm tra thêm
    
  case OBJ_FUNCTION:  // Tên hàm -> cần kiểm tra đặc biệt
    // Kiểm tra xem có đang ở trong thân hàm này không
    scope = symtab->currentScope;  // Bắt đầu từ scope hiện tại
    
    // Duyệt từ scope hiện tại ra ngoài để tìm scope của hàm
    while ((scope != NULL) && (scope != obj->funcAttrs->scope)) 
      scope = scope->outer;

    if (scope == NULL)
      // Không tìm thấy scope của hàm -> đang ở ngoài hàm -> không được gán
      error(ERR_INVALID_IDENT,currentToken->lineNo, currentToken->colNo);
    // Nếu scope != NULL -> đang ở trong hàm -> OK
    break;
    
  default:  // Các loại khác (hằng, kiểu, thủ tục...) -> không phải L-value
    error(ERR_INVALID_IDENT,currentToken->lineNo, currentToken->colNo);
  }

  return obj;  // Trả về đối tượng L-value hợp lệ 
}

// ===== CÁC HÀM KIỂM TRA KIỂU DỮ LIỆU =====

/**
 * @brief Kiểm tra kiểu dữ liệu là INTEGER
 * 
 * Sử dụng khi yêu cầu kiểu phải là số nguyên.
 * Ví dụ: chỉ số mảng, toán tử số học (+, -, *, /), điều kiện FOR...
 * 
 * @param type Con trỏ đến Type cần kiểm tra
 * @throws ERR_TYPE_INCONSISTENCY nếu không phải INTEGER
 */
void checkIntType(Type* type) {
  // Kiểm tra type không NULL và có typeClass là TP_INT
  if ((type != NULL) && (type->typeClass == TP_INT))
    return;  // Đúng kiểu INTEGER -> OK
  else 
    // Sai kiểu -> báo lỗi không khớp kiểu
    error(ERR_TYPE_INCONSISTENCY, currentToken->lineNo, currentToken->colNo);
}

/**
 * @brief Kiểm tra kiểu dữ liệu là CHAR
 * 
 * Sử dụng khi yêu cầu kiểu phải là ký tự.
 * Ví dụ: gán giá trị ký tự, so sánh ký tự...
 * 
 * @param type Con trỏ đến Type cần kiểm tra
 * @throws ERR_TYPE_INCONSISTENCY nếu không phải CHAR
 */
void checkCharType(Type* type) {
  // Kiểm tra type không NULL và có typeClass là TP_CHAR
  if ((type != NULL) && (type->typeClass == TP_CHAR))
    return;  // Đúng kiểu CHAR -> OK
  else 
    // Sai kiểu -> báo lỗi không khớp kiểu
    error(ERR_TYPE_INCONSISTENCY, currentToken->lineNo, currentToken->colNo);
}

/**
 * @brief Kiểm tra kiểu dữ liệu cơ bản (INTEGER hoặc CHAR)
 * 
 * Sử dụng khi chấp nhận cả INTEGER và CHAR, nhưng không chấp nhận ARRAY.
 * Ví dụ: 
 * - Phần tử mảng phải có kiểu cơ bản (không được là mảng lồng vô hạn)
 * - Kiểu tham số của hàm/thủ tục phải là kiểu cơ bản
 * - Kiểu trong so sánh phải là kiểu cơ bản
 * 
 * @param type Con trỏ đến Type cần kiểm tra
 * @throws ERR_TYPE_INCONSISTENCY nếu không phải INTEGER hay CHAR
 */
void checkBasicType(Type* type) {
  // Kiểm tra type không NULL và là TP_INT hoặc TP_CHAR
  if ((type != NULL) && ((type->typeClass == TP_INT) || (type->typeClass == TP_CHAR)))
    return;  // Đúng kiểu cơ bản -> OK
  else 
    // Sai kiểu (có thể là ARRAY hoặc NULL) -> báo lỗi
    error(ERR_TYPE_INCONSISTENCY, currentToken->lineNo, currentToken->colNo);
}

/**
 * @brief Kiểm tra kiểu dữ liệu là ARRAY
 * 
 * Sử dụng khi cần đảm bảo đối tượng là mảng.
 * Ví dụ: khi xử lý chỉ số mảng arr[i], cần kiểm tra arr có phải mảng không.
 * 
 * @param type Con trỏ đến Type cần kiểm tra
 * @throws ERR_TYPE_INCONSISTENCY nếu không phải ARRAY
 */
void checkArrayType(Type* type) {
  // Kiểm tra type không NULL và có typeClass là TP_ARRAY
  if ((type != NULL) && (type->typeClass == TP_ARRAY))
    return;  // Đúng kiểu ARRAY -> OK
  else 
    // Sai kiểu (INTEGER, CHAR hoặc NULL) -> báo lỗi
    error(ERR_TYPE_INCONSISTENCY, currentToken->lineNo, currentToken->colNo);
}

/**
 * @brief Kiểm tra 2 kiểu dữ liệu bằng nhau
 * 
 * So sánh 2 kiểu để đảm bảo khớp nhau.
 * Sử dụng trong:
 * - Gán giá trị: kiểu biến = kiểu biểu thức
 * - Truyền đối số: kiểu đối số = kiểu tham số
 * - So sánh: kiểu vế trái = kiểu vế phải
 * 
 * Hàm compareType() (từ symtab.c) so sánh đệ quy:
 * - Kiểu cơ bản: so sánh typeClass
 * - Mảng: so sánh arraySize và elementType
 * 
 * @param type1 Kiểu thứ nhất
 * @param type2 Kiểu thứ hai
 * @throws ERR_TYPE_INCONSISTENCY nếu 2 kiểu không khớp
 */
void checkTypeEquality(Type* type1, Type* type2) {
  // compareType() trả về 1 nếu bằng nhau, 0 nếu khác nhau
  if (compareType(type1, type2) == 0)
    // Hai kiểu khác nhau -> báo lỗi không khớp kiểu
    error(ERR_TYPE_INCONSISTENCY, currentToken->lineNo, currentToken->colNo);
  // Nếu compareType() trả về 1 -> OK, 2 kiểu bằng nhau
}


