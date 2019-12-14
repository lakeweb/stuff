
#define BOOST_SPIRIT_X3_DEBUG
#include <iostream>
#include <iomanip>
#include <string>
#include <assert.h>
#include <any>
#include <variant>
#include <exception>
#include <boost/date_time/gregorian/greg_date.hpp>
#include <boost/locale.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/spirit/home/x3.hpp>

using namespace std::literals;

//and, error handling started courtesy of Seth's post:
//https://stackoverflow.com/questions/57048008/spirit-x3-is-this-error-handling-approach-useful

//support ............................................
inline std::wstring to_wide(const char* ps) { return std::wstring(boost::locale::conv::utf_to_utf<wchar_t>(ps)); }
inline std::wstring to_wide(const std::string& str) { return to_wide(str.c_str()); }
inline std::string to_narrow(const wchar_t* ps) { return std::string(boost::locale::conv::utf_to_utf<char>(ps)); }
inline std::string to_narrow(const std::wstring& str) { return to_narrow(str.c_str()); }

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

//targets ..............................................
struct str_format {
	enum f_es {
		none = 0x00,
		cents = 0x01,
		sign  = 0x02,
	};
	f_es ftypes = none;
	size_t lines = 0;
	void operator |= (size_t i) { ((size_t&)ftypes) |= i; }
};

using stack_type = std::vector<int>;

struct op {
	using funct_op = void(*)(op&, void* ptr);
	stack_type& stack;
	const funct_op& funct;
	std::any extra_info;
	op(stack_type& stack, const funct_op& funct) :stack(stack), funct(funct) {}
};
using funct_op = op::funct_op;
using ops_cont = std::list<op>;

struct ops {
	stack_type stack;
	ops_cont set;
};

//as an example........................................
auto format = [](const double val, const str_format& f = str_format{}) {
	std::wstring out;
	if (f.ftypes & str_format::sign)
		out += L'$';
	auto dols = (unsigned long)val;
	out += as_wstring(dols);
	if (f.ftypes & str_format::cents) {
		unsigned long c = (unsigned long)((val - (double)dols) * 100 + .5);
		auto cs = as_wstring(c);
		out += L'.' + cs;
	}
	if (f.lines)
		out += L'_' + as_wstring( f.lines );
	return out;
};

static funct_op a_funct = [](op&, void* ptr) {};

//parsers .............................................
using namespace boost::spirit::x3;
struct with_error_handling {
	struct diags;

	template<typename It, typename Ctx>
	error_handler_result on_error(It f, It l, expectation_failure<It> const& ef, Ctx const& ctx) const {
		std::string s(f, l);
		auto pos = std::distance(f, ef.where());

		std::ostringstream oss;
		oss << "Expecting " << ef.which() << " at "
			<< "\n\t" << s
			<< "\n\t" << std::setw(pos) << std::setfill('-') << "" << "^";

		get<diags>(ctx).push_back(oss.str());

		return error_handler_result::fail;
	}
};
struct parser_class : with_error_handling {};


template <typename T>
auto as = [](auto p) {
	return rule<struct _, T> {typeid(T).name()} = p;
};

auto const str_format_rule = rule<class _, str_format>("str_format") = [] {
	struct str_format_ : symbols<str_format::f_es> { str_format_() { add
		("c", str_format::cents)
		("s", str_format::sign)
		("n", str_format::none);}
	}str_format;
	auto const to_format_traits = [](auto& ctx) {_val(ctx) |= _attr(ctx); };
	auto const to_format_val = [](auto& ctx) {_val(ctx).lines = boost::lexical_cast<int>(_attr(ctx)); };
	auto const to_str_format = [](auto& ctx) { std::cout << _val(ctx); };
	return *(str_format[to_format_traits] | char_("1-3")[to_format_val]);
}();

using parser_type = bool (*)(const std::string_view str, op& an_op);
bool parse_currancy(const std::string_view str, op& an_op) {
	str_format test;
	auto begin = str.begin();
	bool r;
	if( r = parse(begin, str.end(), str_format_rule, test))
		an_op.extra_info = test;
	return r;
}
const auto t_parse = [](const std::string_view str, op& an_op) { return true; };

static struct ops_name_ {
	std::string_view name;
	const funct_op& funct;
	parser_type a_parser;
}ops_name[] = {
	{"nothing",a_funct, t_parse},
	{"ptr_one",a_funct, parse_currancy},
	{"ptr_xone",a_funct},
	{"ptr_xxone",a_funct},
};

static ops_name_* ops_name_end = ops_name + sizeof(ops_name) / (sizeof(ops_name_) + 1);

//more parsers...........................................
auto const words = lexeme[+char_("a-zA-Z ")];

auto const big_rule = rule<parser_class, ops>("big_one") = [] {
	struct op_type_ :symbols<ops_name_*> {} op_type;
	ops_name_* hold;
	//op* the_push;
	std::for_each(ops_name, ops_name_end, [&op_type](ops_name_& op) {op_type.add(op.name, &op); });

	auto const set = [&hold](auto& ctx) {
		_pass(ctx) = hold->a_parser(std::string_view(_attr(ctx)), _val(ctx).set.back());
		std::cout << "***" << _attr(ctx);
	};
	auto const push = [&hold](auto& ctx) {
		hold = _attr(ctx);
		_val(ctx).set.insert(_val(ctx).set.end(), op( _val(ctx).stack, hold->funct));
	};
	return *((op_type[push]) >> -('(' > (*char_("scn1-3"))[set] > ')') > -lit(','));
}();

//test .................................................
#if 1
int main() {
	//the format.........
	str_format f; f.ftypes = f.cents; f.lines = 1;
	std::wcout << format(23.346, f) << std::endl;

	ops targ;
	std::vector<std::string> errors;

	auto str = "ops(nothing,nothing,ptr_one(sc1),ptr_one(fail),nothing)"s;
	auto begin = str.begin();
	auto r = parse(begin, str.end(), lit("ops(") >> with<with_error_handling::diags>(errors)[big_rule] >> ')', targ);
	std::cout << errors.front() << std::endl;
	return 0;
}
#endif
