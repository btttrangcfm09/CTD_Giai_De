
/* 
 * @copyright (c) 2008, Hedspi, Hanoi University of Technology
 * @author Huu-Duc Nguyen
 * @version 1.0
 * File symtab.c - Quản lý bảng ký hiệu (Symbol Table)
 * 
 * Chức năng:
 * - Lưu trữ thông tin về các định danh trong chương trình (biến, hằng, hàm, thủ tục, kiểu)
 * - Quản lý scope (phạm vi) - hỗ trợ nested scope
 * - Cung cấp các hàm tiện ích để tạo, tìm kiếm, và giải phóng các đối tượng
 * - Hỗ trợ kiểm tra ngữ nghĩa (semantic analysis) trong quá trình biên dịch
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "symtab.h"
#include "error.h"
#include "codegen.h"

// Khai báo trước các hàm nội bộ để giải phóng bộ nhớ
//  Giải phóng đối tượng
void freeObject(Object* obj);
// Giải phóng block/scope
void freeScope(Scope* scope);
// Giải phóng danh sách đối tượng
void freeObjectList(ObjectNode *objList);
// Giải phóng danh sách tham chiếu (chỉ node, không free object)
void freeReferenceList(ObjectNode *objList);

/**
 * Biến toàn cục symtab:
 * - Con trỏ đến bảng ký hiệu chính của chương trình
 * - Lưu trữ tất cả các đối tượng và scope trong quá trình biên dịch
 * - Chứa currentScope để theo dõi scope hiện tại đang xử lý
 */
SymTab* symtab;

/**
 * Biến toàn cục intType:
 * - Đại diện cho kiểu dữ liệu INTEGER cơ bản
 * - Được sử dụng chung trong toàn bộ chương trình để tránh tạo nhiều bản sao
 */
Type* intType;

/**
 * Biến toàn cục charType:
 * - Đại diện cho kiểu dữ liệu CHAR cơ bản
 * - Được sử dụng chung trong toàn bộ chương trình để tránh tạo nhiều bản sao
 */
Type* charType;
Object* writeiProcedure;
Object* writecProcedure;
Object* writelnProcedure;
Object* readiFunction;
Object* readcFunction;

/******************* Type utilities ******************************/

/**
 * Hàm makeIntType():
 * Chức năng: Tạo một đối tượng Type đại diện cho kiểu INTEGER
 * 
 * Trả về:
 * @return Con trỏ đến Type với typeClass = TP_INT
 * 
 * Đặc điểm:
 * - Kiểu cơ bản, không có thuộc tính con
 * - Được sử dụng cho biến số nguyên, tham số, giá trị trả về
 * 
 * Ví dụ:
 * Type* t = makeIntType();  // Tạo kiểu INTEGER
 */
Type* makeIntType(void) {
  Type* type = (Type*) malloc(sizeof(Type));
  type->typeClass = TP_INT;
  return type;
}

/**
 * Hàm makeCharType():
 * Chức năng: Tạo một đối tượng Type đại diện cho kiểu CHAR
 * 
 * Trả về:
 * @return Con trỏ đến Type với typeClass = TP_CHAR
 * 
 * Đặc điểm:
 * - Kiểu cơ bản dùng cho ký tự đơn
 * - Kích thước 1 byte
 * - Được sử dụng cho biến ký tự, tham số, giá trị trả về
 * 
 * Ví dụ:
 * Type* t = makeCharType();  // Tạo kiểu CHAR
 */
Type* makeCharType(void) {
  Type* type = (Type*) malloc(sizeof(Type));
  type->typeClass = TP_CHAR;
  return type;
}

/**
 * Hàm makeArrayType():
 * Chức năng: Tạo một đối tượng Type đại diện cho kiểu ARRAY
 * 
 * Tham số:
 * @param arraySize - Số lượng phần tử trong mảng
 * @param elementType - Kiểu dữ liệu của từng phần tử
 * 
 * Trả về:
 * @return Con trỏ đến Type với typeClass = TP_ARRAY
 * 
 * Đặc điểm:
 * - Kiểu phức hợp chứa nhiều phần tử cùng kiểu
 * - elementType có thể là kiểu cơ bản hoặc mảng khác (mảng đa chiều)
 * - Kích thước bộ nhớ = arraySize * sizeOfType(elementType)
 * 
 * Ví dụ:
 * Type* intArray = makeArrayType(10, makeIntType());  // ARRAY[10] OF INTEGER
 * Type* matrix = makeArrayType(5, makeArrayType(5, makeIntType()));  // Mảng 2 chiều
 */
Type* makeArrayType(int arraySize, Type* elementType) {
  Type* type = (Type*) malloc(sizeof(Type));
  type->typeClass = TP_ARRAY;
  type->arraySize = arraySize;
  type->elementType = elementType;
  return type;
}

/**
 * Hàm duplicateType():
 * Chức năng: Tạo một bản sao sâu (deep copy) của một Type
 * 
 * Tham số:
 * @param type - Type cần sao chép
 * 
 * Trả về:
 * @return Con trỏ đến Type mới - bản sao độc lập của type gốc
 * 
 * Đặc điểm:
 * - Tạo bản sao độc lập, không chia sẻ bộ nhớ với type gốc
 * - Với kiểu ARRAY, đệ quy sao chép cả elementType
 * - Đảm bảo thay đổi bản sao không ảnh hưởng đến type gốc
 * 
 * Ví dụ:
 * Type* original = makeArrayType(10, makeIntType());
 * Type* copy = duplicateType(original);  // Tạo bản sao độc lập
 */
Type* duplicateType(Type* type) {
  Type* resultType = (Type*) malloc(sizeof(Type));
  resultType->typeClass = type->typeClass;
  if (type->typeClass == TP_ARRAY) {
    resultType->arraySize = type->arraySize;
    resultType->elementType = duplicateType(type->elementType);
  }
  return resultType;
}

/**
 * Hàm compareType():
 * Chức năng: So sánh hai Type để kiểm tra xem chúng có tương đương không
 * 
 * Tham số:
 * @param type1 - Type thứ nhất
 * @param type2 - Type thứ hai
 * 
 * Trả về:
 * @return 1 nếu hai type giống nhau
 * @return 0 nếu hai type khác nhau
 * 
 * Quy tắc so sánh:
 * - Hai type cơ bản (INT, CHAR) phải có cùng typeClass
 * - Hai mảng phải có:
 *   + Cùng arraySize
 *   + Cùng elementType (so sánh đệ quy)
 * 
 * Ví dụ:
 * compareType(makeIntType(), makeIntType()) -> 1
 * compareType(makeIntType(), makeCharType()) -> 0
 * compareType(makeArrayType(10, intType), makeArrayType(10, intType)) -> 1
 * compareType(makeArrayType(10, intType), makeArrayType(20, intType)) -> 0
 */
int compareType(Type* type1, Type* type2) {
  if (type1->typeClass == type2->typeClass) {
    if (type1->typeClass == TP_ARRAY) {
      if (type1->arraySize == type2->arraySize)
	return compareType(type1->elementType, type2->elementType);
      else return 0;
    } else return 1;
  } else return 0;
}

/**
 * Hàm freeType():
 * Chức năng: Giải phóng bộ nhớ được cấp phát cho một Type
 * 
 * Tham số:
 * @param type - Type cần giải phóng
 * 
 * Đặc điểm:
 * - Với INT và CHAR: Chỉ cần free chính nó
 * - Với ARRAY: Đệ quy giải phóng elementType trước, sau đó free chính nó
 * - Tránh memory leak bằng cách giải phóng toàn bộ cấu trúc lồng nhau
 * 
 * Lưu ý: Không free intType và charType toàn cục vì chúng được free riêng trong cleanSymTab()
 */
void freeType(Type* type) {
  switch (type->typeClass) {
  case TP_INT:
  case TP_CHAR:
    free(type);
    break;
  case TP_ARRAY:
    freeType(type->elementType);
    freeType(type);
    break;
  }
}

/**
 * Hàm sizeOfType():
 * Chức năng: Tính kích thước bộ nhớ cần cấp phát cho một Type
 * 
 * Tham số:
 * @param type - Type cần tính kích thước
 * 
 * Trả về:
 * @return Kích thước tính bằng byte
 * 
 * Quy tắc tính:
 * - INT: INT_SIZE byte
 * - CHAR: CHAR_SIZE byte
 * - ARRAY: arraySize * sizeOfType(elementType) - đệ quy tính
 * 
 * Sử dụng cho:
 * - Cấp phát bộ nhớ cho biến
 * - Tính frame size của scope
 * - Tối ưu hóa bộ nhớ
 * 
 * Ví dụ:
 * sizeOfType(makeIntType()) -> INT_SIZE
 * sizeOfType(makeArrayType(10, makeIntType())) -> 10 * INT_SIZE
 */
int sizeOfType(Type* type) {
  switch (type->typeClass) {
  case TP_INT:
    return INT_SIZE;
  case TP_CHAR:
    return CHAR_SIZE;
  case TP_ARRAY:
    return (type->arraySize * sizeOfType(type->elementType));
  }
  return 0;
}

/******************* Constant utility ******************************/

/**
 * Hàm makeIntConstant():
 * Chức năng: Tạo một giá trị hằng số kiểu INTEGER
 * 
 * Tham số:
 * @param i - Giá trị số nguyên
 * 
 * Trả về:
 * @return Con trỏ đến ConstantValue chứa giá trị nguyên i
 * 
 * Ví dụ sử dụng:
 * ConstantValue* c = makeIntConstant(100);  // Hằng số 100
 * ConstantValue* max = makeIntConstant(999); // Hằng số 999
 */
ConstantValue* makeIntConstant(int i) {
  ConstantValue* value = (ConstantValue*) malloc(sizeof(ConstantValue));
  value->type = TP_INT;
  value->intValue = i;
  return value;
}

/**
 * Hàm makeCharConstant():
 * Chức năng: Tạo một giá trị hằng số kiểu CHAR
 * 
 * Tham số:
 * @param ch - Ký tự
 * 
 * Trả về:
 * @return Con trỏ đến ConstantValue chứa ký tự ch
 * 
 * Ví dụ sử dụng:
 * ConstantValue* c = makeCharConstant('A');  // Hằng số 'A'
 * ConstantValue* newline = makeCharConstant('\n'); // Hằng số xuống dòng
 */
ConstantValue* makeCharConstant(char ch) {
  ConstantValue* value = (ConstantValue*) malloc(sizeof(ConstantValue));
  value->type = TP_CHAR;
  value->charValue = ch;
  return value;
}

/**
 * Hàm duplicateConstantValue():
 * Chức năng: Tạo một bản sao của ConstantValue
 * 
 * Tham số:
 * @param v - ConstantValue cần sao chép
 * 
 * Trả về:
 * @return Con trỏ đến ConstantValue mới - bản sao của v
 * 
 * Đặc điểm:
 * - Sao chép cả type và giá trị (intValue hoặc charValue)
 * - Tạo bản sao độc lập không chia sẻ bộ nhớ với v gốc
 */
ConstantValue* duplicateConstantValue(ConstantValue* v) {
  ConstantValue* value = (ConstantValue*) malloc(sizeof(ConstantValue));
  value->type = v->type;
  if (v->type == TP_INT) 
    value->intValue = v->intValue;
  else
    value->charValue = v->charValue;
  return value;
}

/******************* Object utilities ******************************/

/**
 * Hàm createScope():
 * Chức năng: Tạo một scope (phạm vi) mới
 * 
 * Tham số:
 * @param owner - Đối tượng sở hữu scope này (Program, Function, hoặc Procedure)
 * @param outer - Scope bên ngoài (scope cha)
 * 
 * Trả về:
 * @return Con trỏ đến Scope mới
 * 
 * Scope là gì:
 * - Phạm vi mà các định danh có hiệu lực
 * - Mỗi Program, Function, Procedure có scope riêng
 * - Các scope có thể lồng nhau (nested scope)
 * 
 * Ví dụ:
 * Scope* globalScope = createScope(programObj, NULL);  // Scope toàn cục
 * Scope* funcScope = createScope(funcObj, globalScope); // Scope của hàm
 */
Scope* createScope(Object* owner) {
  Scope* scope = (Scope*) malloc(sizeof(Scope));
  scope->objList = NULL;
  scope->owner = owner; // Đối tượng sở hữu scope
  scope->outer = NULL;
  scope->frameSize = RESERVED_WORDS;
  return scope;
}

/**
 * Hàm createProgramObject():
 * Chức năng: Tạo một đối tượng chương trình mới
 * 
 * Tham số:
 * @param programName - Tên chương trình
 * 
 * Trả về:
 * @return Con trỏ đến Object đại diện cho chương trình
 * 
 * Đặc điểm:
 * - Đối tượng chương trình có kind là OBJ_PROGRAM
 * - Tạo scope mới cho chương trình
 * - Khởi tạo codeAddress với giá trị DC_VALUE
 */
Object* createProgramObject(char *programName) {
  Object* program = (Object*) malloc(sizeof(Object));
  strcpy(program->name, programName);
  program->kind = OBJ_PROGRAM;
  program->progAttrs = (ProgramAttributes*) malloc(sizeof(ProgramAttributes));
  program->progAttrs->scope = createScope(program);
  program->progAttrs->codeAddress = DC_VALUE;
  symtab->program = program;

  return program;
}

/**
 * Hàm createConstantObject():
 * Chức năng: Tạo đối tượng Constant - đại diện cho hằng số
 * 
 * Tham số:
 * @param name - Tên của hằng số
 * 
 * Trả về:
 * @return Con trỏ đến Object Constant mới
 * 
 * Ví dụ:
 * Object* maxConst = createConstantObject("MAX");
 * maxConst->constAttrs->value = makeIntConstant(100);
 */
Object* createConstantObject(char *name) {
  Object* obj = (Object*) malloc(sizeof(Object));
  strcpy(obj->name, name);
  obj->kind = OBJ_CONSTANT;
  obj->constAttrs = (ConstantAttributes*) malloc(sizeof(ConstantAttributes));
  return obj;
}

/**
 * Hàm createTypeObject():
 * Chức năng: Tạo đối tượng Type - đại diện cho kiểu dữ liệu tùy chỉnh
 * 
 * Tham số:
 * @param name - Tên của kiểu dữ liệu
 * 
 * Trả về:
 * @return Con trỏ đến Object Type mới
 * 
 * Sử dụng cho:
 * - Khai báo kiểu tùy chỉnh trong phần TYPE của chương trình
 * - Ví dụ: TYPE IntArray = ARRAY[10] OF INTEGER;
 * 
 * Ví dụ:
 * Object* intArrayType = createTypeObject("IntArray");
 * intArrayType->typeAttrs->actualType = makeArrayType(10, makeIntType());
 */
Object* createTypeObject(char *name) {
  Object* obj = (Object*) malloc(sizeof(Object));
  strcpy(obj->name, name);
  obj->kind = OBJ_TYPE;
  obj->typeAttrs = (TypeAttributes*) malloc(sizeof(TypeAttributes));
  return obj;
}

/**
 * Hàm createVariableObject():
 * Chức năng: Tạo đối tượng Variable - đại diện cho biến
 * 
 * Tham số:
 * @param name - Tên của biến
 * 
 * Trả về:
 * @return Con trỏ đến Object Variable mới
 * 
 * Đặc điểm:
 * - Lưu trữ scope hiện tại nơi biến được khai báo
 * - Biến có thể là kiểu cơ bản (INT, CHAR) hoặc phức tạp (ARRAY)
 * 
 * Ví dụ:
 * Object* var = createVariableObject("x");
 * var->varAttrs->type = makeIntType();
 */
Object* createVariableObject(char *name) {
  Object* obj = (Object*) malloc(sizeof(Object));
  strcpy(obj->name, name);
  obj->kind = OBJ_VARIABLE;
  obj->varAttrs = (VariableAttributes*) malloc(sizeof(VariableAttributes));
  obj->varAttrs->type = NULL;
  obj->varAttrs->scope = NULL;
  obj->varAttrs->localOffset = 0;
  return obj;
}

/**
 * Hàm createFunctionObject():
 * Chức năng: Tạo đối tượng Function - đại diện cho hàm
 * 
 * Tham số:
 * @param name - Tên của hàm
 * 
 * Trả về:
 * @return Con trỏ đến Object Function mới
 * 
 * Đặc điểm:
 * - Có danh sách tham số (paramList)
 * - Có kiểu trả về (returnType)
 * - Có scope riêng cho các biến cục bộ
 * 
 * Ví dụ:
 * Object* maxFunc = createFunctionObject("max");
 * maxFunc->funcAttrs->returnType = makeIntType();
 */
Object* createFunctionObject(char *name) {
  Object* obj = (Object*) malloc(sizeof(Object));
  strcpy(obj->name, name);
  obj->kind = OBJ_FUNCTION;
  obj->funcAttrs = (FunctionAttributes*) malloc(sizeof(FunctionAttributes));
  obj->funcAttrs->returnType = NULL;
  obj->funcAttrs->paramList = NULL;
  obj->funcAttrs->paramCount = 0;
  obj->funcAttrs->codeAddress = DC_VALUE;
  obj->funcAttrs->scope = createScope(obj);
  return obj;
}

/**
 * Hàm createProcedureObject():
 * Chức năng: Tạo đối tượng Procedure - đại diện cho thủ tục
 * 
 * Tham số:
 * @param name - Tên của thủ tục
 * 
 * Trả về:
 * @return Con trỏ đến Object Procedure mới
 * 
 * Khác biệt với Function:
 * - Không có kiểu trả về (returnType)
 * - Chỉ thực hiện các tác vụ, không trả về giá trị
 * 
 * Giống với Function:
 * - Có danh sách tham số (paramList)
 * - Có scope riêng
 * 
 * Ví dụ:
 * Object* printProc = createProcedureObject("print");
 */
Object* createProcedureObject(char *name) {
  Object* obj = (Object*) malloc(sizeof(Object));
  strcpy(obj->name, name);
  obj->kind = OBJ_PROCEDURE;
  obj->procAttrs = (ProcedureAttributes*) malloc(sizeof(ProcedureAttributes));
  obj->procAttrs->paramList = NULL;
  obj->procAttrs->paramCount = 0;
  obj->procAttrs->codeAddress = DC_VALUE;
  obj->procAttrs->scope = createScope(obj);
  return obj;
}

/**
 * Hàm createParameterObject():
 * Chức năng: Tạo đối tượng Parameter - đại diện cho tham số của hàm/thủ tục
 * 
 * Tham số:
 * @param name - Tên của tham số
 * @param kind - Loại tham số (PARAM_VALUE hoặc PARAM_REFERENCE)
 * 
 * Trả về:
 * @return Con trỏ đến Object Parameter mới
 * 
 * Loại tham số:
 * - PARAM_VALUE: Truyền theo giá trị (pass by value)
 *   + Sao chép giá trị, thay đổi không ảnh hưởng biến gốc
 *   + Ví dụ: FUNCTION add(x: INTEGER)
 * 
 * - PARAM_REFERENCE: Truyền theo tham chiếu (pass by reference)
 *   + Truyền địa chỉ, thay đổi ảnh hưởng biến gốc
 *   + Ví dụ: PROCEDURE swap(VAR a: INTEGER)
 * 
 * Ví dụ:
 * Object* param1 = createParameterObject("x", PARAM_VALUE, funcObj);
 * Object* param2 = createParameterObject("arr", PARAM_REFERENCE, procObj);
 */
Object* createParameterObject(char *name, enum ParamKind kind) {
  Object* obj = (Object*) malloc(sizeof(Object));
  strcpy(obj->name, name);
  obj->kind = OBJ_PARAMETER;
  obj->paramAttrs = (ParameterAttributes*) malloc(sizeof(ParameterAttributes));
  obj->paramAttrs->kind = kind;
  obj->paramAttrs->type = NULL;
  obj->paramAttrs->scope = NULL;
  obj->paramAttrs->localOffset = 0;
  return obj;
}

/**
 * Hàm freeObject():
 * Chức năng: Giải phóng bộ nhớ được cấp phát cho một Object
 * 
 * Tham số:
 * @param obj - Object cần giải phóng
 * 
 * Đặc điểm:
 * - Giải phóng các thuộc tính con dựa trên loại đối tượng (kind)
 * - Đối với các đối tượng có scope (Function, Procedure, Program), giải phóng scope đó
 * - Đối với danh sách tham số, giải phóng cả danh sách và các tham số bên trong
 * - Đảm bảo không để lại memory leak bằng cách giải phóng toàn bộ cấu trúc liên quan
 */
void freeObject(Object* obj) {
  switch (obj->kind) {
  case OBJ_CONSTANT:
    free(obj->constAttrs->value);
    free(obj->constAttrs);
    break;
  case OBJ_TYPE:
    free(obj->typeAttrs->actualType);
    free(obj->typeAttrs);
    break;
  case OBJ_VARIABLE:
    free(obj->varAttrs->type);
    free(obj->varAttrs);
    break;
  case OBJ_FUNCTION:
    freeReferenceList(obj->funcAttrs->paramList);
    freeType(obj->funcAttrs->returnType);
    freeScope(obj->funcAttrs->scope);
    free(obj->funcAttrs);
    break;
  case OBJ_PROCEDURE:
    freeReferenceList(obj->procAttrs->paramList);
    freeScope(obj->procAttrs->scope);
    free(obj->procAttrs);
    break;
  case OBJ_PROGRAM:
    freeScope(obj->progAttrs->scope);
    free(obj->progAttrs);
    break;
  case OBJ_PARAMETER:
    freeType(obj->paramAttrs->type);
    free(obj->paramAttrs);
  }
  free(obj);
}

/**
 * Hàm freeScope():
 * Chức năng: Giải phóng một scope và tất cả đối tượng bên trong nó
 * 
 * Tham số:
 * @param scope - Scope cần giải phóng
 * 
 * Đặc điểm:
 * - Giải phóng toàn bộ danh sách đối tượng trong scope (objList)
 * - Giải phóng chính struct Scope
 * - Không giải phóng scope->outer (vì đó là tham chiếu đến scope khác)
 * 
 * Lưu ý:
 * - Phải giải phóng objList trước khi giải phóng scope
 * - Được gọi khi giải phóng Function, Procedure, hoặc Program
 */
void freeScope(Scope* scope) {
  freeObjectList(scope->objList);
  free(scope);
}

/**
 * Hàm freeObjectList():
 * Chức năng: Giải phóng danh sách đối tượng và tất cả đối tượng bên trong
 * 
 * Tham số:
 * @param objList - Danh sách liên kết các ObjectNode cần giải phóng
 * 
 * Đặc điểm:
 * - Duyệt qua từng node trong danh sách liên kết
 * - Giải phóng cả Object và ObjectNode
 * - Xử lý đầy đủ danh sách, không để lại memory leak
 * 
 * Quy trình:
 * 1. Duyệt từ đầu đến cuối danh sách
 * 2. Với mỗi node: giải phóng object bên trong, sau đó giải phóng node
 * 3. Tiếp tục đến node tiếp theo
 */
void freeObjectList(ObjectNode *objList) {
  ObjectNode* list = objList;

  while (list != NULL) {
    ObjectNode* node = list;
    list = list->next;
    freeObject(node->object);
    free(node);
  }
}

/**
 * Hàm freeReferenceList():
 * Chức năng: Giải phóng danh sách tham chiếu (chỉ node, không free object)
 * 
 * Tham số:
 * @param objList - Danh sách liên kết các ObjectNode cần giải phóng
 * 
 * Đặc điểm:
 * - Chỉ giải phóng các ObjectNode, KHÔNG giải phóng Object bên trong
 * - Sử dụng cho danh sách tham số (paramList) vì các tham số
 *   đã được lưu trong scope và sẽ được giải phóng khi giải phóng scope
 * 
 * Khác với freeObjectList():
 * - freeObjectList: Giải phóng cả node và object (ownership)
 * - freeReferenceList: Chỉ giải phóng node (reference only)
 * 
 * Lưu ý:
 * - Tránh double-free vì object đã thuộc sở hữu của scope
 */
void freeReferenceList(ObjectNode *objList) {
  ObjectNode* list = objList;

  while (list != NULL) {
    ObjectNode* node = list;
    list = list->next;
    free(node);
  }
}

/**
 * Hàm addObject():
 * Chức năng: Thêm một đối tượng vào cuối danh sách đối tượng
 * 
 * Tham số:
 * @param objList - Con trỏ đến con trỏ danh sách (để có thể thay đổi head)
 * @param obj - Object cần thêm vào
 * 
 * Đặc điểm:
 * - Tạo ObjectNode mới để chứa obj
 * - Thêm vào cuối danh sách (append)
 * - Nếu danh sách rỗng, node mới trở thành head
 * 
 * Thuật toán:
 * 1. Tạo node mới chứa obj
 * 2. Nếu danh sách rỗng: đặt node làm head
 * 3. Nếu không: duyệt đến cuối danh sách và gắn node vào
 */
void addObject(ObjectNode **objList, Object* obj) {
  ObjectNode* node = (ObjectNode*) malloc(sizeof(ObjectNode));
  node->object = obj;
  node->next = NULL;
  if ((*objList) == NULL) 
    *objList = node;
  else {
    ObjectNode *n = *objList;
    while (n->next != NULL) 
      n = n->next;
    n->next = node;
  }
}

/**
 * Hàm findObject():
 * Chức năng: Tìm một đối tượng trong danh sách theo tên
 * 
 * Tham số:
 * @param objList - Danh sách các ObjectNode cần tìm kiếm
 * @param name - Tên của đối tượng cần tìm
 * 
 * Trả về:
 * @return Con trỏ đến Object nếu tìm thấy
 * @return NULL nếu không tìm thấy
 * 
 * Đặc điểm:
 * - Tìm kiếm tuyến tính từ đầu đến cuối danh sách
 * - So sánh tên bằng strcmp (case-sensitive)
 * - Trả về ngay khi tìm thấy đối tượng đầu tiên khớp tên
 * 
 * Sử dụng cho:
 * - Kiểm tra trùng lặp tên khi khai báo
 * - Tìm kiếm định danh trong phân tích ngữ nghĩa
 */
Object* findObject(ObjectNode *objList, char *name) {
  while (objList != NULL) {
    if (strcmp(objList->object->name, name) == 0) 
      return objList->object;
    else objList = objList->next;
  }
  return NULL;
}

/******************* others ******************************/

/**
 * Hàm initSymTab():
 * Chức năng: Khởi tạo bảng ký hiệu và các đối tượng chuẩn (standard objects)
 * 
 * Đặc điểm:
 * - Tạo symtab toàn cục và khởi tạo các trường của nó
 * - Khai báo các hàm thư viện chuẩn: READI, READC
 * - Khai báo các thủ tục thư viện chuẩn: WRITEI, WRITEC, WRITELN
 * - Khởi tạo intType và charType toàn cục
 * 
 * Các hàm/thủ tục chuẩn:
 * - READI: Hàm đọc số nguyên từ input, trả về INTEGER
 * - READC: Hàm đọc ký tự từ input, trả về CHAR
 * - WRITEI(i: INTEGER): Thủ tục in số nguyên ra output
 * - WRITEC(ch: CHAR): Thủ tục in ký tự ra output
 * - WRITELN: Thủ tục in ký tự xuống dòng
 * 
 * Lưu ý:
 * - Phải gọi hàm này trước khi bắt đầu phân tích chương trình
 * - Các đối tượng chuẩn được thêm vào globalObjectList
 */
void initSymTab(void) {
  // Biến tạm để lưu tham số khi khai báo hàm/thủ tục
  Object* param;

  // Cấp phát bộ nhớ cho bảng ký hiệu toàn cục
  symtab = (SymTab*) malloc(sizeof(SymTab));
  
  // Khởi tạo danh sách đối tượng toàn cục = rỗng (chưa có đối tượng nào)
  symtab->globalObjectList = NULL;
  
  // Khởi tạo con trỏ chương trình = NULL (chưa có chương trình)
  symtab->program = NULL;
  
  // Khởi tạo scope hiện tại = NULL (chưa vào scope nào)
  symtab->currentScope = NULL;
  
  // === Khai báo hàm READC: đọc 1 ký tự từ input ===
  // Tạo đối tượng hàm với tên "READC"
  readcFunction = createFunctionObject("READC");
  
  // Thêm hàm READC vào bảng ký hiệu toàn cục
  declareObject(readcFunction);

  // Thiết lập kiểu trả về của READC là CHAR
  readcFunction->funcAttrs->returnType = makeCharType();

  // === Khai báo hàm READI: đọc 1 số nguyên từ input ===
  // Tạo đối tượng hàm với tên "READI"
  readiFunction = createFunctionObject("READI");
  
  // Thêm hàm READI vào bảng ký hiệu toàn cục
  declareObject(readiFunction);
  
  // Thiết lập kiểu trả về của READI là INTEGER
  readiFunction->funcAttrs->returnType = makeIntType();

  // === Khai báo thủ tục WRITEI: in số nguyên ra output ===
  // Tạo đối tượng thủ tục với tên "WRITEI"
  writeiProcedure = createProcedureObject("WRITEI");
  
  // Thêm thủ tục WRITEI vào bảng ký hiệu toàn cục
  declareObject(writeiProcedure);
  
  // Vào scope của thủ tục WRITEI để khai báo tham số
  enterBlock(writeiProcedure->procAttrs->scope);
  
    // Tạo tham số "i" kiểu truyền theo giá trị (PARAM_VALUE)
    param = createParameterObject("i", PARAM_VALUE);
    
    // Thiết lập kiểu dữ liệu của tham số "i" là INTEGER
    param->paramAttrs->type = makeIntType();
    
    // Khai báo tham số "i" vào scope của WRITEI
    declareObject(param);
    
  // Thoát khỏi scope của WRITEI (kết thúc khai báo tham số)
  exitBlock();

  // === Khai báo thủ tục WRITEC: in ký tự ra output ===
  // Tạo đối tượng thủ tục với tên "WRITEC"
  writecProcedure = createProcedureObject("WRITEC");
  
  // Thêm thủ tục WRITEC vào bảng ký hiệu toàn cục
  declareObject(writecProcedure);
  
  // Vào scope của thủ tục WRITEC để khai báo tham số
  enterBlock(writecProcedure->procAttrs->scope);
  
    // Tạo tham số "ch" kiểu truyền theo giá trị (PARAM_VALUE)
    param = createParameterObject("ch", PARAM_VALUE);
    
    // Thiết lập kiểu dữ liệu của tham số "ch" là CHAR
    param->paramAttrs->type = makeCharType();
    
    // Khai báo tham số "ch" vào scope của WRITEC
    declareObject(param);
    
  // Thoát khỏi scope của WRITEC (kết thúc khai báo tham số)
  exitBlock();

  // === Khai báo thủ tục WRITELN: in ký tự xuống dòng ===
  // Tạo đối tượng thủ tục với tên "WRITELN" (không có tham số)
  writelnProcedure = createProcedureObject("WRITELN");
  
  // Thêm thủ tục WRITELN vào bảng ký hiệu toàn cục
  declareObject(writelnProcedure);

  // === Khởi tạo các kiểu dữ liệu cơ bản toàn cục ===
  // Tạo kiểu INTEGER toàn cục (dùng chung cho toàn bộ chương trình)
  intType = makeIntType();
  
  // Tạo kiểu CHAR toàn cục (dùng chung cho toàn bộ chương trình)
  charType = makeCharType();
}

/**
 * Hàm cleanSymTab():
 * Chức năng: Giải phóng toàn bộ bộ nhớ của bảng ký hiệu
 * 
 * Đặc điểm:
 * - Giải phóng đối tượng chương trình (symtab->program)
 * - Giải phóng danh sách đối tượng toàn cục (globalObjectList)
 * - Giải phóng struct symtab chính
 * - Giải phóng intType và charType toàn cục
 * 
 * Lưu ý:
 * - Phải gọi sau khi hoàn tất phân tích và biên dịch
 * - Giải phóng đệ quy tất cả đối tượng, scope, type liên quan
 * - Tránh memory leak bằng cách giải phóng đầy đủ toàn bộ cấu trúc
 */
void cleanSymTab(void) {
  freeObject(symtab->program);
  freeObjectList(symtab->globalObjectList);
  free(symtab);
  freeType(intType);
  freeType(charType);
}

/**
 * Hàm enterBlock():
 * Chức năng: Vào một scope mới (khối lệnh mới)
 * 
 * Tham số:
 * @param scope - Scope cần vào
 * 
 * Đặc điểm:
 * - Đặt scope làm currentScope của symtab
 * - Các khai báo tiếp theo sẽ được thêm vào scope này
 * - Hỗ trợ nested scope (scope lồng nhau)
 * 
 * Sử dụng khi:
 * - Bắt đầu phân tích thân chương trình (Program body)
 * - Bắt đầu phân tích thân hàm (Function body)
 * - Bắt đầu phân tích thân thủ tục (Procedure body)
 * 
 * Ví dụ luồng:
 * enterBlock(programScope);  // Vào scope chương trình
 *   declareObject(var1);     // Khai báo trong program scope
 *   enterBlock(funcScope);   // Vào scope hàm lồng trong program
 *     declareObject(var2);   // Khai báo trong func scope
 *   exitBlock();             // Thoát func scope
 * exitBlock();               // Thoát program scope
 */
void enterBlock(Scope* scope) {
  symtab->currentScope = scope;
}

/**
 * Hàm exitBlock():
 * Chức năng: Thoát khỏi scope hiện tại, quay về scope bên ngoài
 * 
 * Đặc điểm:
 * - Đặt currentScope về scope->outer (scope cha)
 * - Kết thúc phân tích một khối lệnh
 * - Các khai báo tiếp theo sẽ thuộc scope bên ngoài
 * 
 * Sử dụng khi:
 * - Kết thúc phân tích thân chương trình
 * - Kết thúc phân tích thân hàm
 * - Kết thúc phân tích thân thủ tục
 * 
 * Lưu ý:
 * - Phải gọi sau khi đã enterBlock() tương ứng
 * - Số lần gọi exitBlock() phải khớp với enterBlock()
 */
void exitBlock(void) {
  symtab->currentScope = symtab->currentScope->outer;
}

/**
 * Hàm declareObject():
 * Chức năng: Khai báo một đối tượng vào bảng ký hiệu
 * 
 * Tham số:
 * @param obj - Object cần khai báo
 * 
 * Đặc điểm:
 * - Nếu currentScope = NULL: thêm vào globalObjectList (đối tượng toàn cục)
 * - Nếu currentScope != NULL: thêm vào objList của scope hiện tại
 * - Xử lý đặc biệt cho từng loại đối tượng:
 *   + VARIABLE: Lưu scope, tính localOffset, cập nhật frameSize
 *   + PARAMETER: Lưu scope, tính localOffset, thêm vào paramList của owner
 *   + FUNCTION: Thiết lập scope->outer
 *   + PROCEDURE: Thiết lập scope->outer
 * 
 * Vai trò của localOffset và frameSize:
 * - localOffset: Vị trí của biến/tham số trong stack frame
 * - frameSize: Tổng kích thước bộ nhớ cần cho scope (tăng dần khi khai báo)
 * 
 * Ví dụ:
 * declareObject(varObj);   // Khai báo biến
 * declareObject(funcObj);  // Khai báo hàm
 */
void declareObject(Object* obj) {
  // Biến tạm lưu đối tượng sở hữu scope (Function hoặc Procedure)
  Object* owner;

  // Kiểm tra xem có đang ở trong scope nào không
  if (symtab->currentScope == NULL) {
    // Nếu currentScope = NULL => Đây là đối tượng toàn cục (global object)
    // Thêm đối tượng vào danh sách đối tượng toàn cục của bảng ký hiệu
    addObject(&(symtab->globalObjectList), obj);
  }
  else {
    // Nếu đang ở trong một scope => Xử lý theo loại đối tượng
    switch (obj->kind) {
    
    case OBJ_VARIABLE:
      // === Xử lý khai báo BIẾN ===
      
      // Lưu scope hiện tại nơi biến được khai báo
      obj->varAttrs->scope = symtab->currentScope;
      
      // Gán vị trí offset của biến trong stack frame = frameSize hiện tại
      // (frameSize cho biết đã dùng bao nhiêu byte cho các biến trước đó)
      obj->varAttrs->localOffset = symtab->currentScope->frameSize;
      
      // Tăng frameSize lên bằng kích thước của biến này
      // Ví dụ: biến int tăng INT_SIZE, mảng[10] int tăng 10*INT_SIZE
      symtab->currentScope->frameSize += sizeOfType(obj->varAttrs->type);
      break;
      
    case OBJ_PARAMETER:
      // === Xử lý khai báo THAM SỐ ===
      
      // Lưu scope hiện tại nơi tham số được khai báo
      obj->paramAttrs->scope = symtab->currentScope;
      
      // Gán vị trí offset của tham số trong stack frame
      obj->paramAttrs->localOffset = symtab->currentScope->frameSize;
      
      // Tăng frameSize lên 1 (mỗi tham số chiếm 1 vị trí)
      symtab->currentScope->frameSize ++;
      
      // Lấy đối tượng sở hữu scope này (Function hoặc Procedure)
      owner = symtab->currentScope->owner;
      
      // Kiểm tra xem owner là Function hay Procedure
      switch (owner->kind) {
      case OBJ_FUNCTION:
        // Nếu là Function: Thêm tham số vào paramList của Function
        addObject(&(owner->funcAttrs->paramList), obj);
        
        // Tăng số lượng tham số của Function lên 1
        owner->funcAttrs->paramCount ++;
        break;
        
      case OBJ_PROCEDURE:
        // Nếu là Procedure: Thêm tham số vào paramList của Procedure
        addObject(&(owner->procAttrs->paramList), obj);
        
        // Tăng số lượng tham số của Procedure lên 1
        owner->procAttrs->paramCount ++;
        break;
        
      default:
        // Các trường hợp khác: không làm gì
        break;
      }
      break;
      
    case OBJ_FUNCTION:
      // === Xử lý khai báo HÀM ===
      
      // Thiết lập scope bên ngoài (outer) của Function là scope hiện tại
      // Điều này tạo ra cấu trúc scope lồng nhau (nested scope)
      obj->funcAttrs->scope->outer = symtab->currentScope;
      break;
      
    case OBJ_PROCEDURE:
      // === Xử lý khai báo THỦ TỤC ===
      
      // Thiết lập scope bên ngoài (outer) của Procedure là scope hiện tại
      // Điều này tạo ra cấu trúc scope lồng nhau (nested scope)
      obj->procAttrs->scope->outer = symtab->currentScope;
      break;
      
    default: 
      // Các loại đối tượng khác (CONSTANT, TYPE): không cần xử lý đặc biệt
      break;
    }
    
    // Sau khi xử lý xong, thêm đối tượng vào danh sách của scope hiện tại
    // (Mọi đối tượng được khai báo trong scope đều được lưu vào objList)
    addObject(&(symtab->currentScope->objList), obj);
  }
  
}


