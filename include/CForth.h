#ifndef CFORTH_H
#define CFORTH_H

#include <string>
#include <vector>
#include <memory>
#include <iostream>
#include <iomanip>
#include <cstring>
#include <cmath>
#include <cassert>

namespace CForth {
  struct abortSignal : std::exception {
  };

  struct quitSignal : std::exception {
  };

  bool isBaseChar(int c, int base, int *value=nullptr);

  // success/failure state
  class State {
   public:
    bool valid() const { return valid_; }

    const std::string &msg() const { return msg_; }

    operator bool() { return valid_; }

    static State success() {
      return State(true);
    }

    static State error(const std::string &msg) {
      lastError_ = State(false, msg);

      return lastError_;
    }

    static State lastError() { return lastError_; }

   private:
    State(bool valid=true, const std::string &msg="") :
     valid_(valid), msg_(msg) {
    }

   private:
    static State lastError_;

    bool        valid_;
    std::string msg_;
  };

  //------

  // word with validity
  class Word {
   public:
    Word() :
     valid_(false) {
    }

    bool isValid() const { return valid_; }

    void reset() { valid_ = false; }

    const std::string &value() const {
      assert(valid_);

      return str_;
    }

    void setValue(const std::string &str) {
      valid_ = true;
      str_   = str;
    }

    bool operator==(const std::string &str) const {
      return (str_ == str);
    }

   private:
    bool        valid_;
    std::string str_;
  };

  //------

  // line (text with current read position)
  class Line {
   public:
    Line(const std::string &line="") :
     str_(line), pos_(0), len_(int(str_.size())) {
    }

    void clear() {
      str_ = "";
      pos_ = 0;
      len_ = 0;
    }

    const std::string &str() const { return str_; }

    int pos() const { return pos_; }

    void setPos(int pos) { pos_ = pos; }

    void addChar(char c) {
      str_ += c;

      ++len_;
    }

    char lookChar() const { return str_[pos_]; }

    char getChar() { return str_[pos_++]; }

    char lookNextChar(int offset=1) {
      if (pos_ + offset <= len_)
        return str_[pos_ + offset];
      else
        return '\0';
    }

    void skipChar() { ++pos_; }

    bool isValid() const { return pos_ < len_; }

    void skipSpace() {
      while (pos_ < len_ && isSpace())
        ++pos_;
    }

    bool isSpace() const {
      return isspace(str_[pos_]);
    }

    bool isDigit() const {
      return isdigit(str_[pos_]);
    }

    bool isBaseChar(int base) const {
      return CForth::isBaseChar(str_[pos_], base, nullptr);
    }

    bool isAlpha() const {
      return isalpha(str_[pos_]);
    }

    bool isAlNum() const {
      return isalnum(str_[pos_]);
    }

    bool isChar(char c) const {
      return str_[pos_] == c;
    }

    bool isOneOf(const std::string &chars) const {
      return std::strchr(chars.c_str(), str_[pos_]) != nullptr;
    }

    void insert(const std::string &str) {
      str_ = str_.substr(0, pos_) + str + str_.substr(pos_);

      len_ += int(str.size());
    }

   private:
    std::string str_;
    int         pos_;
    int         len_;
  };

  //------

  // file
  class File {
   public:
    File(const char *filename="") :
     filename_(filename), fp_(nullptr) {
    }

   ~File() {
      close();
    }

    bool isValid() const { return (fp_ != nullptr); }

    State open() {
      close();

      fp_ = fopen(filename_.c_str(), "r");

      if (! fp_)
        return State::error("Failed to open'" + filename_ + "'");

      return State::success();
    }

    void close() {
      if (! fp_) return;

      fclose(fp_);

      fp_ = nullptr;
    }

    bool readLine(Line &line) {
      if (! fp_ || feof(fp_))
        return false;

      line.clear();

      int c;

      while (fp_ && ((c = fgetc(fp_)) != EOF)) {
        line.addChar(char(c));

        if (c == '\n') break;
      }

      return true;
    }

   private:
    std::string  filename_;
    FILE        *fp_;
  };

  //------

  // number (integer or real)
  class Number {
   public:
    static Number makeBoolean(bool   b) { return Number(b); }
    static Number makeInteger(int    i) { return Number(i); }
    static Number makeReal   (double r) { return Number(r); }

    static Number plus  (const Number &n1, const Number &n2) { return doOp(n1, n2, Plus  ()); }
    static Number minus (const Number &n1, const Number &n2) { return doOp(n1, n2, Minus ()); }
    static Number times (const Number &n1, const Number &n2) { return doOp(n1, n2, Times ()); }
    static Number divide(const Number &n1, const Number &n2) { return doOp(n1, n2, Divide()); }
    static Number mod   (const Number &n1, const Number &n2) { return doOp(n1, n2, Mod   ()); }

    static Number And(const Number &n1, const Number &n2) { return doBoolOp(n1, n2, doAnd()); }
    static Number Or (const Number &n1, const Number &n2) { return doBoolOp(n1, n2, doOr ()); }
    static Number Xor(const Number &n1, const Number &n2) { return doBoolOp(n1, n2, doXor()); }

    static Number min(const Number &n1, const Number &n2) { return doOp(n1, n2, Min()); }
    static Number max(const Number &n1, const Number &n2) { return doOp(n1, n2, Max()); }

    Number() :
     t_(INTEGER_TYPE) {
      v_.i = 0;
    }

    bool isBoolean() const { return t_ == BOOLEAN_TYPE; }
    bool isInteger() const { return t_ == INTEGER_TYPE; }
    bool isReal   () const { return t_ == REAL_TYPE   ; }

    bool   boolean() const { return (! isReal() ? bool  (v_.i) : bool  (v_.r)); }
    int    integer() const { return (! isReal() ? int   (v_.i) : int   (v_.r)); }
    double real   () const { return (! isReal() ? double(v_.i) : double(v_.r)); }

    void setBoolean(bool   b) { t_ = BOOLEAN_TYPE; v_.i = b; }
    void setInteger(int    i) { t_ = INTEGER_TYPE; v_.i = i; }
    void setReal   (double r) { t_ = REAL_TYPE   ; v_.r = r; }

    Number abs() const { return (! isReal() ? makeInteger(std:: abs(integer())) :
                                              makeReal   (std::fabs(real   ()))); }

    Number neg() const { return (! isReal() ? makeInteger(-integer()) : makeReal(-real())); }

    Number Not() const {
      return (isBoolean() ? makeBoolean(! boolean()) : makeInteger(~integer()));
    }

    static int cmp(const Number &n1, const Number &n2) {
      if (! n1.isReal() && ! n2.isReal()) {
        if      (n1.integer() > n2.integer()) return  1;
        else if (n1.integer() < n2.integer()) return -1;
        else                                  return  0;
      }
      else {
        if      (n1.real() > n2.real()) return  1;
        else if (n1.real() < n2.real()) return -1;
        else                            return  0;
      }
    }

    void inc(const Number &i) {
      if (! isReal() && ! i.isReal()) setInteger(integer() + i.integer());
      else                            setReal   (real   () + i.real   ());
    }

    void print(std::ostream &os) const {
      if (isBoolean())
        os << (boolean() ? "TRUE" : "FALSE");
      else
        os << (isInteger() ? integer() : real());
    }

   private:
    template<typename OP>
    static Number doOp(const Number &n1, const Number &n2, OP op) {
      if (! n1.isReal() && ! n2.isReal()) return Number(op.exec(n1.integer(), n2.integer()));
      else                                return Number(op.exec(n1.real   (), n2.real   ()));
    }

    template<typename OP>
    static Number doBoolOp(const Number &n1, const Number &n2, OP op) {
      if (n1.isBoolean() && n2.isBoolean())
        return makeBoolean(op.exec(n1.boolean(), n2.boolean()));
      else
        return makeInteger(op.exec(n1.integer(), n2.integer()));
    }

    struct Plus   { template<typename T> T exec(const T &a, const T &b) { return a + b; } };
    struct Minus  { template<typename T> T exec(const T &a, const T &b) { return a - b; } };
    struct Times  { template<typename T> T exec(const T &a, const T &b) { return a * b; } };
    struct Divide { template<typename T> T exec(const T &a, const T &b) { return a / b; } };

    struct Mod {
      template<typename T> T exec(const T &a, const T &b) {
        assert(b != T(0));

        int factor = int(a/b);

        return a - (b*factor);
      }
    };

    struct Min { template<typename T> T exec(const T &a, const T &b) { return std::min(a, b); } };
    struct Max { template<typename T> T exec(const T &a, const T &b) { return std::max(a, b); } };

    struct doAnd { int exec(int a, int b) { return (a & b); } };
    struct doOr  { int exec(int a, int b) { return (a | b); } };
    struct doXor { int exec(int a, int b) { return (a ^ b); } };

   private:
    Number(bool   b) : t_(BOOLEAN_TYPE) { v_.i = b; }
    Number(int    i) : t_(INTEGER_TYPE) { v_.i = i; }
    Number(double r) : t_(REAL_TYPE   ) { v_.r = r; }

   private:
    enum Type {
      BOOLEAN_TYPE,
      INTEGER_TYPE,
      REAL_TYPE
    };

    union Value {
      int    i;
      double r;
    };

    Type  t_;
    Value v_;
  };

  //------

  class Token;

  typedef std::shared_ptr<Token> TokenP;

  // token (boolean, number, builtin, variable, procedure)
  class Token {
   public:
    enum TokenType {
      NO_TOKEN,
      BOOLEAN_TOKEN,
      NUMBER_TOKEN,
      BUILTIN_TOKEN,
      VAR_BASE_TOKEN,
      PROCEDURE_TOKEN
    };

   public:
    static TokenP makeNumber(Number n);

    Token(TokenType tokenType=NO_TOKEN) :
     tokenType_(tokenType) {
    }

    virtual ~Token() { }

    TokenType type() const { return tokenType_; }

    bool isBoolean  () const { return type() == BOOLEAN_TOKEN  ; }
    bool isNumber   () const { return type() == NUMBER_TOKEN   ; }
    bool isBuiltin  () const { return type() == BUILTIN_TOKEN  ; }
    bool isVarBase  () const { return type() == VAR_BASE_TOKEN ; }
    bool isProcedure() const { return type() == PROCEDURE_TOKEN; }

    bool isVariable() const;
    bool isVarRef  () const;

    virtual TokenP dup() const { assert(false); }

    virtual bool isImmutable() const { return true; }

    virtual bool isExecutable() const { return false; }

    virtual bool isNull() const { return false; }

    virtual bool isBlock() const { return false; }

    virtual State cmp(const TokenP &, int &) const {
      return State::error("cmp not supported");
    }

    virtual State inc(const Number &) {
      return State::error("inc not supported");
    }

    virtual State exec() { return State::error("Not implemented"); }

    virtual void print(std::ostream &os) const = 0;

   protected:
    TokenType tokenType_;
  };

  typedef std::vector<TokenP> TokenArray;

  class BooleanToken;

  typedef std::shared_ptr<BooleanToken> BooleanTokenP;

  // boolean token (needed ?)
  class BooleanToken : public Token {
   public:
    static BooleanTokenP fromToken(TokenP token) {
      return std::static_pointer_cast<BooleanToken>(token);
    }

    BooleanToken(bool b) :
     Token(BOOLEAN_TOKEN), b_(b) {
    }

    bool value() const { return b_; }

    void print(std::ostream &os) const override { os << (b_ ? "TRUE" : "FALSE"); }

   protected:
    bool b_;
  };

  class NumberToken;

  typedef std::shared_ptr<NumberToken> NumberTokenP;

  // number token
  class NumberToken : public Token {
   public:
    static NumberTokenP fromToken(TokenP token) {
      return std::static_pointer_cast<NumberToken>(token);
    }

    static NumberTokenP makeBoolean(int b) {
      return std::make_shared<NumberToken>(Number::makeBoolean(b));
    }

    static NumberTokenP makeInteger(int i) {
      return std::make_shared<NumberToken>(Number::makeInteger(i));
    }

    static NumberTokenP makeReal(double r) {
      return std::make_shared<NumberToken>(Number::makeReal(r));
    }

    static NumberTokenP makeNumber(const Number &n) {
      return std::make_shared<NumberToken>(n);
    }

    TokenP dup() const override { return std::make_shared<NumberToken>(number_); }

    const Number &number() const { return number_; }

    bool isInteger() const { return number_.isInteger(); }
    bool isReal   () const { return number_.isReal   (); }

    int    integer() const { return number_.integer(); }
    double real   () const { return number_.real   (); }

    void setInteger(int    i) { number_.setInteger(i); }
    void setReal   (double r) { number_.setReal   (r); }

    State cmp(const TokenP &token, int &res) const override {
      if (token->isNumber())
         res = Number::cmp(number_, NumberToken::fromToken(token)->number_);
      else
        return State::error("cmp not supported");

      return State::success();
    }

    State inc(const Number &n) override {
      number_.inc(n);

      return State::success();
    }

    void print(std::ostream &os) const override;

   public: // private
    NumberToken(const Number &number) :
     Token(NUMBER_TOKEN), number_(number) {
    }

   protected:
    Number number_;
  };

  class Builtin;

  typedef std::shared_ptr<Builtin> BuiltinP;

  // builtin token
  class Builtin : public Token {
   public:
    enum BuiltinType {
      // Stack manipulation
      DUP_BUILTIN,
      DROP_BUILTIN,
      SWAP_BUILTIN,
      OVER_BUILTIN,
      ROT_BUILTIN,
      PICK_BUILTIN,
      ROLL_BUILTIN,
      QDUP_BUILTIN,
      DEPTH_BUILTIN,
      POP_RET_BUILTIN,
      PUSH_RET_BUILTIN,
      COPY_RET_BUILTIN,

      // Comparison
      LESS_BUILTIN,
      EQUAL_BUILTIN,
      GREATER_BUILTIN,
      ULESS_BUILTIN,
      NOT_BUILTIN,

      // Arithmetic and Logical
      PLUS_BUILTIN,
      MINUS_BUILTIN,
      TIMES_BUILTIN,
      DIVIDE_BUILTIN,
      MOD_BUILTIN,
      DMOD_BUILTIN,
      PLUS1_BUILTIN,
      PLUS2_BUILTIN,
      MULDIV_BUILTIN,
      MAX_BUILTIN,
      MIN_BUILTIN,
      ABS_BUILTIN,
      NEGATE_BUILTIN,
      AND_BUILTIN,
      OR_BUILTIN,
      XOR_BUILTIN,

      // Memory
      FETCH_BUILTIN,
      STORE_BUILTIN,
      PFETCH_BUILTIN,
      ADDSTORE_BUILTIN,
      MOVE_BUILTIN,
      FILL_BUILTIN,

      // Control structures
      DO_BUILTIN,
      LOOP_BUILTIN,
      ILOOP_BUILTIN,
      I_BUILTIN,
      J_BUILTIN,
      LEAVE_BUILTIN,
      IF_BUILTIN,
      ELSE_BUILTIN,
      THEN_BUILTIN,
      BEGIN_BUILTIN,
      UNTIL_BUILTIN,
      WHILE_BUILTIN,
      REPEAT_BUILTIN,
      // EXIT
      // EXECUTE

      // Input/Output
      EMIT_BUILTIN,
      PRINTTO_BUILTIN,
      TYPE_BUILTIN,
      COUNT_BUILTIN,
      TRAILING_BUILTIN,
      KEY_BUILTIN,
      EXPECT_BUILTIN,
      QUERY_BUILTIN,
      WORD_BUILTIN,

      // Number Input/Output
      DECIMAL_BUILTIN,
      PRINT_BUILTIN,
      PSTACK_BUILTIN,
      // U.
      // CONVERT
      // <#

      // Mass storgate input/output
      // LIST
      LOAD_BUILTIN,
      // SCR
      // BLOCK
      // UPDATE
      // BUFFER
      // SAVE-BUFFERS
      // EMPTY-BUFFERS

      // Defining Words
      DEFINE_BUILTIN,
      VARIABLE_BUILTIN,
      CONSTANT_BUILTIN,
      // VOCABULARY
      CREATE_BUILTIN,
      COMMA_BUILTIN,
      DOES_BUILTIN,
      // CONTEXT
      // CURRENT
      // FORTH
      // DEFINITIONS
      // '
      // FIND
      FORGET_BUILTIN,

      // Compiler
      ALLOT_BUILTIN,
      // IMMEDIATE
      // LITERAL
      // STATE
      // [
      // ]
      // COMPILE
      // [COMPILE]

      // Misc
      COMMENT_BUILTIN,
      HERE_BUILTIN,
      // PAD
      // >IN
      // BLk
      ABORT_BUILTIN,
      QUIT_BUILTIN,

      DEBUG_BUILTIN,

      USER_BUILTIN=1000
    };

   public:
    static BuiltinP fromToken(TokenP token) {
      return std::static_pointer_cast<Builtin>(token);
    }

    Builtin(BuiltinType builtinType, const std::string &name) :
     Token(BUILTIN_TOKEN), builtinType_(builtinType), name_(name) {
    }

    BuiltinType builtinType() const { return builtinType_; }

    const std::string &name() const { return name_; }

    bool isExecutable() const override { return true; }

    virtual bool hasModifier() const { return false; }

    virtual State readModifier() { return State::success(); }

    virtual State exec() override = 0;

    void print(std::ostream &os) const override { os << name_; }

   private:
    BuiltinType builtinType_;
    std::string name_;
  };

  //------

  class VarBase;

  typedef std::shared_ptr<VarBase> VarBaseP;

  class VariableRef;

  typedef std::shared_ptr<VariableRef> VariableRefP;

  // base class for variable type tokens
  class VarBase : public Token {
   public:
    enum VarBaseType {
      VARIABLE_TYPE,
      VAR_REF_TYPE
    };

   public:
    static VarBaseP fromToken(TokenP token) {
      return std::static_pointer_cast<VarBase>(token);
    }

    VarBase(VarBaseType varBaseType) :
     Token(VAR_BASE_TOKEN), varBaseType_(varBaseType) {
    }

    bool isVariable() const { return (varBaseType_ == VARIABLE_TYPE); }
    bool isVarRef  () const { return (varBaseType_ == VAR_REF_TYPE ); }

    virtual const std::string &name() const = 0;

    virtual int ind() const = 0;

    virtual void setInd(int ind) = 0;

    virtual TokenP value() const = 0;

    virtual bool setValue(const TokenP &token) = 0;

    virtual TokenP indValue(int ind) const = 0;

    virtual bool setIndValue(int ind, TokenP value) = 0;

    virtual int length() const = 0;

    virtual bool isConstant() const { return false; }

    virtual long addr() const = 0;

    virtual VariableRefP indexVar(VarBaseP var, int ind) = 0;

    State cmp(const TokenP &token, int &res) const override {
      if (token->isVarBase()) {
        long p1 =                            addr();
        long p2 = VarBase::fromToken(token)->addr();

        if      (p1 > p2) res =  1;
        else if (p1 < p2) res = -1;
        else              res =  0;
      }
      else
        return State::error("cmp not supported");

      return State::success();
    }

    State inc(const Number &n) override {
      setInd(ind() + n.integer());

      return State::success();
    }

   private:
    VarBaseType varBaseType_;
  };

  //------

  class Variable;

  typedef std::shared_ptr<Variable> VariableP;

  // variable token
  class Variable : public VarBase {
   public:
    static VariableP fromToken(TokenP token) {
      return std::static_pointer_cast<Variable>(token);
    }

    Variable(const std::string &name) :
     VarBase(VARIABLE_TYPE), name_(name), ind_(0), constant_(false) {
    }

    const std::string &name() const override { return name_; }

    int ind() const override { return ind_; }

    void setInd(int ind) override { ind_ = ind; }

    TokenP value() const override {
      return indValue(ind_);
    }

    virtual bool setValue(const TokenP &value) override {
      return setIndValue(ind_, value);
    }

    TokenP indValue(int ind) const override {
      if (ind >= 0 && ind < int(values_.size()))
        return values_[ind];
      else
        return TokenP();
    }

    bool setIndValue(int ind, TokenP value) override {
      if (ind < 0 || ind >= int(values_.size()))
        return false;

      values_[ind] = value;

      return true;
    }

    int length() const override { return int(values_.size() - ind_); }

    void setInteger(int i);

    bool getInteger(int &i) const;

    bool isConstant() const override { return constant_; }

    void setConstant(bool constant) { constant_ = constant; }

    void setExecTokens(const TokenArray &execTokens) { execTokens_ = execTokens; }

    State execTokens();

    void allot(int n) {
      for (int i = 0; i < n; ++i)
        addValue(NumberToken::makeInteger(0));
    }

    void addValue(TokenP token) {
      values_.push_back(token);
    }

    long addr() const override { return long(this) + ind_; }

    VariableRefP indexVar(VarBaseP var, int ind) override {
      return std::make_shared<VariableRef>(var, ind + ind_);
    }

    void print(std::ostream &os) const override {
      if (isConstant())
        value()->print(os);
      else
        os << "$" << name();
    }

   protected:
    std::string name_;
    TokenArray  values_;
    int         ind_;
    bool        constant_;
    TokenArray  execTokens_;
  };

  //------

  // variable ref token
  // TODO: better base class
  class VariableRef : public VarBase {
   public:
    VariableRef(VarBaseP var, int ind) :
     VarBase(VAR_REF_TYPE), var_(var), ind_(ind) {
    }

    TokenP dup() const override { return std::make_shared<VariableRef>(var_, ind_); }

    const std::string &name() const override { return var_->name(); }

    int ind() const override { return ind_; }

    void setInd(int ind) override { ind_ = ind; }

    TokenP value() const override {
      return var_->indValue(ind_);
    }

    bool setValue(const TokenP &value) override {
      return var_->setIndValue(ind_, value);
    }

    TokenP indValue(int ind) const override {
      return var_->indValue(ind_ + ind);
    }

    bool setIndValue(int ind, TokenP value) override {
      return var_->setIndValue(ind_ + ind, value);
    }

    int length() const override { return var_->length() - ind_; }

    long addr() const override { return var_->addr() + ind_; }

    VariableRefP indexVar(VarBaseP, int ind) override {
      return var_->indexVar(var_, ind + ind_);
    }

    void print(std::ostream &os) const override {
      var_->print(os);

      os << "[" << ind_ << "]";
    }

   private:
    VarBaseP var_;
    int      ind_;
  };

  //------

  class Procedure;

  typedef std::shared_ptr<Procedure> ProcedureP;

  // procedure token
  class Procedure : public Token {
   public:
    static ProcedureP fromToken(TokenP token) {
      return std::static_pointer_cast<Procedure>(token);
    }

    Procedure(const std::string &name, const TokenArray &tokens) :
     Token(PROCEDURE_TOKEN), name_(name), tokens_(tokens) {
    }

    const std::string &name() const { return name_; }

    const TokenArray &tokens() { return tokens_; }

    bool isExecutable() const override { return true; }

    State exec() override;

    void print(std::ostream &os) const override {
      os << ": " << name_ << " ";

      for (auto token : tokens_) {
        token->print(os);

        os << " ";
      }

      os << ";";
    }

   private:
    std::string name_;
    TokenArray  tokens_;
  };

  //------

  // if builtin exec data
  struct IfTokens {
    TokenArray ifTokens;
    TokenArray elseTokens;
  };

  // do builtin exec data
  struct DoTokens {
    TokenArray tokens;
    bool       incToken;
    bool       leave;

    DoTokens() : incToken(false), leave(false) { }
  };

  // begin/while begin/while builtin exec data
  struct BeginTokens {
    TokenArray tokens;
    TokenArray whileTokens;
    bool       is_until;
    bool       is_while;
    bool       leave;
  };

  // builtin class builder
  #define BUILTIN_DEF(ID,N,STR) \
  class ID##Builtin : public Builtin { \
   public: \
    ID##Builtin() : Builtin(N##_BUILTIN,STR) { } \
    State exec() override; \
  }; \
  typedef std::shared_ptr<ID##Builtin> ID##BuiltinP;

  // empty builtin class builder
  #define NULL_BUILTIN_DEF(ID,N,STR) \
  class ID##Builtin : public Builtin { \
   public: \
    ID##Builtin() : Builtin(N##_BUILTIN,STR) { } \
    State exec() override { return State::success(); } \
  }; \
  typedef std::shared_ptr<ID##Builtin> ID##BuiltinP;

  // builtin class builder (with parsing support)
  #define MOD_BUILTIN_DEF(ID,N,STR,VALUE,VNAME,EXTRA) \
  class ID##Builtin : public Builtin { \
   public: \
    ID##Builtin(const VALUE &value=VALUE()) : Builtin(N##_BUILTIN,STR), VNAME(value) { } \
    TokenP dup() const override { return std::make_shared<ID##Builtin>(VNAME); } \
    bool hasModifier() const override { return true; } \
    const VALUE &getValue() const { return VNAME; } \
    VALUE &getValue() { return VNAME; } \
    State readModifier() override; \
    void print(std::ostream &os) const override; \
    State exec() override; \
    EXTRA \
   private: \
    VALUE VNAME; \
  }; \
  typedef std::shared_ptr<ID##Builtin> ID##BuiltinP;

  #define DO_BLOCK bool isBlock() const override { return true; } State exec1(TokenP &, TokenP &);
  #define IS_BLOCK bool isBlock() const override { return true; }
  #define IS_NULL  bool isNull () const override { return true; }
  #define NO_DEF

  // Stack manipulation
  BUILTIN_DEF(Dup    , DUP     , "DUP"  )
  BUILTIN_DEF(Drop   , DROP    , "DROP" )
  BUILTIN_DEF(Swap   , SWAP    , "SWAP" )
  BUILTIN_DEF(Over   , OVER    , "OVER" )
  BUILTIN_DEF(Rot    , ROT     , "ROT"  )
  BUILTIN_DEF(Pick   , PICK    , "PICK" )
  BUILTIN_DEF(Roll   , ROLL    , "ROLL" )
  BUILTIN_DEF(QDup   , QDUP    , "?DUP" )
  BUILTIN_DEF(Depth  , DEPTH   , "DEPTH")
  BUILTIN_DEF(PopRet , POP_RET , ">R"   )
  BUILTIN_DEF(PushRet, PUSH_RET, "R>"   )
  BUILTIN_DEF(CopyRet, COPY_RET, "e@"   )

  // Comparison
  BUILTIN_DEF(Less   , LESS   , "<"  )
  BUILTIN_DEF(Equal  , EQUAL  , "="  )
  BUILTIN_DEF(Greater, GREATER, ">"  )
  BUILTIN_DEF(ULess  , ULESS  , "U<" )
  BUILTIN_DEF(Not    , NOT    , "NOT")

  // Arithmetic and Logical
  BUILTIN_DEF(Plus  , PLUS  , "+"     )
  BUILTIN_DEF(Minus , MINUS , "-"     )
  BUILTIN_DEF(Times , TIMES , "*"     )
  BUILTIN_DEF(Divide, DIVIDE, "/"     )
  BUILTIN_DEF(Mod   , MOD   , "MOD"   )
  BUILTIN_DEF(DMod  , DMOD  , "/MOD"  )
  BUILTIN_DEF(Plus1 , PLUS1 , "1+"    )
  BUILTIN_DEF(Plus2 , PLUS2 , "2+"    )
  BUILTIN_DEF(MulDiv, MULDIV, "*/"    )
  BUILTIN_DEF(Max   , MAX   , "MAX"   )
  BUILTIN_DEF(Min   , MIN   , "MIN"   )
  BUILTIN_DEF(Abs   , ABS   , "ABS"   )
  BUILTIN_DEF(Negate, NEGATE, "NEGATE")
  BUILTIN_DEF(And   , AND   , "AND"   )
  BUILTIN_DEF(Or    , OR    , "OR"    )
  BUILTIN_DEF(Xor   , XOR   , "XOR"   )

  // Memory
  BUILTIN_DEF(Fetch   , FETCH   , "@")
  BUILTIN_DEF(Store   , STORE   , "!")
  BUILTIN_DEF(PFetch  , PFETCH  , "?")
  BUILTIN_DEF(AddStore, ADDSTORE, "+!")
  BUILTIN_DEF(Move    , MOVE    , "MOVE")
  BUILTIN_DEF(Fill    , FILL    , "FILL")

  // Control structures
  MOD_BUILTIN_DEF (Do    , DO    , "DO"    , DoTokens, tokens_, DO_BLOCK)
  NULL_BUILTIN_DEF(Loop  , LOOP  , "LOOP"  )
  NULL_BUILTIN_DEF(ILoop , ILOOP , "+LOOP" )
  BUILTIN_DEF     (I     , I     , "I"     )
  BUILTIN_DEF     (J     , J     , "J"     )
  BUILTIN_DEF     (Leave , LEAVE , "LEAVE" )
  MOD_BUILTIN_DEF (If    , IF    , "IF"    , IfTokens   , tokens_, IS_BLOCK)
  NULL_BUILTIN_DEF(Else  , ELSE  , "ELSE"  )
  NULL_BUILTIN_DEF(Then  , THEN  , "THEN"  )
  MOD_BUILTIN_DEF (Begin , BEGIN , "BEGIN" , BeginTokens, tokens_, IS_BLOCK)
  NULL_BUILTIN_DEF(Until , UNTIL , "UNTIL" )
  NULL_BUILTIN_DEF(While , WHILE , "WHILE" )
  NULL_BUILTIN_DEF(Repeat, REPEAT, "REPEAT")

  // Input/Output
  BUILTIN_DEF    (Emit    , EMIT    , "EMIT")
  MOD_BUILTIN_DEF(PrintTo , PRINTTO , ".\"", std::string, text_, NO_DEF)
  BUILTIN_DEF    (Type    , TYPE    , "TYPE")
  BUILTIN_DEF    (Count   , COUNT   , "COUNT")
  BUILTIN_DEF    (Trailing, TRAILING, "-TRAILING")
  BUILTIN_DEF    (Key     , KEY     , "KEY")
  BUILTIN_DEF    (Expect  , EXPECT  , "EXPECT")
  BUILTIN_DEF    (Query   , QUERY   , "QUERY")
  BUILTIN_DEF    (Word    , WORD    , "WORD")

  // Number Input/Output
  BUILTIN_DEF(Decimal, DECIMAL, "DECIMAL")
  BUILTIN_DEF(Print  , PRINT  , ".")
  BUILTIN_DEF(PStack , PSTACK , "PSTACK")

  // Mass storgate input/output
  MOD_BUILTIN_DEF(Load, LOAD, "LOAD", std::string, filename_, NO_DEF)

  // Defining Words
  BUILTIN_DEF    (Define  , DEFINE  , ":"       )
  BUILTIN_DEF    (Variable, VARIABLE, "VARIABLE")
  BUILTIN_DEF    (Constant, CONSTANT, "CONSTANT")
  BUILTIN_DEF    (Create  , CREATE  , "CREATE"  )
  BUILTIN_DEF    (Comma   , COMMA   , ","       )
  MOD_BUILTIN_DEF(Does    , DOES    , "DOES>", TokenArray, tokens_, NO_DEF)
  BUILTIN_DEF    (Forget  , FORGET  , "FORGET"  )

  // Compiler
  BUILTIN_DEF(Allot, ALLOT, "ALLOT")

  // Misc
  MOD_BUILTIN_DEF(Comment, COMMENT, "(", std::string, text_, IS_NULL)
  BUILTIN_DEF    (Here   , HERE   , "HERE")
  BUILTIN_DEF    (Abort  , ABORT  , "ABORT")
  BUILTIN_DEF    (Quit   , QUIT   , "QUIT" )
  BUILTIN_DEF    (Debug  , DEBUG  , "DEBUG")

  //------

  void setDebug(bool debug=true);

  bool isDebug();

  State init();

  State parseFile(const char *filename);
  State parseLine(const Line &line);

  State parseTokens();
  State parseToken(TokenP &token);

  bool fillBuffer();

  State parseWord(const Word &word, TokenP &token);

  bool readWord(Word &word);
  bool readWord(Line &line, Word &word);

  State readNumberToken(std::string &word, NumberTokenP &number);
  State readNumberToken(Line &line, NumberTokenP &number);

  bool lookupBuiltin(const std::string &str, BuiltinP &builtin);

  template<typename T>
  void defBuiltin() {
    addBuiltin(std::make_shared<T>());
  }

  void addBuiltin(const BuiltinP &builtin);

  void pushToken(const TokenP &token);
  void pushDupToken(const TokenP &token);

  void pushBoolean(bool b);
  void pushInteger(int n);
  void pushNumber(const Number &n);

  State peekToken(TokenP &token);
  State peekToken(int n, TokenP &token);

  State popToken (TokenP &token);
  State popToken (int n, TokenP &token);
  State popTokens(TokenP &token1, TokenP &token2);
  State popTokens(TokenP &token1, TokenP &token2, TokenP &token3);

  State popBoolean(bool &b);

  State popNumber (Number &n);
  State popNumbers(Number &n1, Number &n2);
  State popNumbers(Number &n1, Number &n2, Number &n3);

  State popBoolOrNumber (Number &n);
  State popBoolOrNumbers(Number &n1, Number &n2);

  State popVarBase (VarBaseP &var);
  State popVarRef  (VarBaseP &var);
  State popVariable(VariableP &var);

  State popProcedure(ProcedureP &var);

  void clearTokens();
  void clearRetTokens();
  void clearExecTokens();

  State execToken(const TokenP &token);

  State cmpOp (int &cmp);
  State ucmpOp(int &cmp);

  VariableP defineVariable(const std::string &name, int i);
  VariableP defineVariable(const std::string &name, TokenP token);
  VariableP defineVariable(const std::string &name);
  bool      forgetVariable(const std::string &name);
  bool      lookupVariable(const std::string &name, VariableP &var);

  ProcedureP defineProcedure(const std::string &name, const TokenArray &tokens);
  bool       forgetProcedure(const std::string &name);
  bool       lookupProcedure(const std::string &name, ProcedureP &proc);

  void addBlockToken(TokenArray &tokens, const TokenP &token);

  int getBase();

  bool isBaseChar(int c, int base, int *value);

  State toBaseInteger(const std::string &str, int base, long *integer);

  std::string toBaseString(int base, int integer);

  std::string toUpper(const std::string &str);
}

#endif
