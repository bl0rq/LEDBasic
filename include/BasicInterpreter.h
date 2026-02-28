#ifndef BASIC_INTERPRETER_H
#define BASIC_INTERPRETER_H

#include <Arduino.h>
#include <FastLED.h>
#include <map>
#include <vector>

// Token types for the lexer
enum TokenType {
    // Literals
    TOK_NUMBER,
    TOK_STRING,
    TOK_IDENTIFIER,
    
    // Keywords
    TOK_SETUP,
    TOK_LOOP,
    TOK_IF,
    TOK_ELSE,
    TOK_WHILE,
    TOK_FOR,
    TOK_TO,
    TOK_STEP,
    TOK_NEXT,
    TOK_FUNCTION,
    TOK_RETURN,
    TOK_END,
    TOK_DIM,
    TOK_PARAM,
    
    // Operators
    TOK_PLUS,
    TOK_MINUS,
    TOK_MULTIPLY,
    TOK_DIVIDE,
    TOK_MODULO,
    TOK_POWER,
    TOK_ASSIGN,
    TOK_EQUALS,
    TOK_NOT_EQUALS,
    TOK_LESS_THAN,
    TOK_GREATER_THAN,
    TOK_LESS_EQUAL,
    TOK_GREATER_EQUAL,
    TOK_AND,
    TOK_OR,
    TOK_NOT,
    
    // Punctuation
    TOK_LPAREN,
    TOK_RPAREN,
    TOK_LBRACKET,
    TOK_RBRACKET,
    TOK_COMMA,
    TOK_SEMICOLON,
    TOK_NEWLINE,
    
    // Math functions
    TOK_SIN,
    TOK_COS,
    TOK_TAN,
    TOK_SQRT,
    TOK_POW,
    TOK_LOG,
    TOK_LN,
    TOK_ABS,
    TOK_FLOOR,
    TOK_CEIL,
    TOK_ROUND,
    TOK_MIN,
    TOK_MAX,
    TOK_RANDOM,
    TOK_MAP,
    TOK_MILLIS,
    TOK_DELAY,
    TOK_HSV_TO_RGB,
    TOK_WHEEL,
    
    // LED functions
    TOK_SETLED,
    TOK_SETCOLOR,
    TOK_SETHSV,
    TOK_SHOW,
    TOK_CLEAR,
    TOK_FILL,
    TOK_BRIGHTNESS,
    TOK_NUMLED,
    TOK_HSV,
    TOK_RGB,
    TOK_GET_LED_R,
    TOK_GET_LED_G,
    TOK_GET_LED_B,
    TOK_SET_LED,
    TOK_SET_ALL,
    TOK_GET_LED_COUNT,
    
    // Special
    TOK_EOF,
    TOK_INVALID
};

// Token structure
struct Token {
    TokenType type;
    String value;
    double numberValue;
    int line;
    int column;
    
    Token(TokenType t = TOK_INVALID, String v = "", double n = 0, int l = 0, int c = 0) 
        : type(t), value(v), numberValue(n), line(l), column(c) {}
};

// Value types for variables
enum ValueType {
    VAL_NUMBER,
    VAL_STRING,
    VAL_ARRAY
};

// Variable value structure
struct Value {
    ValueType type;
    double numberValue;
    String stringValue;
    std::vector<Value> arrayValue;
    
    Value() : type(VAL_NUMBER), numberValue(0) {}
    Value(double n) : type(VAL_NUMBER), numberValue(n) {}
    Value(String s) : type(VAL_STRING), stringValue(s) {}
    Value(std::vector<Value> a) : type(VAL_ARRAY), arrayValue(a) {}
};

// Parameter types for program customization
enum ParameterType {
    PARAM_BOOLEAN,
    PARAM_NUMBER,
    PARAM_ENUM
};

// Parameter definition structure
struct Parameter {
    String name;
    ParameterType type;
    Value defaultValue;
    Value currentValue;
    
    // For number parameters
    double minValue;
    double maxValue;
    double stepValue;
    
    // For enum parameters
    std::vector<String> enumValues;
    
    // Default constructor
    Parameter() : name(""), type(PARAM_BOOLEAN), defaultValue(0.0), currentValue(0.0), 
                 minValue(0), maxValue(1), stepValue(1) {}
    
    Parameter(const String& n, bool defaultVal) 
        : name(n), type(PARAM_BOOLEAN), defaultValue(defaultVal ? 1.0 : 0.0), 
          currentValue(defaultVal ? 1.0 : 0.0), minValue(0), maxValue(1), stepValue(1) {}
    
    Parameter(const String& n, double defaultVal, double minVal, double maxVal, double step = 1.0)
        : name(n), type(PARAM_NUMBER), defaultValue(defaultVal), currentValue(defaultVal),
          minValue(minVal), maxValue(maxVal), stepValue(step) {}
    
    Parameter(const String& n, const std::vector<String>& values, int defaultIndex = 0)
        : name(n), type(PARAM_ENUM), defaultValue((double)defaultIndex), currentValue((double)defaultIndex),
          minValue(0), maxValue(values.size() - 1), stepValue(1), enumValues(values) {}
          
    String getStringValue() const {
        if (type == PARAM_BOOLEAN) {
            return currentValue.numberValue != 0 ? "true" : "false";
        } else if (type == PARAM_NUMBER) {
            return String(currentValue.numberValue);
        } else if (type == PARAM_ENUM) {
            int index = (int)currentValue.numberValue;
            if (index >= 0 && index < enumValues.size()) {
                return enumValues[index];
            }
        }
        return "";
    }
    
    void setValue(const Value& val) {
        if (type == PARAM_BOOLEAN) {
            currentValue = Value(val.numberValue != 0 ? 1.0 : 0.0);
        } else if (type == PARAM_NUMBER) {
            double clampedVal = constrain(val.numberValue, minValue, maxValue);
            currentValue = Value(clampedVal);
        } else if (type == PARAM_ENUM) {
            int index = constrain((int)val.numberValue, 0, (int)enumValues.size() - 1);
            currentValue = Value((double)index);
        }
    }
};

// AST Node types
enum NodeType {
    NODE_PROGRAM,
    NODE_SETUP,
    NODE_LOOP,
    NODE_ASSIGNMENT,
    NODE_BINARY_OP,
    NODE_UNARY_OP,
    NODE_FUNCTION_CALL,
    NODE_NUMBER,
    NODE_STRING,
    NODE_IDENTIFIER,
    NODE_IF,
    NODE_WHILE,
    NODE_FOR,
    NODE_BLOCK,
    NODE_RETURN,
    NODE_ARRAY_DECLARATION,
    NODE_ARRAY_ACCESS,
    NODE_ARRAY_ASSIGNMENT,
    NODE_PARAM_DECL
};

// Forward declaration
struct ASTNode;

// AST Node structure
struct ASTNode {
    NodeType type;
    Token token;
    std::vector<ASTNode*> children;
    Value value;
    String name;
    
    ASTNode(NodeType t, Token tok = Token()) : type(t), token(tok) {}
    
    void addChild(ASTNode* child) {
        children.push_back(child);
    }
    
    ~ASTNode() {
        for (ASTNode* child : children) {
            delete child;
        }
    }
};

// Lexer class
class BasicLexer {
private:
    String source;
    size_t position;
    size_t line;
    size_t column;
    std::map<String, TokenType> keywords;
    
    void initKeywords();
    char peek(int offset = 0);
    char advance();
    void skipWhitespace();
    void skipComment();
    void lexerError(const String& message, size_t errLine, size_t errColumn, const String& snippet = "");
    Token lexerError(char unexpected);
    Token readNumber();
    Token readString();
    Token readIdentifier();
    
public:
    BasicLexer(const String& src);
    Token nextToken();
    std::vector<Token> tokenize();
};

// Parser class
class BasicParser {
private:
    std::vector<Token> tokens;
    size_t current;
    
    Token peek(int offset = 0);
    Token advance();
    bool match(TokenType type);
    bool check(TokenType type);
    Token consume(TokenType type, const String& message);
    
    ASTNode* parseProgram();
    ASTNode* parseSetup();
    ASTNode* parseLoop();
    ASTNode* parseParamDecl();
    ASTNode* parseStatement();
    ASTNode* parseAssignment();
    ASTNode* parseDimStatement();
    ASTNode* parseArrayAssignment();
    ASTNode* parseIf();
    ASTNode* parseWhile();
    ASTNode* parseFor();
    ASTNode* parseBlock();
    ASTNode* parseExpression();
    ASTNode* parseLogicalOr();
    ASTNode* parseLogicalAnd();
    ASTNode* parseEquality();
    ASTNode* parseComparison();
    ASTNode* parseTerm();
    ASTNode* parseFactor();
    ASTNode* parseUnary();
    ASTNode* parsePrimary();
    ASTNode* parseFunctionCall();
    
public:
    BasicParser(const std::vector<Token>& tokens);
    ASTNode* parse();
};

// Interpreter class
class BasicInterpreter {
private:
    std::map<String, Value> variables;
    std::map<String, ASTNode*> functions;
    std::map<String, Parameter> parameters;
    CRGB* leds;
    int numLeds;
    bool showCalled;
    ASTNode* setupNode;
    ASTNode* loopNode;
    
    Value evaluate(ASTNode* node);
    void execute(ASTNode* node);
    void processParameterDeclaration(ASTNode* node);
    Value callFunction(const String& name, const std::vector<Value>& args);
    Value callMathFunction(TokenType func, const std::vector<Value>& args);
    Value callLedFunction(TokenType func, const std::vector<Value>& args);
    
public:
    BasicInterpreter(CRGB* ledArray, int ledCount);
    void run(ASTNode* program);
    void runSetup();
    void runLoop(unsigned long timeMs);
    void setVariable(const String& name, const Value& value);
    Value getVariable(const String& name);
    
    // Parameter management
    void addParameter(const Parameter& param);
    void setParameterValue(const String& name, const Value& value);
    Parameter* getParameter(const String& name);
    std::vector<Parameter> getAllParameters() const;
    void clearParameters();
};

// Forward declaration
class BasicLEDController;

// Virtual Strip Blending Modes
enum BlendMode {
    BLEND_REPLACE,    // Replace (highest Z-order wins)
    BLEND_ADD,        // Additive blending
    BLEND_SUBTRACT,   // Subtractive blending
    BLEND_MULTIPLY,   // Multiply blending
    BLEND_SCREEN,      // Screen blending
    BLEND_COLOR_SPACE  // Color space blending
};

// Virtual LED Strip
class VirtualStrip {
private:
    CRGB* virtualLeds;
    int startPos;
    int length;
    int zOrder;
    int stripIndex;
    BlendMode blendMode;
    BasicLEDController* controller;
    String programSource;
    bool enabled;
    bool reversed;

public:
    VirtualStrip(int stripIndex, int start, int len, int z = 0, BlendMode blend = BLEND_REPLACE, bool reverse = false);
    ~VirtualStrip();
    
    bool loadProgram(const String& source);
    void runSetup();
    void runLoop(unsigned long timeMs);
    void setEnabled(bool enable) { enabled = enable; }
    bool isEnabled() const { return enabled; }
    
    // Getters
    int getStartPos() const { return startPos; }
    int getLength() const { return length; }
    int getZOrder() const { return zOrder; }
    int getStripIndex() const { return stripIndex; }
    BlendMode getBlendMode() const { return blendMode; }
    CRGB* getVirtualLeds() { return virtualLeds; }
    bool isReversed() const { return reversed; }
    
    // Setters
    void setZOrder(int z) { zOrder = z; }
    void setBlendMode(BlendMode blend) { blendMode = blend; }
    void setReversed(bool reverse) { reversed = reverse; }
    void setRegion(int start, int len);
};

// Virtual Strip Manager
class VirtualStripManager {
private:
    std::vector<VirtualStrip*> strips;
    struct PhysicalStrip {
        CRGB* leds;
        int length;
    };
    std::vector<PhysicalStrip> physicalStrips;

    void blendPixel(CRGB& dest, const CRGB& src, BlendMode mode);

public:
    VirtualStripManager();
    ~VirtualStripManager();

    void addPhysicalStrip(CRGB* leds, int length);
    VirtualStrip* createStrip(int stripIndex, int start, int length, int zOrder = 0, BlendMode blend = BLEND_REPLACE, bool reverse = false);
    void removeStrip(VirtualStrip* strip);
    void removeAllStrips();
    
    void runAllSetups();
    void runAllLoops(unsigned long timeMs);
    void renderToPhysical();
    
    // Utility functions
    int getStripCount() const { return strips.size(); }
    VirtualStrip* getStrip(int index);
    void sortStripsByZOrder();
};

// Main BASIC LED Controller class
class BasicLEDController {
private:
    BasicLexer* lexer;
    BasicParser* parser;
    BasicInterpreter* interpreter;
    String program;
    ASTNode* ast;
    bool programLoaded;
    
public:
    BasicLEDController(CRGB* ledArray, int ledCount);
    ~BasicLEDController();
    
    bool loadProgram(const String& source);
    void runSetup();
    void runLoop(unsigned long timeMs);
    void setVariable(const String& name, double value);
    void setVariable(const String& name, const String& value);
    double getNumberVariable(const String& name);
    String getStringVariable(const String& name);
    
    // Parameter management
    void addParameter(const Parameter& param);
    void setParameterValue(const String& name, const Value& value);
    Parameter* getParameter(const String& name);
    std::vector<Parameter> getAllParameters() const;
    void clearParameters();
};

#endif // BASIC_INTERPRETER_H
