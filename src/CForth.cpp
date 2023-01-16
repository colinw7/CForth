#include <CForth.h>
#include <termios.h>
#include <climits>
#include <unistd.h>

#include <map>

namespace CForth {

static std::string base_chars = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

enum ParseState {
  INTERP_STATE,
  COMPILE_STATE
};

typedef std::vector<VariableP>           Variables;
typedef std::vector<ProcedureP>          Procedures;
typedef std::vector<ParseState>          ParseStateStack;
typedef std::vector<Line>                Lines;
typedef std::map<std::string,TokenP>     NameTokenMap;
typedef std::map<std::string,Variables>  NameVariablesMap;
typedef std::map<std::string,Procedures> NameProceduresMap;
typedef std::map<std::string,BuiltinP>   NameBuiltinMap;

State State::lastError_ = State(false, "Unknown Error");

bool debug_       = false;
bool ignore_base_ = false;

struct IgnoreBase {
  IgnoreBase() { ignore_base_ = true ; }
 ~IgnoreBase() { ignore_base_ = false; }
};

File              file_;
Lines             lines_;
Line              line_;
TokenArray        tokens_;
TokenArray        execTokens_;
TokenArray        retTokens_;
NameVariablesMap  variables_;
NameProceduresMap procedures_;
NameBuiltinMap    builtins_;
VariableP         currentVar_;
VariableP         wordVar_;

ParseState      parseState_ = INTERP_STATE;
ParseStateStack parseStateStack_;

struct SetParseState {
  SetParseState(ParseState state) {
    parseStateStack_.push_back(parseState_);

    parseState_ = state;
  }

 ~SetParseState() {
    parseState_ = parseStateStack_.back();

    parseStateStack_.pop_back();
  }
};

int
getch()
{
  struct termios oldt;

  tcgetattr(STDIN_FILENO, &oldt); /*store old settings */

  struct termios newt = oldt; /* copy old settings to new settings */

  newt.c_lflag &= ~(ICANON | ECHO); /* make one change to old settings in new settings */

  tcsetattr(STDIN_FILENO, TCSANOW, &newt); /*apply the new settings immediatly */

  int ch = getchar(); /* standard getchar call */

  tcsetattr(STDIN_FILENO, TCSANOW, &oldt); /*reapply the old settings */

  return ch; /*return received char */
}

void
setDebug(bool debug)
{
  debug_ = debug;
}

bool
isDebug()
{
  return debug_;
}

State
init()
{
  VariableP var = defineVariable("BASE", 10);

  //----

  const char *env = getenv("HOME");

  if (! env)
    return State::success();

  std::string filename = std::string(env) + "/.CForth";

  File file(filename.c_str());

  if (! file.open())
    return State::success();

  Line line;

  while (file.readLine(line)) {
    if (! parseLine(line))
      return State::lastError();
  }

  return State::success();
}

State
parseFile(const char *filename)
{
  file_ = File(filename);

  if (! file_.open())
    return State::lastError();

  try {
    if (! parseTokens())
      return State::lastError();
  }
  catch (...) {
  }

  if (isDebug() && ! tokens_.empty()) {
    IgnoreBase ib;

    for (auto token : tokens_) {
      token->print(std::cout);

      std::cout << " ";
    }

    std::cout << std::endl;
  }

  std::cout << "ok" << std::endl;

  file_.close();

  return State::success();
}

State
parseLine(const Line &line)
{
  lines_.push_back(line);

  try {
    if (! parseTokens())
      return State::lastError();
  }
  catch (...) {
  }

  if (isDebug() && ! tokens_.empty()) {
    IgnoreBase ib;

    for (auto token : tokens_) {
      token->print(std::cout);

      std::cout << " ";
    }

    std::cout << std::endl;
  }

  return State::success();
}

State
parseTokens()
{
  for (;;) {
    if (! fillBuffer())
      break;

    TokenP token;

    if (! parseToken(token))
      return State::lastError();

    if (! token.get())
      break;

    if (! execToken(token))
      return State::lastError();
  }

  return State::success();
}

State
parseToken(TokenP &token)
{
  Word word;

  if (! readWord(word))
    return State::success();

  if (! parseWord(word, token))
    return State::lastError();

  return State::success();
}

bool
fillBuffer()
{
  if (line_.isValid())
    line_.skipSpace();

  if (file_.isValid()) {
    while (! line_.isValid()) {
      if (! file_.readLine(line_))
        return false;

      line_.skipSpace();
    }
  }
  else {
    while (! line_.isValid()) {
      if (lines_.empty())
        return false;

      line_ = lines_.back();

      lines_.pop_back();

      line_.skipSpace();
    }
  }

  return true;
}

State
parseWord(const Word &word, TokenP &token)
{
  std::string str = word.value();

  VariableP    var;
  ProcedureP   proc;
  BuiltinP     builtin;
  NumberTokenP number;

  if      (lookupVariable(str, var))
    token = (var->isConstant() ? var->value() : var);
  else if (lookupProcedure(str, proc))
    token = proc;
  else if (lookupBuiltin(str, builtin)) {
    if (builtin->hasModifier()) {
      builtin = std::static_pointer_cast<Builtin>(builtin->dup());

      if (! builtin->readModifier())
        return State::lastError();
    }

    token = builtin;
  }
  else if (readNumberToken(str, number))
    token = number;
  else
    return State::error(str + " ?");

  return State::success();
}

bool
readWord(Word &word)
{
  word.reset();

  if (! fillBuffer())
    return false;

  return readWord(line_, word);
}

bool
readWord(Line &line, Word &word)
{
  word.reset();

  std::string str;

  str += line.getChar();

  while (line.isValid() && ! line.isSpace())
    str += line.getChar();

  word.setValue(str);

  while (line.isValid() && line.isSpace())
    line.skipChar();

  return true;
}

State
readNumberToken(std::string &word, NumberTokenP &token)
{
  Line line(word);

  return readNumberToken(line, token);
}

State
readNumberToken(Line &line, NumberTokenP &token)
{
  int pos = line.pos();

  int base = getBase();

  std::string str;

  int sign = 1;

  if (line.isOneOf("+-"))
    sign = (line.getChar() == '-' ? -1 : 1);

  if (! line.isBaseChar(base)) {
    line.setPos(pos);
    return State::error("Not a number");
  }

  while (line.isValid() && line.isBaseChar(base))
    str += line.getChar();

  bool real = false;

  if (line.isValid() && line.isChar('.')) {
    real = true;

    str += line.getChar();

    while (line.isValid() && line.isBaseChar(base))
      str += line.getChar();
  }

  if (line.isValid() && line.isOneOf("Ee")) {
    char c = line.lookNextChar();

    if (isBaseChar(c, base)) {
      real = true;

      str += line.getChar();

      while (line.isValid() && line.isBaseChar(base))
        str += line.getChar();
    }
  }

  if (line.isValid() && ! line.isSpace()) {
    line.setPos(pos);
    return State::error("Not a number");
  }

  if (! real) {
    long il;

    if (! toBaseInteger(str, base, &il)) {
      line.setPos(pos);
      return State::lastError();
    }

    token = NumberToken::makeInteger(int(sign*il));
  }
  else {
    double r = atof(str.c_str());

    token = NumberToken::makeReal(sign*r);
  }

  return State::success();
}

bool
lookupBuiltin(const std::string &str, BuiltinP &builtin)
{
  if (builtins_.empty()) {
    // Stack manipulation
    defBuiltin<DupBuiltin    >();
    defBuiltin<DropBuiltin   >();
    defBuiltin<SwapBuiltin   >();
    defBuiltin<OverBuiltin   >();
    defBuiltin<RotBuiltin    >();
    defBuiltin<PickBuiltin   >();
    defBuiltin<RollBuiltin   >();
    defBuiltin<QDupBuiltin   >();
    defBuiltin<DepthBuiltin  >();
    defBuiltin<PopRetBuiltin >();
    defBuiltin<PushRetBuiltin>();
    defBuiltin<CopyRetBuiltin>();

    // Comparison
    defBuiltin<LessBuiltin   >();
    defBuiltin<EqualBuiltin  >();
    defBuiltin<GreaterBuiltin>();
    // D<
    defBuiltin<ULessBuiltin  >();
    defBuiltin<NotBuiltin    >();

    // Arithmetic and Logical
    defBuiltin<PlusBuiltin  >();
    defBuiltin<MinusBuiltin >();
    defBuiltin<TimesBuiltin >();
    defBuiltin<DivideBuiltin>();
    defBuiltin<ModBuiltin   >();
    defBuiltin<DModBuiltin  >();
    defBuiltin<Plus1Builtin >();
    // 1-
    defBuiltin<Plus2Builtin >();
    // 2-
    // D+
    defBuiltin<MulDivBuiltin>();
    // */MOD
    // U*
    // U/MOD
    defBuiltin<MaxBuiltin   >();
    defBuiltin<MinBuiltin   >();
    defBuiltin<AbsBuiltin   >();
    defBuiltin<NegateBuiltin>();
    // DNEGATE
    defBuiltin<AndBuiltin>();
    defBuiltin<OrBuiltin >();
    defBuiltin<XorBuiltin>();

    // Memory
    defBuiltin<FetchBuiltin   >();
    defBuiltin<StoreBuiltin   >();
    // C@
    // C!
    defBuiltin<PFetchBuiltin  >();
    defBuiltin<AddStoreBuiltin>();
    defBuiltin<MoveBuiltin    >();
    // CMOVE
    defBuiltin<FillBuiltin>();

    // Control structures
    defBuiltin<DoBuiltin    >();
    defBuiltin<LoopBuiltin  >();
    defBuiltin<ILoopBuiltin >();
    defBuiltin<IBuiltin     >();
    defBuiltin<JBuiltin     >();
    defBuiltin<LeaveBuiltin >();
    defBuiltin<IfBuiltin    >();
    defBuiltin<ElseBuiltin  >();
    defBuiltin<ThenBuiltin  >();
    defBuiltin<BeginBuiltin >();
    defBuiltin<UntilBuiltin >();
    defBuiltin<WhileBuiltin >();
    defBuiltin<RepeatBuiltin>();
    // EXIT
    // EXECUTE

    // Input/Output
    defBuiltin<EmitBuiltin    >();
    defBuiltin<PrintToBuiltin >();
    defBuiltin<TypeBuiltin    >();
    defBuiltin<CountBuiltin   >();
    defBuiltin<TrailingBuiltin>();
    defBuiltin<KeyBuiltin     >();
    defBuiltin<ExpectBuiltin  >();
    defBuiltin<QueryBuiltin   >();
    defBuiltin<WordBuiltin    >();

    // Number Input/Output
    defBuiltin<DecimalBuiltin>();
    defBuiltin<PrintBuiltin  >();
    defBuiltin<PStackBuiltin >();
    // U.
    // CONVERT
    // <#

    // Mass storgate input/output
    // LIST
    defBuiltin<LoadBuiltin>();
    // SCR
    // BLOCK
    // UPDATE
    // BUFFER
    // SAVE-BUFFERS
    // EMPTY-BUFFERS

    // Defining Words
    defBuiltin<DefineBuiltin>  ();
    defBuiltin<VariableBuiltin>();
    defBuiltin<ConstantBuiltin>();
    // VOCABULARY
    defBuiltin<CreateBuiltin>();
    defBuiltin<CommaBuiltin >();
    defBuiltin<DoesBuiltin  >();
    // CONTEXT
    // CURRENT
    // FORTH
    // DEFINITIONS
    // '
    // FIND
    defBuiltin<ForgetBuiltin>();

    // Compiler
    defBuiltin<AllotBuiltin>();
    // IMMEDIATE
    // LITERAL
    // STATE
    // [
    // ]
    // COMPILE
    // [COMPILE]

    // Misc
    defBuiltin<CommentBuiltin>();
    defBuiltin<HereBuiltin   >();
    // PAD
    // >IN
    // BLk
    defBuiltin<AbortBuiltin>();
    defBuiltin<QuitBuiltin >();
    defBuiltin<DebugBuiltin>();
  }

  auto p = builtins_.find(toUpper(str));

  if (p == builtins_.end())
    return false;

  builtin = (*p).second;

  return true;
}

void
addBuiltin(const BuiltinP &builtin)
{
  builtins_[builtin->name()] = builtin;
}

void
pushToken(const TokenP &token)
{
  if (isDebug()) {
    IgnoreBase ib;

    std::cout << "Push: ";
    token->print(std::cout);
    std::cout << std::endl;
  }

  tokens_.push_back(token);
}

void
pushDupToken(const TokenP &token)
{
  TokenP dtoken = (! token->isImmutable() ? token->dup() : token);

  tokens_.push_back(dtoken);
}

void
pushBoolean(bool b)
{
  TokenP token = std::make_shared<BooleanToken>(b);

  pushToken(token);
}

void
pushInteger(int i)
{
  TokenP token = NumberToken::makeInteger(i);

  pushToken(token);
}

void
pushNumber(const Number &n)
{
  TokenP token = NumberToken::makeNumber(n);

  pushToken(token);
}

State
peekToken(TokenP &token)
{
  if (tokens_.empty())
    return State::error("STACK EMPTY");

  token = tokens_.back();

  if (isDebug()) {
    IgnoreBase ib;

    std::cout << "Peek: ";
    token->print(std::cout);
    std::cout << std::endl;
  }

  return State::success();
}

State
peekToken(int n, TokenP &token)
{
  auto nt = tokens_.size();

  if (n <= 0) return State::error("Invalid index");

  if (n > int(nt)) return State::error("Stack too small");

  token = tokens_[nt - n];

  if (isDebug()) {
    IgnoreBase ib;

    std::cout << "Peek(" << n << ") : ";
    token->print(std::cout);
    std::cout << std::endl;
  }

  return State::success();
}

State
popToken(TokenP &token)
{
  if (tokens_.empty())
    return State::error("STACK EMPTY");

  token = tokens_.back();

  tokens_.pop_back();

  if (isDebug()) {
    IgnoreBase ib;

    std::cout << "Pop: ";
    token->print(std::cout);
    std::cout << std::endl;
  }

  return State::success();
}

State
popToken(int n, TokenP &token)
{
  auto nt = tokens_.size();

  if (n <= 0) return State::error("Invalid index");

  if (n > int(nt)) return State::error("Stack too small");

  token = tokens_[nt - n];

  for (uint nn1 = uint(nt - n); nn1 < nt; ++nn1)
    tokens_[nn1] = tokens_[nn1 + 1];

  tokens_.pop_back();

  if (isDebug()) {
    IgnoreBase ib;

    std::cout << "Pop(" << n << ") : ";
    token->print(std::cout);
    std::cout << std::endl;
  }

  return State::success();
}

State
popTokens(TokenP &token1, TokenP &token2)
{
  if (! popToken(token2)) return State::lastError();
  if (! popToken(token1)) return State::lastError();

  return State::success();
}

State
popTokens(TokenP &token1, TokenP &token2, TokenP &token3)
{
  if (! popToken(token3)) return State::lastError();
  if (! popToken(token2)) return State::lastError();
  if (! popToken(token1)) return State::lastError();

  return State::success();
}

State
popBoolean(bool &b)
{
  TokenP token;

  if (! popToken(token)) return State::lastError();

  if      (token->isNumber())
    b = (NumberToken::fromToken(token)->integer() != 0);
  else if (token->isBoolean())
    b = BooleanToken::fromToken(token)->value();
  else
    return State::error("must be integer or boolean");

  return State::success();
}

State
tokenToNumber(TokenP token, Number &n)
{
  if (token->isVarBase()) {
    VarBaseP var = VarBase::fromToken(token);

    if (var->isConstant())
      token = var->value();
  }

  if (! token->isNumber())
    return State::error("must be number");

  n = NumberToken::fromToken(token)->number();

  return State::success();
}

State
popNumber(Number &n)
{
  TokenP token;

  if (! popToken(token)) return State::lastError();

  if (! tokenToNumber(token, n)) return State::lastError();

  return State::success();
}

State
popBoolOrNumber(Number &n)
{
  TokenP token;

  if (! popToken(token)) return State::lastError();

  if (token->isVarBase()) {
    VarBaseP var = VarBase::fromToken(token);

    if (var->isConstant())
      token = var->value();
  }

  if      (token->isNumber())
    n = NumberToken::fromToken(token)->number();
  else if (token->isBoolean())
    n = Number::makeBoolean(BooleanToken::fromToken(token)->value());
  else
    return State::error("must be integer or boolean");

  return State::success();
}

State
popNumbers(Number &n1, Number &n2)
{
  if (! popNumber(n2)) return State::lastError();
  if (! popNumber(n1)) return State::lastError();

  return State::success();
}

State
popBoolOrNumbers(Number &n1, Number &n2)
{
  if (! popBoolOrNumber(n2)) return State::lastError();
  if (! popBoolOrNumber(n1)) return State::lastError();

  return State::success();
}

State
popNumbers(Number &n1, Number &n2, Number &n3)
{
  if (! popNumber(n3)) return State::lastError();
  if (! popNumber(n2)) return State::lastError();
  if (! popNumber(n1)) return State::lastError();

  return State::success();
}

State
popVarBase(VarBaseP &var)
{
  TokenP token;

  if (! popToken(token)) return State::lastError();

  if (! token->isVarBase()) return State::error("must be base variable");

  var = VarBase::fromToken(token);

  return State::success();
}

State
popVarRef(VarBaseP &var)
{
  TokenP token;

  if (! popToken(token)) return State::lastError();

  if (! token->isVarRef()) return State::error("must be ref variable");

  var = VarBase::fromToken(token);

  return State::success();
}

State
popVariable(VariableP &var)
{
  TokenP token;

  if (! popToken(token)) return State::lastError();

  if (! token->isVariable()) return State::error("must be variable");

  var = Variable::fromToken(token);

  return State::success();
}

State
popProcedure(ProcedureP &var)
{
  TokenP token;

  if (! popToken(token)) return State::lastError();

  if (! token->isProcedure()) return State::error("must be procedure");

  var = Procedure::fromToken(token);

  return State::success();
}

void
clearTokens()
{
  tokens_.clear();
}

void
clearRetTokens()
{
  retTokens_.clear();
}

void
clearExecTokens()
{
  execTokens_.clear();
}

State
execToken(const TokenP &token)
{
  if (token->isExecutable()) {
    if (isDebug()) {
      IgnoreBase ib;

      std::cout << "Exec: ";
      token->print(std::cout);
      std::cout << std::endl;
    }

    if (token->isBlock()) {
      execTokens_.push_back(token);

      State state = token->exec();

      execTokens_.pop_back();

      return state;
    }
    else
      return token->exec();
  }
  else {
    pushToken(token);

    if (token->isVariable()) {
      currentVar_ = Variable::fromToken(token);

      if (! currentVar_->execTokens())
        return State::lastError();
    }

    return State::success();
  }
}

State
cmpOp(int &cmp)
{
  Number n1, n2;

  if (! popNumbers(n1, n2)) return State::lastError();

  cmp = Number::minus(n1, n2).integer();

  return State::success();
}

State
ucmpOp(int &cmp)
{
  Number n1, n2;

  if (! popNumbers(n1, n2)) return State::lastError();

  uint i1 = uint(n1.integer());
  uint i2 = uint(n2.integer());

  if      (i1 > i2) cmp =  1;
  else if (i1 < i2) cmp = -1;
  else              cmp =  0;

  return State::success();
}

VariableP
defineVariable(const std::string &name, int i)
{
  return defineVariable(name, NumberToken::makeInteger(i));
}

VariableP
defineVariable(const std::string &name, TokenP token)
{
  VariableP var = defineVariable(name);

  var->addValue(token);

  return var;
}

VariableP
defineVariable(const std::string &name)
{
  VariableP var = std::make_shared<Variable>(name);

  variables_[name].push_back(var);

  if (isDebug()) {
    IgnoreBase ib;

    std::cout << "Define Var: " << name << std::endl;
  }

  return var;
}

bool
forgetVariable(const std::string &name)
{
  auto p = variables_.find(name);

  if (p == variables_.end())
    return false;

  if (p->second.empty())
    return false;

  p->second.pop_back();

  if (isDebug()) {
    IgnoreBase ib;

    std::cout << "Forget Var: " << name << std::endl;
  }

  return true;
}

bool
lookupVariable(const std::string &name, VariableP &var)
{
  auto p = variables_.find(name);

  if (p == variables_.end())
    return false;

  if (p->second.empty())
    return false;

  var = p->second.back();

  return true;
}

ProcedureP
defineProcedure(const std::string &name, const TokenArray &tokens)
{
  ProcedureP proc = std::make_shared<Procedure>(name, tokens);

  procedures_[name].push_back(proc);

  if (isDebug()) {
    IgnoreBase ib;

    std::cout << "Define Procedure ";
    proc->print(std::cout);
    std::cout << std::endl;
  }

  return proc;
}

bool
forgetProcedure(const std::string &name)
{
  auto p = procedures_.find(name);

  if (p == procedures_.end())
    return false;

  if (p->second.empty())
    return false;

  p->second.pop_back();

  if (isDebug()) {
    IgnoreBase ib;

    std::cout << "Forget Procedure" << name << std::endl;
  }

  return true;
}

bool
lookupProcedure(const std::string &name, ProcedureP &proc)
{
  auto p = procedures_.find(name);

  if (p == procedures_.end())
    return false;

  if (p->second.empty())
    return false;

  proc = p->second.back();

  return true;
}

void
addBlockToken(TokenArray &tokens, const TokenP &token)
{
  // auto expand procedure
  // TODO: option
  if (token->isProcedure()) {
    for (auto ptoken : Procedure::fromToken(token)->tokens())
      tokens.push_back(ptoken);
  }
  // add if not null token
  else if (! token->isNull())
    tokens.push_back(token);
}

VariableP
getWordVar()
{
  if (! wordVar_.get())
    wordVar_ = std::make_shared<Variable>("WORD");

  return wordVar_;
}

int
getBase()
{
  if (ignore_base_) return 10;

  int base = 10;

  VariableP var;

  if (! lookupVariable("BASE", var))
    return base;

  if (! var->getInteger(base))
    return base;

  base = std::min(std::max(base, 2), 36);

  return base;
}

bool
isBaseChar(int c, int base, int *value)
{
  if (base < 2 || base > int(base_chars.size()))
    return false;

  int c1 = c;

  if (::islower(c1))
    c1 = ::toupper(c1);

  std::string::size_type pos = base_chars.find(char(c1));

  if (pos == std::string::npos || int(pos) >= base)
    return false;

  if (value != NULL)
    *value = int(pos);

  return true;
}

State
toBaseInteger(const std::string &str, int base, long *integer)
{
  *integer = 0;

  if (base < 2 || base > int(base_chars.size()))
    return State::error("Invalid Base");

  uint i   = 0;
  auto len = str.size();

  while (i < len) {
    int c = str[i];

    int value;

    if (! isBaseChar(c, base, &value))
      return State::error("Invalid Char");

    long integer1 = base*(*integer) + value;

    if (long((integer1 - long(value))/base) != *integer)
      return State::error("Overflow");

    *integer = integer1;

    ++i;
  }

  if (*integer > INT_MAX || *integer < INT_MIN)
    return State::error("Overflow");

  return State::success();
}

std::string
toBaseString(int base, int integer)
{
  if (base < 2 || base > int(base_chars.size()))
    return "";

  std::string str;

  if (integer < 0) {
    str += '-';
    integer = -integer;
  }

  while (integer >= base) {
    int n = integer % base;

    str = base_chars[n] + str;

    integer /= base;
  }

  str = base_chars[integer] + str;

  return str;
}

std::string
toUpper(const std::string &str)
{
  std::string ustr = str;

  auto len = ustr.size();

  for (uint i = 0; i < len; ++i)
    ustr[i] = char(toupper(ustr[i]));

  return ustr;
}

//----------

State
Variable::
execTokens()
{
  if (isDebug() && ! tokens_.empty()) {
    IgnoreBase ib;

    std::cout << "DOES>";

    for (auto token : execTokens_) {
      std::cout << " ";

      token->print(std::cout);
    }

    std::cout << std::endl;
  }

  for (auto token : execTokens_) {
    if (! execToken(token))
      return State::lastError();
  }

  return State::success();
}

void
Variable::
setInteger(int i)
{
  setValue(NumberToken::makeInteger(i));
}

bool
Variable::
getInteger(int &i) const
{
  i = 0;

  if (! value()->isNumber())
    return false;

  NumberTokenP number = NumberToken::fromToken(value());

  i = number->integer();

  return true;
}

//----------

State
Procedure::
exec()
{
  for (auto token : tokens_) {
    if (! execToken(token))
      return State::lastError();
  }

  return State::success();
}

//----------

void
NumberToken::
print(std::ostream &os) const
{
  int base = getBase();

  if (base != 10 && number_.isInteger())
    os << toBaseString(base, number_.integer());
  else
    number_.print(os);
}

//----------

bool
Token::
isVariable() const
{
  if (! isVarBase()) return false;

  const VarBase *var = static_cast<const VarBase *>(this);

  return var->isVariable();
}

bool
Token::
isVarRef() const
{
  if (! isVarBase()) return false;

  const VarBase *var = static_cast<const VarBase *>(this);

  return ! var->isConstant();
}

//-------------

// Stack manipulation
State
DupBuiltin::
exec()
{
  if (tokens_.empty()) return State::error("STACK EMPTY");

  TokenP token = tokens_.back();

  pushDupToken(token);

  if (isDebug()) {
    IgnoreBase ib;
    std::cout << "Dup: "; token->print(std::cout); std::cout << std::endl;
  }

  return State::success();
}

State
DropBuiltin::
exec()
{
  if (tokens_.empty()) return State::error("STACK EMPTY");

  if (isDebug()) {
    IgnoreBase ib;
    std::cout << "Drop: "; tokens_.back()->print(std::cout); std::cout << std::endl;
  }

  tokens_.pop_back();

  return State::success();
}

State
SwapBuiltin::
exec()
{
  auto n = tokens_.size();

  if (n < 2) return State::error("STACK EMPTY");

  if (isDebug()) {
    IgnoreBase ib;
    std::cout << "Swap: "; tokens_[n - 1]->print(std::cout);
    std::cout << " "; tokens_[n - 2]->print(std::cout); std::cout << std::endl;
  }

  std::swap(tokens_[n - 1], tokens_[n - 2]);

  return State::success();
}

State
OverBuiltin::
exec()
{
  auto nt = tokens_.size();

  if (nt < 2) return State::error("STACK UNDERFLOW");

  TokenP token = tokens_[nt - 2];

  pushDupToken(token);

  if (isDebug()) {
    IgnoreBase ib;
    std::cout << "Over: "; token->print(std::cout); std::cout << std::endl;
  }

  return State::success();
}

State
RotBuiltin::
exec()
{
  auto nt = tokens_.size();

  if (nt < 3) return State::error("STACK UNDERFLOW");

  // 1 2 3 -> 2 3 1
  TokenP token = tokens_[nt - 3];

  tokens_[nt - 3] = tokens_[nt - 2];
  tokens_[nt - 2] = tokens_[nt - 1];
  tokens_[nt - 1] = token;

  if (isDebug()) {
    IgnoreBase ib;
    std::cout << "Rot: "; token->print(std::cout); std::cout << std::endl;
  }

  return State::success();
}

State
PickBuiltin::
exec()
{
  Number n;

  if (! popNumber(n)) return State::lastError();

  if (! n.isInteger()) return State::error("Must be integer");

  int i = n.integer();

  TokenP token;

  if (! peekToken(i, token)) return State::lastError();

  pushDupToken(token);

  return State::success();
}

State
RollBuiltin::
exec()
{
  Number n;

  if (! popNumber(n)) return State::lastError();

  if (! n.isInteger()) return State::error("Must be integer");

  int i = n.integer();

  auto nt = tokens_.size();

  if (i > int(nt)) return State::error("STACK UNDERFLOW");

  TokenP token = tokens_[nt - i];

  for (uint n1 = uint(nt - i); n1 < nt; ++n1)
    tokens_[n1] = tokens_[n1 + 1];

  tokens_.pop_back();

  tokens_.push_back(token);

  if (isDebug()) {
    IgnoreBase ib;

    std::cout << "Roll(" << i << ") : ";
    token->print(std::cout);
    std::cout << std::endl;
  }

  return State::success();
}

State
QDupBuiltin::
exec()
{
  Number n;

  if (! popNumber(n)) return State::lastError();

  if (n.integer())
    pushNumber(n);

  return State::success();
}

State
DepthBuiltin::
exec()
{
  pushInteger(int(tokens_.size()));

  return State::success();
}

State
PopRetBuiltin::
exec()
{
  TokenP token;

  if (! popToken(token)) return State::lastError();

  retTokens_.push_back(token);

  return State::success();
}

State
PushRetBuiltin::
exec()
{
  if (retTokens_.empty()) return State::error("STACK EMPTY");

  TokenP token = retTokens_.back();

  retTokens_.pop_back();

  pushToken(token);

  return State::success();
}

State
CopyRetBuiltin::
exec()
{
  if (retTokens_.empty()) return State::error("STACK EMPTY");

  TokenP token = retTokens_.back();

  pushToken(token);

  return State::success();
}

// Comparison
State
LessBuiltin::
exec()
{
  int cmp = 0;

  if (! cmpOp(cmp))
    return State::lastError();

  pushBoolean(cmp < 0);

  return State::success();
}

State
EqualBuiltin::
exec()
{
  int cmp = 0;

  if (! cmpOp(cmp))
    return State::lastError();

  pushBoolean(cmp == 0);

  return State::success();
}

State
GreaterBuiltin::
exec()
{
  int cmp = 0;

  if (! cmpOp(cmp))
    return State::lastError();

  pushBoolean(cmp > 0);

  return State::success();
}

State
ULessBuiltin::
exec()
{
  int cmp = 0;

  if (! ucmpOp(cmp))
    return State::lastError();

  pushBoolean(cmp < 0);

  return State::success();
}

State
NotBuiltin::
exec()
{
  Number n;

  if (! popBoolOrNumber(n)) return State::lastError();

  pushNumber(n.Not());

  return State::success();
}

// Arithmetic and Logical
State
PlusBuiltin::
exec()
{
  auto nt = tokens_.size();

  if (nt < 2) return State::error("STACK UNDERFLOW");

  if      (tokens_[nt - 2]->isVarRef()) {
    Number n;

    if (! popNumber(n)) return State::lastError();

    VarBaseP var;

    if (! popVarRef(var)) return State::lastError();

    pushToken(var->indexVar(var, n.integer()));
  }
  else if (tokens_[nt - 1]->isVarRef()) {
    VarBaseP var;

    if (! popVarRef(var)) return State::lastError();

    Number n;

    if (! popNumber(n)) return State::lastError();

    pushToken(var->indexVar(var, n.integer()));
  }
  else {
    Number n1, n2;

    if (! popNumbers(n1, n2)) return State::lastError();

    pushNumber(Number::plus(n1, n2));
  }

  return State::success();
}

State
MinusBuiltin::
exec()
{
  auto nt = tokens_.size();

  if (nt < 2) return State::error("STACK UNDERFLOW");

  if      (tokens_[nt - 2]->isVarRef()) {
    Number n;

    if (! popNumber(n)) return State::lastError();

    VarBaseP var;

    if (! popVarRef(var)) return State::lastError();

    pushToken(var->indexVar(var, -n.integer()));
  }
  else if (tokens_[nt - 1]->isVarRef()) {
    VarBaseP var;

    if (! popVarRef(var)) return State::lastError();

    Number n;

    if (! popNumber(n)) return State::lastError();

    pushToken(var->indexVar(var, -n.integer()));
  }
  else {
    Number n1, n2;

    if (! popNumbers(n1, n2)) return State::lastError();

    pushNumber(Number::minus(n1, n2));
  }

  return State::success();
}

State
TimesBuiltin::
exec()
{
  Number n1, n2;

  if (! popNumbers(n1, n2)) return State::lastError();

  pushNumber(Number::times(n1, n2));

  return State::success();
}

State
DivideBuiltin::
exec()
{
  Number n1, n2;

  if (! popNumbers(n1, n2)) return State::lastError();

  pushNumber(Number::divide(n1, n2));

  return State::success();
}

State
ModBuiltin::
exec()
{
  Number n1, n2;

  if (! popNumbers(n1, n2)) return State::lastError();

  pushNumber(Number::mod(n1, n2));

  return State::success();
}

State
DModBuiltin::
exec()
{
  Number n1, n2;

  if (! popNumbers(n1, n2)) return State::lastError();

  pushNumber(Number::mod   (n1, n2));
  pushNumber(Number::divide(n1, n2));

  return State::success();
}

State
Plus1Builtin::
exec()
{
  if (tokens_.empty()) return State::error("STACK UNDERFLOW");

  TokenP token = tokens_.back();

  tokens_.pop_back();

  if      (token->isVarRef()) {
    VarBaseP var = VarBase::fromToken(token);

    pushToken(var->indexVar(var, 1));
  }
  else {
    Number n;

    if (! tokenToNumber(token, n)) return State::lastError();

    pushNumber(Number::plus(n, Number::makeInteger(1)));
  }

  return State::success();
}

State
Plus2Builtin::
exec()
{
  if (tokens_.empty()) return State::error("STACK UNDERFLOW");

  TokenP token = tokens_.back();

  tokens_.pop_back();

  if      (token->isVarRef()) {
    VarBaseP var = VarBase::fromToken(token);

    pushToken(var->indexVar(var, 2));
  }
  else {
    Number n;

    if (! tokenToNumber(token, n)) return State::lastError();

    pushNumber(Number::plus(n, Number::makeInteger(2)));
  }

  return State::success();
}

State
MulDivBuiltin::
exec()
{
  Number n1, n2, n3;

  if (! popNumbers(n1, n2, n3)) return State::lastError();

  pushNumber(Number::divide(Number::times(n1, n2), n3));

  return State::success();
}

State
MaxBuiltin::
exec()
{
  // TODO: use token1->cmp(token2) ???
  Number n1, n2;

  if (! popNumbers(n1, n2)) return State::lastError();

  pushNumber(Number::max(n1, n2));

  return State::success();
}

State
MinBuiltin::
exec()
{
  // TODO: use token1->cmp(token2) ???
  Number n1, n2;

  if (! popNumbers(n1, n2)) return State::lastError();

  pushNumber(Number::min(n1, n2));

  return State::success();
}

State
AbsBuiltin::
exec()
{
  Number n;

  if (! popNumber(n)) return State::lastError();

  pushNumber(n.abs());

  return State::success();
}

State
NegateBuiltin::
exec()
{
  Number n;

  if (! popNumber(n)) return State::lastError();

  pushNumber(n.neg());

  return State::success();
}

State
AndBuiltin::
exec()
{
  Number n1, n2;

  if (! popBoolOrNumbers(n1, n2)) return State::lastError();

  pushNumber(Number::And(n1, n2));

  return State::success();
}

State
OrBuiltin::
exec()
{
  Number n1, n2;

  if (! popBoolOrNumbers(n1, n2)) return State::lastError();

  pushNumber(Number::Or(n1, n2));

  return State::success();
}

State
XorBuiltin::
exec()
{
  Number n1, n2;

  if (! popBoolOrNumbers(n1, n2)) return State::lastError();

  pushNumber(Number::Xor(n1, n2));

  return State::success();
}

// Memory
State
FetchBuiltin::
exec()
{
  if (tokens_.empty()) return State::error("STACK UNDERFLOW");

  TokenP token = tokens_.back();

  tokens_.pop_back();

  if (! token->isVarRef()) return State::error("Not a variable");

  VarBaseP var = VarBase::fromToken(token);

  token = var->value();

  if (! token.get()) return State::error("invalid variable");

  tokens_.push_back(token);

  if (isDebug()) {
    IgnoreBase ib;

    std::cout << "Fetch ";
    var->print(std::cout);
    std::cout << " = ";
    token->print(std::cout);
    std::cout << std::endl;
  }

  return State::success();
}

State
StoreBuiltin::
exec()
{
  auto nt = tokens_.size();

  if (nt < 2) return State::error("STACK UNDERFLOW");

  TokenP token1 = tokens_[nt - 1];
  TokenP token2 = tokens_[nt - 2];

  tokens_.pop_back();
  tokens_.pop_back();

  if (! token1->isVarRef()) return State::error("Not a variable");

  VarBaseP var = VarBase::fromToken(token1);

  if (! var->setValue(token2)) return State::error("invalid variable");

  if (isDebug()) {
    IgnoreBase ib;

    std::cout << "Store ";
    var->print(std::cout);
    std::cout << " = ";
    token2->print(std::cout);
    std::cout << std::endl;
  }

  return State::success();
}

State
PFetchBuiltin::
exec()
{
  VarBaseP var;

  if (! popVarRef(var)) return State::lastError();

  TokenP token = var->value();

  if (! token.get()) return State::error("invalid variable");

  token->print(std::cout);

  std::cout << " ";

  return State::success();
}

State
AddStoreBuiltin::
exec()
{
  VariableP var;

  if (! popVariable(var)) return State::lastError();

  Number n;

  if (! popNumber(n)) return State::lastError();

  TokenP token = var->value();

  if (! token.get()) return State::error("invalid variable");

  if (! token->isNumber()) return State::error("var must be number");

  Number vn = NumberToken::fromToken(token)->number();

  var->setValue(NumberToken::makeNumber(Number::plus(vn, n)));

  if (isDebug()) {
    IgnoreBase ib;

    std::cout << "Set " << var->name() << " = ";
    n.print(std::cout);
    std::cout << std::endl;
  }

  return State::success();
}

State
MoveBuiltin::
exec()
{
  Number n;

  if (! popNumber(n)) return State::lastError();

  VarBaseP var1, var2;

  if (! popVarRef(var2)) return State::lastError();
  if (! popVarRef(var1)) return State::lastError();

  for (int i = 0; i < n.integer(); ++i)
    var2->setIndValue(i, var1->indValue(i));

  return State::success();
}

State
FillBuiltin::
exec()
{
  TokenP token;

  if (! popToken(token)) return State::lastError();

  Number n;

  if (! popNumber(n)) return State::lastError();

  VarBaseP var;

  if (! popVarRef(var)) return State::lastError();

  for (int i = 0; i < n.integer(); ++i)
    var->setIndValue(i, token);

  return State::success();
}

// Control structures
State
DoBuiltin::
exec()
{
  TokenP startToken, endToken;

  if (! popTokens(endToken, startToken)) return State::lastError();

  // push end and start on return stack
  startToken = startToken->dup();

  retTokens_.push_back(startToken);
  retTokens_.push_back(endToken  );

  if (! exec1(startToken, endToken)) return State::lastError();

  retTokens_.pop_back();
  retTokens_.pop_back();

  return State::success();
}

State
DoBuiltin::
exec1(TokenP &startToken, TokenP &endToken)
{
  int cmp;

  if (! endToken->cmp(startToken, cmp)) return State::lastError();

  bool up = (cmp > 0);

  Number inc = Number::makeInteger(1);

  // TODO check against return stack value
  tokens_.leave = false;

  for (;;) {
    if (! endToken->cmp(startToken, cmp)) return State::lastError();

    if (up ? cmp <= 0 : cmp >= 0) break;

    for (auto token : tokens_.tokens) {
      if (! execToken(token))
        return State::lastError();

      if (tokens_.leave)
        break;
    }

    if (tokens_.leave)
      break;

    if (tokens_.incToken) {
      Number n;

      if (! popNumber(n)) return State::lastError();

      inc = n;
    }

    if (! startToken->inc(inc)) return State::lastError();
  }

  return State::success();
}

State
DoBuiltin::
readModifier()
{
  SetParseState state(COMPILE_STATE);

  for (;;) {
    Word word;

    if (! readWord(word))
      return State::error("Unterminated DO");

    TokenP token;

    if      (word == "LOOP")
      break;
    else if (word == "+LOOP") {
      tokens_.incToken = true;
      break;
    }
    else {
      if (! parseWord(word, token))
        return State::lastError();
    }

    addBlockToken(tokens_.tokens, token);
  }

  return State::success();
}

void
DoBuiltin::
print(std::ostream &os) const
{
  os << "DO ";

  for (auto token : tokens_.tokens) {
    token->print(os);

    os << " ";
  }

  if (tokens_.incToken)
    os << "+LOOP";
  else
    os << "LOOP";
}

State
IBuiltin::
exec()
{
  auto n = retTokens_.size();

  if (n < 2) return State::error("Not in DO");

  pushToken(retTokens_[n - 2]);

  return State::success();
}

State
JBuiltin::
exec()
{
  auto n = retTokens_.size();

  if (n < 4) return State::error("Not in double nested DO");

  pushToken(retTokens_[n - 4]);

  return State::success();
}

State
LeaveBuiltin::
exec()
{
  for (int n = int(execTokens_.size()) - 1; n >= 0; --n) {
    TokenP execToken = execTokens_[n];

    if (! execToken->isBuiltin()) continue;

    BuiltinP builtin = Builtin::fromToken(execToken);

    if      (builtin->builtinType() == Builtin::DO_BUILTIN) {
      DoBuiltinP doBuiltin = std::static_pointer_cast<DoBuiltin>(builtin);

      doBuiltin->getValue().leave = true;

      return State::success();
    }
    else if (builtin->builtinType() == Builtin::BEGIN_BUILTIN) {
      BeginBuiltinP beginBuiltin = std::static_pointer_cast<BeginBuiltin>(builtin);

      beginBuiltin->getValue().leave = true;

      return State::success();
    }
  }

  return State::error("Leave not inside do");
}

State
IfBuiltin::
exec()
{
  bool b;

  if (! popBoolean(b)) return State::lastError();

  if (b) {
    for (auto token : tokens_.ifTokens) {
      if (! execToken(token))
        return State::lastError();
    }
  }
  else {
    for (auto token : tokens_.elseTokens) {
      if (! execToken(token))
        return State::lastError();
    }
  }

  return State::success();
}

State
IfBuiltin::
readModifier()
{
  SetParseState state(COMPILE_STATE);

  bool in_else = false;

  for (;;) {
    Word word;

    if (! readWord(word))
      return State::error("Unterminated IF");

    TokenP token;

    if      (word == "ELSE") {
      in_else = true;
      continue;
    }
    else if (word == "THEN")
      break;
    else {
      if (! parseWord(word, token))
        return State::lastError();
    }

    if (! in_else)
      addBlockToken(tokens_.ifTokens, token);
    else
      addBlockToken(tokens_.elseTokens, token);
  }

  return State::success();
}

void
IfBuiltin::
print(std::ostream &os) const
{
  os << "IF ";

  for (auto token : tokens_.ifTokens) {
    token->print(os);

    os << " ";
  }

  if (! tokens_.elseTokens.empty()) {
    os << "ELSE ";

    for (auto token : tokens_.elseTokens) {
      token->print(os);

      os << " ";
    }
  }

  os << "THEN";
}

State
BeginBuiltin::
exec()
{
  tokens_.leave = false;

  if (tokens_.is_until) {
    for (;;) {
      for (auto token : tokens_.tokens) {
        if (! execToken(token))
          return State::lastError();

        if (tokens_.leave)
          break;
      }

      if (tokens_.leave)
        break;

      bool b;

      if (! popBoolean(b))
        return State::lastError();

      if (b)
        break;
    }
  }
  else {
    for (;;) {
      for (auto token : tokens_.whileTokens) {
        if (! execToken(token))
          return State::lastError();

        if (tokens_.leave)
          break;
      }

      if (tokens_.leave)
        break;

      bool b;

      if (! popBoolean(b))
        return State::lastError();

      if (b)
        break;

      for (auto token : tokens_.tokens) {
        if (! execToken(token))
          return State::lastError();

        if (tokens_.leave)
          break;
      }

      if (tokens_.leave)
        break;
    }
  }

  return State::success();
}

State
BeginBuiltin::
readModifier()
{
  SetParseState state(COMPILE_STATE);

  tokens_.is_until = false;
  tokens_.is_while = false;

  for (;;) {
    Word word;

    if (! readWord(word))
      return State::error("Unterminated BEGIN");

    TokenP token;

    if      (word == "UNTIL") {
      tokens_.is_until = true;
      tokens_.is_while = false;

      break;
    }
    else if (word == "REPEAT") {
      if (! tokens_.is_while)
        return State::error("Missing WHILE");

      break;
    }
    else if (word == "WHILE") {
      tokens_.is_until = false;
      tokens_.is_while = true;

      tokens_.whileTokens = tokens_.tokens;

      tokens_.tokens.clear();

      continue;
    }
    else {
      if (! parseWord(word, token))
        return State::lastError();
    }

    addBlockToken(tokens_.tokens, token);
  }

  return State::success();
}

void
BeginBuiltin::
print(std::ostream &os) const
{
  os << "BEGIN ";

  if (tokens_.is_until) {
    for (auto token : tokens_.tokens) {
      token->print(os);

      os << " ";
    }

    os << "UNTIL";
  }
  else {
    for (auto token : tokens_.whileTokens) {
      token->print(os);

      os << " ";
    }

    os << "WHILE";

    for (auto token : tokens_.tokens) {
      token->print(os);

      os << " ";
    }

    os << "REPEAT";
  }
}

// Input/Output
State
EmitBuiltin::
exec()
{
  Number n;

  if (! popNumber(n))
    return State::lastError();

  std::cout << char(n.integer());

  return State::success();
}

State
PrintToBuiltin::
exec()
{
  std::cout << text_;

  return State::success();
}

State
PrintToBuiltin::
readModifier()
{
  if (! fillBuffer())
    return State::error("Missing char");

  text_ = line_.getChar();

  while (line_.isValid() && ! line_.isChar('"'))
    text_ += line_.getChar();

  if (line_.isChar('"'))
    line_.skipChar();

  return State::success();
}

void
PrintToBuiltin::
print(std::ostream &os) const
{
  os << ".\" ";
  os << text_;
  os << "\"";
}

State
TypeBuiltin::
exec()
{
  Number n;

  if (! popNumber(n)) return State::lastError();

  VarBaseP var;

  if (! popVarRef(var)) return State::lastError();

  for (int i = 0; i < n.integer(); ++i) {
    TokenP token = var->indValue(i);

    if (token->isNumber())
      std::cout << char(NumberToken::fromToken(token)->integer());
  }

  return State::success();
}

State
KeyBuiltin::
exec()
{
  char c = char(getch());

  pushInteger(c);

  return State::success();
}

State
ExpectBuiltin::
exec()
{
  Number n;

  if (! popNumber(n)) return State::lastError();

  VarBaseP var;

  if (! popVarRef(var)) return State::lastError();

  for (int i = 0; i < n.integer(); ++i) {
    char c = char(fgetc(stdin));

    if (c == '\n')
      break;

    var->setIndValue(i, NumberToken::makeInteger(c));
  }

  return State::success();
}

State
QueryBuiltin::
exec()
{
  int n = 80;

  std::string str;

  for (int i = 0; i < n; ++i) {
    char c = char(fgetc(stdin));

    if (! str.empty() && c == '\n')
      break;

    str += c;
  }

  line_.insert(str);

  return State::success();
}

State
WordBuiltin::
exec()
{
  wordVar_ = getWordVar();

  Number n;

  if (! popNumber(n)) return State::lastError();

  if (! fillBuffer())
    return State::error("Missing char");

  char lastC = char(n.integer());

  std::string str;

  str += line_.getChar();

  while (line_.isValid() && ! line_.isChar(lastC))
    str += line_.getChar();

  if (line_.isChar(lastC))
    line_.getChar();

  if (isDebug())
    std::cout << "Word: '" << str << "'" << std::endl;

  auto len = str.size();

  if (wordVar_->length() < int(len + 1))
    wordVar_->allot(int(len + 1 - wordVar_->length()));

  wordVar_->setIndValue(0, NumberToken::makeInteger(int(len)));

  for (size_t i = 1; i <= len; ++i)
    wordVar_->setIndValue(int(i), NumberToken::makeInteger(str[i - 1]));

  pushToken(wordVar_);

  return State::success();
}

State
CountBuiltin::
exec()
{
  VarBaseP var;

  if (! popVarRef(var)) return State::lastError();

  pushToken(var->indexVar(var, 1));

  pushToken(var->indValue(0));

  return State::success();
}

State
TrailingBuiltin::
exec()
{
  Number n;

  if (! popNumber(n)) return State::lastError();

  VarBaseP var;

  if (! popVarRef(var)) return State::lastError();

  int i = n.integer() - 1;

  while (i >= 0) {
    TokenP token = var->indValue(i);

    if (! token->isNumber())
      break;

    if (! isspace(char(NumberToken::fromToken(token)->integer())))
      break;

    --i;
  }

  pushToken(var);

  pushNumber(Number::makeInteger(i + 1));

  return State::success();
}

// Number Input/Output
State
DecimalBuiltin::
exec()
{
  VariableP var;

  if (! lookupVariable("BASE", var))
    var = defineVariable("BASE", 10);

  return State::success();
}

State
PrintBuiltin::
exec()
{
  TokenP token;

  if (! popToken(token)) {
    std::cout << "0" << std::endl;

    return State::lastError();
  }

  if (token->isVariable()) {
    VariableP var = Variable::fromToken(token);

    if (var->isConstant())
      token = var->value();
  }

  token->print(std::cout);

  std::cout << " ";

  return State::success();
}

State
PStackBuiltin::
exec()
{
  auto nt = tokens_.size();

  for (size_t i = 0; i < nt; ++i) {
    if (i > 0) std::cout << " ";

    tokens_[i]->print(std::cout);
  }

  return State::success();
}

// Mass storage input/output
State
LoadBuiltin::
exec()
{
  if (! parseFile(filename_.c_str()))
    return State::success();

  return State::success();
}

State
LoadBuiltin::
readModifier()
{
  if (! fillBuffer())
    return State::error("Missing char");

  Word filename;

  if (! readWord(filename))
    return State::error("Missing word");

  filename_ = filename.value();

  return State::success();
}

void
LoadBuiltin::
print(std::ostream &os) const
{
  os << "LOAD \"" << filename_ << "\"";
}

// Defining Words
State
DefineBuiltin::
exec()
{
  SetParseState state(COMPILE_STATE);

  TokenArray tokens;

  Word name;

  if (! readWord(name))
    return State::error("Missing word");

  Word word;

  for (;;) {
    if (! readWord(word))
      return State::error("Missing word");

    if (word == ";")
      break;

    TokenP token;

    if (! parseWord(word, token))
      return State::lastError();

    addBlockToken(tokens, token);
  }

  defineProcedure(name.value(), tokens);

  return State::success();
}

State
VariableBuiltin::
exec()
{
  Word word;

  if (! readWord(word))
    return State::error("Missing word");

  currentVar_ = defineVariable(word.value(), 0);

  return State::success();
}

State
ConstantBuiltin::
exec()
{
  TokenP token;

  if (! popToken(token)) return State::lastError();

  Word word;

  if (! readWord(word))
    return State::error("Missing word");

  VariableP var = defineVariable(word.value(), token);

  var->setConstant(true);

  return State::success();
}

State
CreateBuiltin::
exec()
{
  Word word;

  if (! readWord(word))
    return State::error("Missing word");

  currentVar_ = defineVariable(word.value());

  return State::success();
}

State
CommaBuiltin::
exec()
{
  if (tokens_.empty()) return State::error("STACK EMPTY");

  TokenP token = tokens_.back();

  tokens_.pop_back();

  if (! currentVar_.get()) return State::error("No current variable");

  currentVar_->addValue(token);

  if (isDebug()) {
    currentVar_->print(std::cout);
    std::cout << " , ";
    token->print(std::cout);
    std::cout << std::endl;
  }

  return State::success();
}

State
DoesBuiltin::
exec()
{
  if (! currentVar_.get())
    return State::error("No current variable");

  currentVar_->setExecTokens(tokens_);

  return State::success();
}

State
DoesBuiltin::
readModifier()
{
  SetParseState state(COMPILE_STATE);

  for (;;) {
    if (! fillBuffer())
      return State::error("Missing char");

    int pos = line_.pos();

    Word word;

    if (! readWord(word))
      return State::error("Missing word");

    if (word == ";") {
      line_.setPos(pos);
      break;
    }

    TokenP token;

    if (! parseWord(word, token))
      return State::lastError();

    addBlockToken(tokens_, token);
  }

  return State::success();
}

void
DoesBuiltin::
print(std::ostream &os) const
{
  os << "DOES> ";

  for (auto token : tokens_) {
    token->print(os);

    os << " ";
  }
}

State
ForgetBuiltin::
exec()
{
  Word word;

  if (! readWord(word))
    return State::error("Missing word");

  VariableP  var;
  ProcedureP proc;

  if      (lookupVariable(word.value(), var)) {
    if (forgetVariable(word.value()))
      return State::error("Unknown variable");
  }
  else if (lookupProcedure(word.value(), proc)) {
    if (forgetProcedure(word.value()))
      return State::error("Unknown procedure");
  }
  else
    return State::error("Unknown word");

  return State::success();
}

// Compiler
State
AllotBuiltin::
exec()
{
  Number n;

  if (! popNumber(n)) return State::lastError();

  if (! currentVar_.get())
    return State::error("No current variable");

  currentVar_->allot(n.integer());

  return State::success();
}

// Misc
State
CommentBuiltin::
exec()
{
  return State::success();
}

State
CommentBuiltin::
readModifier()
{
  if (! fillBuffer())
    return State::error("Missing char");

  text_ = line_.getChar();

  while (line_.isValid() && ! line_.isChar(')'))
    text_ += line_.getChar();

  if (line_.isChar(')'))
    line_.skipChar();

  return State::success();
}

void
CommentBuiltin::
print(std::ostream &os) const
{
  os << "( ";
  os << text_;
  os << ")";
}

State
HereBuiltin::
exec()
{
  wordVar_ = getWordVar();

  pushToken(wordVar_);

  return State::success();
}

State
AbortBuiltin::
exec()
{
  clearRetTokens ();
  clearExecTokens();
  clearTokens    ();

  throw abortSignal();

  return State::success();
}

State
QuitBuiltin::
exec()
{
  clearRetTokens ();
  clearExecTokens();

  throw quitSignal();

  return State::success();
}

State
DebugBuiltin::
exec()
{
  Number n;

  if (! popNumber(n)) return State::lastError();

  setDebug(n.integer());

  return State::success();
}

}
