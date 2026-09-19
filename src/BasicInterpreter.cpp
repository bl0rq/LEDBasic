#include "BasicInterpreter.h"
#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif
#include <cmath>
#include <algorithm>
#include <ctype.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#ifndef M_E
#define M_E 2.71828182845904523536
#endif

// Platform-portable random number generation.
// LEDBASIC_RANDOM_MAX is the inclusive maximum ledbasic_random() can return.
#ifdef ESP32
  #include <esp_random.h>
  static inline uint32_t ledbasic_random() { return esp_random(); }
  static const float LEDBASIC_RANDOM_MAX = 4294967295.0f;
#else
  static const long LEDBASIC_RANDOM_EXCLUSIVE = 0x7FFFFFFF;
  static inline uint32_t ledbasic_random() {
      return (uint32_t)random(0, LEDBASIC_RANDOM_EXCLUSIVE);
  }
  static const float LEDBASIC_RANDOM_MAX = (float)(LEDBASIC_RANDOM_EXCLUSIVE - 1);
#endif

static inline bool isSpaceChar(char c) { return isspace((unsigned char)c) != 0; }
static inline bool isDigitChar(char c) { return isdigit((unsigned char)c) != 0; }
static inline bool isAlphaChar(char c) { return isalpha((unsigned char)c) != 0; }
static inline bool isAlnumChar(char c) { return isalnum((unsigned char)c) != 0; }

static int wrapHue360(int h) {
    h %= 360;
    if (h < 0) h += 360;
    return h;
}

static CRGB hsvToCRGB(int h, int s, int v) {
    h = wrapHue360(h);
    s = constrain(s, 0, 255);
    v = constrain(v, 0, 255);
    return CRGB(CHSV((uint8_t)(h * 255 / 360), (uint8_t)s, (uint8_t)v));
}

static CRGB wheelToCRGB(int pos) {
    pos %= 256;
    if (pos < 0) pos += 256;
    if (pos < 85) {
        return CRGB(pos * 3, 255 - pos * 3, 0);
    } else if (pos < 170) {
        pos -= 85;
        return CRGB(255 - pos * 3, 0, pos * 3);
    }
    pos -= 170;
    return CRGB(0, pos * 3, 255 - pos * 3);
}

// =============================================================================
// BasicLexer Implementation
// =============================================================================

BasicLexer::BasicLexer(const String& src)
    : source(src), position(0), line(1), column(1), hadError(false) {
    initKeywords();
}

void BasicLexer::initKeywords() {
    keywords["setup"] = TOK_SETUP;
    keywords["loop"] = TOK_LOOP;
    keywords["if"] = TOK_IF;
    keywords["else"] = TOK_ELSE;
    keywords["while"] = TOK_WHILE;
    keywords["for"] = TOK_FOR;
    keywords["to"] = TOK_TO;
    keywords["step"] = TOK_STEP;
    keywords["next"] = TOK_NEXT;
    keywords["function"] = TOK_FUNCTION;
    keywords["return"] = TOK_RETURN;
    keywords["end"] = TOK_END;
    keywords["dim"] = TOK_DIM;
    keywords["param"] = TOK_PARAM;
    keywords["and"] = TOK_AND;
    keywords["or"] = TOK_OR;
    keywords["not"] = TOK_NOT;
    keywords["true"] = TOK_TRUE;
    keywords["false"] = TOK_FALSE;
    
    // Math functions
    keywords["sin"] = TOK_SIN;
    keywords["cos"] = TOK_COS;
    keywords["tan"] = TOK_TAN;
    keywords["sqrt"] = TOK_SQRT;
    keywords["pow"] = TOK_POW;
    keywords["log"] = TOK_LOG;
    keywords["ln"] = TOK_LN;
    keywords["abs"] = TOK_ABS;
    keywords["floor"] = TOK_FLOOR;
    keywords["ceil"] = TOK_CEIL;
    keywords["round"] = TOK_ROUND;
    keywords["min"] = TOK_MIN;
    keywords["max"] = TOK_MAX;
    keywords["random"] = TOK_RANDOM;
    keywords["map"] = TOK_MAP;
    keywords["millis"] = TOK_MILLIS;
    keywords["delay"] = TOK_DELAY;
    keywords["hsv_to_rgb"] = TOK_HSV_TO_RGB;
    keywords["wheel"] = TOK_WHEEL;
    
    // LED functions
    keywords["setled"] = TOK_SETLED;
    keywords["setcolor"] = TOK_SETCOLOR;
    keywords["sethsv"] = TOK_SETHSV;
    keywords["show"] = TOK_SHOW;
    keywords["clear"] = TOK_CLEAR;
    keywords["fill"] = TOK_FILL;
    keywords["brightness"] = TOK_BRIGHTNESS;
    keywords["numled"] = TOK_NUMLED;
    keywords["hsv"] = TOK_HSV;
    keywords["rgb"] = TOK_RGB;
    keywords["get_led_r"] = TOK_GET_LED_R;
    keywords["get_led_g"] = TOK_GET_LED_G;
    keywords["get_led_b"] = TOK_GET_LED_B;
    keywords["set_led"] = TOK_SET_LED;
    keywords["set_all"] = TOK_SET_ALL;
    keywords["get_led_count"] = TOK_GET_LED_COUNT;
}

char BasicLexer::peek(int offset) {
    size_t pos = position + offset;
    if (pos >= source.length()) return '\0';
    return source[pos];
}

char BasicLexer::advance() {
    if (position >= source.length()) return '\0';
    char c = source[position++];
    if (c == '\n') {
        line++;
        column = 1;
    } else {
        column++;
    }
    return c;
}

void BasicLexer::skipWhitespace() {
    while (position < source.length() && isSpaceChar(peek()) && peek() != '\n') {
        advance();
    }
}

void BasicLexer::skipComment() {
    // Handle both // and # style comments
    if ((peek() == '/' && peek(1) == '/') || peek() == '#') {
        while (position < source.length() && peek() != '\n') {
            advance();
        }
    }
}

void BasicLexer::addDiagnostic(const String& message, size_t errLine, size_t errColumn) {
    hadError = true;
    diagnostics.push_back(Diagnostic(Diagnostic::Lex, (int)errLine, (int)errColumn, message));
    Serial.println("Lexer error at line " + String(errLine) + ", column " + String(errColumn) + ": " + message);
}

void BasicLexer::lexerError(const String& message, size_t errLine, size_t errColumn, const String& snippet) {
    String msg = message;
    if (snippet.length() > 0) {
        msg += " \"" + snippet + "\"";
    }
    addDiagnostic(msg, errLine, errColumn);
}

Token BasicLexer::lexerError(char unexpected) {
    size_t errLine = line;
    size_t errColumn = column > 0 ? column - 1 : 0;
    String msg = "Unexpected character '";
    msg += unexpected;
    msg += "'";
    addDiagnostic(msg, errLine, errColumn);
    return Token(TOK_INVALID, String(unexpected), 0, errLine, errColumn);
}

Token BasicLexer::readNumber() {
    size_t start = position;
    size_t startColumn = column;
    
    while (position < source.length() && (isDigitChar(peek()) || peek() == '.')) {
        advance();
    }

    String value = source.substring(start, position);
    return Token(TOK_NUMBER, value, value.toFloat(), line, startColumn);
}

Token BasicLexer::readString() {
    size_t startLine = line;
    size_t startColumn = column;
    advance(); // Skip opening quote

    String value = "";
    while (position < source.length() && peek() != '"') {
        if (peek() == '\\') {
            advance();
            char escaped = advance();
            switch (escaped) {
                case 'n': value += '\n'; break;
                case 't': value += '\t'; break;
                case 'r': value += '\r'; break;
                case '\\': value += '\\'; break;
                case '"': value += '"'; break;
                default: value += escaped; break;
            }
        } else {
            value += advance();
        }
    }

    if (position >= source.length() || peek() != '"') {
        String snippet = value.substring(0, 10);
        if (value.length() > 10) snippet += "...";
        lexerError("Unterminated string literal", startLine, startColumn, snippet);
        return Token(TOK_INVALID, value, 0, startLine, startColumn);
    }

    advance(); // Skip closing quote
    return Token(TOK_STRING, value, 0, startLine, startColumn);
}

Token BasicLexer::readIdentifier() {
    size_t start = position;
    size_t startColumn = column;
    
    while (position < source.length() && (isAlnumChar(peek()) || peek() == '_')) {
        advance();
    }
    
    String value = source.substring(start, position);
    TokenType type = TOK_IDENTIFIER;
    
    if (keywords.find(value) != keywords.end()) {
        type = keywords[value];
    }
    
    return Token(type, value, 0, line, startColumn);
}

Token BasicLexer::nextToken() {
    skipWhitespace();
    skipComment();
    
    if (position >= source.length()) {
        return Token(TOK_EOF, "", 0, line, column);
    }
    
    char c = peek();
    size_t currentColumn = column;
    
    if (isDigitChar(c)) {
        return readNumber();
    }

    if (c == '"') {
        return readString();
    }

    if (isAlphaChar(c) || c == '_') {
        return readIdentifier();
    }
    
    // Check for comments before processing individual characters
    if (c == '#' || (c == '/' && peek(1) == '/')) {
        skipComment();
        return nextToken(); // Recursively get the next token after the comment
    }
    
    advance();
    
    switch (c) {
        case '+': return Token(TOK_PLUS, "+", 0, line, currentColumn);
        case '-': return Token(TOK_MINUS, "-", 0, line, currentColumn);
        case '*': 
            if (peek() == '*') {
                advance();
                return Token(TOK_POWER, "**", 0, line, currentColumn);
            }
            return Token(TOK_MULTIPLY, "*", 0, line, currentColumn);
        case '/': return Token(TOK_DIVIDE, "/", 0, line, currentColumn);
        case '%': return Token(TOK_MODULO, "%", 0, line, currentColumn);
        case '=':
            if (peek() == '=') {
                advance();
                return Token(TOK_EQUALS, "==", 0, line, currentColumn);
            }
            return Token(TOK_ASSIGN, "=", 0, line, currentColumn);
        case '!':
            if (peek() == '=') {
                advance();
                return Token(TOK_NOT_EQUALS, "!=", 0, line, currentColumn);
            }
            return Token(TOK_NOT, "!", 0, line, currentColumn);
        case '<':
            if (peek() == '=') {
                advance();
                return Token(TOK_LESS_EQUAL, "<=", 0, line, currentColumn);
            }
            return Token(TOK_LESS_THAN, "<", 0, line, currentColumn);
        case '>':
            if (peek() == '=') {
                advance();
                return Token(TOK_GREATER_EQUAL, ">=", 0, line, currentColumn);
            }
            return Token(TOK_GREATER_THAN, ">", 0, line, currentColumn);
        case '(': return Token(TOK_LPAREN, "(", 0, line, currentColumn);
        case ')': return Token(TOK_RPAREN, ")", 0, line, currentColumn);
        case '[': return Token(TOK_LBRACKET, "[", 0, line, currentColumn);
        case ']': return Token(TOK_RBRACKET, "]", 0, line, currentColumn);
        case ',': return Token(TOK_COMMA, ",", 0, line, currentColumn);
        case ';': return Token(TOK_SEMICOLON, ";", 0, line, currentColumn);
        case '\n': return Token(TOK_NEWLINE, "\\n", 0, line, currentColumn);
        default: return lexerError(c);
    }
}

std::vector<Token> BasicLexer::tokenize() {
    std::vector<Token> tokens;
    Token token;
    
    do {
        token = nextToken();
        tokens.push_back(token);
    } while (token.type != TOK_EOF);
    
    return tokens;
}

// =============================================================================
// BasicParser Implementation
// =============================================================================

BasicParser::BasicParser(const std::vector<Token>& tokens)
    : tokens(tokens), current(0), hadError(false) {}

Token BasicParser::peek(int offset) {
    size_t pos = current + offset;
    if (pos >= tokens.size()) return Token(TOK_EOF);
    return tokens[pos];
}

Token BasicParser::advance() {
    if (current < tokens.size()) current++;
    return tokens[current - 1];
}

bool BasicParser::match(TokenType type) {
    if (check(type)) {
        advance();
        return true;
    }
    return false;
}

bool BasicParser::check(TokenType type) {
    return peek().type == type;
}

void BasicParser::error(const String& message) {
    hadError = true;
    Token t = peek();
    diagnostics.push_back(Diagnostic(Diagnostic::Parse, t.line, t.column, message));
    Serial.println("Parse error at line " + String(t.line) + ", column " + String(t.column) + ": " + message);
}

Token BasicParser::consume(TokenType type, const String& message) {
    if (check(type)) return advance();
    error(message);
    return Token(TOK_INVALID);
}

ASTNode* BasicParser::parseProgram() {
    ASTNode* program = new ASTNode(NODE_PROGRAM);
    
    while (!check(TOK_EOF)) {
        if (check(TOK_NEWLINE)) {
            advance();
            continue;
        }
        
        if (check(TOK_PARAM)) {
            program->addChild(parseParamDecl());
        } else if (check(TOK_SETUP)) {
            program->addChild(parseSetup());
        } else if (check(TOK_LOOP)) {
            program->addChild(parseLoop());
        } else {
            program->addChild(parseStatement());
        }
    }
    
    return program;
}

ASTNode* BasicParser::parseSetup() {
    Token setupToken = consume(TOK_SETUP, "Expected 'setup'");
    ASTNode* setup = new ASTNode(NODE_SETUP, setupToken);
    setup->addChild(parseBlock());
    return setup;
}

ASTNode* BasicParser::parseLoop() {
    Token loopToken = consume(TOK_LOOP, "Expected 'loop'");
    consume(TOK_LPAREN, "Expected '(' after 'loop'");
    
    ASTNode* loop = new ASTNode(NODE_LOOP, loopToken);
    
    // Parse the time parameter
    Token timeParam = consume(TOK_IDENTIFIER, "Expected time parameter");
    ASTNode* param = new ASTNode(NODE_IDENTIFIER, timeParam);
    param->name = timeParam.value;
    loop->addChild(param);
    
    consume(TOK_RPAREN, "Expected ')' after parameter");
    loop->addChild(parseBlock());
    
    return loop;
}

bool BasicParser::parseParamLiteral(ASTNode* paramDecl) {
    bool negative = match(TOK_MINUS);

    if (check(TOK_NUMBER)) {
        Token numToken = advance();
        ASTNode* numNode = new ASTNode(NODE_NUMBER, numToken);
        float v = numToken.numberValue;
        if (negative) v = -v;
        numNode->value = Value(v);
        paramDecl->addChild(numNode);
        return true;
    }

    if (negative) {
        error("Expected number after '-' in parameter attributes");
        return false;
    }

    if (match(TOK_TRUE)) {
        ASTNode* numNode = new ASTNode(NODE_NUMBER);
        numNode->value = Value(1.0f);
        paramDecl->addChild(numNode);
        return true;
    }

    if (match(TOK_FALSE)) {
        ASTNode* numNode = new ASTNode(NODE_NUMBER);
        numNode->value = Value(0.0f);
        paramDecl->addChild(numNode);
        return true;
    }

    if (check(TOK_STRING)) {
        Token strToken = advance();
        ASTNode* strNode = new ASTNode(NODE_STRING, strToken);
        strNode->value = Value(strToken.value);
        paramDecl->addChild(strNode);
        return true;
    }

    if (check(TOK_LBRACKET)) {
        advance();
        if (!check(TOK_RBRACKET)) {
            do {
                Token strToken = consume(TOK_STRING, "Expected string in enum array");
                ASTNode* strNode = new ASTNode(NODE_STRING, strToken);
                strNode->value = Value(strToken.value);
                paramDecl->addChild(strNode);
            } while (match(TOK_COMMA));
        }
        consume(TOK_RBRACKET, "Expected ']' after enum values");
        return true;
    }

    error("Unexpected token in parameter attributes");
    return false;
}

ASTNode* BasicParser::parseParamDecl() {
    consume(TOK_PARAM, "Expected 'param'");

    Token nameToken = consume(TOK_IDENTIFIER, "Expected parameter name");
    ASTNode* paramDecl = new ASTNode(NODE_PARAM_DECL, nameToken);
    paramDecl->name = nameToken.value;

    if (!match(TOK_IDENTIFIER)) {
        error("Expected parameter type (number, boolean, or enum)");
        return paramDecl;
    }

    Token typeToken = tokens[current - 1];
    String paramType = typeToken.value;
    if (paramType != "number" && paramType != "boolean" && paramType != "enum") {
        error("Unknown parameter type '" + paramType + "' (expected number, boolean, or enum)");
    }
    paramDecl->value = Value(paramType);

    if (match(TOK_LPAREN)) {
        if (!check(TOK_RPAREN)) {
            do {
                if (!parseParamLiteral(paramDecl)) {
                    break;
                }
            } while (match(TOK_COMMA));
        }
        consume(TOK_RPAREN, "Expected ')' after parameter attributes");
    }

    return paramDecl;
}

ASTNode* BasicParser::parseStatement() {
    if (check(TOK_DIM)) {
        return parseDimStatement();
    }
    
    if (check(TOK_IDENTIFIER) && peek(1).type == TOK_ASSIGN) {
        return parseAssignment();
    }
    
    if (check(TOK_IDENTIFIER) && peek(1).type == TOK_LBRACKET) {
        return parseArrayAssignment();
    }
    
    if (check(TOK_IF)) {
        return parseIf();
    }
    
    if (check(TOK_WHILE)) {
        return parseWhile();
    }
    
    if (check(TOK_FOR)) {
        return parseFor();
    }
    
    // Expression statement
    ASTNode* expr = parseExpression();
    if (check(TOK_NEWLINE) || check(TOK_SEMICOLON)) {
        advance();
    }
    return expr;
}

ASTNode* BasicParser::parseAssignment() {
    Token identifier = consume(TOK_IDENTIFIER, "Expected identifier");
    consume(TOK_ASSIGN, "Expected '='");
    
    ASTNode* assignment = new ASTNode(NODE_ASSIGNMENT, identifier);
    ASTNode* var = new ASTNode(NODE_IDENTIFIER, identifier);
    var->name = identifier.value;
    assignment->addChild(var);
    assignment->addChild(parseExpression());
    
    if (check(TOK_NEWLINE) || check(TOK_SEMICOLON)) {
        advance();
    }
    
    return assignment;
}

ASTNode* BasicParser::parseDimStatement() {
    consume(TOK_DIM, "Expected 'dim'");
    Token identifier = consume(TOK_IDENTIFIER, "Expected identifier");
    
    ASTNode* dimNode = new ASTNode(NODE_ARRAY_DECLARATION, identifier);
    ASTNode* var = new ASTNode(NODE_IDENTIFIER, identifier);
    var->name = identifier.value;
    dimNode->addChild(var);
    
    // Check if it's an array declaration: DIM array_name(size)
    if (match(TOK_LPAREN)) {
        ASTNode* sizeExpr = parseExpression();
        dimNode->addChild(sizeExpr);
        consume(TOK_RPAREN, "Expected ')' after array size");
    } else {
        // Regular variable declaration - size of 1 (scalar)
        ASTNode* sizeNode = new ASTNode(NODE_NUMBER);
        sizeNode->value = Value(1.0);
        dimNode->addChild(sizeNode);
    }
    
    if (check(TOK_NEWLINE) || check(TOK_SEMICOLON)) {
        advance();
    }
    
    return dimNode;
}

ASTNode* BasicParser::parseArrayAssignment() {
    Token identifier = consume(TOK_IDENTIFIER, "Expected identifier");
    consume(TOK_LBRACKET, "Expected '['");
    
    ASTNode* arrayAssign = new ASTNode(NODE_ARRAY_ASSIGNMENT, identifier);
    ASTNode* var = new ASTNode(NODE_IDENTIFIER, identifier);
    var->name = identifier.value;
    arrayAssign->addChild(var);
    arrayAssign->addChild(parseExpression()); // index
    
    consume(TOK_RBRACKET, "Expected ']'");
    consume(TOK_ASSIGN, "Expected '='");
    arrayAssign->addChild(parseExpression()); // value
    
    if (check(TOK_NEWLINE) || check(TOK_SEMICOLON)) {
        advance();
    }
    
    return arrayAssign;
}

ASTNode* BasicParser::parseIf() {
    Token ifToken = consume(TOK_IF, "Expected 'if'");
    
    ASTNode* ifNode = new ASTNode(NODE_IF, ifToken);
    ifNode->addChild(parseExpression()); // condition
    ifNode->addChild(parseBlock(true, true)); // then block may end at else

    if (match(TOK_ELSE)) {
        ifNode->addChild(parseBlock(true, false));
    }
    
    return ifNode;
}

ASTNode* BasicParser::parseWhile() {
    Token whileToken = consume(TOK_WHILE, "Expected 'while'");
    
    ASTNode* whileNode = new ASTNode(NODE_WHILE, whileToken);
    whileNode->addChild(parseExpression()); // condition
    whileNode->addChild(parseBlock()); // body
    
    return whileNode;
}

ASTNode* BasicParser::parseFor() {
    Token forToken = consume(TOK_FOR, "Expected 'for'");
    
    ASTNode* forNode = new ASTNode(NODE_FOR, forToken);
    
    // Parse: for i = start to end [step stepvalue]
    Token var = consume(TOK_IDENTIFIER, "Expected variable name");
    consume(TOK_ASSIGN, "Expected '='");
    
    ASTNode* varNode = new ASTNode(NODE_IDENTIFIER, var);
    varNode->name = var.value;
    forNode->addChild(varNode); // variable
    forNode->addChild(parseExpression()); // start value
    
    consume(TOK_TO, "Expected 'to'");
    forNode->addChild(parseExpression()); // end value
    
    if (match(TOK_STEP)) {
        forNode->addChild(parseExpression()); // step value
    } else {
        // Default step of 1
        ASTNode* stepNode = new ASTNode(NODE_NUMBER);
        stepNode->value = Value(1.0);
        forNode->addChild(stepNode);
    }
    
    forNode->addChild(parseBlock(false)); // body terminated by next

    consume(TOK_NEXT, "Expected 'next'");
    
    return forNode;
}

ASTNode* BasicParser::parseBlock(bool requireEnd, bool allowElse) {
    ASTNode* block = new ASTNode(NODE_BLOCK);

    while (!check(TOK_EOF) && !check(TOK_END) && !check(TOK_NEXT)) {
        if (check(TOK_NEWLINE)) {
            advance();
            continue;
        }
        if (check(TOK_ELSE)) {
            if (allowElse) break;
            error("Unexpected 'else'");
            break;
        }
        block->addChild(parseStatement());
    }

    if (requireEnd) {
        if (!(allowElse && check(TOK_ELSE))) {
            consume(TOK_END, "Expected 'end'");
        }
    }

    return block;
}

ASTNode* BasicParser::parseExpression() {
    return parseLogicalOr();
}

ASTNode* BasicParser::parseLogicalOr() {
    ASTNode* expr = parseLogicalAnd();
    
    while (match(TOK_OR)) {
        Token op = tokens[current - 1];
        ASTNode* right = parseLogicalAnd();
        ASTNode* binary = new ASTNode(NODE_BINARY_OP, op);
        binary->addChild(expr);
        binary->addChild(right);
        expr = binary;
    }
    
    return expr;
}

ASTNode* BasicParser::parseLogicalAnd() {
    ASTNode* expr = parseEquality();
    
    while (match(TOK_AND)) {
        Token op = tokens[current - 1];
        ASTNode* right = parseEquality();
        ASTNode* binary = new ASTNode(NODE_BINARY_OP, op);
        binary->addChild(expr);
        binary->addChild(right);
        expr = binary;
    }
    
    return expr;
}

ASTNode* BasicParser::parseEquality() {
    ASTNode* expr = parseComparison();
    
    while (match(TOK_EQUALS) || match(TOK_NOT_EQUALS)) {
        Token op = tokens[current - 1];
        ASTNode* right = parseComparison();
        ASTNode* binary = new ASTNode(NODE_BINARY_OP, op);
        binary->addChild(expr);
        binary->addChild(right);
        expr = binary;
    }
    
    return expr;
}

ASTNode* BasicParser::parseComparison() {
    ASTNode* expr = parseTerm();
    
    while (match(TOK_GREATER_THAN) || match(TOK_GREATER_EQUAL) || 
           match(TOK_LESS_THAN) || match(TOK_LESS_EQUAL)) {
        Token op = tokens[current - 1];
        ASTNode* right = parseTerm();
        ASTNode* binary = new ASTNode(NODE_BINARY_OP, op);
        binary->addChild(expr);
        binary->addChild(right);
        expr = binary;
    }
    
    return expr;
}

ASTNode* BasicParser::parseTerm() {
    ASTNode* expr = parseFactor();
    
    while (match(TOK_MINUS) || match(TOK_PLUS)) {
        Token op = tokens[current - 1];
        ASTNode* right = parseFactor();
        ASTNode* binary = new ASTNode(NODE_BINARY_OP, op);
        binary->addChild(expr);
        binary->addChild(right);
        expr = binary;
    }
    
    return expr;
}

ASTNode* BasicParser::parseFactor() {
    ASTNode* expr = parsePower();

    while (match(TOK_DIVIDE) || match(TOK_MULTIPLY) || match(TOK_MODULO)) {
        Token op = tokens[current - 1];
        ASTNode* right = parsePower();
        ASTNode* binary = new ASTNode(NODE_BINARY_OP, op);
        binary->addChild(expr);
        binary->addChild(right);
        expr = binary;
    }

    return expr;
}

ASTNode* BasicParser::parsePower() {
    ASTNode* expr = parseUnary();

    if (match(TOK_POWER)) {
        Token op = tokens[current - 1];
        ASTNode* right = parsePower();
        ASTNode* binary = new ASTNode(NODE_BINARY_OP, op);
        binary->addChild(expr);
        binary->addChild(right);
        return binary;
    }

    return expr;
}

ASTNode* BasicParser::parseUnary() {
    if (match(TOK_NOT) || match(TOK_MINUS)) {
        Token op = tokens[current - 1];
        ASTNode* right = parseUnary();
        ASTNode* unary = new ASTNode(NODE_UNARY_OP, op);
        unary->addChild(right);
        return unary;
    }
    
    return parsePrimary();
}

ASTNode* BasicParser::parsePrimary() {
    if (match(TOK_NUMBER)) {
        Token token = tokens[current - 1];
        ASTNode* number = new ASTNode(NODE_NUMBER, token);
        number->value = Value(token.numberValue);
        return number;
    }

    if (match(TOK_TRUE)) {
        ASTNode* number = new ASTNode(NODE_NUMBER);
        number->value = Value(1.0f);
        return number;
    }

    if (match(TOK_FALSE)) {
        ASTNode* number = new ASTNode(NODE_NUMBER);
        number->value = Value(0.0f);
        return number;
    }

    if (match(TOK_STRING)) {
        Token token = tokens[current - 1];
        ASTNode* string = new ASTNode(NODE_STRING, token);
        string->value = Value(token.value);
        return string;
    }
    
    if (check(TOK_IDENTIFIER)) {
        if (peek(1).type == TOK_LPAREN) {
            return parseFunctionCall();
        } else if (peek(1).type == TOK_LBRACKET) {
            // Array access: identifier[index]
            Token token = advance();
            consume(TOK_LBRACKET, "Expected '['");
            
            ASTNode* arrayAccess = new ASTNode(NODE_ARRAY_ACCESS, token);
            ASTNode* identifier = new ASTNode(NODE_IDENTIFIER, token);
            identifier->name = token.value;
            arrayAccess->addChild(identifier);
            arrayAccess->addChild(parseExpression()); // index
            
            consume(TOK_RBRACKET, "Expected ']'");
            return arrayAccess;
        } else {
            Token token = advance();
            ASTNode* identifier = new ASTNode(NODE_IDENTIFIER, token);
            identifier->name = token.value;
            return identifier;
        }
    }
    
    if (match(TOK_LPAREN)) {
        ASTNode* expr = parseExpression();
        consume(TOK_RPAREN, "Expected ')' after expression");
        return expr;
    }
    
    // Math and LED functions
    if (check(TOK_SIN) || check(TOK_COS) || check(TOK_TAN) || check(TOK_SQRT) ||
        check(TOK_POW) || check(TOK_LOG) || check(TOK_LN) || check(TOK_ABS) ||
        check(TOK_FLOOR) || check(TOK_CEIL) || check(TOK_ROUND) || check(TOK_MIN) ||
        check(TOK_MAX) || check(TOK_RANDOM) || check(TOK_MAP) || check(TOK_MILLIS) ||
        check(TOK_DELAY) || check(TOK_HSV_TO_RGB) || check(TOK_WHEEL) || check(TOK_SETLED) || check(TOK_SETCOLOR) || 
        check(TOK_SETHSV) || check(TOK_SHOW) || check(TOK_CLEAR) || check(TOK_FILL) || 
        check(TOK_BRIGHTNESS) || check(TOK_NUMLED) || check(TOK_HSV) || check(TOK_RGB) ||
        check(TOK_GET_LED_R) || check(TOK_GET_LED_G) || check(TOK_GET_LED_B) ||
        check(TOK_SET_LED) || check(TOK_SET_ALL) || check(TOK_GET_LED_COUNT)) {
        return parseFunctionCall();
    }
    
    error("Unexpected token in primary at " + String((int)current));
    Token token = peek();
    int offset = 1;
    constexpr int MAX_TOKEN_LOOKAHEAD = 20;
    while (token.type != TOK_NEWLINE && token.type != TOK_EOF && offset < MAX_TOKEN_LOOKAHEAD) {
        Serial.print(token.value);
        token = peek(offset);
        offset++;
    }
    Serial.println();
    if (!check(TOK_EOF)) {
        advance();
    }
    return new ASTNode(NODE_NUMBER);
}

ASTNode* BasicParser::parseFunctionCall() {
    Token funcToken = advance();
    
    ASTNode* funcCall = new ASTNode(NODE_FUNCTION_CALL, funcToken);
    funcCall->name = funcToken.value;
    
    consume(TOK_LPAREN, "Expected '(' after function name");
    
    if (!check(TOK_RPAREN)) {
        do {
            funcCall->addChild(parseExpression());
        } while (match(TOK_COMMA));
    }
    
    consume(TOK_RPAREN, "Expected ')' after function arguments");
    
    return funcCall;
}

ASTNode* BasicParser::parse() {
    return parseProgram();
}

ASTNode* BasicParser::parseSnippet() {
    while (check(TOK_NEWLINE)) {
        advance();
    }
    if (check(TOK_EOF)) {
        error("Empty snippet");
        return nullptr;
    }

    ASTNode* node = nullptr;
    if (check(TOK_IDENTIFIER) && peek(1).type == TOK_ASSIGN) {
        node = parseAssignment();
    } else if (check(TOK_IDENTIFIER) && peek(1).type == TOK_LBRACKET) {
        node = parseArrayAssignment();
    } else if (check(TOK_DIM)) {
        node = parseDimStatement();
    } else {
        node = parseExpression();
        if (check(TOK_NEWLINE) || check(TOK_SEMICOLON)) {
            advance();
        }
    }

    while (check(TOK_NEWLINE)) {
        advance();
    }
    if (!check(TOK_EOF) && !hadError) {
        error("Unexpected input after snippet");
    }
    return node;
}

// =============================================================================
// BasicInterpreter Implementation
// =============================================================================

static unsigned long ledbasicDefaultMillis(void*) { return millis(); }
static void ledbasicDefaultDelay(int ms, void*) { delay(ms); }

static bool isDebuggableStatement(NodeType t) {
    switch (t) {
        case NODE_ASSIGNMENT:
        case NODE_IF:
        case NODE_WHILE:
        case NODE_FOR:
        case NODE_FUNCTION_CALL:
        case NODE_ARRAY_DECLARATION:
        case NODE_ARRAY_ASSIGNMENT:
        case NODE_RETURN:
            return true;
        default:
            return false;
    }
}

BasicInterpreter::BasicInterpreter(CRGB* ledArray, int ledCount)
    : leds(ledArray), numLeds(ledCount), showCalled(false), ownsPhysicalOutput(true),
      autoShow(true), outputBrightness(255), setupNode(nullptr), loopNode(nullptr),
      abortExecution(false), currentLine(0), currentColumn(0), statementDepth(0),
      millisFn(ledbasicDefaultMillis), delayFn(ledbasicDefaultDelay), clockUser(nullptr),
      debugHook(nullptr), debugHookUser(nullptr) {
    reset();
}

int BasicInterpreter::internName(const String& name) {
    auto it = nameToSlot.find(name);
    if (it != nameToSlot.end()) return it->second;
    int id = (int)slots.size();
    nameToSlot[name] = id;
    slots.push_back(Value(0.0f));
    return id;
}

int BasicInterpreter::ensureSlot(ASTNode* node) {
    if (!node) return internName("");
    if (node->slot < 0) {
        node->slot = internName(node->name);
    }
    return node->slot;
}

void BasicInterpreter::reset() {
    setupNode = nullptr;
    loopNode = nullptr;
    showCalled = false;
    outputBrightness = 255;
    abortExecution = false;
    currentLine = 0;
    currentColumn = 0;
    statementDepth = 0;
    diagnostics.clear();
    parameters.clear();
    nameToSlot.clear();
    slots.clear();
    internName("PI");
    internName("E");
    slots[nameToSlot["PI"]] = Value((float)M_PI);
    slots[nameToSlot["E"]] = Value((float)M_E);
}

unsigned long BasicInterpreter::hostMillis() const {
    return millisFn ? millisFn(clockUser) : millis();
}

void BasicInterpreter::hostDelay(int ms) {
    if (delayFn) delayFn(ms, clockUser);
    else delay(ms);
}

void BasicInterpreter::runtimeError(ASTNode* node, const String& message) {
    int line = node ? node->token.line : currentLine;
    int col = node ? node->token.column : currentColumn;
    diagnostics.push_back(Diagnostic(Diagnostic::Runtime, line, col, message));
    Serial.println("Runtime error: " + message);
    abortExecution = true;
}

void BasicInterpreter::hitStatement(ASTNode* node) {
    if (!node) return;
    currentLine = node->token.line;
    currentColumn = node->token.column;
    if (debugHook) {
        debugHook(currentLine, currentColumn, statementDepth, debugHookUser);
    }
}

void BasicInterpreter::setClock(LedBasicMillisFn m, LedBasicDelayFn d, void* user) {
    millisFn = m ? m : ledbasicDefaultMillis;
    delayFn = d ? d : ledbasicDefaultDelay;
    clockUser = user;
}

void BasicInterpreter::setDebugHook(LedBasicDebugHook hook, void* user) {
    debugHook = hook;
    debugHookUser = user;
}

void BasicInterpreter::run(ASTNode* program) {
    setupNode = nullptr;
    loopNode = nullptr;
    if (!program || program->type != NODE_PROGRAM) return;

    for (ASTNode* child : program->children) {
        if (child->type == NODE_SETUP) {
            setupNode = child;
        } else if (child->type == NODE_LOOP) {
            loopNode = child;
        } else if (child->type == NODE_PARAM_DECL) {
            processParameterDeclaration(child);
        } else {
            execute(child);
        }
    }
}

void BasicInterpreter::processParameterDeclaration(ASTNode* node) {
    if (!node || node->type != NODE_PARAM_DECL) return;
    
    String paramName = node->name;
    String paramType = node->value.stringValue; // Type stored as string value
    
    Parameter param;
    param.name = paramName;
    
    if (paramType == "boolean") {
        param.type = PARAM_BOOLEAN;
        param.defaultValue = Value(node->children.size() > 0 ? (node->children[0]->value.asNumber() != 0) : 0.0f);
        param.currentValue = param.defaultValue;
        param.minValue = 0;
        param.maxValue = 1;
        param.stepValue = 1;
    } else if (paramType == "number") {
        param.type = PARAM_NUMBER;
        param.defaultValue = Value(node->children.size() > 0 ? node->children[0]->value.asNumber() : 0.0f);
        param.currentValue = param.defaultValue;
        param.minValue = node->children.size() > 1 ? node->children[1]->value.asNumber() : 0.0f;
        param.maxValue = node->children.size() > 2 ? node->children[2]->value.asNumber() : 100.0f;
        param.stepValue = node->children.size() > 3 ? node->children[3]->value.asNumber() : 1.0f;
    } else if (paramType == "enum") {
        param.type = PARAM_ENUM;
        param.enumValues.clear();
        for (ASTNode* child : node->children) {
            if (child->value.type == VAL_STRING) {
                param.enumValues.push_back(child->value.stringValue);
            }
        }
        param.defaultValue = Value(0.0f);
        param.currentValue = param.defaultValue;
        param.minValue = 0;
        param.maxValue = param.enumValues.empty() ? 0.0f : (float)(param.enumValues.size() - 1);
        param.stepValue = 1;
    } else {
        return;
    }

    addParameter(param);
}

void BasicInterpreter::runSetup() {
    abortExecution = false;
    statementDepth = 0;
    if (setupNode && setupNode->children.size() > 0) {
        execute(setupNode->children[0]); // Execute the block
    }
}

void BasicInterpreter::runLoop(unsigned long timeMs) {
    abortExecution = false;
    statementDepth = 0;
    if (loopNode && loopNode->children.size() >= 2) {
        if (loopNode->children[0]->type == NODE_IDENTIFIER) {
            int slot = ensureSlot(loopNode->children[0]);
            slots[slot] = Value((float)timeMs);
        }

        showCalled = false;
        execute(loopNode->children[1]);

        if (!showCalled && autoShow && !abortExecution) {
            FastLED.show();
        }
    }
}

Value BasicInterpreter::evaluate(ASTNode* node) {
    if (!node) return Value(0);

    switch (node->type) {
        case NODE_NUMBER:
        case NODE_STRING:
            return node->value;

        case NODE_IDENTIFIER: {
            int slot = ensureSlot(node);
            return slots[slot];
        }

        case NODE_BINARY_OP: {
            Value left = evaluate(node->children[0]);
            Value right = evaluate(node->children[1]);
            float lv = left.asNumber();
            float rv = right.asNumber();

            switch (node->token.type) {
                case TOK_PLUS:
                    if (left.type == VAL_STRING || right.type == VAL_STRING) {
                        auto toString = [](const Value& v) -> String {
                            if (v.type == VAL_STRING) return v.stringValue;
                            return String(v.asNumber());
                        };
                        return Value(toString(left) + toString(right));
                    }
                    return Value(lv + rv);
                case TOK_MINUS:
                    return Value(lv - rv);
                case TOK_MULTIPLY:
                    return Value(lv * rv);
                case TOK_DIVIDE:
                    if (rv != 0) return Value(lv / rv);
                    return Value(0);
                case TOK_MODULO:
                    if (rv != 0) return Value(fmodf(lv, rv));
                    return Value(0);
                case TOK_POWER:
                    return Value(powf(lv, rv));
                case TOK_EQUALS:
                case TOK_NOT_EQUALS: {
                    bool eq;
                    if (left.type == VAL_STRING && right.type == VAL_STRING) {
                        eq = left.stringValue == right.stringValue;
                    } else if (left.type == VAL_COLOR && right.type == VAL_COLOR) {
                        eq = left.colorValue == right.colorValue;
                    } else if (left.type == VAL_STRING || right.type == VAL_STRING ||
                               left.type == VAL_COLOR || right.type == VAL_COLOR) {
                        eq = false;
                    } else {
                        eq = lv == rv;
                    }
                    float result = eq ? 1.0f : 0.0f;
                    if (node->token.type == TOK_NOT_EQUALS) result = eq ? 0.0f : 1.0f;
                    return Value(result);
                }
                case TOK_LESS_THAN:
                    return Value(lv < rv ? 1.0f : 0.0f);
                case TOK_GREATER_THAN:
                    return Value(lv > rv ? 1.0f : 0.0f);
                case TOK_LESS_EQUAL:
                    return Value(lv <= rv ? 1.0f : 0.0f);
                case TOK_GREATER_EQUAL:
                    return Value(lv >= rv ? 1.0f : 0.0f);
                case TOK_AND:
                    return Value((lv != 0 && rv != 0) ? 1.0f : 0.0f);
                case TOK_OR:
                    return Value((lv != 0 || rv != 0) ? 1.0f : 0.0f);
                default:
                    return Value(0);
            }
        }

        case NODE_UNARY_OP: {
            Value operand = evaluate(node->children[0]);
            float ov = operand.asNumber();

            switch (node->token.type) {
                case TOK_MINUS:
                    return Value(-ov);
                case TOK_NOT:
                    return Value(ov == 0 ? 1.0f : 0.0f);
                default:
                    return operand;
            }
        }

        case NODE_FUNCTION_CALL: {
            std::vector<Value> args;
            args.reserve(node->children.size());
            for (size_t i = 0; i < node->children.size(); i++) {
                args.push_back(evaluate(node->children[i]));
            }
            return callFunction(node->token.type, args);
        }

        case NODE_ARRAY_ACCESS: {
            if (node->children.size() >= 2) {
                int slot = ensureSlot(node->children[0]);
                Value indexValue = evaluate(node->children[1]);
                int index = (int)indexValue.asNumber();

                if (slots[slot].type == VAL_ARRAY &&
                    index >= 0 && index < (int)slots[slot].arrayValue.size()) {
                    return slots[slot].arrayValue[index];
                }
            }
            return Value(0);
        }

        default:
            return Value(0);
    }
}

void BasicInterpreter::execute(ASTNode* node) {
    if (!node || abortExecution) return;

    const bool debugStmt = isDebuggableStatement(node->type);
    if (debugStmt) {
        statementDepth++;
        hitStatement(node);
        if (abortExecution) {
            statementDepth--;
            return;
        }
    }

    switch (node->type) {
        case NODE_BLOCK:
            for (size_t i = 0; i < node->children.size(); i++) {
                execute(node->children[i]);
                if (abortExecution) break;
            }
            break;
            
        case NODE_ASSIGNMENT: {
            if (node->children.size() >= 2) {
                int slot = ensureSlot(node->children[0]);
                slots[slot] = evaluate(node->children[1]);
            }
            break;
        }

        case NODE_IF: {
            if (node->children.size() >= 2) {
                Value condition = evaluate(node->children[0]);
                if (abortExecution) break;
                if (condition.asNumber() != 0) {
                    execute(node->children[1]);
                } else if (node->children.size() >= 3) {
                    execute(node->children[2]);
                }
            }
            break;
        }

        case NODE_WHILE: {
            if (node->children.size() >= 2) {
                int iterations = 0;
                while (!abortExecution) {
                    Value condition = evaluate(node->children[0]);
                    if (abortExecution || condition.asNumber() == 0) break;
                    if (++iterations > kMaxLoopIterations) {
                        runtimeError(node, "while loop exceeded iteration limit");
                        break;
                    }
                    execute(node->children[1]);
                }
            }
            break;
        }

        case NODE_FOR: {
            if (node->children.size() >= 5) {
                int slot = ensureSlot(node->children[0]);
                Value start = evaluate(node->children[1]);
                Value end = evaluate(node->children[2]);
                Value step = evaluate(node->children[3]);
                float stepN = step.asNumber();
                float endN = end.asNumber();

                if (stepN == 0) {
                    runtimeError(node, "for step cannot be 0");
                    break;
                }

                slots[slot] = start;
                int iterations = 0;
                while (!abortExecution) {
                    float current = slots[slot].asNumber();
                    if ((stepN > 0 && current > endN) || (stepN < 0 && current < endN)) {
                        break;
                    }
                    if (++iterations > kMaxLoopIterations) {
                        runtimeError(node, "for loop exceeded iteration limit");
                        break;
                    }
                    execute(node->children[4]);
                    if (abortExecution) break;
                    slots[slot] = Value(current + stepN);
                }
            }
            break;
        }

        case NODE_FUNCTION_CALL: {
            std::vector<Value> args;
            args.reserve(node->children.size());
            for (size_t i = 0; i < node->children.size(); i++) {
                args.push_back(evaluate(node->children[i]));
            }
            if (!abortExecution) {
                callFunction(node->token.type, args);
            }
            break;
        }

        case NODE_ARRAY_DECLARATION: {
            if (node->children.size() >= 2) {
                int slot = ensureSlot(node->children[0]);
                Value sizeValue = evaluate(node->children[1]);
                int size = (int)sizeValue.asNumber();
                if (size < 0) size = 0;
                if (size > kMaxArraySize) size = kMaxArraySize;
                std::vector<Value> arrayData(size, Value(0.0f));
                slots[slot] = Value(arrayData);
            }
            break;
        }

        case NODE_ARRAY_ASSIGNMENT: {
            if (node->children.size() >= 3) {
                int slot = ensureSlot(node->children[0]);
                Value indexValue = evaluate(node->children[1]);
                Value assignValue = evaluate(node->children[2]);
                int index = (int)indexValue.asNumber();

                if (slots[slot].type == VAL_ARRAY &&
                    index >= 0 && index < (int)slots[slot].arrayValue.size()) {
                    slots[slot].arrayValue[index] = assignValue;
                }
            }
            break;
        }
        
        default:
            evaluate(node); // For expression statements
            break;
    }

    if (debugStmt) {
        statementDepth--;
    }
}

Value BasicInterpreter::callFunction(TokenType func, const std::vector<Value>& args) {
    switch (func) {
        case TOK_SIN: case TOK_COS: case TOK_TAN: case TOK_SQRT: case TOK_POW:
        case TOK_LOG: case TOK_LN: case TOK_ABS: case TOK_FLOOR: case TOK_CEIL:
        case TOK_ROUND: case TOK_MIN: case TOK_MAX: case TOK_RANDOM: case TOK_MAP:
        case TOK_MILLIS: case TOK_DELAY: case TOK_HSV_TO_RGB: case TOK_WHEEL:
            return callMathFunction(func, args);
        case TOK_SETLED: case TOK_SETCOLOR: case TOK_SETHSV: case TOK_SHOW:
        case TOK_CLEAR: case TOK_FILL: case TOK_BRIGHTNESS: case TOK_NUMLED:
        case TOK_HSV: case TOK_RGB: case TOK_GET_LED_R: case TOK_GET_LED_G:
        case TOK_GET_LED_B: case TOK_SET_LED: case TOK_SET_ALL: case TOK_GET_LED_COUNT:
            return callLedFunction(func, args);
        default:
            return Value(0);
    }
}

Value BasicInterpreter::callMathFunction(TokenType func, const std::vector<Value>& args) {
    switch (func) {
        case TOK_SIN:
            if (args.size() >= 1) return Value(sinf(args[0].asNumber()));
            break;
        case TOK_COS:
            if (args.size() >= 1) return Value(cosf(args[0].asNumber()));
            break;
        case TOK_TAN:
            if (args.size() >= 1) return Value(tanf(args[0].asNumber()));
            break;
        case TOK_SQRT:
            if (args.size() >= 1) {
                float x = args[0].asNumber();
                if (x < 0) return Value(0);
                return Value(sqrtf(x));
            }
            break;
        case TOK_POW:
            if (args.size() >= 2) return Value(powf(args[0].asNumber(), args[1].asNumber()));
            break;
        case TOK_LOG:
            if (args.size() >= 1) {
                float x = args[0].asNumber();
                if (x <= 0) return Value(0);
                return Value(log10f(x));
            }
            break;
        case TOK_LN:
            if (args.size() >= 1) {
                float x = args[0].asNumber();
                if (x <= 0) return Value(0);
                return Value(logf(x));
            }
            break;
        case TOK_ABS:
            if (args.size() >= 1) return Value(fabsf(args[0].asNumber()));
            break;
        case TOK_FLOOR:
            if (args.size() >= 1) return Value(floorf(args[0].asNumber()));
            break;
        case TOK_CEIL:
            if (args.size() >= 1) return Value(ceilf(args[0].asNumber()));
            break;
        case TOK_ROUND:
            if (args.size() >= 1) return Value(roundf(args[0].asNumber()));
            break;
        case TOK_MIN:
            if (args.size() >= 2) return Value(min(args[0].asNumber(), args[1].asNumber()));
            break;
        case TOK_MAX:
            if (args.size() >= 2) return Value(max(args[0].asNumber(), args[1].asNumber()));
            break;
        case TOK_RANDOM:
            if (args.size() >= 1) {
                float maxVal = args[0].asNumber();
                if (maxVal <= 0) return Value(0);
                uint32_t span = (uint32_t)maxVal;
                if (span == 0) return Value(0);
                return Value((float)(ledbasic_random() % span));
            }
            return Value((float)ledbasic_random() / LEDBASIC_RANDOM_MAX);
        case TOK_MAP:
            if (args.size() >= 5) {
                float value = args[0].asNumber();
                float fromLow = args[1].asNumber();
                float fromHigh = args[2].asNumber();
                float toLow = args[3].asNumber();
                float toHigh = args[4].asNumber();
                float denom = fromHigh - fromLow;
                if (denom == 0) return Value(toLow);
                return Value((value - fromLow) * (toHigh - toLow) / denom + toLow);
            }
            break;
        case TOK_MILLIS:
            return Value((float)hostMillis());
        case TOK_DELAY:
            if (args.size() >= 1) {
                int delayMs = (int)args[0].asNumber();
                if (delayMs > 0) hostDelay(delayMs);
            }
            break;
        case TOK_HSV_TO_RGB: {
            if (args.size() >= 3) {
                int h = (int)args[0].asNumber();
                int s = (int)args[1].asNumber();
                int v = (int)args[2].asNumber();
                // Accept 0-100 saturation/value (prompt style) or 0-255 (sethsv style)
                if (s <= 100 && v <= 100) {
                    s = s * 255 / 100;
                    v = v * 255 / 100;
                }
                CRGB rgb = hsvToCRGB(h, s, v);
                return Value::color(rgb.r, rgb.g, rgb.b);
            }
            break;
        }
        case TOK_WHEEL: {
            if (args.size() >= 1) {
                CRGB color = wheelToCRGB((int)args[0].asNumber());
                return Value::color(color.r, color.g, color.b);
            }
            break;
        }
        default:
            break;
    }
    return Value(0);
}

void BasicInterpreter::applyLedColor(int index, const CRGB& color) {
    if (index >= 0 && index < numLeds) {
        leds[index] = color;
    }
}

Value BasicInterpreter::callLedFunction(TokenType func, const std::vector<Value>& args) {
    switch (func) {
        case TOK_SETLED:
        case TOK_SET_LED:
        case TOK_SETCOLOR: {
            if (args.size() >= 4) {
                int index = (int)args[0].asNumber();
                int r = constrain((int)args[1].asNumber(), 0, 255);
                int g = constrain((int)args[2].asNumber(), 0, 255);
                int b = constrain((int)args[3].asNumber(), 0, 255);
                applyLedColor(index, CRGB(r, g, b));
            } else if (args.size() >= 2) {
                int index = (int)args[0].asNumber();
                applyLedColor(index, args[1].asCRGB());
            }
            break;
        }

        case TOK_RGB: {
            if (args.size() >= 3) {
                int r = constrain((int)args[0].asNumber(), 0, 255);
                int g = constrain((int)args[1].asNumber(), 0, 255);
                int b = constrain((int)args[2].asNumber(), 0, 255);
                return Value::color((uint8_t)r, (uint8_t)g, (uint8_t)b);
            }
            break;
        }

        case TOK_HSV: {
            if (args.size() >= 3) {
                CRGB rgb = hsvToCRGB((int)args[0].asNumber(), (int)args[1].asNumber(), (int)args[2].asNumber());
                return Value::color(rgb.r, rgb.g, rgb.b);
            }
            break;
        }

        case TOK_SETHSV: {
            if (args.size() >= 4) {
                int index = (int)args[0].asNumber();
                applyLedColor(index, hsvToCRGB(
                    (int)args[1].asNumber(),
                    (int)args[2].asNumber(),
                    (int)args[3].asNumber()));
            }
            break;
        }

        case TOK_CLEAR: {
            for (int i = 0; i < numLeds; i++) {
                leds[i] = CRGB::Black;
            }
            break;
        }

        case TOK_FILL:
        case TOK_SET_ALL: {
            CRGB color = CRGB::Black;
            if (args.size() >= 3) {
                color = CRGB(
                    constrain((int)args[0].asNumber(), 0, 255),
                    constrain((int)args[1].asNumber(), 0, 255),
                    constrain((int)args[2].asNumber(), 0, 255));
            } else if (args.size() >= 1) {
                color = args[0].asCRGB();
            } else {
                break;
            }
            for (int i = 0; i < numLeds; i++) {
                leds[i] = color;
            }
            break;
        }

        case TOK_SHOW: {
            showCalled = true;
            if (autoShow) {
                FastLED.show();
            }
            break;
        }

        case TOK_BRIGHTNESS: {
            if (args.size() >= 1) {
                outputBrightness = (uint8_t)constrain((int)args[0].asNumber(), 0, 255);
                if (ownsPhysicalOutput) {
                    FastLED.setBrightness(outputBrightness);
                }
            }
            break;
        }

        case TOK_NUMLED:
        case TOK_GET_LED_COUNT:
            return Value(numLeds);

        case TOK_GET_LED_R: {
            if (args.size() >= 1) {
                int index = (int)args[0].asNumber();
                if (index >= 0 && index < numLeds) return Value(leds[index].r);
            }
            return Value(0);
        }

        case TOK_GET_LED_G: {
            if (args.size() >= 1) {
                int index = (int)args[0].asNumber();
                if (index >= 0 && index < numLeds) return Value(leds[index].g);
            }
            return Value(0);
        }

        case TOK_GET_LED_B: {
            if (args.size() >= 1) {
                int index = (int)args[0].asNumber();
                if (index >= 0 && index < numLeds) return Value(leds[index].b);
            }
            return Value(0);
        }

        default:
            break;
    }

    return Value(0);
}

void BasicInterpreter::setVariable(const String& name, const Value& value) {
    int slot = internName(name);
    slots[slot] = value;
}

Value BasicInterpreter::getVariable(const String& name) {
    auto it = nameToSlot.find(name);
    if (it != nameToSlot.end()) {
        return slots[it->second];
    }
    return Value(0);
}

std::vector<VariableBinding> BasicInterpreter::getAllVariables() const {
    std::vector<VariableBinding> out;
    out.reserve(nameToSlot.size());
    for (const auto& pair : nameToSlot) {
        VariableBinding b;
        b.name = pair.first;
        b.value = slots[pair.second];
        out.push_back(b);
    }
    return out;
}

bool BasicInterpreter::evalSnippet(const String& source, Value& result, Diagnostic& err) {
    err = Diagnostic();
    result = Value(0);

    LedBasicDebugHook savedHook = debugHook;
    void* savedUser = debugHookUser;
    debugHook = nullptr;
    debugHookUser = nullptr;
    abortExecution = false;

    BasicLexer lexer(source);
    std::vector<Token> tokens = lexer.tokenize();
    if (lexer.hasError()) {
        const std::vector<Diagnostic>& diags = lexer.getDiagnostics();
        err = diags.empty() ? Diagnostic(Diagnostic::Lex, 1, 1, "Lexer error") : diags[0];
        debugHook = savedHook;
        debugHookUser = savedUser;
        return false;
    }

    BasicParser parser(tokens);
    ASTNode* node = parser.parseSnippet();
    if (!node || parser.hasError()) {
        const std::vector<Diagnostic>& diags = parser.getDiagnostics();
        err = diags.empty() ? Diagnostic(Diagnostic::Parse, 1, 1, "Parse error") : diags[0];
        delete node;
        debugHook = savedHook;
        debugHookUser = savedUser;
        return false;
    }

    if (node->type == NODE_ASSIGNMENT || node->type == NODE_ARRAY_ASSIGNMENT ||
        node->type == NODE_ARRAY_DECLARATION) {
        execute(node);
        if (node->type == NODE_ASSIGNMENT && node->children.size() >= 1) {
            result = getVariable(node->children[0]->name);
        } else {
            result = Value(0);
        }
    } else {
        result = evaluate(node);
    }

    bool ok = !abortExecution;
    if (!ok && !diagnostics.empty()) {
        err = diagnostics.back();
    }
    delete node;
    debugHook = savedHook;
    debugHookUser = savedUser;
    return ok;
}

// =============================================================================
// Parameter Management Implementation
// =============================================================================

void BasicInterpreter::addParameter(const Parameter& param) {
    parameters[param.name] = param;
    int slot = internName(param.name);
    slots[slot] = param.currentValue;
}

void BasicInterpreter::setParameterValue(const String& name, const Value& value) {
    if (parameters.find(name) != parameters.end()) {
        parameters[name].setValue(value);
        int slot = internName(name);
        slots[slot] = parameters[name].currentValue;
    }
}

Parameter* BasicInterpreter::getParameter(const String& name) {
    if (parameters.find(name) != parameters.end()) {
        return &parameters[name];
    }
    return nullptr;
}

std::vector<Parameter> BasicInterpreter::getAllParameters() const {
    std::vector<Parameter> result;
    for (const auto& pair : parameters) {
        result.push_back(pair.second);
    }
    return result;
}

void BasicInterpreter::clearParameters() {
    for (const auto& pair : parameters) {
        auto it = nameToSlot.find(pair.first);
        if (it != nameToSlot.end()) {
            slots[it->second] = Value(0.0f);
        }
    }
    parameters.clear();
}

// =============================================================================
// BasicLEDController Implementation
// =============================================================================

BasicLEDController::BasicLEDController(CRGB* ledArray, int ledCount)
    : interpreter(nullptr), ast(nullptr), programLoaded(false) {
    interpreter = new BasicInterpreter(ledArray, ledCount);
}

BasicLEDController::~BasicLEDController() {
    delete interpreter;
    delete ast;
}

bool BasicLEDController::loadProgram(const String& source) {
    delete ast;
    ast = nullptr;
    programLoaded = false;
    loadDiagnostics.clear();
    interpreter->reset();

    BasicLexer lexer(source);
    std::vector<Token> tokens = lexer.tokenize();
    for (size_t i = 0; i < lexer.getDiagnostics().size(); i++) {
        loadDiagnostics.push_back(lexer.getDiagnostics()[i]);
    }

    BasicParser parser(tokens);
    ast = parser.parse();
    for (size_t i = 0; i < parser.getDiagnostics().size(); i++) {
        loadDiagnostics.push_back(parser.getDiagnostics()[i]);
    }

    if (lexer.hasError() || !ast || parser.hasError()) {
        delete ast;
        ast = nullptr;
        interpreter->reset();
        return false;
    }

    interpreter->run(ast);
    programLoaded = true;
    return true;
}

void BasicLEDController::runSetup() {
    if (programLoaded) {
        interpreter->runSetup();
    }
}

void BasicLEDController::runLoop(unsigned long timeMs) {
    if (programLoaded) {
        interpreter->runLoop(timeMs);
    }
}

void BasicLEDController::setVariable(const String& name, double value) {
    if (interpreter) {
        interpreter->setVariable(name, Value(value));
    }
}

void BasicLEDController::setVariable(const String& name, const String& value) {
    if (interpreter) {
        interpreter->setVariable(name, Value(value));
    }
}

double BasicLEDController::getNumberVariable(const String& name) {
    if (interpreter) {
        return interpreter->getVariable(name).numberValue;
    }
    return 0.0;
}

String BasicLEDController::getStringVariable(const String& name) {
    if (interpreter) {
        return interpreter->getVariable(name).stringValue;
    }
    return "";
}

std::vector<VariableBinding> BasicLEDController::getAllVariables() const {
    if (interpreter) {
        return interpreter->getAllVariables();
    }
    return std::vector<VariableBinding>();
}

bool BasicLEDController::evalSnippet(const String& source, Value& result, Diagnostic& err) {
    if (interpreter) {
        return interpreter->evalSnippet(source, result, err);
    }
    err = Diagnostic(Diagnostic::Runtime, 0, 0, "No interpreter");
    return false;
}

std::vector<Diagnostic> BasicLEDController::getDiagnostics() const {
    std::vector<Diagnostic> out = loadDiagnostics;
    if (interpreter) {
        const std::vector<Diagnostic>& runtime = interpreter->getDiagnostics();
        for (size_t i = 0; i < runtime.size(); i++) {
            out.push_back(runtime[i]);
        }
    }
    return out;
}

void BasicLEDController::setClock(LedBasicMillisFn millisFn, LedBasicDelayFn delayFn, void* user) {
    if (interpreter) {
        interpreter->setClock(millisFn, delayFn, user);
    }
}

void BasicLEDController::setDebugHook(LedBasicDebugHook hook, void* user) {
    if (interpreter) {
        interpreter->setDebugHook(hook, user);
    }
}

bool BasicLEDController::hasRuntimeError() const {
    return interpreter && interpreter->hasRuntimeError();
}

void BasicLEDController::interrupt() {
    if (interpreter) {
        interpreter->interrupt();
    }
}

int BasicLEDController::getCurrentLine() const {
    return interpreter ? interpreter->getCurrentLine() : 0;
}

int BasicLEDController::getCurrentColumn() const {
    return interpreter ? interpreter->getCurrentColumn() : 0;
}

int BasicLEDController::getStatementDepth() const {
    return interpreter ? interpreter->getStatementDepth() : 0;
}

CRGB* BasicLEDController::getLeds() {
    return interpreter ? interpreter->getLeds() : nullptr;
}

const CRGB* BasicLEDController::getLeds() const {
    return interpreter ? interpreter->getLeds() : nullptr;
}

int BasicLEDController::getNumLeds() const {
    return interpreter ? interpreter->getNumLeds() : 0;
}

// =============================================================================
// Parameter Management for BasicLEDController
// =============================================================================

void BasicLEDController::addParameter(const Parameter& param) {
    if (interpreter) {
        interpreter->addParameter(param);
    }
}

void BasicLEDController::setParameterValue(const String& name, const Value& value) {
    if (interpreter) {
        interpreter->setParameterValue(name, value);
    }
}

Parameter* BasicLEDController::getParameter(const String& name) {
    if (interpreter) {
        return interpreter->getParameter(name);
    }
    return nullptr;
}

std::vector<Parameter> BasicLEDController::getAllParameters() const {
    if (interpreter) {
        return interpreter->getAllParameters();
    }
    return std::vector<Parameter>();
}

void BasicLEDController::clearParameters() {
    if (interpreter) {
        interpreter->clearParameters();
    }
}

void BasicLEDController::setOwnsPhysicalOutput(bool owns) {
    if (interpreter) {
        interpreter->setOwnsPhysicalOutput(owns);
    }
}

void BasicLEDController::setAutoShow(bool enable) {
    if (interpreter) {
        interpreter->setAutoShow(enable);
    }
}

uint8_t BasicLEDController::getOutputBrightness() const {
    if (interpreter) {
        return interpreter->getOutputBrightness();
    }
    return 255;
}

// =============================================================================
// VirtualStrip Implementation
// =============================================================================

VirtualStrip::VirtualStrip(int stripIdx, int start, int len, int z, BlendMode blend, bool reverse)
    : startPos(start), length(len), zOrder(z), stripIndex(stripIdx), blendMode(blend), controller(nullptr), enabled(true), reversed(reverse) {
    virtualLeds = new CRGB[length];
    for (int i = 0; i < length; i++) {
        virtualLeds[i] = CRGB::Black;
    }
    controller = new BasicLEDController(virtualLeds, length);
    controller->setOwnsPhysicalOutput(false);
}

VirtualStrip::~VirtualStrip() {
    delete[] virtualLeds;
    delete controller;
}

bool VirtualStrip::loadProgram(const String& source) {
    if (controller) {
        programSource = source;
        return controller->loadProgram(source);
    }
    return false;
}

void VirtualStrip::runSetup() {
    if (controller && enabled) {
        controller->runSetup();
    }
}

void VirtualStrip::runLoop(unsigned long timeMs) {
    if (controller && enabled) {
        controller->runLoop(timeMs);
    }
}

uint8_t VirtualStrip::getOutputBrightness() const {
    return controller ? controller->getOutputBrightness() : 255;
}

void VirtualStrip::setRegion(int start, int len) {
    if (len != length) {
        // Need to recreate the virtual LED array
        delete[] virtualLeds;
        delete controller;
        
        length = len;
        virtualLeds = new CRGB[length];
        for (int i = 0; i < length; i++) {
            virtualLeds[i] = CRGB::Black;
        }
        controller = new BasicLEDController(virtualLeds, length);
        controller->setOwnsPhysicalOutput(false);

        if (programSource.length() > 0) {
            controller->loadProgram(programSource);
        }
    }
    startPos = start;
}

// =============================================================================
// VirtualStripManager Implementation
// =============================================================================

VirtualStripManager::VirtualStripManager() {}

VirtualStripManager::~VirtualStripManager() {
    removeAllStrips();
}

void VirtualStripManager::addPhysicalStrip(CRGB* leds, int length) {
    physicalStrips.push_back({leds, length});
    virtualOwned.push_back(0);
}

void VirtualStripManager::clearPhysicalStrip(int idx) {
    if (idx < 0 || idx >= (int)physicalStrips.size()) return;
    PhysicalStrip& phys = physicalStrips[idx];
    for (int j = 0; j < phys.length; j++) {
        phys.leds[j] = CRGB::Black;
    }
    if (idx < (int)virtualOwned.size()) {
        virtualOwned[idx] = 0;
    }
}

bool VirtualStripManager::hasEnabledLayerOn(int idx) const {
    for (VirtualStrip* strip : strips) {
        if (strip && strip->isEnabled() && strip->getStripIndex() == idx) {
            return true;
        }
    }
    return false;
}

VirtualStrip* VirtualStripManager::createStrip(int stripIndex, int start, int length, int zOrder, BlendMode blend, bool reverse) {
    if (stripIndex < 0 || stripIndex >= physicalStrips.size()) {
        printf("Error: Invalid stripIndex %d in createStrip (physicalStrips size: %d)\n", stripIndex, (int)physicalStrips.size());
        return nullptr;
    }

    PhysicalStrip& phys = physicalStrips[stripIndex];
    start = constrain(start, 0, phys.length - 1);
    length = constrain(length, 1, phys.length - start);

    VirtualStrip* strip = new VirtualStrip(stripIndex, start, length, zOrder, blend, reverse);
    strips.push_back(strip);
    sortStripsByZOrder();
    return strip;
}

void VirtualStripManager::removeStrip(VirtualStrip* strip) {
    if (!strip) return;
    int idx = strip->getStripIndex();
    for (auto it = strips.begin(); it != strips.end(); ++it) {
        if (*it == strip) {
            delete strip;
            strips.erase(it);
            break;
        }
    }
    if (!hasEnabledLayerOn(idx) &&
        idx >= 0 && idx < (int)virtualOwned.size() && virtualOwned[idx]) {
        clearPhysicalStrip(idx);
    }
}

void VirtualStripManager::removeAllStrips() {
    for (VirtualStrip* strip : strips) {
        delete strip;
    }
    strips.clear();
    for (size_t i = 0; i < physicalStrips.size(); i++) {
        if (i < virtualOwned.size() && virtualOwned[i]) {
            clearPhysicalStrip((int)i);
        }
    }
}

void VirtualStripManager::runAllSetups() {
    for (VirtualStrip* strip : strips) {
        strip->runSetup();
    }
}

void VirtualStripManager::runAllLoops(unsigned long timeMs) {
    for (VirtualStrip* strip : strips) {
        strip->runLoop(timeMs);
    }
}

void VirtualStripManager::renderToPhysical() {
    if (virtualOwned.size() < physicalStrips.size()) {
        virtualOwned.resize(physicalStrips.size(), 0);
    }

    std::vector<uint8_t> touched(physicalStrips.size(), 0);
    for (VirtualStrip* strip : strips) {
        if (!strip || !strip->isEnabled()) continue;
        int idx = strip->getStripIndex();
        if (idx >= 0 && idx < (int)physicalStrips.size()) {
            touched[idx] = 1;
        }
    }

    for (size_t i = 0; i < physicalStrips.size(); i++) {
        if (!touched[i] && !virtualOwned[i]) continue;
        PhysicalStrip& phys = physicalStrips[i];
        for (int j = 0; j < phys.length; j++) {
            phys.leds[j] = CRGB::Black;
        }
    }

    for (VirtualStrip* strip : strips) {
        if (!strip->isEnabled()) continue;

        int idx = strip->getStripIndex();
        if (idx < 0 || idx >= (int)physicalStrips.size()) continue;
        if (!touched[idx]) continue;

        PhysicalStrip& phys = physicalStrips[idx];
        int startPos = strip->getStartPos();
        int length = strip->getLength();
        CRGB* virtualLeds = strip->getVirtualLeds();
        BlendMode blend = strip->getBlendMode();
        bool reversed = strip->isReversed();
        uint8_t brightness = strip->getOutputBrightness();

        for (int i = 0; i < length; i++) {
            int virtualIndex = reversed ? (length - 1 - i) : i;
            int physicalPos = startPos + i;

            if (physicalPos >= 0 && physicalPos < phys.length) {
                CRGB src = virtualLeds[virtualIndex];
                if (brightness != 255) {
                    src.nscale8(brightness);
                }
                if (blend == BLEND_REPLACE) {
                    // Black is transparent so lower-Z layers show through
                    if (src != CRGB::Black) {
                        phys.leds[physicalPos] = src;
                    }
                } else {
                    blendPixel(phys.leds[physicalPos], src, blend);
                }
            }
        }
    }

    for (size_t i = 0; i < physicalStrips.size(); i++) {
        virtualOwned[i] = touched[i];
    }
}

void VirtualStripManager::blendPixel(CRGB& dest, const CRGB& src, BlendMode mode) {
    if (src == CRGB::Black && mode != BLEND_SUBTRACT && mode != BLEND_COLOR_SPACE) {
        return; // Skip black pixels for most blend modes
    }
    
    switch (mode) {
        case BLEND_ADD: {
            int r = dest.r + src.r;
            int g = dest.g + src.g;
            int b = dest.b + src.b;
            dest = CRGB(constrain(r, 0, 255), constrain(g, 0, 255), constrain(b, 0, 255));
            break;
        }
        
        case BLEND_SUBTRACT: {
            int r = dest.r - src.r;
            int g = dest.g - src.g;
            int b = dest.b - src.b;
            dest = CRGB(constrain(r, 0, 255), constrain(g, 0, 255), constrain(b, 0, 255));
            break;
        }
        
        case BLEND_MULTIPLY: {
            dest.r = (dest.r * src.r) / 255;
            dest.g = (dest.g * src.g) / 255;
            dest.b = (dest.b * src.b) / 255;
            break;
        }
        
        case BLEND_SCREEN: {
            dest.r = 255 - (((255 - dest.r) * (255 - src.r)) / 255);
            dest.g = 255 - (((255 - dest.g) * (255 - src.g)) / 255);
            dest.b = 255 - (((255 - dest.b) * (255 - src.b)) / 255);
            break;
        }
        
        case BLEND_COLOR_SPACE: {
            CHSV srcHSV = rgb2hsv_approximate(src);
            CHSV destHSV = rgb2hsv_approximate(dest);
            CHSV blendedHSV = CHSV((srcHSV.h + destHSV.h) / 2, (srcHSV.s + destHSV.s) / 2, (srcHSV.v + destHSV.v) / 2);

            hsv2rgb_rainbow(blendedHSV, dest);
            break;
        }
        
        case BLEND_REPLACE:
        default:
            if (src != CRGB::Black) {
                dest = src;
            }
            break;
    }
}

VirtualStrip* VirtualStripManager::getStrip(int index) {
    if (index >= 0 && index < strips.size()) {
        return strips[index];
    }
    return nullptr;
}

void VirtualStripManager::sortStripsByZOrder() {
    std::sort(strips.begin(), strips.end(), 
        [](const VirtualStrip* a, const VirtualStrip* b) {
            return a->getZOrder() < b->getZOrder();
        });
}
