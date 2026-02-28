#include "BasicInterpreter.h"
#include <cmath>
#include <algorithm>

// Platform-portable random number generation
#ifdef ESP32
  #include <esp_random.h>
  static inline uint32_t ledbasic_random() { return esp_random(); }
#else
  static inline uint32_t ledbasic_random() { return (uint32_t)random(0, 0x7FFFFFFF); }
#endif

// =============================================================================
// BasicLexer Implementation
// =============================================================================

BasicLexer::BasicLexer(const String& src) : source(src), position(0), line(1), column(1) {
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
    while (position < source.length() && isspace(peek()) && peek() != '\n') {
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

void BasicLexer::lexerError(const String& message, size_t errLine, size_t errColumn, const String& snippet) {
    String msg = "Lexer error at line " + String(errLine) + ", column " + String(errColumn) + ": " + message;
    if (snippet.length() > 0) {
        msg += " \"" + snippet + "\"";
    }
    Serial.println(msg);
}

Token BasicLexer::lexerError(char unexpected) {
    size_t errLine = line;
    size_t errColumn = column > 0 ? column - 1 : 0;
    Serial.printf("Unexpected character '%c' at line %u, column %u.\n", unexpected, static_cast<unsigned int>(errLine), static_cast<unsigned int>(errColumn));
    return Token(TOK_INVALID, String(unexpected), 0, errLine, errColumn);
}

Token BasicLexer::readNumber() {
    size_t start = position;
    size_t startColumn = column;
    
    while (position < source.length() && (isdigit(peek()) || peek() == '.')) {
        advance();
    }
    
    String value = source.substring(start, position);
    return Token(TOK_NUMBER, value, value.toDouble(), line, startColumn);
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
    
    while (position < source.length() && (isalnum(peek()) || peek() == '_')) {
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
    
    if (isdigit(c)) {
        return readNumber();
    }
    
    if (c == '"') {
        return readString();
    }
    
    if (isalpha(c) || c == '_') {
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

BasicParser::BasicParser(const std::vector<Token>& tokens) : tokens(tokens), current(0) {}

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

Token BasicParser::consume(TokenType type, const String& message) {
    if (check(type)) return advance();
    
    // Error handling - for now just return invalid token
    Serial.println("Parse error: " + message);
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

ASTNode* BasicParser::parseParamDecl() {
    consume(TOK_PARAM, "Expected 'param'");
    
    Token nameToken = consume(TOK_IDENTIFIER, "Expected parameter name");
    ASTNode* paramDecl = new ASTNode(NODE_PARAM_DECL, nameToken);
    paramDecl->name = nameToken.value;
    
    // Parse parameter type and attributes
    if (match(TOK_IDENTIFIER)) {
        Token typeToken = tokens[current - 1]; // Get the token we just consumed
        String paramType = typeToken.value;
        
        // Store the parameter type in the value field as a string
        paramDecl->value = Value(paramType);
        
        // Parse parameter attributes in parentheses
        if (match(TOK_LPAREN)) {
            // For simplicity, we'll store parameter attributes as child nodes
            // instead of trying to use arrayValue which doesn't exist
            
            if (!check(TOK_RPAREN)) {
                do {
                    if (check(TOK_NUMBER)) {
                        Token numToken = advance();
                        ASTNode* numNode = new ASTNode(NODE_NUMBER, numToken);
                        numNode->value = Value(numToken.value.toDouble());
                        paramDecl->addChild(numNode);
                    } else if (check(TOK_STRING)) {
                        Token strToken = advance();
                        ASTNode* strNode = new ASTNode(NODE_STRING, strToken);
                        strNode->value = Value(strToken.value);
                        paramDecl->addChild(strNode);
                    } else if (check(TOK_LBRACKET)) {
                        // Parse string array for enum values
                        advance(); // consume '['
                        
                        if (!check(TOK_RBRACKET)) {
                            do {
                                Token strToken = consume(TOK_STRING, "Expected string in enum array");
                                ASTNode* strNode = new ASTNode(NODE_STRING, strToken);
                                strNode->value = Value(strToken.value);
                                paramDecl->addChild(strNode);
                            } while (match(TOK_COMMA));
                        }
                        
                        consume(TOK_RBRACKET, "Expected ']' after enum values");
                    }
                } while (match(TOK_COMMA));
            }
            
            consume(TOK_RPAREN, "Expected ')' after parameter attributes");
        }
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
    
    ASTNode* assignment = new ASTNode(NODE_ASSIGNMENT);
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
    
    ASTNode* dimNode = new ASTNode(NODE_ARRAY_DECLARATION);
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
    
    ASTNode* arrayAssign = new ASTNode(NODE_ARRAY_ASSIGNMENT);
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
    ifNode->addChild(parseBlock()); // then block
    
    if (match(TOK_ELSE)) {
        ifNode->addChild(parseBlock()); // else block
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
    
    forNode->addChild(parseBlock()); // body
    
    consume(TOK_NEXT, "Expected 'next'");
    
    return forNode;
}

ASTNode* BasicParser::parseBlock() {
    ASTNode* block = new ASTNode(NODE_BLOCK);
    
    while (!check(TOK_EOF) && !check(TOK_END) && !check(TOK_ELSE) && !check(TOK_NEXT)) {
        if (check(TOK_NEWLINE)) {
            advance();
            continue;
        }
        block->addChild(parseStatement());
    }
    
    if (check(TOK_END)) {
        advance();
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
    ASTNode* expr = parseUnary();
    
    while (match(TOK_DIVIDE) || match(TOK_MULTIPLY) || match(TOK_MODULO) || match(TOK_POWER)) {
        Token op = tokens[current - 1];
        ASTNode* right = parseUnary();
        ASTNode* binary = new ASTNode(NODE_BINARY_OP, op);
        binary->addChild(expr);
        binary->addChild(right);
        expr = binary;
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
    
    Serial.println("Unexpected token in primary at " + String(current) );
    Token token = peek();
    int offset = 1;
    constexpr int MAX_TOKEN_LOOKAHEAD = 20;
    while(token.type != Token(TOK_NEWLINE).type && offset < MAX_TOKEN_LOOKAHEAD) {
        Serial.print(token.value);
        token = peek(offset);
        offset++;
    }
    return new ASTNode(NODE_NUMBER); // Return dummy node
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

// =============================================================================
// BasicInterpreter Implementation
// =============================================================================

BasicInterpreter::BasicInterpreter(CRGB* ledArray, int ledCount) 
    : leds(ledArray), numLeds(ledCount), showCalled(false), setupNode(nullptr), loopNode(nullptr) {
    // Initialize built-in variables
    variables["PI"] = Value(M_PI);
    variables["E"] = Value(M_E);
}

void BasicInterpreter::run(ASTNode* program) {
    if (!program || program->type != NODE_PROGRAM) return;
    
    // Find setup and loop functions, and process parameter declarations
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
        param.defaultValue = Value(node->children.size() > 0 ? (node->children[0]->value.numberValue != 0) : false);
        param.currentValue = param.defaultValue;
        param.minValue = 0;
        param.maxValue = 1;
        param.stepValue = 1;
    } else if (paramType == "number") {
        param.type = PARAM_NUMBER;
        // Format: param name number(default, min, max, step)
        param.defaultValue = Value(node->children.size() > 0 ? node->children[0]->value.numberValue : 0.0);
        param.currentValue = param.defaultValue;
        param.minValue = node->children.size() > 1 ? node->children[1]->value.numberValue : 0.0;
        param.maxValue = node->children.size() > 2 ? node->children[2]->value.numberValue : 100.0;
        param.stepValue = node->children.size() > 3 ? node->children[3]->value.numberValue : 1.0;
    } else if (paramType == "enum") {
        param.type = PARAM_ENUM;
        // Extract enum values from child nodes (all should be strings)
        param.enumValues.clear();
        for (ASTNode* child : node->children) {
            if (child->value.type == VAL_STRING) {
                param.enumValues.push_back(child->value.stringValue);
            }
        }
        param.defaultValue = Value(0.0); // Default to first enum value
        param.currentValue = param.defaultValue;
        param.minValue = 0;
        param.maxValue = param.enumValues.size() - 1;
        param.stepValue = 1;
    }
    
    // Add the parameter and create a variable for it
    addParameter(param);
    variables[paramName] = param.currentValue;
}

void BasicInterpreter::runSetup() {
    if (setupNode && setupNode->children.size() > 0) {
        execute(setupNode->children[0]); // Execute the block
    }
}

void BasicInterpreter::runLoop(unsigned long timeMs) {
    if (loopNode && loopNode->children.size() >= 2) {
        // Set the time parameter
        if (loopNode->children[0]->type == NODE_IDENTIFIER) {
            variables[loopNode->children[0]->name] = Value((double)timeMs);
        }
        
        showCalled = false;
        execute(loopNode->children[1]); // Execute the block
        
        // Auto-show if not explicitly called
        if (!showCalled) {
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
            
        case NODE_IDENTIFIER:
            if (variables.find(node->name) != variables.end()) {
                return variables[node->name];
            }
            return Value(0);
            
        case NODE_BINARY_OP: {
            Value left = evaluate(node->children[0]);
            Value right = evaluate(node->children[1]);
            
            switch (node->token.type) {
                case TOK_PLUS:
                    return Value(left.numberValue + right.numberValue);
                case TOK_MINUS:
                    return Value(left.numberValue - right.numberValue);
                case TOK_MULTIPLY:
                    return Value(left.numberValue * right.numberValue);
                case TOK_DIVIDE:
                    if (right.numberValue != 0) {
                        return Value(left.numberValue / right.numberValue);
                    }
                    return Value(0);
                case TOK_MODULO:
                    if (right.numberValue != 0) {
                        return Value(fmod(left.numberValue, right.numberValue));
                    }
                    return Value(0);
                case TOK_POWER:
                    return Value(pow(left.numberValue, right.numberValue));
                case TOK_EQUALS:
                    return Value(left.numberValue == right.numberValue ? 1.0 : 0.0);
                case TOK_NOT_EQUALS:
                    return Value(left.numberValue != right.numberValue ? 1.0 : 0.0);
                case TOK_LESS_THAN:
                    return Value(left.numberValue < right.numberValue ? 1.0 : 0.0);
                case TOK_GREATER_THAN:
                    return Value(left.numberValue > right.numberValue ? 1.0 : 0.0);
                case TOK_LESS_EQUAL:
                    return Value(left.numberValue <= right.numberValue ? 1.0 : 0.0);
                case TOK_GREATER_EQUAL:
                    return Value(left.numberValue >= right.numberValue ? 1.0 : 0.0);
                case TOK_AND:
                    return Value((left.numberValue != 0 && right.numberValue != 0) ? 1.0 : 0.0);
                case TOK_OR:
                    return Value((left.numberValue != 0 || right.numberValue != 0) ? 1.0 : 0.0);
                default:
                    return Value(0);
            }
        }
        
        case NODE_UNARY_OP: {
            Value operand = evaluate(node->children[0]);
            
            switch (node->token.type) {
                case TOK_MINUS:
                    return Value(-operand.numberValue);
                case TOK_NOT:
                    return Value(operand.numberValue == 0 ? 1.0 : 0.0);
                default:
                    return operand;
            }
        }
        
        case NODE_FUNCTION_CALL: {
            std::vector<Value> args;
            for (size_t i = 0; i < node->children.size(); i++) {
                args.push_back(evaluate(node->children[i]));
            }
            return callFunction(node->name, args);
        }
        
        case NODE_ARRAY_ACCESS: {
            if (node->children.size() >= 2) {
                String varName = node->children[0]->name;
                Value indexValue = evaluate(node->children[1]);
                int index = (int)indexValue.numberValue;
                
                if (variables.find(varName) != variables.end() && 
                    variables[varName].type == VAL_ARRAY &&
                    index >= 0 && index < variables[varName].arrayValue.size()) {
                    return variables[varName].arrayValue[index];
                }
            }
            return Value(0);
        }
        
        default:
            return Value(0);
    }
}

void BasicInterpreter::execute(ASTNode* node) {
    if (!node) return;
    
    switch (node->type) {
        case NODE_BLOCK:
            for (ASTNode* child : node->children) {
                execute(child);
            }
            break;
            
        case NODE_ASSIGNMENT: {
            if (node->children.size() >= 2) {
                String varName = node->children[0]->name;
                Value value = evaluate(node->children[1]);
                variables[varName] = value;
            }
            break;
        }
        
        case NODE_IF: {
            if (node->children.size() >= 2) {
                Value condition = evaluate(node->children[0]);
                if (condition.numberValue != 0) {
                    execute(node->children[1]); // then block
                } else if (node->children.size() >= 3) {
                    execute(node->children[2]); // else block
                }
            }
            break;
        }
        
        case NODE_WHILE: {
            if (node->children.size() >= 2) {
                while (true) {
                    Value condition = evaluate(node->children[0]);
                    if (condition.numberValue == 0) break;
                    execute(node->children[1]);
                }
            }
            break;
        }
        
        case NODE_FOR: {
            if (node->children.size() >= 5) {
                String varName = node->children[0]->name;
                Value start = evaluate(node->children[1]);
                Value end = evaluate(node->children[2]);
                Value step = evaluate(node->children[3]);
                
                variables[varName] = start;
                
                while (true) {
                    Value current = variables[varName];
                    if ((step.numberValue > 0 && current.numberValue > end.numberValue) ||
                        (step.numberValue < 0 && current.numberValue < end.numberValue)) {
                        break;
                    }
                    
                    execute(node->children[4]); // body
                    
                    variables[varName] = Value(current.numberValue + step.numberValue);
                }
            }
            break;
        }
        
        case NODE_FUNCTION_CALL: {
            std::vector<Value> args;
            for (size_t i = 0; i < node->children.size(); i++) {
                args.push_back(evaluate(node->children[i]));
            }
            callFunction(node->name, args);
            break;
        }
        
        case NODE_ARRAY_DECLARATION: {
            if (node->children.size() >= 2) {
                String varName = node->children[0]->name;
                Value sizeValue = evaluate(node->children[1]);
                int size = (int)sizeValue.numberValue;
                
                // Create array with specified size, initialized to 0
                std::vector<Value> arrayData(size, Value(0.0));
                variables[varName] = Value(arrayData);
            }
            break;
        }
        
        case NODE_ARRAY_ASSIGNMENT: {
            if (node->children.size() >= 3) {
                String varName = node->children[0]->name;
                Value indexValue = evaluate(node->children[1]);
                Value assignValue = evaluate(node->children[2]);
                int index = (int)indexValue.numberValue;
                
                if (variables.find(varName) != variables.end() && 
                    variables[varName].type == VAL_ARRAY &&
                    index >= 0 && index < variables[varName].arrayValue.size()) {
                    variables[varName].arrayValue[index] = assignValue;
                }
            }
            break;
        }
        
        default:
            evaluate(node); // For expression statements
            break;
    }
}

Value BasicInterpreter::callFunction(const String& name, const std::vector<Value>& args) {
    // Check if it's a math function
    TokenType func = TOK_INVALID;
    if (name == "sin") func = TOK_SIN;
    else if (name == "cos") func = TOK_COS;
    else if (name == "tan") func = TOK_TAN;
    else if (name == "sqrt") func = TOK_SQRT;
    else if (name == "pow") func = TOK_POW;
    else if (name == "log") func = TOK_LOG;
    else if (name == "ln") func = TOK_LN;
    else if (name == "abs") func = TOK_ABS;
    else if (name == "floor") func = TOK_FLOOR;
    else if (name == "ceil") func = TOK_CEIL;
    else if (name == "round") func = TOK_ROUND;
    else if (name == "min") func = TOK_MIN;
    else if (name == "max") func = TOK_MAX;
    else if (name == "random") func = TOK_RANDOM;
    else if (name == "map") func = TOK_MAP;
    else if (name == "millis") func = TOK_MILLIS;
    else if (name == "delay") func = TOK_DELAY;
    else if (name == "hsv_to_rgb") func = TOK_HSV_TO_RGB;
    else if (name == "wheel") func = TOK_WHEEL;
    
    if (func != TOK_INVALID) {
        return callMathFunction(func, args);
    }
    
    // Check if it's an LED function
    if (name == "setled") func = TOK_SETLED;
    else if (name == "setcolor") func = TOK_SETCOLOR;
    else if (name == "sethsv") func = TOK_SETHSV;
    else if (name == "show") func = TOK_SHOW;
    else if (name == "clear") func = TOK_CLEAR;
    else if (name == "fill") func = TOK_FILL;
    else if (name == "brightness") func = TOK_BRIGHTNESS;
    else if (name == "numled") func = TOK_NUMLED;
    else if (name == "hsv") func = TOK_HSV;
    else if (name == "rgb") func = TOK_RGB;
    else if (name == "get_led_r") func = TOK_GET_LED_R;
    else if (name == "get_led_g") func = TOK_GET_LED_G;
    else if (name == "get_led_b") func = TOK_GET_LED_B;
    else if (name == "set_led") func = TOK_SET_LED;
    else if (name == "set_all") func = TOK_SET_ALL;
    else if (name == "get_led_count") func = TOK_GET_LED_COUNT;
    
    if (func != TOK_INVALID) {
        return callLedFunction(func, args);
    }
    
    return Value(0);
}

Value BasicInterpreter::callMathFunction(TokenType func, const std::vector<Value>& args) {
    switch (func) {
        case TOK_SIN:
            if (args.size() >= 1) return Value(sin(args[0].numberValue));
            break;
        case TOK_COS:
            if (args.size() >= 1) return Value(cos(args[0].numberValue));
            break;
        case TOK_TAN:
            if (args.size() >= 1) return Value(tan(args[0].numberValue));
            break;
        case TOK_SQRT:
            if (args.size() >= 1) return Value(sqrt(args[0].numberValue));
            break;
        case TOK_POW:
            if (args.size() >= 2) return Value(pow(args[0].numberValue, args[1].numberValue));
            break;
        case TOK_LOG:
            if (args.size() >= 1) return Value(log10(args[0].numberValue));
            break;
        case TOK_LN:
            if (args.size() >= 1) return Value(log(args[0].numberValue));
            break;
        case TOK_ABS:
            if (args.size() >= 1) return Value(abs(args[0].numberValue));
            break;
        case TOK_FLOOR:
            if (args.size() >= 1) return Value(floor(args[0].numberValue));
            break;
        case TOK_CEIL:
            if (args.size() >= 1) return Value(ceil(args[0].numberValue));
            break;
        case TOK_ROUND:
            if (args.size() >= 1) return Value(round(args[0].numberValue));
            break;
        case TOK_MIN:
            if (args.size() >= 2) return Value(min(args[0].numberValue, args[1].numberValue));
            break;
        case TOK_MAX:
            if (args.size() >= 2) return Value(max(args[0].numberValue, args[1].numberValue));
            break;
        case TOK_RANDOM:
            // Use hardware RNG on ESP32, Arduino random() elsewhere
            if (args.size() >= 1) {
                // random(max) - return 0 to max-1
                double maxVal = args[0].numberValue;
                return Value(fmod(ledbasic_random(), maxVal));
            } else {
                // random() - return 0.0 to 1.0
                return Value((double)ledbasic_random() / 4294967295.0);
            }
            break;
        case TOK_MAP:
            // map(value, fromLow, fromHigh, toLow, toHigh)
            if (args.size() >= 5) {
                double value = args[0].numberValue;
                double fromLow = args[1].numberValue;
                double fromHigh = args[2].numberValue;
                double toLow = args[3].numberValue;
                double toHigh = args[4].numberValue;
                
                double mapped = (value - fromLow) * (toHigh - toLow) / (fromHigh - fromLow) + toLow;
                return Value(mapped);
            }
            break;
        case TOK_MILLIS:
            // millis() - return current time in milliseconds
            return Value(millis());
        case TOK_DELAY:
            // delay(ms) - pause execution
            if (args.size() >= 1) {
                int delayMs = (int)args[0].numberValue;
                delay(delayMs);
            }
            break;
        case TOK_HSV_TO_RGB: {
            // hsv_to_rgb(h, s, v) - convert HSV to RGB, returns red component (simplified)
            if (args.size() >= 3) {
                int h = ((int)args[0].numberValue) % 360;
                int s = constrain((int)args[1].numberValue, 0, 100);
                int v = constrain((int)args[2].numberValue, 0, 100);
                
                // Convert to FastLED CHSV and back to get RGB
                CHSV hsv(h * 255 / 360, s * 255 / 100, v * 255 / 100);
                CRGB rgb;
                hsv2rgb_rainbow(hsv, rgb);
                
                // For now, just return the red component
                // In a real implementation, this would need to set multiple variables
                return Value(rgb.r);
            }
            break;
        }
        case TOK_WHEEL: {
            // wheel(pos) - rainbow color wheel, returns red component (simplified)
            if (args.size() >= 1) {
                int pos = ((int)args[0].numberValue) % 256;
                CRGB color;
                
                if (pos < 85) {
                    color = CRGB(pos * 3, 255 - pos * 3, 0);
                } else if (pos < 170) {
                    pos -= 85;
                    color = CRGB(255 - pos * 3, 0, pos * 3);
                } else {
                    pos -= 170;
                    color = CRGB(0, pos * 3, 255 - pos * 3);
                }
                
                // For now, just return the red component
                return Value(color.r);
            }
            break;
        }
        default:
            break;
    }
    return Value(0);
}

Value BasicInterpreter::callLedFunction(TokenType func, const std::vector<Value>& args) {
    switch (func) {
        case TOK_SETLED: {
            // setled(index, r, g, b)
            if (args.size() >= 4) {
                int index = (int)args[0].numberValue;
                int r = constrain((int)args[1].numberValue, 0, 255);
                int g = constrain((int)args[2].numberValue, 0, 255);
                int b = constrain((int)args[3].numberValue, 0, 255);
                
                if (index >= 0 && index < numLeds) {
                    leds[index] = CRGB(r, g, b);
                }
            }
            break;
        }
        
        case TOK_RGB: {
            // rgb(r, g, b) - returns a color value (just return red component for now)
            if (args.size() >= 3) {
                int r = constrain((int)args[0].numberValue, 0, 255);
                return Value(r); // Simplified - in real implementation would return color struct
            }
            break;
        }
        
        case TOK_SETHSV: {
            // sethsv(index, h, s, v)
            if (args.size() >= 4) {
                int index = (int)args[0].numberValue;
                int h = (int)args[1].numberValue % 360;
                int s = constrain((int)args[2].numberValue, 0, 255);
                int v = constrain((int)args[3].numberValue, 0, 255);
                
                if (index >= 0 && index < numLeds) {
                    leds[index] = CHSV(h * 255 / 360, s, v);
                }
            }
            break;
        }
        
        case TOK_CLEAR: {
            // clear() - clear all LEDs
            for (int i = 0; i < numLeds; i++) {
                leds[i] = CRGB::Black;
            }
            break;
        }
        
        case TOK_FILL: {
            // fill(r, g, b) - fill all LEDs with color
            if (args.size() >= 3) {
                int r = constrain((int)args[0].numberValue, 0, 255);
                int g = constrain((int)args[1].numberValue, 0, 255);
                int b = constrain((int)args[2].numberValue, 0, 255);
                
                for (int i = 0; i < numLeds; i++) {
                    leds[i] = CRGB(r, g, b);
                }
            }
            break;
        }
        
        case TOK_SHOW: {
            // show() - update the LED strip
            FastLED.show();
            showCalled = true;
            break;
        }
        
        case TOK_BRIGHTNESS: {
            // brightness(value) - set global brightness
            if (args.size() >= 1) {
                int brightness = constrain((int)args[0].numberValue, 0, 255);
                FastLED.setBrightness(brightness);
            }
            break;
        }
        
        case TOK_NUMLED: {
            // numled() - return number of LEDs
            return Value(numLeds);
        }
        
        case TOK_GET_LED_R: {
            // get_led_r(index) - get red component of LED
            if (args.size() >= 1) {
                int index = (int)args[0].numberValue;
                if (index >= 0 && index < numLeds) {
                    return Value(leds[index].r);
                }
            }
            return Value(0);
        }
        
        case TOK_GET_LED_G: {
            // get_led_g(index) - get green component of LED
            if (args.size() >= 1) {
                int index = (int)args[0].numberValue;
                if (index >= 0 && index < numLeds) {
                    return Value(leds[index].g);
                }
            }
            return Value(0);
        }
        
        case TOK_GET_LED_B: {
            // get_led_b(index) - get blue component of LED
            if (args.size() >= 1) {
                int index = (int)args[0].numberValue;
                if (index >= 0 && index < numLeds) {
                    return Value(leds[index].b);
                }
            }
            return Value(0);
        }
        
        case TOK_SET_LED: {
            // set_led(index, r, g, b) - alias for setled
            if (args.size() >= 4) {
                int index = (int)args[0].numberValue;
                int r = constrain((int)args[1].numberValue, 0, 255);
                int g = constrain((int)args[2].numberValue, 0, 255);
                int b = constrain((int)args[3].numberValue, 0, 255);
                
                if (index >= 0 && index < numLeds) {
                    leds[index] = CRGB(r, g, b);
                }
            }
            break;
        }
        
        case TOK_SET_ALL: {
            // set_all(r, g, b) - alias for fill
            if (args.size() >= 3) {
                int r = constrain((int)args[0].numberValue, 0, 255);
                int g = constrain((int)args[1].numberValue, 0, 255);
                int b = constrain((int)args[2].numberValue, 0, 255);
                
                for (int i = 0; i < numLeds; i++) {
                    leds[i] = CRGB(r, g, b);
                }
            }
            break;
        }
        
        case TOK_GET_LED_COUNT: {
            // get_led_count() - alias for numled
            return Value(numLeds);
        }
        
        default:
            break;
    }
    
    return Value(0);
}

void BasicInterpreter::setVariable(const String& name, const Value& value) {
    variables[name] = value;
}

Value BasicInterpreter::getVariable(const String& name) {
    if (variables.find(name) != variables.end()) {
        return variables[name];
    }
    return Value(0);
}

// =============================================================================
// Parameter Management Implementation
// =============================================================================

void BasicInterpreter::addParameter(const Parameter& param) {
    parameters[param.name] = param;
    // Also set the parameter as a variable so BASIC programs can access it
    variables[param.name] = param.currentValue;
}

void BasicInterpreter::setParameterValue(const String& name, const Value& value) {
    if (parameters.find(name) != parameters.end()) {
        parameters[name].setValue(value);
        // Update the variable as well
        variables[name] = parameters[name].currentValue;
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
    // Remove parameter variables from the variable map
    for (const auto& pair : parameters) {
        variables.erase(pair.first);
    }
    parameters.clear();
}

// =============================================================================
// BasicLEDController Implementation
// =============================================================================

BasicLEDController::BasicLEDController(CRGB* ledArray, int ledCount) 
    : lexer(nullptr), parser(nullptr), interpreter(nullptr), ast(nullptr), programLoaded(false) {
    interpreter = new BasicInterpreter(ledArray, ledCount);
}

BasicLEDController::~BasicLEDController() {
    delete lexer;
    delete parser;
    delete interpreter;
    delete ast;
}

bool BasicLEDController::loadProgram(const String& source) {
    // Clean up previous program
    delete lexer;
    delete parser;
    delete ast;
    
    lexer = new BasicLexer(source);
    std::vector<Token> tokens = lexer->tokenize();
    
    parser = new BasicParser(tokens);
    ast = parser->parse();
    
    if (ast) {
        interpreter->run(ast);
        programLoaded = true;
        return true;
    }
    
    programLoaded = false;
    return false;
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
        
        // Reload the program if we had one
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
    for (auto it = strips.begin(); it != strips.end(); ++it) {
        if (*it == strip) {
            delete strip;
            strips.erase(it);
            break;
        }
    }
}

void VirtualStripManager::removeAllStrips() {
    for (VirtualStrip* strip : strips) {
        delete strip;
    }
    strips.clear();
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
    // Clear all physical strips first
    for (PhysicalStrip& phys : physicalStrips) {
        for (int i = 0; i < phys.length; i++) {
            phys.leds[i] = CRGB::Black;
        }
    }

    // Render strips in Z-order (lowest to highest)
    for (VirtualStrip* strip : strips) {
        if (!strip->isEnabled()) continue;

        int idx = strip->getStripIndex();
        if (idx < 0 || idx >= physicalStrips.size()) continue;

        PhysicalStrip& phys = physicalStrips[idx];
        int startPos = strip->getStartPos();
        int length = strip->getLength();
        CRGB* virtualLeds = strip->getVirtualLeds();
        BlendMode blend = strip->getBlendMode();
        bool reversed = strip->isReversed();

        for (int i = 0; i < length; i++) {
            // Calculate the virtual LED index (may be reversed)
            int virtualIndex = reversed ? (length - 1 - i) : i;
            
            // Calculate the physical position
            int physicalPos = startPos + i;
            
            if (physicalPos >= 0 && physicalPos < phys.length) {
                if (blend == BLEND_REPLACE) {
                    // Simple replacement (highest Z wins)
                    if (virtualLeds[virtualIndex] != CRGB::Black) {
                        phys.leds[physicalPos] = virtualLeds[virtualIndex];
                    }
                } else {
                    // Use blending function
                    blendPixel(phys.leds[physicalPos], virtualLeds[virtualIndex], blend);
                }
            }
        }
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
