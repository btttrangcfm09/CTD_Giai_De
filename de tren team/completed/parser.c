/* Parser (Bộ phân tích cú pháp)
 * @copyright (c) 2008, Hedspi, Hanoi University of Technology
 * @author Huu-Duc Nguyen
 * @version 1.0
 *
 * Mô tả:
 *   Module này thực hiện phân tích cú pháp cho ngôn ngữ KPL.
 *   Parser kiểm tra xem chuỗi token có tuân theo ngữ pháp của ngôn ngữ hay không.
 *   Đồng thời xây dựng bảng ký hiệu và sinh mã máy ảo.
 *
 * Phương pháp: Recursive Descent Parsing (Phân tích đệ quy xuống)
 *   - Mỗi quy tắc ngữ pháp được biểu diễn bằng một hàm
 *   - Các hàm gọi lẫn nhau theo cấu trúc ngữ pháp
 */
#include <stdio.h>
#include <stdlib.h>

#include "reader.h"    // Module đọc file nguồn
#include "scanner.h"   // Module phân tích từ vựng
#include "parser.h"    // Định nghĩa các hàm parser
#include "semantics.h" // Kiểm tra ngữ nghĩa (kiểu dữ liệu, phạm vi biến...)
#include "error.h"     // Xử lý lỗi
#include "debug.h"     // Debug và in thông tin
#include "codegen.h"   // Sinh mã máy ảo

// ===== BIẾN TOÀN CỤC =====

/**
 * currentToken: Token hiện tại đang được xử lý
 * Parser sử dụng token này để lấy thông tin (giá trị, vị trí...)
 */
Token *currentToken;

/**
 * lookAhead: Token tiếp theo (nhìn trước 1 token)
 * Dùng để quyết định hướng phân tích cú pháp
 * Kỹ thuật: LL(1) - Left-to-right, Leftmost derivation, 1 token lookahead
 */
Token *lookAhead;

// Các biến extern (được định nghĩa trong module khác)
extern Type *intType;  // Kiểu integer được định nghĩa sẵn
extern Type *charType; // Kiểu char được định nghĩa sẵn
extern SymTab *symtab; // Bảng ký hiệu toàn cục

// ===== CÁC HÀM CƠ BẢN =====

/**
 * @brief Di chuyển sang token tiếp theo
 *
 * Hàm này thực hiện:
 * 1. Giải phóng bộ nhớ của currentToken cũ
 * 2. Di chuyển lookAhead thành currentToken mới
 * 3. Đọc token mới vào lookAhead
 *
 * Đây là hàm cơ bản nhất trong parser, được gọi sau mỗi lần "ăn" một token
 */
void scan(void)
{
  Token *tmp = currentToken;   // Lưu tạm currentToken để giải phóng sau
  currentToken = lookAhead;    // Di chuyển lookAhead thành currentToken
  lookAhead = getValidToken(); // Đọc token hợp lệ tiếp theo vào lookAhead
  free(tmp);                   // Giải phóng bộ nhớ token cũ
}

/**
 * @brief "Ăn" một token với loại mong đợi
 *
 * Hàm này kiểm tra xem lookAhead có đúng loại token mong đợi không.
 * - Nếu đúng: gọi scan() để di chuyển sang token tiếp theo
 * - Nếu sai: báo lỗi thiếu token
 *
 * @param tokenType Loại token mong đợi (ví dụ: KW_BEGIN, SB_SEMICOLON...)
 */
void eat(TokenType tokenType)
{
  if (lookAhead->tokenType == tokenType)
  {
    // Token đúng như mong đợi
    //    printToken(lookAhead);  // Có thể in token để debug (đang bị comment)
    scan(); // Di chuyển sang token tiếp theo
  }
  else
  {
    // Token không đúng -> báo lỗi thiếu token
    missingToken(tokenType, lookAhead->lineNo, lookAhead->colNo);
  }
}

// ===== CÁC HÀM BIÊN DỊCH CHƯƠNG TRÌNH =====

/**
 * @brief Biên dịch toàn bộ chương trình KPL
 *
 * Cấu trúc: PROGRAM <tên>; <block> .
 */
void compileProgram(void)
{
  Object *program; // Đối tượng chương trình

  eat(KW_PROGRAM); // Ăn từ khóa "PROGRAM"
  eat(TK_IDENT);   // Ăn tên chương trình

  // Tạo đối tượng chương trình và lưu địa chỉ code
  program = createProgramObject(currentToken->string);
  program->progAttrs->codeAddress = getCurrentCodeAddress();
  enterBlock(program->progAttrs->scope); // Vào scope chương trình

  eat(SB_SEMICOLON); // Ăn dấu ';'

  compileBlock(); // Biên dịch block chính
  eat(SB_PERIOD); // Ăn dấu '.' kết thúc

  genHL(); // Sinh lệnh HALT - dừng chương trình

  exitBlock(); // Thoát scope
}

/**
 * @brief Biên dịch các khai báo hằng số
 * Cú pháp: CONST <ident> = <hằng>; ...
 */
void compileConstDecls(void)
{
  Object *constObj;          // Đối tượng hằng số
  ConstantValue *constValue; // Giá trị hằng

  if (lookAhead->tokenType == KW_CONST)
  {
    eat(KW_CONST);
    do
    {
      eat(TK_IDENT);
      checkFreshIdent(currentToken->string); // Kiểm tra tên chưa dùng
      constObj = createConstantObject(currentToken->string);
      declareObject(constObj); // Thêm vào bảng ký hiệu

      eat(SB_EQ);
      constValue = compileConstant();
      constObj->constAttrs->value = constValue;

      eat(SB_SEMICOLON);
    } while (lookAhead->tokenType == TK_IDENT);
  }
}

/**
 * @brief Biên dịch các khai báo kiểu
 * Cú pháp: TYPE <ident> = <type>; ...
 */
void compileTypeDecls(void)
{
  Object *typeObj;  // Đối tượng kiểu
  Type *actualType; // Kiểu thực tế

  if (lookAhead->tokenType == KW_TYPE)
  {
    eat(KW_TYPE);
    do
    {
      eat(TK_IDENT);

      checkFreshIdent(currentToken->string); // Kiểm tra tên chưa dùng
      typeObj = createTypeObject(currentToken->string);
      declareObject(typeObj);

      eat(SB_EQ);
      actualType = compileType(); // Biên dịch định nghĩa kiểu
      typeObj->typeAttrs->actualType = actualType;

      eat(SB_SEMICOLON);
    } while (lookAhead->tokenType == TK_IDENT);
  }
}

/**
 * @brief Biên dịch các khai báo biến
 * Cú pháp: VAR <ident>: <type>; ...
 */
void compileVarDecls(void)
{
  Object *varObj; // Đối tượng biến
  Type *varType;  // Kiểu của biến

  if (lookAhead->tokenType == KW_VAR)
  {
    eat(KW_VAR);
    do
    {
      eat(TK_IDENT);
      checkFreshIdent(currentToken->string);
      varObj = createVariableObject(currentToken->string);
      eat(SB_COLON);
      varType = compileType(); // Biên dịch kiểu biến
      varObj->varAttrs->type = varType;
      declareObject(varObj);
      eat(SB_SEMICOLON);
    } while (lookAhead->tokenType == TK_IDENT);
  }
}

/**
 * @brief Biên dịch một block (khối lệnh)
 *
 * Block bao gồm:
 * - Các khai báo: CONST, TYPE, VAR, hàm/thủ tục
 * - Phần thân: BEGIN ... END
 *
 * Sinh mã:
 * - Lệnh J để nhảy qua phần khai báo hàm/thủ tục
 * - Lệnh INT để cấp phát bộ nhớ cho biến local
 */
void compileBlock(void)
{
  Instruction *jmp; // Lệnh jump để nhảy qua khai báo sub-program

  jmp = genJ(DC_VALUE); // Sinh lệnh J với địa chỉ tạm (sẽ cập nhật sau)

  compileConstDecls(); // Biên dịch khai báo hằng
  compileTypeDecls();  // Biên dịch khai báo kiểu
  compileVarDecls();   // Biên dịch khai báo biến
  compileSubDecls();   // Biên dịch khai báo hàm/thủ tục

  updateJ(jmp, getCurrentCodeAddress());   // Cập nhật địa chỉ nhảy (sau khai báo)
  genINT(symtab->currentScope->frameSize); // Cấp phát stack frame

  eat(KW_BEGIN);
  compileStatements(); // Biên dịch các câu lệnh
  eat(KW_END);
}

/**
 * @brief Biên dịch các khai báo hàm và thủ tục
 */
void compileSubDecls(void)
{
  while ((lookAhead->tokenType == KW_FUNCTION) || (lookAhead->tokenType == KW_PROCEDURE))
  {
    if (lookAhead->tokenType == KW_FUNCTION)
      compileFuncDecl(); // Biên dịch khai báo hàm
    else
      compileProcDecl(); // Biên dịch khai báo thủ tục
  }
}

/**
 * @brief Biên dịch khai báo hàm
 * Cú pháp: FUNCTION <tên>(<params>): <kiểu_trả_về>; <block>;
 */
void compileFuncDecl(void)
{
  Object *funcObj;  // Đối tượng hàm
  Type *returnType; // Kiểu trả về

  eat(KW_FUNCTION);
  eat(TK_IDENT);

  checkFreshIdent(currentToken->string);
  funcObj = createFunctionObject(currentToken->string);
  funcObj->funcAttrs->codeAddress = getCurrentCodeAddress(); // Lưu địa chỉ code hàm
  declareObject(funcObj);

  enterBlock(funcObj->funcAttrs->scope); // Vào scope của hàm

  compileParams(); // Biên dịch danh sách tham số

  eat(SB_COLON);
  returnType = compileBasicType(); // Biên dịch kiểu trả về
  funcObj->funcAttrs->returnType = returnType;

  eat(SB_SEMICOLON);

  compileBlock(); // Biên dịch thân hàm

  genEF(); // Sinh lệnh Exit Function
  eat(SB_SEMICOLON);

  exitBlock(); // Thoát scope hàm
}

/**
 * @brief Biên dịch khai báo thủ tục
 * Cú pháp: PROCEDURE <tên>(<params>); <block>;
 */
void compileProcDecl(void)
{
  Object *procObj; // Đối tượng thủ tục

  eat(KW_PROCEDURE);
  eat(TK_IDENT);

  checkFreshIdent(currentToken->string);
  procObj = createProcedureObject(currentToken->string);
  procObj->procAttrs->codeAddress = getCurrentCodeAddress(); // Lưu địa chỉ code
  declareObject(procObj);

  enterBlock(procObj->procAttrs->scope); // Vào scope thủ tục

  compileParams(); // Biên dịch tham số

  eat(SB_SEMICOLON);
  compileBlock(); // Biên dịch thân thủ tục

  genEP(); // Sinh lệnh Exit Procedure
  eat(SB_SEMICOLON);

  exitBlock(); // Thoát scope
}

/**
 * @brief Biên dịch hằng số không dấu
 * Có thể là: số nguyên, tên hằng đã khai báo, hoặc ký tự
 * @return Giá trị hằng số
 */
ConstantValue *compileUnsignedConstant(void)
{
  ConstantValue *constValue; // Giá trị hằng
  Object *obj;               // Đối tượng (nếu là tên hằng)

  switch (lookAhead->tokenType)
  {
  case TK_NUMBER: // Trường hợp là số nguyên trực tiếp
    eat(TK_NUMBER);
    constValue = makeIntConstant(currentToken->value); // Tạo hằng integer
    break;

  case TK_IDENT: // Trường hợp là tên hằng đã khai báo
    eat(TK_IDENT);

    obj = checkDeclaredConstant(currentToken->string);           // Kiểm tra đã khai báo
    constValue = duplicateConstantValue(obj->constAttrs->value); // Sao chép giá trị

    break;

  case TK_CHAR: // Trường hợp là ký tự
    eat(TK_CHAR);
    constValue = makeCharConstant(currentToken->string[0]); // Tạo hằng char
    break;

  default:
    error(ERR_INVALID_CONSTANT, lookAhead->lineNo, lookAhead->colNo);
    break;
  }
  return constValue;
}

/**
 * @brief Biên dịch hằng số (có thể có dấu +/-)
 * Xử lý: +<hằng>, -<hằng>, hoặc <hằng>
 * @return Giá trị hằng số (đã xử lý dấu nếu có)
 */
ConstantValue *compileConstant(void)
{
  ConstantValue *constValue;

  switch (lookAhead->tokenType)
  {
  case SB_PLUS: // Dấu + đứng trước (vd: +5)
    eat(SB_PLUS);
    constValue = compileConstant2(); // Đọc hằng sau dấu +
    // Giá trị không đổi (dấu + không ảnh hưởng)
    break;

  case SB_MINUS: // Dấu - đứng trước (vd: -5)
    eat(SB_MINUS);
    constValue = compileConstant2();              // Đọc hằng sau dấu -
    constValue->intValue = -constValue->intValue; // Đổi dấu giá trị
    break;

  case TK_CHAR: // Ký tự không có dấu
    eat(TK_CHAR);
    constValue = makeCharConstant(currentToken->string[0]);
    break;

  default: // Không có dấu, gọi compileConstant2
    constValue = compileConstant2();
    break;
  }
  return constValue;
}

/**
 * @brief Biên dịch hằng số kiểu số nguyên (không có dấu +/-)
 * Chỉ chấp nhận số hoặc tên hằng integer
 * @return Giá trị hằng số integer
 */
ConstantValue *compileConstant2(void)
{
  ConstantValue *constValue;
  Object *obj;

  switch (lookAhead->tokenType)
  {
  case TK_NUMBER: // Số nguyên trực tiếp
    eat(TK_NUMBER);
    constValue = makeIntConstant(currentToken->value);
    break;

  case TK_IDENT: // Tên hằng đã khai báo
    eat(TK_IDENT);
    obj = checkDeclaredConstant(currentToken->string);
    // PHẢI là hằng kiểu integer (không cho phép char ở đây)
    if (obj->constAttrs->value->type == TP_INT)
      constValue = duplicateConstantValue(obj->constAttrs->value);
    else
      error(ERR_UNDECLARED_INT_CONSTANT, currentToken->lineNo, currentToken->colNo);
    break;

  default:
    error(ERR_INVALID_CONSTANT, lookAhead->lineNo, lookAhead->colNo);
    break;
  }
  return constValue;
}

/**
 * @brief Biên dịch định nghĩa kiểu dữ liệu
 * Hỗ trợ: INTEGER, CHAR, ARRAY, hoặc kiểu đã định nghĩa
 * @return Đối tượng Type đại diện cho kiểu dữ liệu
 */
Type *compileType(void)
{
  Type *type;        // Kiểu kết quả
  Type *elementType; // Kiểu phần tử (cho mảng)
  int arraySize;     // Kích thước mảng
  Object *obj;       // Đối tượng kiểu (nếu là kiểu người dùng định nghĩa)

  switch (lookAhead->tokenType)
  {
  case KW_INTEGER: // Kiểu INTEGER
    eat(KW_INTEGER);
    type = makeIntType();
    break;

  case KW_CHAR: // Kiểu CHAR
    eat(KW_CHAR);
    type = makeCharType();
    break;

  case KW_ARRAY: // Kiểu ARRAY [kích_thước] OF <kiểu_phần_tử>
    eat(KW_ARRAY);
    eat(SB_LSEL);   // Ăn dấu '[' (hoặc '(.')
    eat(TK_NUMBER); // Ăn kích thước mảng

    arraySize = currentToken->value; // Lưu kích thước

    eat(SB_RSEL);                                 // Ăn dấu ']' (hoặc '.)')
    eat(KW_OF);                                   // Ăn từ khóa OF
    elementType = compileType();                  // Biên dịch kiểu phần tử (đệ quy)
    type = makeArrayType(arraySize, elementType); // Tạo kiểu mảng
    break;

  case TK_IDENT: // Kiểu người dùng định nghĩa
    eat(TK_IDENT);
    obj = checkDeclaredType(currentToken->string);    // Kiểm tra đã khai báo
    type = duplicateType(obj->typeAttrs->actualType); // Sao chép định nghĩa kiểu
    break;

  default:
    error(ERR_INVALID_TYPE, lookAhead->lineNo, lookAhead->colNo);
    break;
  }
  return type;
}

/**
 * @brief Biên dịch kiểu cơ bản (INTEGER hoặc CHAR)
 * Chỉ chấp nhận kiểu cơ bản, không cho phép ARRAY hay kiểu người dùng
 * @return Đối tượng Type (chỉ là intType hoặc charType)
 */
Type *compileBasicType(void)
{
  Type *type;

  switch (lookAhead->tokenType)
  {
  case KW_INTEGER: // Kiểu INTEGER
    eat(KW_INTEGER);
    type = makeIntType();
    break;

  case KW_CHAR: // Kiểu CHAR
    eat(KW_CHAR);
    type = makeCharType();
    break;

  default:
    error(ERR_INVALID_BASICTYPE, lookAhead->lineNo, lookAhead->colNo);
    break;
  }
  return type;
}

/**
 * @brief Biên dịch danh sách tham số của hàm/thủ tục
 * Cú pháp: (<param1>; <param2>; ...) hoặc không có tham số
 */
void compileParams(void)
{
  if (lookAhead->tokenType == SB_LPAR)
  {                 // Có danh sách tham số
    eat(SB_LPAR);   // Ăn dấu '('
    compileParam(); // Biên dịch tham số đầu tiên

    while (lookAhead->tokenType == SB_SEMICOLON)
    {                    // Có tham số tiếp theo
      eat(SB_SEMICOLON); // Ăn dấu ';' phân cách
      compileParam();    // Biên dịch tham số tiếp theo
    }

    eat(SB_RPAR); // Ăn dấu ')' kết thúc
  }
  // Nếu không có '(', nghĩa là không có tham số
}

/**
 * @brief Biên dịch một tham số
 * Cú pháp: [VAR] <tên>: <kiểu_cơ_bản>
 * - Có VAR: truyền theo tham chiếu (PARAM_REFERENCE)
 * - Không VAR: truyền theo giá trị (PARAM_VALUE)
 */
void compileParam(void)
{
  Object *param;
  Type *type;
  enum ParamKind paramKind = PARAM_VALUE; // Mặc định: truyền theo giá trị

  if (lookAhead->tokenType == KW_VAR)
  {                              // Có từ khóa VAR
    paramKind = PARAM_REFERENCE; // Chuyển sang truyền theo tham chiếu
    eat(KW_VAR);
  }

  eat(TK_IDENT);                                                  // Ăn tên tham số
  checkFreshIdent(currentToken->string);                          // Kiểm tra tên chưa dùng
  param = createParameterObject(currentToken->string, paramKind); // Tạo đối tượng tham số

  eat(SB_COLON);                  // Ăn dấu ':'
  type = compileBasicType();      // Biên dịch kiểu (chỉ kiểu cơ bản)
  param->paramAttrs->type = type; // Gán kiểu cho tham số
  declareObject(param);           // Thêm vào bảng ký hiệu
}

/**
 * @brief Biên dịch các câu lệnh (cách nhau bởi dấu ;)
 */
void compileStatements(void)
{
  compileStatement(); // Biên dịch câu lệnh đầu tiên
  while (lookAhead->tokenType == SB_SEMICOLON)
  {
    eat(SB_SEMICOLON);
    compileStatement(); // Biên dịch câu lệnh tiếp theo
  }
}

/**
 * @brief Biên dịch một câu lệnh
 * Phân loại dựa vào token đầu tiên (FIRST set)
 */
void compileStatement(void)
{
  switch (lookAhead->tokenType)
  {
  case TK_IDENT: // Câu lệnh gán: x := ...
    compileAssignSt();
    break;
  case KW_CALL: // Gọi thủ tục: CALL proc(...)
    compileCallSt();
    break;
  case KW_BEGIN: // Nhóm lệnh: BEGIN ... END
    compileGroupSt();
    break;
  case KW_IF: // Điều kiện: IF ... THEN ... ELSE ...
    compileIfSt();
    break;
  case KW_WHILE: // Vòng lặp: WHILE ... DO ...
    compileWhileSt();
    break;
  case KW_FOR: // Vòng lặp: FOR ... TO ... DO ...
    compileForSt();
    break;
    // Câu lệnh rỗng - kiểm tra FOLLOW tokens
  case SB_SEMICOLON:
  case KW_END:
  case KW_ELSE:
    break;
    // Lỗi: token không hợp lệ
  default:
    error(ERR_INVALID_STATEMENT, lookAhead->lineNo, lookAhead->colNo);
    break;
  }
}

/**
 * @brief Biên dịch L-value (giá trị bên trái có thể gán)
 * L-value có thể là: biến, tham số, phần tử mảng, giá trị trả về hàm
 * @return Kiểu dữ liệu của L-value
 */
Type *compileLValue(void)
{
  Object *var;   // Đối tượng (biến/tham số/hàm)
  Type *varType; // Kiểu của đối tượng

  eat(TK_IDENT); // Ăn tên định danh

  var = checkDeclaredLValueIdent(currentToken->string); // Kiểm tra đã khai báo

  switch (var->kind)
  {
  case OBJ_VARIABLE:         // Trường hợp là biến
    genVariableAddress(var); // Sinh mã: nạp địa chỉ biến

    if (var->varAttrs->type->typeClass == TP_ARRAY)
    {
      // Nếu là mảng, xử lý các chỉ số [i][j]...
      varType = compileIndexes(var->varAttrs->type);
    }
    else
      varType = var->varAttrs->type;

    break;

  case OBJ_PARAMETER: // Trường hợp là tham số
    if (var->paramAttrs->kind == PARAM_VALUE)
      genParameterAddress(var); // Tham số giá trị: lấy địa chỉ
    else
      genParameterValue(var); // Tham số tham chiếu: lấy giá trị (là địa chỉ)

    varType = var->paramAttrs->type;
    break;

  case OBJ_FUNCTION: // Trường hợp là tên hàm (gán giá trị trả về trong thân hàm)
    genReturnValueAddress(var);
    varType = var->funcAttrs->returnType;
    break;

  default:
    error(ERR_INVALID_LVALUE, currentToken->lineNo, currentToken->colNo);
  }

  return varType;
}

// đề team - bai 2 
/**
 * @brief Biên dịch câu lệnh gán (đơn hoặc đa biến)
 * Cú pháp đơn: <L-value> := <Expression>
 * Cú pháp đa: <L-value>, <L-value>, ... := <Expression>, <Expression>, ...
 *
 * Sinh mã cho gán đơn:
 * - compileLValue(): sinh địa chỉ của biến (LA)
 * - compileExpression(): sinh giá trị cần gán
 * - genST(): lưu giá trị vào địa chỉ
 *
 * Sinh mã cho gán đa (ví dụ: x, y := y, x):
 * QUAN TRỌNG: Phải tính TẤT CẢ giá trị TRƯỚC KHI gán bất kỳ giá trị nào!
 * 1. Lưu tất cả OBJECTS (chưa sinh mã địa chỉ)
 * 2. Tính TẤT CẢ expressions: LV(y), LV(x) → stack = [val_y, val_x]
 * 3. Gán từ CUỐI về ĐẦU:
 *    - LA(y), ST → y = val_x
 *    - LA(x), ST → x = val_y
 * -> Điều này cho phép swap không cần biến tạm!
 */
void compileAssignSt(void)
{
  Object *lvalues[100]; // Mảng lưu các đối tượng L-value (tối đa 100)
  Type *varTypes[100];  // Mảng lưu kiểu của các biến
  Type *expTypes[100];  // Mảng lưu kiểu của các biểu thức
  int count = 0;        // Số lượng biến/biểu thức

  // Bước 1: Lưu L-value đầu tiên (chỉ parse, chưa sinh mã)
  eat(TK_IDENT);
  lvalues[count] = checkDeclaredLValueIdent(currentToken->string);
  count++;

  // Gán nhiều biến: x, y, z := exp1, exp2, exp3
  if (lookAhead->tokenType == SB_COMMA)
  {
    // Đọc các L-value còn lại
    while (lookAhead->tokenType == SB_COMMA)
    {
      eat(SB_COMMA);
      eat(TK_IDENT);
      lvalues[count] = checkDeclaredLValueIdent(currentToken->string);
      count++;
    }

    eat(SB_ASSIGN);
    
    // Lấy kiểu của các L-values để kiểm tra
    for (int i = 0; i < count; i++)
    {
      Object *var = lvalues[i];
      switch (var->kind)
      {
      case OBJ_VARIABLE:
        if (var->varAttrs->type->typeClass == TP_ARRAY)
          error(ERR_TYPE_INCONSISTENCY, currentToken->lineNo, currentToken->colNo);
        varTypes[i] = var->varAttrs->type;
        break;
      case OBJ_PARAMETER:
        varTypes[i] = var->paramAttrs->type;
        break;
      case OBJ_FUNCTION:
        varTypes[i] = var->funcAttrs->returnType;
        break;
      default:
        error(ERR_INVALID_LVALUE, currentToken->lineNo, currentToken->colNo);
      }
    }
    
    // Sinh mã xen kẽ địa chỉ-giá trị: LA(var_i), exp_i
    // Ví dụ x, y := a, b → LA x, LV a, LA y, LV b → stack = [addr_x, val_a, addr_y, val_b]
    for (int i = 0; i < count; i++)
    {
      Object *var = lvalues[i];
      
      // Sinh địa chỉ biến
      switch (var->kind)
      {
      case OBJ_VARIABLE:
        genVariableAddress(var);
        break;
      case OBJ_PARAMETER:
        if (var->paramAttrs->kind == PARAM_VALUE)
          genParameterAddress(var);
        else
          genParameterValue(var);
        break;
      case OBJ_FUNCTION:
        genReturnValueAddress(var);
        break;
      }
      
      // Sinh giá trị expression và kiểm tra kiểu
      expTypes[i] = compileExpression();
      checkTypeEquality(varTypes[i], expTypes[i]);
      
      if (i < count - 1)
        eat(SB_COMMA);
    }
    
    // Gán từ cuối về đầu bằng ST
    // Stack: [addr_0, val_0, ..., addr_n, val_n] → ST gán từng cặp cuối
    for (int i = 0; i < count; i++)
    {
      genST();
    }
  }
  else
  {
    // Gán đơn biến: cú pháp cũ  
    // Đã ăn TK_IDENT và lưu vào lvalues[0], giờ cần sinh mã địa chỉ
    Object *var = lvalues[0];
    switch (var->kind)
    {
    case OBJ_VARIABLE:
      genVariableAddress(var);
      if (var->varAttrs->type->typeClass == TP_ARRAY)
      {
        varTypes[0] = compileIndexes(var->varAttrs->type);
      }
      else
        varTypes[0] = var->varAttrs->type;
      break;
    case OBJ_PARAMETER:
      if (var->paramAttrs->kind == PARAM_VALUE)
        genParameterAddress(var);
      else
        genParameterValue(var);
      varTypes[0] = var->paramAttrs->type;
      break;
    case OBJ_FUNCTION:
      genReturnValueAddress(var);
      varTypes[0] = var->funcAttrs->returnType;
      break;
    default:
      error(ERR_INVALID_LVALUE, currentToken->lineNo, currentToken->colNo);
    }

    eat(SB_ASSIGN);
    expTypes[0] = compileExpression();
    checkTypeEquality(varTypes[0], expTypes[0]);
    genST();
  }
}


/**
 * @brief Biên dịch câu lệnh gọi thủ tục
 * Cú pháp: CALL <tên_thủ_tục>(<đối_số>)
 *
 * Xử lý:
 * - Thủ tục có sẵn (built-in): WRITEI, WRITEC, WRITELN, READI, READC
 * - Thủ tục người dùng: cần cấp phát stack frame
 */
void compileCallSt(void)
{
  Object *proc; // Đối tượng thủ tục

  eat(KW_CALL);  // Ăn từ khóa CALL
  eat(TK_IDENT); // Ăn tên thủ tục

  proc = checkDeclaredProcedure(currentToken->string); // Kiểm tra thủ tục đã khai báo

  if (proc == NULL)
  {
    // Trường hợp đặc biệt: có thể là hàm được gọi như thủ tục (sai cú pháp nhưng xử lý)
    if (isPredefinedFunction(proc))
    {
      compileArguments(proc->funcAttrs->paramList);
      genPredefinedFunctionCall(proc);
    }
  }
  else
  {
    if (isPredefinedProcedure(proc))
    {
      // Thủ tục có sẵn: WRITEI, WRITEC, WRITELN, READI, READC
      compileArguments(proc->procAttrs->paramList); // Biên dịch đối số
      genPredefinedProcedureCall(proc);             // Sinh lệnh gọi thủ tục có sẵn
    }
    else
    {
      // Thủ tục người dùng định nghĩa
      genINT(RESERVED_WORDS);                               // Cấp phát cho: static link, dynamic link, return address
      compileArguments(proc->procAttrs->paramList);         // Biên dịch các đối số
      genDCT(RESERVED_WORDS + proc->procAttrs->paramCount); // Giải phóng stack sau khi gọi
      genProcedureCall(proc);                               // Sinh lệnh CALL đến địa chỉ thủ tục
    }
  }
}

/**
 * @brief Biên dịch nhóm lệnh
 * Cú pháp: BEGIN <statements> END
 */
void compileGroupSt(void)
{
  eat(KW_BEGIN);
  compileStatements(); // Biên dịch chuỗi câu lệnh bên trong
  eat(KW_END);
}

/**
 * @brief Biên dịch câu lệnh IF
 * Cú pháp: IF <điều_kiện> THEN <câu_lệnh1> [ELSE <câu_lệnh2>]
 *
 * Sinh mã:
 * - Tính điều kiện
 * - FJ (False Jump): nhảy đến ELSE nếu điều kiện sai
 * - Biên dịch câu lệnh THEN
 * - J (Jump): nhảy qua phần ELSE
 * - Cập nhật địa chỉ nhảy của FJ
 * - Biên dịch câu lệnh ELSE (nếu có)
 */
void compileIfSt(void)
{
  Instruction *fjInstruction; // Lệnh FJ (nhảy nếu sai)
  Instruction *jInstruction;  // Lệnh J (nhảy vô điều kiện)

  eat(KW_IF);
  compileCondition(); // Biên dịch điều kiện, kết quả trên đỉnh stack (0 hoặc 1)
  eat(KW_THEN);

  fjInstruction = genFJ(DC_VALUE); // Sinh FJ với địa chỉ tạm (sẽ cập nhật)
  compileStatement();              // Biên dịch câu lệnh THEN

  if (lookAhead->tokenType == KW_ELSE)
  {                                                   // Có phần ELSE
    jInstruction = genJ(DC_VALUE);                    // Nhảy qua ELSE sau khi thực hiện THEN
    updateFJ(fjInstruction, getCurrentCodeAddress()); // Cập nhật FJ nhảy đến đây (đầu ELSE)
    eat(KW_ELSE);
    compileStatement();                             // Biên dịch câu lệnh ELSE
    updateJ(jInstruction, getCurrentCodeAddress()); // Cập nhật J nhảy đến đây (sau ELSE)
  }
  else
  {                                                   // Không có ELSE
    updateFJ(fjInstruction, getCurrentCodeAddress()); // FJ nhảy đến đây (sau THEN)
  }
}

/**
 * @brief Biên dịch vòng lặp WHILE
 * Cú pháp: WHILE <điều_kiện> DO <câu_lệnh>
 *
 * Sinh mã:
 * - Lưu địa chỉ đầu vòng lặp
 * - Tính điều kiện
 * - FJ: nhảy ra khỏi vòng lặp nếu sai
 * - Biên dịch thân vòng lặp
 * - J: nhảy về đầu vòng lặp
 */
void compileWhileSt(void)
{
  CodeAddress beginWhile;     // Địa chỉ bắt đầu vòng lặp
  Instruction *fjInstruction; // Lệnh FJ (nhảy ra nếu sai)

  beginWhile = getCurrentCodeAddress(); // Lưu địa chỉ bắt đầu
  eat(KW_WHILE);
  compileCondition();              // Biên dịch điều kiện
  fjInstruction = genFJ(DC_VALUE); // Sinh FJ, nhảy ra nếu điều kiện sai
  eat(KW_DO);
  compileStatement();                               // Biên dịch thân vòng lặp
  genJ(beginWhile);                                 // Nhảy về đầu vòng lặp
  updateFJ(fjInstruction, getCurrentCodeAddress()); // Cập nhật FJ nhảy ra đây
}

/**
 * @brief Biên dịch vòng lặp FOR
 * Cú pháp: FOR <biến> := <giá_trị_đầu> TO <giá_trị_cuối> DO <câu_lệnh>
 *
 * Cơ chế hoạt động:
 * 1. Gán giá trị khởi tạo cho biến
 * 2. Lưu địa chỉ biến trên stack để tái sử dụng
 * 3. Vòng lặp: kiểm tra biến <= giá_trị_cuối
 * 4. Thực thi câu lệnh trong vòng lặp
 * 5. Tăng biến lên 1 (biến = biến + 1)
 * 6. Quay lại bước 3
 * 7. Sau vòng lặp: dọn dẹp stack
 *
 * Stack manipulation:
 * - CV: Copy Value - sao chép giá trị trên đỉnh stack
 * - LI: Load Indirect - nạp giá trị từ địa chỉ trên đỉnh stack
 * - ST: Store - lưu giá trị vào địa chỉ
 * - DCT: Decrease stack top - giảm stack (dọn dẹp)
 */
void compileForSt(void)
{
  CodeAddress beginLoop;      // Địa chỉ bắt đầu vòng lặp (để nhảy về)
  Instruction *fjInstruction; // Lệnh FJ (nhảy ra nếu điều kiện sai)
  Type *varType;              // Kiểu của biến đếm
  Type *type;                 // Kiểu của biểu thức

  eat(KW_FOR); // Ăn từ khóa FOR

  // Bước 1: Biên dịch biến đếm (vế trái) và sinh địa chỉ của nó
  varType = compileLValue(); // Ví dụ: FOR i := ... (sinh địa chỉ của i)
  eat(SB_ASSIGN);            // Ăn dấu :=

  // Bước 2: Tính giá trị khởi tạo và gán cho biến
  genCV();                          // Sao chép địa chỉ biến (giữ lại trên stack để dùng sau)
  type = compileExpression();       // Tính giá trị khởi tạo (ví dụ: 1)
  checkTypeEquality(varType, type); // Kiểm tra kiểu khớp
  genST();                          // Lưu giá trị vào biến (i := giá_trị_khởi_tạo)

  // Bước 3: Chuẩn bị cho vòng lặp - giữ địa chỉ biến và giá trị hiện tại trên stack
  genCV();                             // Sao chép địa chỉ biến (dùng để tăng giá trị sau mỗi lần lặp)
  genLI();                             // Nạp giá trị hiện tại của biến lên stack
  beginLoop = getCurrentCodeAddress(); // Lưu địa chỉ đầu vòng lặp
  eat(KW_TO);                          // Ăn từ khóa TO

  // Bước 4: Kiểm tra điều kiện (biến <= giá_trị_cuối)
  type = compileExpression();       // Tính giá trị cuối (ví dụ: 10)
  checkTypeEquality(varType, type); // Kiểm tra kiểu khớp
  genLE();                          // Sinh lệnh LE (Less than or Equal): so sánh <=
  fjInstruction = genFJ(DC_VALUE);  // Nếu sai (biến > giá_trị_cuối), nhảy ra khỏi vòng lặp

  eat(KW_DO);         // Ăn từ khóa DO
  compileStatement(); // Biên dịch thân vòng lặp

  // Bước 5: Tăng biến đếm (biến = biến + 1)
  genCV();  // Sao chép địa chỉ biến (cần 2 bản: 1 cho ST, 1 cho lần lặp tiếp)
  genCV();  // Sao chép địa chỉ lần nữa
  genLI();  // Nạp giá trị hiện tại của biến
  genLC(1); // Nạp hằng số 1
  genAD();  // Cộng: giá_trị_hiện_tại + 1
  genST();  // Lưu kết quả vào biến (i := i + 1)

  // Bước 6: Chuẩn bị cho lần lặp tiếp theo
  genCV(); // Sao chép địa chỉ biến (dùng cho lần lặp tiếp)
  genLI(); // Nạp giá trị mới của biến

  // Bước 7: Quay lại đầu vòng lặp
  genJ(beginLoop);                                  // Nhảy về beginLoop để kiểm tra điều kiện lại
  updateFJ(fjInstruction, getCurrentCodeAddress()); // Cập nhật FJ nhảy ra đây (khi thoát vòng lặp)

  // Bước 8: Dọn dẹp stack sau khi thoát vòng lặp
  genDCT(1); // Giảm stack 1 ô (xóa địa chỉ biến đã lưu)
}

/**
 * @brief Biên dịch một đối số khi gọi hàm/thủ tục
 *
 * Xử lý 2 trường hợp:
 * 1. Tham số truyền theo giá trị (PARAM_VALUE):
 *    - Biên dịch biểu thức -> sinh giá trị lên stack
 * 2. Tham số truyền theo tham chiếu (PARAM_REFERENCE):
 *    - Biên dịch L-value -> sinh địa chỉ lên stack
 *
 * @param param Đối tượng tham số tương ứng trong khai báo hàm/thủ tục
 */
void compileArgument(Object *param)
{
  Type *type; // Kiểu của đối số

  if (param->paramAttrs->kind == PARAM_VALUE)
  {
    // Truyền theo giá trị: cần giá trị của biểu thức
    type = compileExpression();                       // Tính giá trị biểu thức
    checkTypeEquality(type, param->paramAttrs->type); // Kiểm tra kiểu khớp với tham số
  }
  else
  {
    // Truyền theo tham chiếu (VAR): cần địa chỉ của biến
    type = compileLValue();                           // Lấy địa chỉ của L-value
    checkTypeEquality(type, param->paramAttrs->type); // Kiểm tra kiểu khớp
  }
}

/**
 * @brief Biên dịch danh sách đối số khi gọi hàm/thủ tục
 * Cú pháp: (<đối_số1>, <đối_số2>, ...)
 *
 * Kiểm tra:
 * - Số lượng đối số khớp với số tham số
 * - Thứ tự và kiểu của từng đối số
 * - Cách truyền (giá trị/tham chiếu) phù hợp
 *
 * @param paramList Danh sách tham số từ khai báo hàm/thủ tục
 */
void compileArguments(ObjectNode *paramList)
{
  ObjectNode *node = paramList; // Con trỏ duyệt danh sách tham số

  switch (lookAhead->tokenType)
  {
  case SB_LPAR:   // Có danh sách đối số
    eat(SB_LPAR); // Ăn dấu '('

    // Kiểm tra có tham số tương ứng không
    if (node == NULL)
      error(ERR_PARAMETERS_ARGUMENTS_INCONSISTENCY, currentToken->lineNo, currentToken->colNo);

    // Biên dịch đối số đầu tiên
    compileArgument(node->object);
    node = node->next; // Di chuyển đến tham số tiếp theo

    // Biên dịch các đối số còn lại (phân cách bởi dấu phẩy)
    while (lookAhead->tokenType == SB_COMMA)
    {
      eat(SB_COMMA); // Ăn dấu ','

      // Kiểm tra còn tham số tương ứng không
      if (node == NULL)
        error(ERR_PARAMETERS_ARGUMENTS_INCONSISTENCY, currentToken->lineNo, currentToken->colNo);

      compileArgument(node->object); // Biên dịch đối số tiếp theo
      node = node->next;             // Di chuyển tham số
    }

    // Kiểm tra đã hết tham số chưa (không được thừa tham số)
    if (node != NULL)
      error(ERR_PARAMETERS_ARGUMENTS_INCONSISTENCY, currentToken->lineNo, currentToken->colNo);

    eat(SB_RPAR); // Ăn dấu ')' kết thúc
    break;

    // Kiểm tra FOLLOW set - các token có thể xuất hiện sau arguments
    // (không có đối số, hàm/thủ tục không có tham số)
  case SB_TIMES:
  case SB_SLASH:
  case SB_PLUS:
  case SB_MINUS:
  case KW_TO:
  case KW_DO:
  case SB_RPAR:
  case SB_COMMA:
  case SB_EQ:
  case SB_NEQ:
  case SB_LE:
  case SB_LT:
  case SB_GE:
  case SB_GT:
  case SB_RSEL:
  case SB_SEMICOLON:
  case KW_END:
  case KW_ELSE:
  case KW_THEN:
    break; // Không có đối số - hợp lệ
  default:
    // Token không hợp lệ trong FOLLOW set
    error(ERR_INVALID_ARGUMENTS, lookAhead->lineNo, lookAhead->colNo);
  }
}

/**
 * @brief Biên dịch điều kiện (so sánh 2 biểu thức)
 * Cú pháp: <biểu_thức1> <toán_tử_so_sánh> <biểu_thức2>
 *
 * Các toán tử so sánh:
 * - = : bằng (Equal)
 * - <> : khác (Not Equal)
 * - <= : nhỏ hơn hoặc bằng (Less than or Equal)
 * - < : nhỏ hơn (Less Than)
 * - >= : lớn hơn hoặc bằng (Greater than or Equal)
 * - > : lớn hơn (Greater Than)
 *
 * Kết quả: 1 (đúng) hoặc 0 (sai) trên đỉnh stack
 */
void compileCondition(void)
{
  Type *type1;  // Kiểu của biểu thức bên trái
  Type *type2;  // Kiểu của biểu thức bên phải
  TokenType op; // Toán tử so sánh

  // Bước 1: Biên dịch biểu thức bên trái
  type1 = compileExpression();
  checkBasicType(type1); // Chỉ cho phép kiểu cơ bản (INTEGER, CHAR)

  // Bước 2: Nhận diện toán tử so sánh
  op = lookAhead->tokenType;
  switch (op)
  {
  case SB_EQ: // Toán tử =
    eat(SB_EQ);
    break;
  case SB_NEQ: // Toán tử <>
    eat(SB_NEQ);
    break;
  case SB_LE: // Toán tử <=
    eat(SB_LE);
    break;
  case SB_LT: // Toán tử <
    eat(SB_LT);
    break;
  case SB_GE: // Toán tử >=
    eat(SB_GE);
    break;
  case SB_GT: // Toán tử >
    eat(SB_GT);
    break;
  default:
    // Toán tử không hợp lệ
    error(ERR_INVALID_COMPARATOR, lookAhead->lineNo, lookAhead->colNo);
  }

  // Bước 3: Biên dịch biểu thức bên phải
  type2 = compileExpression();
  checkTypeEquality(type1, type2); // Kiểm tra 2 vế cùng kiểu

  // Bước 4: Sinh lệnh so sánh tương ứng
  // Các lệnh này lấy 2 giá trị trên đỉnh stack, so sánh và trả về 0/1
  switch (op)
  {
  case SB_EQ: // Sinh lệnh EQ (Equal)
    genEQ();
    break;
  case SB_NEQ: // Sinh lệnh NE (Not Equal)
    genNE();
    break;
  case SB_LE: // Sinh lệnh LE (Less or Equal)
    genLE();
    break;
  case SB_LT: // Sinh lệnh LT (Less Than)
    genLT();
    break;
  case SB_GE: // Sinh lệnh GE (Greater or Equal)
    genGE();
    break;
  case SB_GT: // Sinh lệnh GT (Greater Than)
    genGT();
    break;
  default:
    break;
  }
}

/**
 * @brief Biên dịch biểu thức (xử lý dấu +/- đầu tiên)
 * Cú pháp: [+|-] <biểu_thức2>
 *
 * Xử lý dấu unary (đơn nguyên) đứng đầu biểu thức:
 * - +5 : dấu + không ảnh hưởng
 * - -5 : sinh lệnh NEG để đổi dấu
 * - 5  : không có dấu
 *
 * @return Kiểu dữ liệu của biểu thức
 */
Type *compileExpression(void)
{
  Type *type; // Kiểu của biểu thức

  switch (lookAhead->tokenType)
  {
  case SB_PLUS: // Dấu + đứng đầu
    eat(SB_PLUS);
    type = compileExpression2(); // Biên dịch phần còn lại
    checkIntType(type);          // Chỉ áp dụng cho INTEGER
    // Không sinh lệnh (dấu + không thay đổi giá trị)
    break;
  case SB_MINUS: // Dấu - đứng đầu
    eat(SB_MINUS);
    type = compileExpression2(); // Biên dịch phần còn lại
    checkIntType(type);          // Chỉ áp dụng cho INTEGER
    genNEG();                    // Sinh lệnh NEG (đổi dấu: -x)
    break;
  default:                       // Không có dấu +/-
    type = compileExpression2(); // Biên dịch biểu thức bình thường
  }
  return type;
}

/**
 * @brief Biên dịch biểu thức bậc 2 (term và các phép +/-)
 * Điểm vào chính cho biểu thức, gọi compileTerm rồi xử lý +/-
 * @return Kiểu dữ liệu của biểu thức
 */
Type *compileExpression2(void)
{
  Type *type;

  type = compileTerm();            // Biên dịch term đầu tiên
  type = compileExpression3(type); // Xử lý các phép +/- tiếp theo (đệ quy)

  return type;
}

/**
 * @brief Biên dịch phần còn lại của biểu thức (xử lý chuỗi +/-)
 * Cú pháp: {+ <term> | - <term>}*
 *
 * Hàm đệ quy xử lý các phép cộng/trừ từ trái sang phải
 * Ví dụ: a + b - c + d
 *        -> ((a + b) - c) + d
 *
 * @param argType1 Kiểu của toán hạng bên trái (kết quả trước đó)
 * @return Kiểu dữ liệu kết quả
 */
Type *compileExpression3(Type *argType1)
{
  Type *argType2;   // Kiểu của toán hạng bên phải
  Type *resultType; // Kiểu kết quả

  switch (lookAhead->tokenType)
  {
  case SB_PLUS: // Phép cộng
    eat(SB_PLUS);
    checkIntType(argType1);   // Toán hạng trái phải là INTEGER
    argType2 = compileTerm(); // Biên dịch term bên phải
    checkIntType(argType2);   // Toán hạng phải cũng phải là INTEGER

    genAD(); // Sinh lệnh AD (Add - cộng)

    resultType = compileExpression3(argType1); // Đệ quy xử lý tiếp (nếu có)
    break;
  case SB_MINUS: // Phép trừ
    eat(SB_MINUS);
    checkIntType(argType1);   // Toán hạng trái phải là INTEGER
    argType2 = compileTerm(); // Biên dịch term bên phải
    checkIntType(argType2);   // Toán hạng phải cũng phải là INTEGER

    genSB(); // Sinh lệnh SB (Subtract - trừ)

    resultType = compileExpression3(argType1); // Đệ quy xử lý tiếp
    break;

    // Kiểm tra FOLLOW set - các token có thể xuất hiện sau expression
    // (không còn phép +/- nữa, kết thúc expression)
  case KW_TO:
  case KW_DO:
  case SB_RPAR:
  case SB_COMMA:
  case SB_EQ:
  case SB_NEQ:
  case SB_LE:
  case SB_LT:
  case SB_GE:
  case SB_GT:
  case SB_RSEL:
  case SB_SEMICOLON:
  case KW_END:
  case KW_ELSE:
  case KW_THEN:
  case KW_RETURN: // team - bai 3
    resultType = argType1; // Trả về kiểu hiện tại (không xử lý thêm)
    break;
  default:
    error(ERR_INVALID_EXPRESSION, lookAhead->lineNo, lookAhead->colNo);
  }
  return resultType;
}

/**
 * @brief Biên dịch term (factor và các phép *,/)
 * Term là đơn vị ưu tiên cao hơn expression
 * Ví dụ: a + b * c -> a + (b * c)
 * @return Kiểu dữ liệu của term
 */
Type *compileTerm(void)
{
  Type *type;
  type = compileFactor();    // Biên dịch factor đầu tiên
  type = compileTerm2(type); // Xử lý các phép *,/ tiếp theo

  return type;
}

/**
 * @brief Biên dịch phần còn lại của term (xử lý chuỗi *,/)
 * Cú pháp: {* <factor> | / <factor>}*
 *
 * Hàm đệ quy xử lý các phép nhân/chia từ trái sang phải
 * Ví dụ: a * b / c * d
 *        -> ((a * b) / c) * d
 *
 * @param argType1 Kiểu của toán hạng bên trái (kết quả trước đó)
 * @return Kiểu dữ liệu kết quả
 */
Type *compileTerm2(Type *argType1)
{
  Type *argType2;   // Kiểu của toán hạng bên phải
  Type *resultType; // Kiểu kết quả

  switch (lookAhead->tokenType)
  {
  case SB_TIMES: // Phép nhân
    eat(SB_TIMES);
    checkIntType(argType1);     // Toán hạng trái phải là INTEGER
    argType2 = compileFactor(); // Biên dịch factor bên phải
    checkIntType(argType2);     // Toán hạng phải cũng phải là INTEGER

    genML(); // Sinh lệnh ML (Multiply - nhân)

    resultType = compileTerm2(argType1); // Đệ quy xử lý tiếp
    break;
  case SB_SLASH: // Phép chia
    eat(SB_SLASH);
    checkIntType(argType1);     // Toán hạng trái phải là INTEGER
    argType2 = compileFactor(); // Biên dịch factor bên phải
    checkIntType(argType2);     // Toán hạng phải cũng phải là INTEGER

    genDV(); // Sinh lệnh DV (Divide - chia)

    resultType = compileTerm2(argType1); // Đệ quy xử lý tiếp
    break;

    // Kiểm tra FOLLOW set - các token có thể xuất hiện sau term
    // (không còn phép *// nữa, kết thúc term)
  case SB_PLUS:
  case SB_MINUS:
  case KW_TO:
  case KW_DO:
  case SB_RPAR:
  case SB_COMMA:
  case SB_EQ:
  case SB_NEQ:
  case SB_LE:
  case SB_LT:
  case SB_GE:
  case SB_GT:
  case SB_RSEL:
  case SB_SEMICOLON:
  case KW_END:
  case KW_ELSE:
  case KW_THEN:
  case KW_RETURN: // team - bai 3
    resultType = argType1; // Trả về kiểu hiện tại (không xử lý thêm)
    break;
  default:
    error(ERR_INVALID_TERM, lookAhead->lineNo, lookAhead->colNo);
  }
  return resultType;
}

/**
 * @brief Biên dịch factor (đơn vị cơ bản nhất của biểu thức)
 *
 * Factor có thể là:
 * 1. Số nguyên (TK_NUMBER): 123
 * 2. Ký tự (TK_CHAR): 'a'
 * 3. Hằng số đã khai báo (TK_IDENT + OBJ_CONSTANT)
 * 4. Biến (TK_IDENT + OBJ_VARIABLE)
 * 5. Phần tử mảng (TK_IDENT + OBJ_VARIABLE + indexes)
 * 6. Tham số (TK_IDENT + OBJ_PARAMETER)
 * 7. Gọi hàm (TK_IDENT + OBJ_FUNCTION)
 * 8. Biểu thức trong ngoặc (SB_LPAR + expression + SB_RPAR)
 *
 * @return Kiểu dữ liệu của factor
 */
Type *compileFactor(void)
{
  Type *type;  // Kiểu của factor
  Object *obj; // Đối tượng (nếu là identifier)

  switch (lookAhead->tokenType)
  {
  case TK_NUMBER: // Trường hợp 1: Số nguyên literal
    eat(TK_NUMBER);
    type = intType;             // Kiểu là INTEGER
    genLC(currentToken->value); // Sinh lệnh LC (Load Constant) - nạp giá trị số
    break;
  case TK_CHAR: // Trường hợp 2: Ký tự literal
    eat(TK_CHAR);
    type = charType;
    genLC(currentToken->value);
    break;
  case TK_IDENT:
    eat(TK_IDENT);
    obj = checkDeclaredIdent(currentToken->string);

    switch (obj->kind)
    {
    case OBJ_CONSTANT:
      switch (obj->constAttrs->value->type)
      {
      case TP_INT:
        type = intType;
        genLC(obj->constAttrs->value->intValue);
        break;
      case TP_CHAR:
        type = charType;
        genLC(obj->constAttrs->value->charValue);
        break;
      default:
        break;
      }
      break;
    case OBJ_VARIABLE:
      if (obj->varAttrs->type->typeClass == TP_ARRAY)
      {
        genVariableAddress(obj);
        type = compileIndexes(obj->varAttrs->type);
        genLI();
      }
      else
      {
        type = obj->varAttrs->type;
        genVariableValue(obj);
      }
      break;
    case OBJ_PARAMETER:
      type = obj->paramAttrs->type;
      genParameterValue(obj);
      if (obj->paramAttrs->kind == PARAM_REFERENCE)
        genLI();
      break;
    case OBJ_FUNCTION:
      if (isPredefinedFunction(obj))
      {
        compileArguments(obj->funcAttrs->paramList);
        genPredefinedFunctionCall(obj);
      }
      else
      {
        genINT(4);
        compileArguments(obj->funcAttrs->paramList);
        genDCT(4 + obj->funcAttrs->paramCount);
        genFunctionCall(obj);
      }
      type = obj->funcAttrs->returnType;
      break;
    default:
      error(ERR_INVALID_FACTOR, currentToken->lineNo, currentToken->colNo);
      break;
    }
    break;
  case SB_LPAR:
    eat(SB_LPAR);               // Ăn dấu '('
    type = compileExpression(); // Biên dịch biểu thức bên trong
    eat(SB_RPAR);               // Ăn dấu ')'
    break;
  
  case KW_IF: // team - bai 3: IF expression: if <cond> return <exp1> else return <exp2>
    eat(KW_IF);
    compileCondition();          // Tính điều kiện → stack: [0/1]
    eat(KW_RETURN);
    
    {
      Instruction *fjInst = genFJ(DC_VALUE); // FJ: nhảy nếu sai
      Type *type1 = compileExpression();     // Tính exp1 (nhánh true)
      Instruction *jInst = genJ(DC_VALUE);   // J: nhảy qua nhánh false
      
      updateFJ(fjInst, getCurrentCodeAddress()); // FJ nhảy đến đây
      eat(KW_ELSE);
      eat(KW_RETURN);
      Type *type2 = compileExpression();     // Tính exp2 (nhánh false)
      
      updateJ(jInst, getCurrentCodeAddress()); // J nhảy đến đây
      checkTypeEquality(type1, type2);
      type = type1;
    }
    break;

  default:
    // Token không hợp lệ cho factor
    error(ERR_INVALID_FACTOR, lookAhead->lineNo, lookAhead->colNo);
  }

  return type;
}

/**
 * @brief Biên dịch các chỉ số mảng (array indexes)
 * Cú pháp: [<expression>] [<expression>] ...
 *
 * Ví dụ: arr[i][j]
 * - arr có địa chỉ base đã ở trên stack
 * - Tính offset: i * size(element) + j * size(sub-element)
 * - Cộng offset vào địa chỉ base
 *
 * Công thức tính địa chỉ phần tử:
 *   address = base + index * element_size
 *
 * @param arrayType Kiểu mảng hiện tại (có thể là mảng nhiều chiều)
 * @return Kiểu của phần tử cuối cùng (phải là kiểu cơ bản)
 */
Type *compileIndexes(Type *arrayType)
{
  Type *type; // Kiểu của biểu thức chỉ số

  // Xử lý từng chỉ số (có thể có nhiều: [i][j][k]...)
  while (lookAhead->tokenType == SB_LSEL)
  {
    eat(SB_LSEL); // Ăn dấu '[' (hoặc '(.')

    type = compileExpression(); // Biên dịch biểu thức chỉ số
    checkIntType(type);         // Chỉ số phải là INTEGER
    checkArrayType(arrayType);  // Kiểm tra đang xử lý kiểu mảng

    // Tính offset:
    // Stack hiện tại: [địa_chỉ_base] [index]
    genLC(sizeOfType(arrayType->elementType)); // Nạp kích thước phần tử
    // Stack: [địa_chỉ_base] [index] [element_size]
    genML(); // Nhân: index * element_size
    // Stack: [địa_chỉ_base] [offset]
    genAD(); // Cộng: địa_chỉ_base + offset
    // Stack: [địa_chỉ_phần_tử]

    arrayType = arrayType->elementType; // Di chuyển xuống mức phần tử
    eat(SB_RSEL);                       // Ăn dấu ']' (hoặc '.)')
  }

  // Sau khi xử lý hết chỉ số, phải đến kiểu cơ bản (không phải mảng nữa)
  checkBasicType(arrayType);
  return arrayType;
}

/**
 * @brief Hàm chính để biên dịch file nguồn KPL
 *
 * Quy trình biên dịch:
 * 1. Mở file nguồn để đọc
 * 2. Khởi tạo currentToken và lookAhead
 * 3. Khởi tạo bảng ký hiệu (symbol table)
 * 4. Biên dịch chương trình (compileProgram)
 * 5. Dọn dẹp: giải phóng bộ nhớ, đóng file
 *
 * @param fileName Tên file nguồn KPL cần biên dịch
 * @return IO_SUCCESS nếu thành công, IO_ERROR nếu lỗi
 */
int compile(char *fileName)
{
  // Bước 1: Mở file nguồn
  if (openInputStream(fileName) == IO_ERROR)
    return IO_ERROR;

  // Bước 2: Khởi tạo token
  currentToken = NULL;         // Chưa có token hiện tại
  lookAhead = getValidToken(); // Đọc token đầu tiên vào lookAhead

  // Bước 3: Khởi tạo bảng ký hiệu
  initSymTab(); // Tạo bảng ký hiệu với các hàm/thủ tục có sẵn

  // Bước 4: Bắt đầu biên dịch từ quy tắc đầu tiên (Program)
  compileProgram(); // Biên dịch toàn bộ chương trình

  // Bước 5: Dọn dẹp
  cleanSymTab();      // Giải phóng bảng ký hiệu
  free(currentToken); // Giải phóng token hiện tại
  free(lookAhead);    // Giải phóng token nhìn trước
  closeInputStream(); // Đóng file nguồn

  return IO_SUCCESS; // Biên dịch thành công
}
