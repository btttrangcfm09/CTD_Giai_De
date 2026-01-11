/* Scanner
 * @copyright (c) 2008, Hedspi, Hanoi University of Technology
 * @author Huu-Duc Nguyen
 * @version 1.0
 * 
 * Mô tả:
 *   Module này thực hiện phân tích từ vựng cho ngôn ngữ lập trình KPL.
 *   Nó đọc mã nguồn từ file đầu vào và chuyển đổi thành chuỗi các token.
 *   
 * Chức năng chính:
 *   - Nhận dạng các từ khóa (keywords) của ngôn ngữ KPL
 *   - Nhận dạng các định danh (identifiers)
 *   - Nhận dạng các hằng số (number literals)
 *   - Nhận dạng các hằng ký tự (character literals)
 *   - Nhận dạng các toán tử và dấu phân cách
 *   - Bỏ qua khoảng trắng và comment
 *   - Phát hiện và báo lỗi từ vựng
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "reader.h"    // Module đọc ký tự từ file đầu vào
#include "charcode.h"  // Định nghĩa các mã ký tự (CHAR_LETTER, CHAR_DIGIT, ...)
#include "token.h"     // Định nghĩa cấu trúc Token và các loại token
#include "error.h"     // Module xử lý lỗi
#include "scanner.h"

/**
 * Các biến toàn cục được khai báo extern (được định nghĩa trong module reader.c)
 */
extern int lineNo;       // Số dòng hiện tại trong file đang đọc (bắt đầu từ 1)
extern int colNo;        // Số cột hiện tại trong dòng đang đọc (bắt đầu từ 1)
extern int currentChar;  // Ký tự hiện tại đang được xét (mã ASCII), hoặc EOF nếu hết file

// Bảng mã ký tự (được định nghĩa trong module charcode.c)
extern CharCode charCodes[];

/***************************************************************/

/**
 * @brief Bỏ qua các ký tự trắng (space, tab, newline) liên tiếp.
 *
 * Hàm này sẽ liên tục đọc ký tự tiếp theo từ luồng vào cho đến khi
 * gặp một ký tự không phải là ký tự trắng hoặc kết thúc file (EOF).
 */
void skipBlank() {
  // Lặp trong khi ký tự hiện tại vẫn là ký tự trắng và chưa hết file
  // Điều kiện dừng: gặp ký tự không phải khoảng trắng HOẶC hết file (EOF)
  while ((currentChar != EOF) && (charCodes[currentChar] == CHAR_SPACE))
    readChar(); // Đọc ký tự tiếp theo và cập nhật currentChar, lineNo, colNo
}

// skip comment // skip until end of line
void skipComment2(){
  int lineBeforSkip = lineNo;
  while (1){
    readChar();
    if(lineBeforSkip != lineNo || currentChar == EOF){
      return;
    } 
  }
  
}

/**
 * @brief Bỏ qua một khối comment.
 *
 * Comment trong ngôn ngữ KPL bắt đầu bằng `(*` và kết thúc bằng `*)`.
 * Hàm này sẽ đọc và bỏ qua tất cả các ký tự cho đến khi gặp chuỗi `*)`.
 * Nếu gặp cuối file (EOF) trước khi comment kết thúc, hàm sẽ báo lỗi.
 */
void skipComment() {
  // Sử dụng máy trạng thái (state machine) để nhận dạng kết thúc comment
  // state = 0: trạng thái ban đầu hoặc đã gặp ký tự thường
  // state = 1: đã gặp dấu '*' (có thể là phần đầu của '*)')
  // state = 2: đã gặp chuỗi '*)' - kết thúc comment
  int state = 0;
  
  // Lặp cho đến khi hết file HOẶC tìm được kết thúc comment (state = 2)
  while ((currentChar != EOF) && (state < 2)) {
    switch (charCodes[currentChar]) {
    case CHAR_TIMES: // Gặp dấu '*'
      state = 1;     // Chuyển sang state 1 (đang chờ dấu ')' để hoàn thành '*)')
      break;
    case CHAR_RPAR:  // Gặp dấu ')'
      if (state == 1) state = 2; // Nếu state=1 (đã có '*'), hoàn thành '*)', chuyển sang state 2
      else state = 0;            // Nếu không, reset về state 0
      break;
    default:         // Gặp ký tự khác
      state = 0;     // Reset về state 0 (không phải chuỗi '*)')
    }
    readChar(); // Đọc ký tự tiếp theo
  }
  
  // Kiểm tra: nếu kết thúc vòng lặp mà state != 2 (chưa tìm thấy '*)')
  // nghĩa là comment không được đóng đúng -> báo lỗi
  if (state != 2) 
    error(ERR_END_OF_COMMENT, lineNo, colNo);
}

/**
 * @brief Đọc một định danh (identifier) hoặc một từ khoá (keyword).
 *
 * Một định danh bắt đầu bằng một chữ cái, theo sau là các chữ cái hoặc chữ số.
 * Hàm này đọc chuỗi ký tự hợp lệ, sau đó kiểm tra xem nó có phải là một
 * từ khoá đã định nghĩa trước hay không.
 *
 * @return Con trỏ tới Token đã được tạo.
 */
Token* readIdentKeyword(void) {
  // Tạo token mới với loại tạm thời là TK_NONE
  // Lưu vị trí dòng và cột hiện tại để báo lỗi chính xác
  Token *token = makeToken(TK_NONE, lineNo, colNo);
  int count = 1; // Đếm số ký tự đã đọc (bắt đầu từ 1 vì đã có ký tự đầu tiên)

  // Lưu ký tự đầu tiên (đã biết là chữ cái) và chuyển thành chữ HOA
  // KPL không phân biệt hoa thường, nên chuẩn hóa tất cả thành HOA
  token->string[0] = toupper((char)currentChar);
  readChar(); // Đọc ký tự tiếp theo

  // Tiếp tục đọc các ký tự tiếp theo nếu là chữ cái hoặc chữ số
  // Định danh/từ khóa có thể chứa chữ cái và chữ số (nhưng phải bắt đầu bằng chữ cái)
  while ((currentChar != EOF) && ((charCodes[currentChar] == CHAR_LETTER) || (charCodes[currentChar] == CHAR_DIGIT))) {
    // Chỉ lưu ký tự nếu chưa vượt quá độ dài tối đa cho phép
    if (count <= MAX_IDENT_LEN) token->string[count++] = toupper((char)currentChar);
    readChar(); // Đọc ký tự tiếp theo
  }

  // Kiểm tra xem định danh có quá dài không
  if (count > MAX_IDENT_LEN) {
    error(ERR_IDENT_TOO_LONG, token->lineNo, token->colNo); // Báo lỗi định danh quá dài
    return token; // Trả về token lỗi (TK_NONE)
  }

  // Kết thúc chuỗi với ký tự null terminator
  token->string[count] = '\0';
  
  // Kiểm tra xem chuỗi vừa đọc có phải là từ khóa (keyword) không
  // Hàm checkKeyword trả về loại từ khóa nếu là keyword, ngược lại trả về TK_NONE
  token->tokenType = checkKeyword(token->string);

  // Nếu không phải từ khóa, đây là một định danh (identifier)
  if (token->tokenType == TK_NONE)
    token->tokenType = TK_IDENT;

  return token; // Trả về token (có thể là keyword hoặc identifier)
}

/**
 * @brief Đọc một hằng số (number literal).
 *
 * Hàm này đọc một chuỗi các chữ số liên tiếp và chuyển đổi chúng thành
 * một giá trị số nguyên.
 *
 * @return Con trỏ tới Token đã được tạo.
 */
Token* readNumber(void) {
  // Tạo token loại TK_NUMBER với vị trí hiện tại
  Token *token = makeToken(TK_NUMBER, lineNo, colNo);
  int count = 0; // Đếm số chữ số đã đọc

  // Đọc liên tiếp các chữ số cho đến khi gặp ký tự không phải số
  // Điều kiện dừng: EOF hoặc gặp ký tự không phải CHAR_DIGIT
  while ((currentChar != EOF) && (charCodes[currentChar] == CHAR_DIGIT)) {
    token->string[count++] = (char)currentChar; // Lưu chữ số vào chuỗi token
    readChar(); // Đọc ký tự tiếp theo
  }

  // Kết thúc chuỗi với null terminator
  token->string[count] = '\0';
  
  // Chuyển đổi chuỗi số thành giá trị số nguyên
  // atoi() (ASCII to Integer) chuyển chuỗi ký tự thành số int
  token->value = atoi(token->string);
  
  return token; // Trả về token số
}

/**
 * @brief Đọc một hằng ký tự (character literal).
 *
 * Hằng ký tự hợp lệ trong KPL có dạng 'c' (một ký tự duy nhất trong cặp nháy đơn).
 * Hàm này xử lý việc đọc và xác thực cấu trúc này.
 *
 * @return Con trỏ tới Token đã được tạo.
 */
Token* readConstChar(void) {
  // Hàm này đọc một hằng ký tự có dạng: 'x' (ký tự nằm giữa 2 dấu nháy đơn)
  // Khi vào hàm này, currentChar đang là dấu nháy đơn mở (')
  Token *token = makeToken(TK_CHAR, lineNo, colNo);

  // Bước 1: Đọc ký tự tiếp theo sau dấu nháy mở
  readChar();
  if (currentChar == EOF) {
    // Lỗi: gặp EOF ngay sau dấu nháy mở (không có ký tự)
    token->tokenType = TK_NONE;
    error(ERR_INVALID_CONSTANT_CHAR, token->lineNo, token->colNo);
    return token;
  }
  
  // Bước 2: Lưu ký tự vào token
  token->string[0] = currentChar;  // Lưu ký tự vào chuỗi
  token->string[1] = '\0';         // Kết thúc chuỗi
  token->value = currentChar;      // Lưu giá trị mã ASCII của ký tự

  // Bước 3: Đọc ký tự tiếp theo (phải là dấu nháy đơn đóng)
  readChar();
  if (currentChar == EOF) {
    // Lỗi: gặp EOF trước khi có dấu nháy đóng
    token->tokenType = TK_NONE;
    error(ERR_INVALID_CONSTANT_CHAR, token->lineNo, token->colNo);
    return token;
  }

  // Bước 4: Kiểm tra dấu nháy đơn đóng
  if (charCodes[currentChar] == CHAR_SINGLEQUOTE) {
    // Đúng: đã hoàn thành đọc hằng ký tự 'x'
    readChar(); // Đọc ký tự tiếp theo (sau dấu nháy đóng)
    return token; // Trả về token hợp lệ
  } else {
    // Lỗi: không có dấu nháy đóng (có thể là chuỗi nhiều ký tự)
    token->tokenType = TK_NONE;
    error(ERR_INVALID_CONSTANT_CHAR, token->lineNo, token->colNo);
    return token;
  }
}

// Token* readString(void){
//   Token *token = makeToken(TK_STRING, lineNo, colNo);

//   readChar();
  
//   int token_index = 0;
//   while(1){
//     if (currentChar != EOF ) {
//       if(charCodes[currentChar] == CHAR_DOUBLEQUOTE){
//         token->string[token_index < MAX_IDENT_LEN ? token_index : MAX_IDENT_LEN] = '\0'; // Kết thúc chuỗi
//         // Hợp lệ, đọc ký tự tiếp theo để di chuyển con trỏ ra khỏi hằng ký tự
//         readChar();
//         return token;
//       }
//       else{
//         // Lấy ký tự bên trong cặp nháy
//         token->string[token_index] = currentChar;
//         token_index++;
//         readChar();   
//       }
//     } else {
//       // Lỗi: Thiếu dấu nháy đơn đóng hoặc có quá nhiều ký tự.
//       error(ERR_INVALID_CONSTANT_CHAR, token->lineNo, token->colNo);
//       return token;
//     }
//   }
// }

/**
 * @brief Lấy token tiếp theo từ luồng vào.
 *
 * Đây là hàm trung tâm của bộ phân tích từ vựng (scanner). Nó đọc ký tự
 * hiện tại và quyết định loại token tương ứng, sau đó gọi hàm xử lý phù hợp.
 *
 * @return Con trỏ tới Token tiếp theo.
 */
Token* getToken(void) {
  // Hàm chính để lấy token tiếp theo từ luồng đầu vào
  // Sử dụng switch-case để xử lý từng loại ký tự khác nhau
  Token *token;
  int ln, cn; // Biến lưu tạm lineNo và colNo để xử lý token nhiều ký tự

  // Trường hợp đặc biệt: đã hết file
  if (currentChar == EOF) 
    return makeToken(TK_EOF, lineNo, colNo); // Trả về token EOF

  // Phân loại ký tự hiện tại và xử lý tương ứng
  switch (charCodes[currentChar]) {
  case CHAR_SPACE: // Gặp khoảng trắng (space, tab, newline)
    skipBlank();   // Bỏ qua tất cả khoảng trắng liên tiếp
    return getToken(); // Gọi đệ quy để lấy token tiếp theo (sau khoảng trắng)
    
  case CHAR_LETTER: // Gặp chữ cái -> có thể là keyword hoặc identifier
    return readIdentKeyword();
    
  case CHAR_DIGIT: // Gặp chữ số -> là số
    return readNumber();
    
  // Các toán tử số học đơn giản (1 ký tự)
  case CHAR_PLUS:  // Dấu '+'
    token = makeToken(SB_PLUS, lineNo, colNo);
    readChar(); // Di chuyển sang ký tự tiếp theo
    return token;
    
  case CHAR_MINUS: // Dấu '-'
    token = makeToken(SB_MINUS, lineNo, colNo);
    readChar(); 
    return token;
    
  // case CHAR_PERCENT:
  //   token = makeToken(SB_MOD, lineNo, colNo);
  //   readChar();
  //   return token;

  case CHAR_TIMES: // Dấu '*'
    token = makeToken(SB_TIMES, lineNo, colNo);
    readChar(); 
    return token;
    
  case CHAR_SLASH: // Dấu '/'
    token = makeToken(SB_SLASH, lineNo, colNo);
    readChar(); 
    if(charCodes[currentChar] == CHAR_SLASH){ 
      free(token);
      skipComment2();
      return getToken();
    }
    return token;
    
  // Toán tử so sánh '<' và '<='  
  case CHAR_LT: // Gặp '<'
    ln = lineNo; // Lưu vị trí bắt đầu token
    cn = colNo;
    readChar(); // Đọc ký tự tiếp theo
    // Kiểm tra xem có phải là '<=' không
    if ((currentChar != EOF) && (charCodes[currentChar] == CHAR_EQ)) {
      readChar(); // Đọc qua dấu '='
      return makeToken(SB_LE, ln, cn); // Trả về token '<=' (Less than or Equal)
    } else return makeToken(SB_LT, ln, cn); // Chỉ là '<' (Less Than)
    
  // Toán tử so sánh '>' và '>='
  case CHAR_GT: // Gặp '>'
    ln = lineNo; // Lưu vị trí bắt đầu token
    cn = colNo;
    readChar(); // Đọc ký tự tiếp theo
    // Kiểm tra xem có phải là '>=' không
    if ((currentChar != EOF) && (charCodes[currentChar] == CHAR_EQ)) {
      readChar(); // Đọc qua dấu '='
      return makeToken(SB_GE, ln, cn); // Trả về token '>=' (Greater than or Equal)
    } else return makeToken(SB_GT, ln, cn); // Chỉ là '>' (Greater Than)
    
  case CHAR_EQ:  // Dấu '=' (phép so sánh bằng)
    token = makeToken(SB_EQ, lineNo, colNo);
    readChar(); 
    return token;
    
  // Toán tử '!=' (không bằng)
  case CHAR_EXCLAIMATION: // Gặp '!'
    ln = lineNo; // Lưu vị trí bắt đầu
    cn = colNo;
    readChar(); // Đọc ký tự tiếp theo
    // Dấu '!' PHẢI theo sau bởi '=' để tạo thành '!='
    if ((currentChar != EOF) && (charCodes[currentChar] == CHAR_EQ)) {
      readChar(); // Đọc qua dấu '='
      return makeToken(SB_NEQ, ln, cn); // Trả về token '!=' (Not Equal)
    } else {
      // Lỗi: dấu '!' đứng một mình không hợp lệ trong KPL
      token = makeToken(TK_NONE, ln, cn);
      error(ERR_INVALID_SYMBOL, ln, cn);
      return token;
    }
    
  // Các dấu phân cách và ký hiệu đặc biệt
  case CHAR_COMMA: // Dấu ','
    token = makeToken(SB_COMMA, lineNo, colNo);
    readChar(); 
    return token;
    
  case CHAR_PERIOD: // Dấu '.' hoặc '.)'
    ln = lineNo;
    cn = colNo;
    readChar();
    // Kiểm tra xem có phải là '.)', dấu đóng của selector không
    if ((currentChar != EOF) && (charCodes[currentChar] == CHAR_RPAR)) {
      readChar();
      return makeToken(SB_RSEL, ln, cn); // Trả về '.)' đóng của array selector
    } else return makeToken(SB_PERIOD, ln, cn); // Chỉ là dấu '.'
    
  case CHAR_SEMICOLON: // Dấu ';'
    token = makeToken(SB_SEMICOLON, lineNo, colNo);
    readChar(); 
    return token;
    
  case CHAR_COLON: // Dấu ':' hoặc ':=' (phép gán)
    ln = lineNo;
    cn = colNo;
    readChar();
    // Kiểm tra xem có phải là ':=' (phép gán) không
    if ((currentChar != EOF) && (charCodes[currentChar] == CHAR_EQ)) {
      readChar();
      return makeToken(SB_ASSIGN, ln, cn); // Trả về token ':=' (Assignment)
    } else return makeToken(SB_COLON, ln, cn); // Chỉ là dấu ':'
    
  case CHAR_SINGLEQUOTE: // Dấu nháy đơn -> hằng ký tự
    return readConstChar();

  // case CHAR_DOUBLEQUOTE:
  //   return readString();
  
  // Dấu ngoặc '(' - có thể là:
  //   - Ngoặc mở thông thường: '('
  //   - Mở array selector: '(.'
  //   - Bắt đầu comment: '(*'
  case CHAR_LPAR: // Gặp '('
    ln = lineNo; // Lưu vị trí bắt đầu
    cn = colNo;
    readChar(); // Đọc ký tự tiếp theo

    if (currentChar == EOF) 
      return makeToken(SB_LPAR, ln, cn); // Chỉ là '(' nếu hết file

    // Kiểm tra ký tự tiếp theo để phân biệt '(', '(.' hay '(*'
    switch (charCodes[currentChar]) {
    case CHAR_PERIOD: // Gặp '(.' - mở array selector
      readChar();
      return makeToken(SB_LSEL, ln, cn); // Trả về token '(.' mở của array selector
      
    case CHAR_TIMES:  // Gặp '(*' - bắt đầu comment
      readChar();
      skipComment(); // Bỏ qua toàn bộ comment cho đến '*)'
      return getToken(); // Gọi đệ quy để lấy token tiếp theo (sau comment)
      
    default: // Các trường hợp khác: chỉ là '(' thông thường
      return makeToken(SB_LPAR, ln, cn);
    }
    
  case CHAR_RPAR: // Dấu ngoặc đóng ')'
    token = makeToken(SB_RPAR, lineNo, colNo);
    readChar(); 
    return token;
    
  default: // Ký tự không hợp lệ - không thuộc bất kỳ loại nào
    token = makeToken(TK_NONE, lineNo, colNo);
    error(ERR_INVALID_SYMBOL, lineNo, colNo); // Báo lỗi ký hiệu không hợp lệ
    readChar(); // Bỏ qua ký tự lỗi và tiếp tục
    return token;
  }
}

Token* getValidToken(void) {
  // Hàm này đảm bảo luôn trả về một token HỢP LỆ (không phải TK_NONE)
  // Nếu gặp token lỗi (TK_NONE), nó sẽ bỏ qua và tiếp tục lấy token tiếp theo
  Token *token = getToken(); // Lấy token đầu tiên
  
  // Lặp cho đến khi tìm được token hợp lệ
  while (token->tokenType == TK_NONE) {
    free(token);         // Giải phóng bộ nhớ của token lỗi
    token = getToken();  // Lấy token tiếp theo
  }
  
  return token; // Trả về token hợp lệ
}


/******************************************************************/

/**
 * @brief Hàm in thông tin chi tiết của một token ra màn hình
 * 
 * Hàm này được sử dụng để debug và kiểm tra kết quả phân tích từ vựng.
 * Nó in ra vị trí (dòng-cột) và loại của token.
 * 
 * @param token Con trỏ đến token cần in
 */
void printToken(Token *token) {

  // In vị trí của token (số dòng - số cột)
  // In vị trí của token (số dòng - số cột)
  printf("%d-%d:", token->lineNo, token->colNo);

  // In loại token và giá trị (nếu có)
  switch (token->tokenType) {
  // Token đặc biệt
  case TK_NONE: printf("TK_NONE\n"); break;  // Token lỗi
  case TK_IDENT: printf("TK_IDENT(%s)\n", token->string); break;  // Định danh
  case TK_NUMBER: printf("TK_NUMBER(%s)\n", token->string); break; // Số
  case TK_CHAR: printf("TK_CHAR(\'%s\')\n", token->string); break; // Ký tự
  case TK_EOF: printf("TK_EOF\n"); break;  // Kết thúc file

  // Các từ khóa (Keywords)
  case KW_PROGRAM: printf("KW_PROGRAM\n"); break;
  case KW_CONST: printf("KW_CONST\n"); break;
  case KW_TYPE: printf("KW_TYPE\n"); break;
  case KW_VAR: printf("KW_VAR\n"); break;
  case KW_INTEGER: printf("KW_INTEGER\n"); break;
  case KW_CHAR: printf("KW_CHAR\n"); break;
  case KW_ARRAY: printf("KW_ARRAY\n"); break;
  case KW_OF: printf("KW_OF\n"); break;
  case KW_FUNCTION: printf("KW_FUNCTION\n"); break;
  case KW_PROCEDURE: printf("KW_PROCEDURE\n"); break;
  case KW_BEGIN: printf("KW_BEGIN\n"); break;
  case KW_END: printf("KW_END\n"); break;
  case KW_CALL: printf("KW_CALL\n"); break;
  case KW_IF: printf("KW_IF\n"); break;
  case KW_THEN: printf("KW_THEN\n"); break;
  case KW_ELSE: printf("KW_ELSE\n"); break;
  case KW_WHILE: printf("KW_WHILE\n"); break;
  case KW_DO: printf("KW_DO\n"); break;
  case KW_FOR: printf("KW_FOR\n"); break;
  case KW_TO: printf("KW_TO\n"); break;

  // Các ký hiệu đặc biệt và toán tử
  case SB_SEMICOLON: printf("SB_SEMICOLON\n"); break;  // ;
  case SB_COLON: printf("SB_COLON\n"); break;          // :
  case SB_PERIOD: printf("SB_PERIOD\n"); break;        // .
  case SB_COMMA: printf("SB_COMMA\n"); break;          // ,
  case SB_ASSIGN: printf("SB_ASSIGN\n"); break;        // :=
  case SB_EQ: printf("SB_EQ\n"); break;                // =
  case SB_NEQ: printf("SB_NEQ\n"); break;              // !=
  case SB_LT: printf("SB_LT\n"); break;                // <
  case SB_LE: printf("SB_LE\n"); break;                // <=
  case SB_GT: printf("SB_GT\n"); break;                // >
  case SB_GE: printf("SB_GE\n"); break;                // >=
  case SB_PLUS: printf("SB_PLUS\n"); break;            // +
  case SB_MINUS: printf("SB_MINUS\n"); break;          // -
  case SB_TIMES: printf("SB_TIMES\n"); break;          // *
  case SB_SLASH: printf("SB_SLASH\n"); break;          // /
  case SB_LPAR: printf("SB_LPAR\n"); break;            // (
  case SB_RPAR: printf("SB_RPAR\n"); break;            // )
  case SB_LSEL: printf("SB_LSEL\n"); break;            // (.
  case SB_RSEL: printf("SB_RSEL\n"); break;            // .)
  }
}

