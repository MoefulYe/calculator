#include <iostream>
#include <string>
#include <unordered_map>

using std::string;
using std::string_view;

typedef std::unordered_map<string, int> Context;

struct Token {
  enum class Type { Oper, Num, Eof, Ident, Assign };

  enum class Op {
    Plus = '+',
    Minus = '-',
    Asterisk = '*',
    Slash = '/',
    Percent = '%',
    Lbrace = '(',
    Rbrace = ')',
  };

  Type type;
  union {
    string ident;
    Op op;
    int num;
  };

  static Token eof() { return Token(); }
  static Token oper(char op) { return Token(op); }
  static Token number(int num) { return Token(num); }
  static Token identifier(string &&ident) { return Token(ident); }
  static Token assign() { return Token(Type::Assign); }

  Token() : type(Type::Eof) {}
  Token(char op) : type(Type::Oper), op(static_cast<Op>(op)) {}
  Token(int num) : type(Type::Num), num(num) {}
  Token(string ident) : type(Type::Ident), ident(ident) {}
  Token(Type type) : type(type) {}
  Token(const Token &token) : type(token.type) {
    switch (this->type) {
    case Type::Oper:
      this->op = token.op;
      break;
    case Type::Num:
      this->num = token.num;
      break;
    case Type::Ident:
      new (&this->ident) string(token.ident);
      break;
    case Type::Eof:
      break;
    case Type::Assign:
      break;
    }
  }
  Token operator=(const Token &token) {
    this->~Token();
    new (this) Token(token);
    return *this;
  }

  ~Token() {
    if (this->type == Type::Ident) {
      this->ident.~string();
    }
  }

  bool not_eof() { return this->type != Type::Eof; }
};

struct Lexer {
public:
  Token next_token() {
    this->skipspace();
    switch (this->ch) {
    case '+':
    case '-':
    case '*':
    case '/':
    case '%':
    case '(':
    case ')': {
      auto token = Token::oper(this->ch);
      read_char();
      return token;
    }
    case '=':
      read_char();
      return Token::assign();
    case '\0':
      return Token::eof();
    default:
      if (is_digit(this->ch)) {
        return Token::number(this->read_number());
      } else if (is_letter(this->ch)) {
        return Token::identifier(this->read_identifier());
      } else {
        return Token::eof();
      }
    }
  }

  Lexer(string_view input) : input(input), cur(0), next(0) { read_char(); }

private:
  string_view input;
  int cur;  // 指向当前字符
  int next; // 指向下一个字符
  char ch;  // 当前字符

  void read_char() {
    if (this->next >= this->input.size()) {
      this->ch = '\0';
    } else {
      this->ch = this->input[this->next];
    }
    this->cur = this->next;
    this->next++;
  }

  char peek() {
    return this->next >= this->input.size() ? '\0' : this->input[this->next];
  }

  int read_number() {
    int num = 0;
    while (is_digit(this->ch)) {
      num = num * 10 + (this->ch - '0');
      read_char();
    }
    return num;
  }

  string read_identifier() {
    int start = this->cur;
    while (is_letter(this->ch)) {
      read_char();
    }
    return string(this->input.substr(start, this->cur - start));
  }

  void skipspace() {
    while (this->ch == ' ') {
      read_char();
    }
  }

  static bool is_digit(char ch) { return ch >= '0' && ch <= '9'; }
  static bool is_letter(char ch) {
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '_';
  }
};

struct AstNode {
public:
  enum class BinaryOp {
    Add = '+',
    Sub = '-',
    Prod = '*',
    Div = '/',
    Mod = '%',
  };

  ~AstNode() {
    switch (this->type) {
    case Type::BinaryExpression:
      delete this->left;
      delete this->right;
      break;
    case Type::NegativeExpression:
      delete this->child;
      break;
    case Type::Identifier:
    case Type::Literal:
      break;
    }
  }

  static AstNode *integer(int num) { return new AstNode(num); }
  static AstNode *expression(BinaryOp op, AstNode *left, AstNode *right) {
    return new AstNode(op, left, right);
  }
  static AstNode *negative(AstNode *child) { return new AstNode(child); }
  static AstNode *identifier(string identifier) {
    return new AstNode(identifier);
  }

  enum class Type {
    BinaryExpression,   // 二元中缀运算符
    NegativeExpression, // 单目前缀运算符，因为算术表达式中只有负号所以直接用NegativeExpression
    Literal,            // 字面量
    Identifier,         // 标识符
  };

  Type type;
  union {
    int num; // for Type::Literal
    struct {
      BinaryOp op;
      AstNode *left;
      AstNode *right;
    }; // for Type::BinaryExpression
    struct {
      AstNode *child;
    };            // for Type::NegativeExpression
    string ident; // for Type::Identifier
  };

private:
  AstNode(int num) : type(Type::Literal), num(num) {}
  AstNode(BinaryOp op, AstNode *left, AstNode *right)
      : type(Type::BinaryExpression), op(op), left(left), right(right) {}
  AstNode(AstNode *child) : type(Type::NegativeExpression), child(child) {}
  AstNode(string identifier) : type(Type::Identifier), ident(identifier) {}
};

typedef AstNode *Expression;

struct Statement {
  enum class Type {
    Expression,
    Assignment,
  };

  static Statement expression(Expression expr) { return Statement(expr); }
  static Statement assignment(string ident, Expression value) {
    return Statement(ident, value);
  }

  Statement(Expression expr) : type(Type::Expression), expr(expr) {}
  Statement(string ident, Expression value)
      : type(Type::Assignment), lhs(ident), rhs(value) {}

  Type type;
  union {
    Expression expr;
    struct {
      string lhs;
      Expression rhs;
    };
  };

  ~Statement() {
    switch (this->type) {
    case Type::Expression:
      delete this->expr;
      break;
    case Type::Assignment:
      lhs.~string();
      delete this->rhs;
      break;
    }
  }
};

struct Parser {
public:
  Parser(string_view input) : lexer(input) {
    cur = lexer.next_token();
    next = lexer.next_token();
  }

  Statement parse_statment() {
    switch (this->next.type) {
    case Token::Type::Assign: {
      auto ident = this->cur.ident;
      read_token();
      read_token();
      auto expr = parse_expression(LOWEST);
      return Statement::assignment(ident, expr);
    }
    default: {
      return Statement::expression(parse_expression(LOWEST));
    }
    }
  }

private:
  Lexer lexer;
  Token cur;
  Token next;

  typedef int Precedence;
  static constexpr Precedence LOWEST = 0;
  static constexpr Precedence ADD_SUB = 1;
  static constexpr Precedence PROD_DIV_MOD = 2;
  static constexpr Precedence PREFIX = 3;
  static Precedence precedence(const Token &tok) {
    using Type = Token::Type;
    using Op = Token::Op;
    switch (tok.type) {
    case Type::Oper:
      switch (tok.op) {
      case Op::Plus:
      case Op::Minus:
        return ADD_SUB;
      case Op::Asterisk:
      case Op::Slash:
      case Op::Percent:
        return PROD_DIV_MOD;
      default:
        return LOWEST;
      }
    default:
      return LOWEST;
    }
  }

  void read_token() {
    this->cur = this->next;
    this->next = this->lexer.next_token();
  }

  Precedence cur_precedence() { return precedence(this->cur); }
  Precedence next_precedence() { return precedence(this->next); }

  Expression parse_integer_literal() { return AstNode::integer(this->cur.num); }
  Expression parse_identifier() { return AstNode::identifier(this->cur.ident); }

  Expression parse_prefix_expression() {
    using Op = Token::Op;
    auto op = this->cur.op;
    read_token();
    switch (op) {
    case Op::Plus:
      return parse_expression(PREFIX);
    case Op::Minus:
      return AstNode::negative(parse_expression(PREFIX));
    default:
      return nullptr;
    }
  }

  Expression parse_grouped_expression() {
    read_token();
    auto expr = parse_expression(LOWEST);
    if (this->next.type != Token::Type::Oper ||
        this->next.op != Token::Op::Rbrace) {
      return nullptr;
    }
    read_token();
    return expr;
  }

  Expression parse_infix_expression(Expression left) {
    using Op = Token::Op;
    using BinaryOp = AstNode::BinaryOp;

    BinaryOp op;
    switch (this->cur.op) {
    case Op::Plus:
      op = BinaryOp::Add;
      break;
    case Op::Minus:
      op = BinaryOp::Sub;
      break;
    case Op::Asterisk:
      op = BinaryOp::Prod;
      break;
    case Op::Slash:
      op = BinaryOp::Div;
      break;
    case Op::Percent:
      op = BinaryOp::Mod;
      break;
    default:
      return nullptr;
    }

    auto precedence = cur_precedence();
    read_token();
    auto right = parse_expression(precedence);
    return AstNode::expression(op, left, right);
  }

  Expression parse_expression(Precedence precedence) {
    using Type = Token::Type;
    using Op = Token::Op;

    Expression left;

    switch (this->cur.type) {
    case Type::Oper:
      switch (this->cur.op) {
      case Op::Plus:
      case Op::Minus:
        left = parse_prefix_expression();
        break;
      case Op::Lbrace:
        left = parse_grouped_expression();
        break;
      default:
        return nullptr;
      }
      break;
    case Type::Ident:
      left = parse_identifier();
      break;
    case Type::Num:
      left = parse_integer_literal();
      break;
    default:
      return nullptr;
    }

    while (this->next.not_eof() && precedence < next_precedence()) {
      read_token();
      switch (this->cur.op) {
      case Op::Plus:
      case Op::Minus:
      case Op::Asterisk:
      case Op::Slash:
      case Op::Percent:
        left = parse_infix_expression(left);
        break;
      default:
        return nullptr;
      }
    }

    return left;
  }
};

struct Evaluator {
public:
  Evaluator() {}
  const Context &vars() const { return this->ctx; }
  int get_var(const string &ident) { return this->ctx.at(ident); }
  void set_var(const string &ident, int value) { this->ctx[ident] = value; }
  void clear_var(const string &ident) { this->ctx.erase(ident); }
  void clear_vars() { this->ctx.clear(); }

  int eval_statment(Statement &stmt) {
    using Type = Statement::Type;
    switch (stmt.type) {
    case Type::Assignment: {
      auto ident = stmt.lhs;
      auto expr = stmt.rhs;
      auto value = eval_expression(expr);
      set_var(ident, value);
      return value;
    }
    case Type::Expression: {
      auto expr = stmt.expr;
      return eval_expression(expr);
    }
    }
  }

private:
  Context ctx;

  int eval_binary(AstNode *node) {
    using Op = AstNode::BinaryOp;
    auto op = node->op;
    auto left = node->left;
    auto right = node->right;
    switch (op) {
    case Op::Add:
      return eval_expression(left) + eval_expression(right);
    case Op::Sub:
      return eval_expression(left) - eval_expression(right);
    case Op::Prod:
      return eval_expression(left) * eval_expression(right);
    case Op::Div:
      return eval_expression(left) / eval_expression(right);
    case Op::Mod:
      return eval_expression(left) % eval_expression(right);
    }
  }

  int eval_negative(AstNode *node) { return -eval_expression(node->child); }

  int eval_identifier(AstNode *node) { return this->ctx[node->ident]; }

  int eval_literal(AstNode *node) { return node->num; }

  int eval_expression(AstNode *node) {
    using Type = AstNode::Type;
    switch (node->type) {
    case Type::BinaryExpression:
      return eval_binary(node);
    case Type::Identifier:
      return eval_identifier(node);
    case Type::Literal:
      return eval_literal(node);
    case Type::NegativeExpression:
      return eval_negative(node);
    }
  }
};

struct Repl {
private:
  Evaluator evaluator;
  static constexpr const char *PROMPT = ">>> ";
  static constexpr const char *RESULT = "=> ";
  static constexpr const char *WELCOME =
      "Welcome to the Calculator REPL!\n"
      "type <expression> to evaluate an expression\n"
      "type 'vars' to list variables\n"
      "type 'clear' to clear variables\n"
      "type 'exit' to exit\n"
      "\n";
  static constexpr const char *EXIT = "Goodbye!\n";

public:
  Repl() {}

  int loop() {
    using std::cin;
    using std::cout;
    using std::endl;
    using std::getline;

    cout << WELCOME;
    for (;;) {
      cout << PROMPT;
      string line;
      getline(cin, line);
      if (line == "exit") {
        cout << EXIT;
        return 0;
      } else if (line == "clear") {
        this->evaluator.clear_vars();
        continue;
      } else if (line == "vars") {
        for (auto &[ident, value] : this->evaluator.vars()) {
          cout << ident << " = " << value << endl;
        }
        continue;
      } else {
        auto parser = Parser(line);
        auto stmt = parser.parse_statment();
        auto value = this->evaluator.eval_statment(stmt);
        cout << RESULT << value << endl;
      }
    }
  }
};

int main() { return Repl().loop(); }
