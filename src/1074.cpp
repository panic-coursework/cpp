#include <cassert>
#include <exception>
#include <fstream>
#include <iostream>
#include <map>
#include <regex>
#include <string>
#include <unordered_map>
#include <vector>

constexpr const char *kDivideByZero = "DIVIDE BY ZERO";
constexpr const char *kInvalidNumber = "INVALID NUMBER";
constexpr const char *kLineNumberError = "LINE NUMBER ERROR";
constexpr const char *kSyntaxError = "SYNTAX ERROR";
constexpr const char *kVariableNotDefined = "VARIABLE NOT DEFINED";
constexpr const char *kHelp = "Yet another basic interpreter";

constexpr const char *keyWords[] = {"REM","LET","PRINT","INPUT","END","GOTO","IF","THEN","RUN","LIST","CLEAR","QUIT","HELP"};

template <typename T>
void print (T var) { std::cout << var << std::endl; }

class Error : public std::exception {
 public:
  const char *message;
  Error (const char *message) : message(message) {}
};

class Statement;

struct ProgramState {
  int pc = -1;
  std::unordered_map<std::string, int> variables;
  std::map<int, Statement *> program;
};

ProgramState state;
std::istream *stream = &std::cin;

const std::regex kReName {R"(^[a-zA-Z0-9]+$)"};
void checkName (const std::string &name) {
  for (const auto &kw : keyWords) {
    if (name.find(kw) != std::string::npos) throw Error(kSyntaxError);
  }
  if (!std::regex_match(name, kReName)) throw Error(kSyntaxError);
}
void checkLine (int line) {
  if (state.program.count(line) < 1) throw Error(kLineNumberError);
}

struct Expression {
  virtual int eval () = 0;
  virtual ~Expression () = default;
};
enum Op { ADD, SUB, MUL, DIV };
struct OpExpression : public Expression {
  Op op;
  Expression *lhs;
  Expression *rhs;
  OpExpression (Op op, Expression *lhs, Expression *rhs) : op(op), lhs(lhs), rhs(rhs) {}
  int eval () {
    int lhsValue = lhs->eval(), rhsValue = rhs->eval();
    switch (op) {
      case ADD: return lhsValue + rhsValue;
      case SUB: return lhsValue - rhsValue;
      case MUL: return lhsValue * rhsValue;
      case DIV: {
        if (rhsValue == 0) throw Error(kDivideByZero);
        return lhsValue / rhsValue;
      }
      default: assert(false);
    }
  }
};
struct GroupExpression : public Expression {
  Expression *group;
  GroupExpression (Expression *group) : group(group) {}
  int eval () { return group->eval(); }
};
struct VariableExpression : public Expression {
  std::string variable;
  VariableExpression (const std::string &variable) : variable(variable) {}
  int eval () {
    if (state.variables.count(variable) < 1) throw Error(kVariableNotDefined);
    return state.variables[variable];
  }
};
struct LiteralExpression : public Expression {
  int literal;
  LiteralExpression (int literal) : literal(literal) {}
  int eval () { return literal; }
};
struct Group {
  int start;
  int end;
  Expression *parsed;
};

struct {
  const int literal = 1;
  const int variable = 2;
  const int group = 3;
  const int lhspm = 4;
  const int pm = 5;
  const int rhspm = 6;
  const int lhsmul = 7;
  const int mul = 8;
  const int rhsmul = 9;
} kExprGroups;
const std::regex kReExpr {R"(^(?:(\d+)|([a-zA-Z0-9]+)|(?:__GRP_(\d+)__)|(?:(.+)\s*([+\-])\s*(.+))|(?:(.+)\s*([*/])\s*(.+)))$)"};
const std::regex kReParens {R"([()])"};
Expression *parseExpr (std::string expr, const std::vector<Group> baseGroups = {}) {
  while (expr.front() == ' ' || expr.front() == '\r' || expr.front() == '\n') expr.erase(expr.begin());
  while (expr.back() == ' ' || expr.back() == '\r' || expr.back() == '\n') expr.pop_back();
  std::vector<Group> groups;
  if (std::regex_search(expr, kReParens)) {
    auto symbol = [] (int i) { return std::string("__GRP_") + std::to_string(i) + std::string("__"); };
    int depth = 0, start;
    for (int i = 0; i < expr.length(); ++i) {
      if (expr[i] == '(') {
        if (depth == 0) start = i;
        ++depth;
      } else if (expr[i] == ')') {
        --depth;
        if (depth == 0) groups.push_back({ .start = start, .end = i, .parsed = parseExpr(expr.substr(start + 1, i - start - 1)) });
        if (depth < 0) throw Error(kSyntaxError);
      }
    }
    if (depth > 0) throw Error(kSyntaxError);
    std::string originalExpr = expr;
    expr = originalExpr.substr(0, groups[0].start);
    for (int i = 0; i < groups.size(); ++i) {
      expr += symbol(i + baseGroups.size());
      expr += originalExpr.substr(groups[i].end + 1, (i + 1 < groups.size() ? groups[i + 1].start : originalExpr.size()) - groups[i].end - 1);
    }
  }
  // I know it's slow... But performance isn't critical here.
  for (int i = baseGroups.size() - 1; i >= 0; --i) groups.insert(groups.begin(), baseGroups[i]);
  std::smatch match;
  if (!std::regex_match(expr, match, kReExpr)) throw Error(kSyntaxError);
  if (match[kExprGroups.literal].matched) return new LiteralExpression(std::stoi(match[kExprGroups.literal].str()));
  if (match[kExprGroups.variable].matched) return new VariableExpression(match[kExprGroups.variable].str());
  if (match[kExprGroups.group].matched) return new GroupExpression(groups[std::stoi(match[kExprGroups.group].str())].parsed);
  if (match[kExprGroups.pm].matched) {
    return new OpExpression(
      match[kExprGroups.pm].str() == "+" ? ADD : SUB,
      parseExpr(match[kExprGroups.lhspm].str(), groups),
      parseExpr(match[kExprGroups.rhspm].str(), groups)
    );
  }
  if (match[kExprGroups.mul].matched) {
    return new OpExpression(
      match[kExprGroups.mul].str() == "*" ? MUL : DIV,
      parseExpr(match[kExprGroups.lhsmul].str(), groups),
      parseExpr(match[kExprGroups.rhsmul].str(), groups)
    );
  }
  assert(false);
}

const std::regex kReStatement {R"(^(?:(?:(\d+)\s*)?(?:(REM\s*.+)|(LET\s*([^ ]+)\s+=\s+(.+))|(PRINT\s*(.+))|(INPUT\s*([^ ]+))|(END\s*(?:))|(GOTO\s*(\d+))|(IF\s*(.+)\s+([<>=])\s+(.+)\s+THEN\s+(\d+))|(RUN\s*)|(LIST\s*)|(CLEAR\s*)|(QUIT\s*)|(HELP\s*))?)$)"};
struct {
  const int lineNumber = 1;
  const int rem = 2;
  const int let = 3;
  const int letName = 4;
  const int letExpr = 5;
  const int print = 6;
  const int printExpr = 7;
  const int input = 8;
  const int inputName = 9;
  const int end = 10;
  const int goto_ = 11;
  const int gotoLine = 12;
  const int if_ = 13;
  const int ifLhs = 14;
  const int ifCmp = 15;
  const int ifRhs = 16;
  const int ifLine = 17;
  const int run = 18;
  const int list = 19;
  const int clear = 20;
  const int quit = 21;
  const int help = 22;
} kStatementGroups;

// WARNING: Memory leak ahead!

enum RunnableType { REM, LET, PRINT, INPUT, END, GOTO, IF, RUN, LIST, CLEAR, QUIT, HELP };
class Runnable {
 public:
  virtual void run () {}
  virtual RunnableType getType () = 0;
  virtual ~Runnable () {}
};

class Statement : public Runnable {
 public:
  std::string source;
};

class RemStatement : public Statement {
 public:
  RunnableType getType () { return REM; }
  RemStatement (std::smatch) {}
};

class LetStatement : public Statement {
 public:
  RunnableType getType () { return LET; }
  Expression *expr;
  std::string name;
  LetStatement (std::smatch match) {
    name = match[kStatementGroups.letName].str();
    checkName(name);
    expr = parseExpr(match[kStatementGroups.letExpr].str());
  }
  void run () {
    state.variables[name] = expr->eval();
  }
};

class PrintStatement : public Statement {
 public:
  RunnableType getType () { return PRINT; }
  Expression *expr;
  PrintStatement (std::smatch match) {
    expr = parseExpr(match[kStatementGroups.printExpr].str());
  }
  void run () { print(expr->eval()); }
};

const std::regex kInputRegex {R"(^-?\d+$)"};
class InputStatement : public Statement {
 public:
  RunnableType getType () { return INPUT; }
  std::string name;
  InputStatement (std::smatch match) {
    name = match[kStatementGroups.inputName].str();
    checkName(name);
  }
  std::string read () {
    std::cout << " ? ";
    std::string input;
    std::getline(*stream, input);
    return input;
  }
  void run () {
    std::string input = read();
    while (!std::regex_match(input, kInputRegex)) {
      print(kInvalidNumber);
      input = read();
    }
    state.variables[name] = std::stoi(input);
  }
};

class EndStatement : public Statement {
 public:
  RunnableType getType () { return END; }
  EndStatement (std::smatch) {}
};

class GotoStatement : public Statement { // considered harmful
 public:
  RunnableType getType () { return GOTO; }
  int line;
  GotoStatement (std::smatch match) {
    line = std::stoi(match[kStatementGroups.gotoLine].str());
  }
  void run () {
    checkLine(line);
    state.pc = line;
  }
};

class IfStatement : public Statement {
 public:
  RunnableType getType () { return IF; }
  Expression *lhs, *rhs;
  char cmp;
  int line;
  IfStatement (std::smatch match) {
    lhs = parseExpr(match[kStatementGroups.ifLhs].str());
    rhs = parseExpr(match[kStatementGroups.ifRhs].str());
    cmp = match[kStatementGroups.ifCmp].str()[0];
    line = std::stoi(match[kStatementGroups.ifLine].str());
  }
  bool conditionMet (int lhs, int rhs) {
    switch (cmp) {
      case '>': return lhs > rhs;
      case '<' : return lhs < rhs;
      case '=': return lhs == rhs;
      default: assert(false);
    }
  }
  void run () {
    if (conditionMet(lhs->eval(),rhs->eval())) {
      checkLine(line);
      state.pc = line;
    }
  }
};

class Command : public Runnable {};

class ListCommand : public Command {
 public:
  RunnableType getType () { return LIST; }
  void run () {
    for (const auto &[ line, expr ] : state.program) print(expr->source);
  }
};

class ClearCommand : public Command {
 public:
  RunnableType getType () { return CLEAR; }
  void run () {
    state = ProgramState();
  }
};
class QuitCommand : public Command {
 public:
  RunnableType getType () { return QUIT; }
  void run () { exit(0); }
};
class HelpCommand : public Command {
 public:
  RunnableType getType () { return HELP; }
  void run () { print(kHelp); }
};

class RunCommand : public Command {
 public:
  RunnableType getType () { return RUN; }
  void run () {
    if (state.program.size() == 0) return;
    state.pc = state.program.begin()->first;
    try {
      while (true) {
        Statement *stmt = state.program[state.pc];
        if (stmt->getType() == END) break;
        auto it = state.program.upper_bound(state.pc);
        state.pc = it == state.program.end() ? -1 : it->first;
        stmt->run();
        if (state.pc == -1) break;
      }
    } catch (const Error &e) {
      print(e.message);
    }
  }
};

constexpr RunnableType kStatementTypes[] = { REM, LET, PRINT, INPUT, END, GOTO, IF };
constexpr RunnableType kCommandTypes[] = { RUN, LIST, CLEAR, QUIT, HELP };
constexpr RunnableType kImmediateStatementTypes[] = { LET, PRINT, INPUT };

void oneLine () {
  if (stream->eof()) exit(0);
  std::string line;
  std::getline(*stream, line);
  if (line.size() == 0) return;
  std::smatch match;
  if (!std::regex_match(line, match, kReStatement)) throw Error(kSyntaxError);
  Runnable *runnable = nullptr;
  if (match[kStatementGroups.rem].matched) runnable = new RemStatement(match);
  if (match[kStatementGroups.let].matched) runnable = new LetStatement(match);
  if (match[kStatementGroups.print].matched) runnable = new PrintStatement(match);
  if (match[kStatementGroups.input].matched) runnable = new InputStatement(match);
  if (match[kStatementGroups.end].matched) runnable = new EndStatement(match);
  if (match[kStatementGroups.goto_].matched) runnable = new GotoStatement(match);
  if (match[kStatementGroups.if_].matched) runnable = new IfStatement(match);
  if (match[kStatementGroups.run].matched) runnable = new RunCommand();
  if (match[kStatementGroups.list].matched) runnable = new ListCommand();
  if (match[kStatementGroups.clear].matched) runnable = new ClearCommand();
  if (match[kStatementGroups.quit].matched) runnable = new QuitCommand();
  if (match[kStatementGroups.help].matched) runnable = new HelpCommand();
  int lineNumber = -1;
  if (match[kStatementGroups.lineNumber].matched) lineNumber = std::stoi(match[kStatementGroups.lineNumber].str());
  if (runnable == nullptr) {
    if (lineNumber < 0) throw Error(kSyntaxError);
    state.program.erase(lineNumber);
    return;
  }
  bool isCommand = false;
  for (const auto &type : kCommandTypes) if (runnable->getType() == type) isCommand = true;
  if (isCommand) {
    if (lineNumber >= 0) throw Error(kSyntaxError);
    runnable->run();
    return;
  }
  dynamic_cast<Statement *>(runnable)->source = line;
  if (lineNumber >= 0) {
    state.program[lineNumber] = dynamic_cast<Statement *>(runnable);
  } else {
    bool isImmediateStatement = false;
    for (const auto &type : kImmediateStatementTypes) if (runnable->getType() == type) isImmediateStatement = true;
    if (!isImmediateStatement) throw Error(kSyntaxError);
    runnable->run();
  }
}

int main (int argc, char **argv) {
  std::ios_base::sync_with_stdio(false);
  std::cin.tie(NULL);
  if (argc > 1) {
    std::fstream fs;
    fs.open(argv[1], std::ios_base::in);
    stream = &fs;
  }
  while (true) try { oneLine(); } catch (const Error &e) { print(e.message); }
}
