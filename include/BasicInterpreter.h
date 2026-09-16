#ifndef BASIC_INTERPRETER_H
#define BASIC_INTERPRETER_H

#include <Arduino.h>
#include <FastLED.h>
#include <map>
#include <vector>
#include <stdint.h>

// Token types for the lexer
enum TokenType {
    // Literals
    TOK_NUMBER,
    TOK_STRING,
    TOK_IDENTIFIER,
    TOK_TRUE,
    TOK_FALSE,

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
    float numberValue;
    int line;
    int column;

    Token(TokenType t = TOK_INVALID, String v = "", float n = 0, int l = 0, int c = 0)
        : type(t), value(v), numberValue(n), line(l), column(c) {}
};

// Value types for variables
enum ValueType {
    VAL_NUMBER,
    VAL_STRING,
    VAL_ARRAY,
    VAL_COLOR
};

// Variable value structure. Number/color copies skip string and array storage.
struct Value {
    ValueType type;
    float numberValue;
    uint32_t colorValue;
    String stringValue;
    std::vector<Value> arrayValue;

    Value() : type(VAL_NUMBER), numberValue(0), colorValue(0) {}
    Value(float n) : type(VAL_NUMBER), numberValue(n), colorValue(0) {}
    Value(double n) : type(VAL_NUMBER), numberValue((float)n), colorValue(0) {}
    Value(int n) : type(VAL_NUMBER), numberValue((float)n), colorValue(0) {}
    Value(String s) : type(VAL_STRING), numberValue(0), colorValue(0), stringValue(s) {}
    Value(std::vector<Value> a) : type(VAL_ARRAY), numberValue(0), colorValue(0), arrayValue(std::move(a)) {}

    Value(const Value& o)
        : type(o.type), numberValue(o.numberValue), colorValue(o.colorValue) {
        if (o.type == VAL_STRING) stringValue = o.stringValue;
        else if (o.type == VAL_ARRAY) arrayValue = o.arrayValue;
    }

    Value& operator=(const Value& o) {
        if (this == &o) return *this;
        type = o.type;
        numberValue = o.numberValue;
        colorValue = o.colorValue;
        if (o.type == VAL_STRING) {
            stringValue = o.stringValue;
            arrayValue.clear();
        } else if (o.type == VAL_ARRAY) {
            arrayValue = o.arrayValue;
            stringValue = String();
        } else {
            stringValue = String();
            arrayValue.clear();
        }
        return *this;
    }

    static Value color(uint8_t r, uint8_t g, uint8_t b) {
        Value v;
        v.type = VAL_COLOR;
        v.colorValue = ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
        return v;
    }

    uint8_t red() const { return (uint8_t)((colorValue >> 16) & 255); }
    uint8_t green() const { return (uint8_t)((colorValue >> 8) & 255); }
    uint8_t blue() const { return (uint8_t)(colorValue & 255); }

    float asNumber() const {
        if (type == VAL_COLOR) return (float)red();
        if (type == VAL_STRING) return stringValue.toFloat();
        return numberValue;
    }

    CRGB asCRGB() const {
        if (type == VAL_COLOR) return CRGB(red(), green(), blue());
        uint8_t n = (uint8_t)constrain((int)numberValue, 0, 255);
        return CRGB(n, n, n);
    }
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
    float minValue;
    float maxValue;
    float stepValue;

    // For enum parameters
    std::vector<String> enumValues;

    Parameter() : name(""), type(PARAM_BOOLEAN), defaultValue(0.0f), currentValue(0.0f),
                 minValue(0), maxValue(1), stepValue(1) {}

    Parameter(const String& n, bool defaultVal)
        : name(n), type(PARAM_BOOLEAN), defaultValue(defaultVal ? 1.0f : 0.0f),
          currentValue(defaultVal ? 1.0f : 0.0f), minValue(0), maxValue(1), stepValue(1) {}

    Parameter(const String& n, float defaultVal, float minVal, float maxVal, float step = 1.0f)
        : name(n), type(PARAM_NUMBER), defaultValue(defaultVal), currentValue(defaultVal),
          minValue(minVal), maxValue(maxVal), stepValue(step) {}

    Parameter(const String& n, const std::vector<String>& values, int defaultIndex = 0)
        : name(n), type(PARAM_ENUM),
          defaultValue((float)defaultIndex), currentValue((float)defaultIndex),
          minValue(0),
          maxValue(values.empty() ? 0.0f : (float)(values.size() - 1)),
          stepValue(1), enumValues(values) {}

    String getStringValue() const {
        if (type == PARAM_BOOLEAN) {
            return currentValue.numberValue != 0 ? "true" : "false";
        } else if (type == PARAM_NUMBER) {
            return String(currentValue.numberValue);
        } else if (type == PARAM_ENUM) {
            int index = (int)currentValue.numberValue;
            if (index >= 0 && index < (int)enumValues.size()) {
                return enumValues[index];
            }
        }
        return "";
    }

    void setValue(const Value& val) {
        if (type == PARAM_BOOLEAN) {
            currentValue = Value(val.asNumber() != 0 ? 1.0f : 0.0f);
        } else if (type == PARAM_NUMBER) {
            float clampedVal = constrain(val.asNumber(), minValue, maxValue);
            currentValue = Value(clampedVal);
        } else if (type == PARAM_ENUM) {
            int hi = enumValues.empty() ? 0 : (int)enumValues.size() - 1;
            int index = constrain((int)val.asNumber(), 0, hi);
            currentValue = Value((float)index);
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

struct ASTNode {
    NodeType type;
    Token token;
    std::vector<ASTNode*> children;
    Value value;
    String name;
    int slot; // interned variable slot, -1 until first use

    ASTNode(NodeType t, Token tok = Token()) : type(t), token(tok), slot(-1) {}

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
    bool hadError;

    Token peek(int offset = 0);
    Token advance();
    bool match(TokenType type);
    bool check(TokenType type);
    Token consume(TokenType type, const String& message);
    void error(const String& message);

    ASTNode* parseProgram();
    ASTNode* parseSetup();
    ASTNode* parseLoop();
    ASTNode* parseParamDecl();
    bool parseParamLiteral(ASTNode* paramDecl);
    ASTNode* parseStatement();
    ASTNode* parseAssignment();
    ASTNode* parseDimStatement();
    ASTNode* parseArrayAssignment();
    ASTNode* parseIf();
    ASTNode* parseWhile();
    ASTNode* parseFor();
    ASTNode* parseBlock(bool requireEnd = true);
    ASTNode* parseExpression();
    ASTNode* parseLogicalOr();
    ASTNode* parseLogicalAnd();
    ASTNode* parseEquality();
    ASTNode* parseComparison();
    ASTNode* parseTerm();
    ASTNode* parseFactor();
    ASTNode* parsePower();
    ASTNode* parseUnary();
    ASTNode* parsePrimary();
    ASTNode* parseFunctionCall();

public:
    BasicParser(const std::vector<Token>& tokens);
    ASTNode* parse();
    bool hasError() const { return hadError; }
};

// Interpreter class
class BasicInterpreter {
private:
    std::vector<Value> slots;
    std::map<String, int> nameToSlot;
    std::map<String, Parameter> parameters;
    CRGB* leds;
    int numLeds;
    bool showCalled;
    bool ownsPhysicalOutput;
    bool autoShow;
    uint8_t outputBrightness;
    ASTNode* setupNode;
    ASTNode* loopNode;

    int internName(const String& name);
    int ensureSlot(ASTNode* node);
    Value evaluate(ASTNode* node);
    void execute(ASTNode* node);
    void processParameterDeclaration(ASTNode* node);
    Value callFunction(TokenType func, const std::vector<Value>& args);
    Value callMathFunction(TokenType func, const std::vector<Value>& args);
    Value callLedFunction(TokenType func, const std::vector<Value>& args);
    void applyLedColor(int index, const CRGB& color);

public:
    static const int kMaxLoopIterations = 10000;
    static const int kMaxArraySize = 1024;

    BasicInterpreter(CRGB* ledArray, int ledCount);
    void reset();
    void run(ASTNode* program);
    void runSetup();
    void runLoop(unsigned long timeMs);
    void setVariable(const String& name, const Value& value);
    Value getVariable(const String& name);

    void setOwnsPhysicalOutput(bool owns) {
        ownsPhysicalOutput = owns;
        if (!owns) autoShow = false;
    }
    bool getOwnsPhysicalOutput() const { return ownsPhysicalOutput; }
    void setAutoShow(bool enable) { autoShow = enable; }
    bool getAutoShow() const { return autoShow; }
    uint8_t getOutputBrightness() const { return outputBrightness; }

    // Parameter management
    void addParameter(const Parameter& param);
    void setParameterValue(const String& name, const Value& value);
    Parameter* getParameter(const String& name);
    std::vector<Parameter> getAllParameters() const;
    void clearParameters();
};

class BasicLEDController;

// Virtual Strip Blending Modes
enum BlendMode {
    BLEND_REPLACE,     // Non-black source replaces dest (black is transparent)
    BLEND_ADD,         // Additive blending
    BLEND_SUBTRACT,    // Subtractive blending
    BLEND_MULTIPLY,    // Multiply blending
    BLEND_SCREEN,      // Screen blending
    BLEND_COLOR_SPACE  // Average HSV (hue mix does not wrap)
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

    int getStartPos() const { return startPos; }
    int getLength() const { return length; }
    int getZOrder() const { return zOrder; }
    int getStripIndex() const { return stripIndex; }
    BlendMode getBlendMode() const { return blendMode; }
    CRGB* getVirtualLeds() { return virtualLeds; }
    bool isReversed() const { return reversed; }
    uint8_t getOutputBrightness() const;

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
    std::vector<uint8_t> virtualOwned;

    void blendPixel(CRGB& dest, const CRGB& src, BlendMode mode);
    void clearPhysicalStrip(int idx);
    bool hasEnabledLayerOn(int idx) const;

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

    int getStripCount() const { return (int)strips.size(); }
    VirtualStrip* getStrip(int index);
    void sortStripsByZOrder();
};

// Main BASIC LED Controller class
class BasicLEDController {
private:
    BasicInterpreter* interpreter;
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

    void setOwnsPhysicalOutput(bool owns);
    void setAutoShow(bool enable);
    uint8_t getOutputBrightness() const;

    void addParameter(const Parameter& param);
    void setParameterValue(const String& name, const Value& value);
    Parameter* getParameter(const String& name);
    std::vector<Parameter> getAllParameters() const;
    void clearParameters();
};

#endif // BASIC_INTERPRETER_H
