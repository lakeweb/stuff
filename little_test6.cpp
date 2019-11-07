
#include <iostream>
#include <assert.h>
#include <variant>
#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/date_time/gregorian/greg_date.hpp>
#include <boost/locale.hpp>
#include <boost/lexical_cast.hpp>
#include <exception>

template<class... Ts> struct overload : Ts... { using Ts::operator()...; };
template<class... Ts> overload(Ts...)->overload<Ts...>;
using bgdate = boost::gregorian::date;
inline std::wstring to_wide(const char* ps) { return std::wstring(boost::locale::conv::utf_to_utf<wchar_t>(ps)); }

template <typename T>
std::wstring as_wstring(T t) {
	using result_type = std::wstring;
	try {
		return boost::lexical_cast<result_type>(t);
	}
	catch (std::exception & e) {
		std::wstring str(L"Bad Cast from type: ");
		str += to_wide(typeid(t).name());
		str += L" What: " + to_wide(e.what());
		return str;
	}
}

namespace rpn { // ...............................................
	using rpn_val = std::variant<int, double, std::wstring, bgdate>;
	// ......
	struct rpn_stack {
		using rpn_stack_type = std::vector<rpn_val>;
		using iterator = rpn_stack_type::iterator;
		rpn_stack_type stack;
		iterator begin() { return stack.begin(); }
		iterator end() { return stack.end(); }
		rpn_val& back() { return stack.back(); }
		size_t size() const { return stack.size(); }
		void push(rpn_val&& in) { stack.push_back(in); }
		rpn_val& top(size_t pos = 0) {
			if (pos >= stack.size())throw(std::exception("Stack too small for top value"));
			return *(stack.end() - pos - 1);
		}
		void pop() { stack.erase(end() - 1); }
		template<typename T>
		void push(T& t) { stack.push_back(t); }
		void push(rpn_val& t) { stack.push_back(t); }
	};
	// rpn base implamentation ..................................................................
	using sub_funct = void(*)(rpn_stack&, bool(*)(rpn_stack&));
	using funct_op = void(*)(rpn_stack&);
	using funct_call = bool(*)(rpn_stack&, funct_op&);

	funct_call check_call_2_1 = [](rpn_stack& stack, funct_op& op)->bool {
		if (stack.size() < 2) {
			stack.push(L"stack size less than 2");
			return false;
		}
		try {
			op(stack);
		}
		catch (std::exception && e) {
			stack.push(to_wide(e.what()));
			return false;
		}
		stack.pop();
		return true;
	};
	//these do no error handling, for runtime.............
	funct_call call_1 = [](rpn_stack& stack, funct_op& op)->bool {op(stack); stack.pop(); return true; };
	funct_call call_0 = [](rpn_stack& stack, funct_op& op)->bool {op(stack); return true; };

	// rpn involked functions ..........................................................
	void operator +=(rpn_val& a, rpn_val& b) {
		std::visit(overload{
		   [](int& ai, int& bi)->rpn_val {return ai += bi; },
		   [](double& ai, int& bi)->rpn_val {return ai += bi; },
		   [](int& ai, double& bi)->rpn_val {return (double&)ai += bi; },
		   [](double& ai, double& bi)->rpn_val {return ai += bi; },
		   [](std::wstring& ai, std::wstring& bi)->rpn_val {return ai += bi; },
		   [](auto& ai, auto& bi)->rpn_val {throw(std::exception("bad values for operation",1)); }
			}, a, b);
	}
	void operator /=(rpn_val& a, rpn_val& b) {
		std::visit(overload{
		   [](int& ai, int& bi)->rpn_val {return ai /= bi; },
		   [](double& ai, int& bi)->rpn_val {return ai /= bi; },
		   [](int& ai, double& bi)->rpn_val {return (double&)ai /= bi; },
		   [](double& ai, double& bi)->rpn_val {return ai /= bi; },
		   //[](std::wstring& ai, std::wstring& bi)->rpn_val {return ai += bi; },
		   [](auto& ai, auto& bi)->rpn_val {throw(std::exception("bad values for operation",1)); }
			}, a, b);
	}
	std::wstring var_as_wstr(rpn_val& val) {
		return std::visit(overload{
			[](int& v) {return as_wstring(v); },
			[](double& v) {return as_wstring(v); },
			[](std::wstring& v) {return v; },
			[](auto& v) {throw(std::exception("bad values for operation",1)); return std::wstring(); }
			}, val);
	}
	static funct_op add_f = [](rpn_stack& stack) {stack.top(1) += stack.top(); };
	static funct_op div_f = [](rpn_stack& stack) {stack.top(1) /= stack.top(); };
	static funct_op pop = [](rpn_stack& stack) { stack.pop(); };
	static funct_op noop = [](rpn_stack& stack) {};

	//now looking up ops by name ...............................................
	static struct funct_id_type {
		std::string name;
		funct_op& funct;
		funct_call& involk_check;
		funct_call& involk;
	}funct_id[] = {
		{"pop", pop, call_0, call_1},
		{"add", add_f, check_call_2_1, call_1},
		{"div", div_f, check_call_2_1, call_1},
		{"not found", noop, call_0, call_0},
	};
	funct_id_type* funct_id_end = funct_id + sizeof(funct_id) / (sizeof(funct_id_type) + 1);

	//the operators .............................................................
	struct op_var {
		rpn_stack& stack;
		funct_op& funct;
		funct_call& call;
		//funct_id_type* id;
		op_var(rpn_stack& stack, funct_op& funct, funct_call& call) : stack(stack), funct(funct), call(call) {}
		bool operator()() { return call(stack, funct); }
	};
	struct op_push_val {
		rpn_stack& stack;
		rpn_val val;
		op_push_val(rpn_stack& stack, rpn_val val) :stack(stack), val(val) {}
		void operator ()() { stack.push(val); }
	};
	struct var_print_cout {
		void operator()(const int& item) { std::cout << "int:     " << item; }
		void operator()(const double& item) { std::cout << "double:  " << item; }
		void operator()(const std::wstring& item) { std::wcout << L"wstring: " << item; }
		void operator()(const bgdate& item) { std::cout << "bgdate:  " << item.day(); }
	};
	inline std::ostream& operator << (std::ostream& os, const rpn::rpn_val& var) { std::visit(var_print_cout(), var); return os; }

	inline void report(op_var& op, rpn_stack& stack, int pos) {
		std::cout << "exception at op: " << pos << " " << stack.back() << std::endl;
	}

}//namespace rpn

//hmmmm have to drag math_asset into this namespace, or not..........
struct math_asset {
	std::wstring name = L"the asset name";
	int useful_life = 24;//two years
	bgdate in_service = boost::gregorian::from_string(std::string("2005/01/01"));
	int get_useful() const { return useful_life; }
	math_asset(const wchar_t* name, int life) :name(name), useful_life(life) {}
};

using ma_ptr = math_asset**;
using ma_ref = math_asset;
using funct_prt = std::wstring(*)(rpn::rpn_stack&, ma_ref);

struct op_print {
	rpn::rpn_stack& stack;
	funct_prt funct;
	ma_ptr pdata;
	op_print(rpn::rpn_stack& stack, funct_prt& funct, ma_ptr pdata)
		:stack(stack), funct(funct), pdata(pdata) {}
	std::wstring operator()() { return funct(stack, **pdata); }
};

using funct_stack = void(*)(rpn::rpn_stack&, ma_ref);
struct op_push {
	rpn::rpn_stack& stack;
	funct_stack funct;
	ma_ptr pdata;
	op_push(rpn::rpn_stack& stack, funct_stack funct, ma_ptr pdata)
		:stack(stack), funct(funct), pdata(pdata) {}
	std::wstring operator()() { funct(stack, **pdata); }
};
using var_op_type = std::variant<rpn::op_var,rpn::op_push_val, op_print,op_push>;
using var_ops_type = std::vector<var_op_type>;

struct rpn_operators { //................
	using iterator = var_ops_type::iterator;
	var_ops_type ops;
	rpn::rpn_stack& stack;
	bool do_check;
	rpn_operators(rpn::rpn_stack& stack, bool do_check = false) : stack(stack), do_check(do_check) {}
	iterator begin() { return ops.begin(); }
	iterator end() { return ops.end(); }
	void push(const char* id);
	void push(const char* id, ma_ptr ptr); //op_val(rpn_stack& stack, funct_op_val& op, rpn_val val)
	void push(rpn::rpn_val val);
};
void rpn_operators::push(const char* id) {
	auto at = std::find_if(rpn::funct_id, rpn::funct_id_end, [&id](const rpn::funct_id_type& f) {return !f.name.compare(id); });
	ops.push_back(rpn::op_var(stack, at->funct, do_check ? at->involk_check : at->involk));
}
void rpn_operators::push(rpn::rpn_val val) {
	ops.push_back(rpn::op_push_val(stack, val));
}

//implament math_asset printing............................
static funct_prt pnoop = [](rpn::rpn_stack& stack, ma_ref) {return std::wstring(L"noop"); };
static funct_prt prt_ma_useful = [](rpn::rpn_stack& stack, ma_ref data) {return as_wstring(data.get_useful()); };
static funct_prt push_ma_useful = [](rpn::rpn_stack& stack, ma_ref data) {stack.push(data.get_useful()); return std::wstring(); };
static funct_prt prt_ma_name = [](rpn::rpn_stack& stack, ma_ref data) {return data.name; };
static funct_prt prt_stack = [](rpn::rpn_stack& stack, ma_ref data) {auto v = stack.back(); stack.pop(); return rpn::var_as_wstr(v); };

static struct funct_ma_id_type {
	std::string name;
	funct_prt& funct;
}funct_ma_id[] = {
	{"prt_ma_name", prt_ma_name},
	{"push_ma_useful", push_ma_useful},
	{"prt_ma_useful", prt_ma_useful},
	{"prt_stack", prt_stack},
	{"not found", pnoop},
};
funct_ma_id_type* funct_ma_id_end = funct_ma_id + sizeof(funct_ma_id) / (sizeof(funct_ma_id_type) + 1);

void rpn_operators::push(const char* id, ma_ptr ptr) {
	auto at = std::find_if(funct_ma_id, funct_ma_id_end, [&id](const funct_ma_id_type& f) {return !f.name.compare(id); });
	ops.push_back(op_print(stack, at->funct,ptr));
}

void stack_look(rpn::rpn_stack& stack) {
	using namespace rpn;
	std::cout << "\nThe Stack:\n";
	for (auto& item : stack)
		std::cout << "  " << item << std::endl;
	std::cout << "\n";
}

//the engine with checking................
bool rpn_engine(rpn_operators& voseq) {
	using namespace rpn;
	size_t pos = 0;
	try {//in the case that checking is not enabled
		for (auto& an_op : voseq)
		{
			std::cout << "OP: " << an_op.index() << std::endl;
			if (auto pv = std::get_if<op_print>(&an_op))
				std::wcout << L"PRINTING: "<< (*pv)() << std::endl;
			else if (auto pv = std::get_if<op_var>(&an_op)) {
				if (!(*pv)()) {
					report(*pv, voseq.stack, pos);
					break;
				}
			}
			else if (auto pv = std::get_if<op_push_val>(&an_op))
				(*pv)(); 
			
			++pos;
			stack_look(voseq.stack);
		}
	}catch (std::exception & e) {
		std::cout << "caught in rpn_engine: " << e.what() << "\n";
		return false;
	}
	return true;
}

// ..........................................................................
int main() { //the puding
	using namespace rpn;
	{
		std::cout << "testing the op_var operators\n";
		rpn_stack stack;
		stack.push(L"a var string");
		stack.push(23.5);
		stack.push(111);
		stack.push(3);
		stack.push(55);
		stack_look(stack);

		rpn_operators ops(stack, true);//with checking
		ops.push("pop");
		ops.push("add");
		ops.push("add");
		ops.push("add");//this one fails

		rpn_engine(ops);
		stack_look(stack);
	}
	{
		std::cout << "testing with op_ptr\n";
		rpn_stack stack;
		rpn_operators ops(stack);//without checking
		math_asset* ptr;
		ops.push("push_ma_useful", &ptr);
		ops.push(12);
		ops.push("div");
		ops.push("prt_stack", &ptr);
		ops.push("prt_ma_name", &ptr);
		stack_look(stack);
		std::vector< math_asset> assets = { {L"first asset", 24},{L"Second One",48} };
		for (auto& asset : assets)
		{
			ptr = &asset;
			rpn_engine(ops);
			std::cout << "\nend of one asset\n";
		}
	}
	return 0;
}
