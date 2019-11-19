
#include <iostream>
#include <vector>
#include <variant>
#include <functional>
#include <algorithm>
#include <exception>
#include <assert.h>

template<class... Ts> struct overload : Ts... { using Ts::operator()...; };
template<class... Ts> overload(Ts...)->overload<Ts...>;

namespace rpn { // ...............................................
	using rpn_val = std::variant<std::monostate, int, std::string>;
	using rpn_stack_type = std::vector<rpn_val>;

	struct rpn_stack {
		rpn_stack_type stack;
		rpn_val& top(size_t pos = 0) {
			if (pos >= stack.size())throw(std::exception("Stack too small for top value"));
			return *(stack.end() - pos - 1);
		}
		void push(rpn_val val) { stack.push_back(val); }
		void pop() { stack.erase(stack.end() - 1); }
		size_t size() const { return stack.size(); }
		void clear() { stack.clear(); }
	};
	using funct_op = void(*)(rpn_stack&, void* ptr);
	using check_op = bool(*)(rpn_stack&, funct_op&, void*);

	static check_op call_check_2 = [](rpn_stack& stack, funct_op& funct, void* ptr) {
		if (stack.size() < 2)
			return false;
		try {
			funct(stack, ptr);
		}
		catch (std::exception && e) {
			stack.push(e.what());
			return false;
		}
		return true;
	};
	static check_op call_check_1 = [](rpn_stack& stack, funct_op& funct, void* ptr) {
		if (!stack.size()) return false;
		try {
			funct(stack, ptr);
		}
		catch (std::exception && e) {
			stack.push(e.what());
			return false;
		}
		return true;
	};
	static check_op noop_check = [](rpn_stack& stack, funct_op& funct, void* ptr) {funct(stack, ptr);  return true; };

	struct rpn_op {
		rpn_stack& stack;
		funct_op& funct;
		rpn_val val;
		void* ptr;
		check_op* check;
		rpn_op(rpn_stack& stack, funct_op& funct, void* ptr = nullptr) :stack(stack), funct(funct), ptr(ptr), check(nullptr), val(std::monostate()){}
		rpn_op(rpn_stack& stack, funct_op& funct, rpn_val& val) :stack(stack), funct(funct), val(val), ptr(this), check(nullptr) {}
		//rpn_op(const rpn_op& ov) :stack(stack),funct(funct),ptr(nullptr) { if (val.index() != std::variant_npos) ptr = this; }
		rpn_op(const rpn_op& ov) :stack(ov.stack),funct(ov.funct),val(ov.val),ptr(ov.ptr),check(ov.check) { if (val.index()) ptr = this; }
		void operator ()() { funct(stack, ptr); }
		bool operator ()(bool) { return (*check)(stack, funct, ptr); }
	};
	struct rpn_ops {
		using rpn_ops_type = std::vector<rpn_op>;
		using iterator = rpn_ops_type::iterator;
		rpn_stack& stack;
		rpn_ops_type ops;
		rpn_ops(rpn_stack& stack) :stack(stack) {}
		void add_op(funct_op& funct, void* ptr = nullptr) { ops.push_back(rpn_op(stack, funct, ptr)); }
		void add_op_var(funct_op& funct, rpn_val var) { ops.push_back(rpn_op(stack, funct, var)); }
		void add_op(const char* name, void* ptr = nullptr);
		void add_op_var(const char* name, rpn_val var);
		void add_op(const char* name, check_op& chk, void* ptr = nullptr);

		iterator begin() { return ops.begin(); }
		iterator end() { return ops.end(); }
		void clear() { ops.clear(); }
	};
	void operator +=(rpn_val& a, rpn_val& b);
	//the actual rpn operators
	static funct_op add = [](rpn_stack& stack, void*) {stack.top(1) += stack.top(); stack.pop(); };
	static funct_op push = [](rpn_stack& stack, void* ptr) {auto test = static_cast<rpn_op*>(ptr);  stack.push(static_cast<rpn_op*>(ptr)->val); };
	static funct_op noop = [](rpn_stack&, void*) {};
	// .........................
	struct var_print_cout {
		void operator()(const std::monostate& item) { std::cout << "variant is null" << '\n'; }
		void operator()(const int& item) { std::cout << "int:     " << item << '\n'; }
		void operator()(const std::string& item) { std::cout << "str: " << item << '\n'; }
	};

}//rpn namespace
inline std::ostream& operator << (std::ostream& os, const rpn::rpn_val& var) { std::visit(rpn::var_print_cout(), var); return os; }

//the test target
struct math_asset {
	math_asset(const std::string& name, int useful):name(name),useful_life(useful){}
	std::string name;;
	int useful_life;
};

struct printer {
	void print(std::string& str) { std::cout << "printer :" << str << "  "; }
	void operator ()(std::string& str) { print(str); }
};
struct target {
	target(printer& printer) :printer(printer) {}
	math_asset* asset;//as it changes...
	printer& printer;
};
using data_vect = std::vector<math_asset>;

static rpn::funct_op print_val = [](rpn::rpn_stack& stack, void* ptr) {std::cout << stack.top(); };
static rpn::funct_op print_val_pop = [](rpn::rpn_stack& stack, void* ptr) {std::cout << stack.top(); stack.pop(); };

static rpn::funct_op print_ma_name = [](rpn::rpn_stack& stack, void* ptr){
	target* targ = static_cast<target*>(ptr); targ->printer(targ->asset->name);
};

//and by name
static struct rpn_names_type {
	const std::string name;
	rpn::funct_op& op;
	rpn::check_op& check;
}rpn_names[] = {
	{"add", rpn::add, rpn::call_check_2 },
	{"push", rpn::push, rpn::noop_check },
	//
	{"print_ma_name", print_ma_name, rpn::call_check_1},
	{"noop", rpn::noop, rpn::noop_check },
};
static rpn_names_type* rpn_names_end = rpn_names + sizeof(rpn_names) / (sizeof(rpn_names_type) + 1);

//end .hpp /begin .cpp
namespace rpn {
	void operator +=(rpn_val& a, rpn_val& b) {
		std::visit(overload{
		   [](int& ai, int& bi)->rpn_val {return ai += bi; },
		   [](std::string& ai, std::string& bi)->rpn_val {return ai += bi; },
		   [](auto& ai, auto& bi)->rpn_val {throw(std::exception("bad values for operation",1)); }
			}, a, b);
	}
}

void rpn::rpn_ops::add_op(const char* name, void* ptr) {
	auto at = std::find_if(rpn_names, rpn_names_end, [&name](const rpn_names_type& f) {return !f.name.compare(name); });
	rpn::rpn_op op(stack, at->op, ptr); op.check = &at->check;
	op.check = &at->check;
	ops.push_back(op);
}

void rpn::rpn_ops::add_op(const char* name, check_op& chk, void* ptr) {
	add_op(name, ptr); ops.back().check = &chk;
}

void rpn::rpn_ops::add_op_var(const char* name, rpn::rpn_val ptr) {
	if (strcmp(name, "push"));
	auto at = std::find_if(rpn_names, rpn_names_end, [&name](const rpn_names_type& f) {return !f.name.compare(name); });
	rpn::rpn_op op(stack, at->op, ptr);
	op.check = &at->check;
	ops.push_back(op);
}

void report_error(size_t pos, rpn::rpn_op& op) {
	std::cout << "Err at: " << pos << " as: " << op.stack.top() << std::endl;
}

int mainvm() {
	using namespace rpn;
	rpn_stack stack;

	rpn_ops ops(stack);
	ops.add_op(add);
	ops.add_op_var(push, 5);
	ops.add_op(add);
	ops.add_op(print_val_pop);

	//run...
	stack.push(3);
	stack.push(4);
	for (auto& op : ops)
		op();
	ops.clear();

	printer ptr;
	target targ(ptr);
	math_asset ma("an asset", 5);
	targ.asset = &ma;

	ops.add_op_var("push", 5);
	ops.add_op_var("push", 6);
	ops.add_op("add");
	ops.add_op("print_val_pop");
	ops.add_op("print_ma_name", &targ);
	for (auto& op : ops)
		if (!op(true))
			std::cout << stack.top() << std::endl;

	std::cout << "\nprint val:\n  ";
	rpn_op (stack, print_val)();

	//there is still an int 12 on the stack
	rpn_op (stack, print_val_pop)();

	stack.push("a string");
	stack.push(9);
	std::cout << "\nwill fail\n";
	ops.clear();
	ops.add_op("add", call_check_2, nullptr);
	//this is a parser check pass
	size_t pos = 1;
	for (auto& op : ops) {
		if (!op(true)) {
			report_error(pos, op);
			break;
		}
		++pos;
	}
	stack.pop();
	stack.pop();
	rpn_op(stack, print_val_pop)();
	assert(!stack.size());

	//final usage
	stack.clear(); ops.clear();
	data_vect data_set = { {"asset one",7}, {"asset two",4} };
	ops.add_op("print_ma_name", &targ);
	for (auto& data : data_set)
	{
		targ.asset = &data;
		for (auto& op : ops)
			op();
		std::cout << '\n';
	}
	std::cout << " stack size: " << stack.stack.size() << std::endl;
	return 0;
}
