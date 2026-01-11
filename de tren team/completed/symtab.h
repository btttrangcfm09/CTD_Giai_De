/* 
 * @copyright (c) 2008, Hedspi, Hanoi University of Technology
 * @author Huu-Duc Nguyen
 * @version 1.0
 */

#ifndef __SYMTAB_H__
#define __SYMTAB_H__

#include "token.h"
#include "instructions.h"

// Phân loại kiểu dữ liệu
enum TypeClass {
  TP_INT,
  TP_CHAR,
  TP_ARRAY
};

// Phân loại ký hiệu 
enum ObjectKind {
  OBJ_CONSTANT,
  OBJ_VARIABLE,
  OBJ_TYPE,
  OBJ_FUNCTION,
  OBJ_PROCEDURE,
  OBJ_PARAMETER,
  OBJ_PROGRAM
};

// Phân loại tham số
enum ParamKind {
  PARAM_VALUE,
  PARAM_REFERENCE
};

// Định nghĩa cấu trúc Type
struct Type_ {
  enum TypeClass typeClass;
  // Chỉ sử dụng cho kiểu mảng
  int arraySize;
  struct Type_ *elementType;
};

typedef struct Type_ Type;
typedef struct Type_ BasicType;

// Hằng số
struct ConstantValue_ {
  enum TypeClass type;
  union {
    int intValue;
    char charValue;
  };
};

typedef struct ConstantValue_ ConstantValue;

struct Scope_;
struct ObjectNode_;
struct Object_;

struct ConstantAttributes_ {
  ConstantValue* value;
};

struct VariableAttributes_ {
  Type *type;
  // Phạm vi của biến (sử dụng cho pha sinh mã)
  struct Scope_ *scope;

  int localOffset;        // offset of the local variable calculated from the base of the stack frame
};

struct TypeAttributes_ {
  Type *actualType;
};

struct ProcedureAttributes_ {
  struct ObjectNode_ *paramList;
  struct Scope_* scope;

  int paramCount;
  CodeAddress codeAddress;
};

struct FunctionAttributes_ {
  struct ObjectNode_ *paramList;
  Type* returnType;
  struct Scope_ *scope;

  int paramCount;
  CodeAddress codeAddress;
};

struct ProgramAttributes_ {
  struct Scope_ *scope;
  CodeAddress codeAddress;
};

struct ParameterAttributes_ {
  // Tham biến hoặc tham trị
  enum ParamKind kind;
  Type* type;
  struct Scope_ *scope;

  int localOffset;
};

typedef struct ConstantAttributes_ ConstantAttributes;
typedef struct TypeAttributes_ TypeAttributes;
typedef struct VariableAttributes_ VariableAttributes;
typedef struct FunctionAttributes_ FunctionAttributes;
typedef struct ProcedureAttributes_ ProcedureAttributes;
typedef struct ProgramAttributes_ ProgramAttributes;
typedef struct ParameterAttributes_ ParameterAttributes;

// Định nghĩa cấu trúc Object
struct Object_ {
  // Tên đối tượng
  char name[MAX_IDENT_LEN];
  // Loại đối tượng
  enum ObjectKind kind;
  // Thuộc tính đối tượng
  union {
    ConstantAttributes* constAttrs;
    VariableAttributes* varAttrs;
    TypeAttributes* typeAttrs;
    FunctionAttributes* funcAttrs;
    ProcedureAttributes* procAttrs;
    ProgramAttributes* progAttrs;
    ParameterAttributes* paramAttrs;
  };
};

typedef struct Object_ Object;

struct ObjectNode_ {
  Object *object;
  struct ObjectNode_ *next;
};

typedef struct ObjectNode_ ObjectNode;

// Phạm vi của một block
struct Scope_ {
  // Danh sách các đối tượng trong block
  ObjectNode *objList;
  // Hàm, thủ tục, chương trình tương ứng block
  Object *owner;
  // Phạm vi bên ngoài (outer scope)
  struct Scope_ *outer;
  // Kích thước khung ngăn xếp cho block này
  int frameSize;
};

typedef struct Scope_ Scope;

/**
 * Biến toàn cục symtab:
 * - Con trỏ đến bảng ký hiệu chính của chương trình
 * - Lưu trữ tất cả các đối tượng và scope trong quá trình biên dịch
 * - Chứa currentScope để theo dõi scope hiện tại đang xử lý
 */
struct SymTab_ {
  Object* program;
  Scope* currentScope;
  ObjectNode *globalObjectList; // Các đối tượng toàn cục như hàm WRITEI, WRITEC, WRITELN READI, READC
};

typedef struct SymTab_ SymTab;

// Các hàm tạo kiểu 
Type* makeIntType(void);
Type* makeCharType(void);
Type* makeArrayType(int arraySize, Type* elementType);
Type* duplicateType(Type* type);
int compareType(Type* type1, Type* type2);
void freeType(Type* type);
int sizeOfType(Type* type);

// Các hàm tạo giá trị hằng số 
ConstantValue* makeIntConstant(int i);
ConstantValue* makeCharConstant(char ch);
ConstantValue* duplicateConstantValue(ConstantValue* v);

// Tạo block/scope mới
Scope* createScope(Object* owner);

// Tạo một đối tượng chương trình
Object* createProgramObject(char *programName);

// Tạo một đối tượng hằng số
Object* createConstantObject(char *name);

// Tạo một đối tượng kiểu dữ liệu
Object* createTypeObject(char *name);

// Tạo một đối tượng biến
Object* createVariableObject(char *name);

// Tạo một đối tượng hàm
Object* createFunctionObject(char *name);

// Tạo một đối tượng thủ tục
Object* createProcedureObject(char *name);

// Tạo một đối tượng tham số hình thức
Object* createParameterObject(char *name, enum ParamKind kind);

Object* findObject(ObjectNode *objList, char *name);

// Khởi tạo bảng ký hiệu
void initSymTab(void);
// Dọn dẹp bảng ký hiệu
void cleanSymTab(void);
// Cập nhật currentScope đến scope mới
void enterBlock(Scope* scope);
// Quay về scope bên ngoài
void exitBlock(void);
// Khai báo một Object vào scope hiện tại
void declareObject(Object* obj);

#endif
