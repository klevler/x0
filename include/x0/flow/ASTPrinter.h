#include <x0/flow/ASTVisitor.h>

namespace x0 {

class ASTNode;

class X0_API ASTPrinter :
	public ASTVisitor
{
public:
	static void print(ASTNode* node);

private:
	ASTPrinter();

	int depth_;

	void enter() { ++depth_; }
	void leave() { --depth_; }
	void prefix();
	void print(const char* title, ASTNode* node);
    void print(const std::pair<std::string, Expr*>& node, size_t pos);

	void printf(const char* msg) { prefix(); std::printf("%s", msg); }
	template<typename... Args> void printf(const char* fmt, Args... args) { prefix(); std::printf(fmt, args...); }

	virtual void accept(Variable& variable);
	virtual void accept(Handler& handler);
	virtual void accept(BuiltinFunction& symbol);
	virtual void accept(BuiltinHandler& symbol);
	virtual void accept(Unit& symbol);
	virtual void accept(UnaryExpr& expr);
	virtual void accept(BinaryExpr& expr);
	virtual void accept(FunctionCall& expr);
	virtual void accept(VariableExpr& expr);
	virtual void accept(HandlerRefExpr& expr);
	virtual void accept(StringExpr& expr);
	virtual void accept(NumberExpr& expr);
	virtual void accept(BoolExpr& expr);
	virtual void accept(RegExpExpr& expr);
	virtual void accept(IPAddressExpr& expr);
	virtual void accept(CidrExpr& cidr);
	virtual void accept(ExprStmt& stmt);
	virtual void accept(CompoundStmt& stmt);
	virtual void accept(CondStmt& stmt);
	virtual void accept(AssignStmt& stmt);
	virtual void accept(HandlerCall& stmt);
};

} // namespace x0