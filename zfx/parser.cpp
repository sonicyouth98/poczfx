#include "ZFXCode.h"
#include "bc.h"
#include <vector>
#include <string>
#include <cctype>
#include <memory>
#include "span.h"
#include "enumtools.h"
#include "overloaded.h"
#include "scope_exit.h"
#include <algorithm>
#include <string_view>
#include <optional>
#include <variant>
#include <array>
#include <map>
#include <typeinfo>
#include <any>

namespace zeno::zfx {
namespace {


enum class Op : uint8_t {
    kAssign,
    kPlus,
    kMinus,
    kMultiply,
    kDivide,
    kModulus,
    kPlusAssign,
    kMinusAssign,
    kMultiplyAssign,
    kDivideAssign,
    kModulusAssign,
    kBitAndAssign,
    kBitOrAssign,
    kBitXorAssign,
    kMember,
    kBitInverse,
    kBitAnd,
    kBitOr,
    kBitXor,
    kBitShl,
    kBitShr,
    kLogicNot,
    kLogicAnd,
    kLogicOr,
    kCmpEqual,
    kCmpNotEqual,
    kCmpLessThan,
    kCmpLessEqual,
    kCmpGreaterThan,
    kCmpGreaterEqual,
    kLeftBrace,
    kRightBrace,
    kLeftBracket,
    kRightBracket,
    kLeftBlock,
    kRightBlock,
    kTernary,
    kTernaryElse,
    kComma,
    kSemicolon,
    kKeywordIf,
    kKeywordElse,
    kKeywordFor,
    kKeywordWhile,
    kKeywordReturn,
};

using Ident = std::string;
using Token = std::variant<Op, Ident, float, int>;


struct ZFXTokenizer {
    std::vector<Token> tokens;

    static const inline std::map<char, Op> lut1{
        {'=', Op::kAssign},
        {'+', Op::kPlus},
        {'-', Op::kMinus},
        {'*', Op::kMultiply},
        {'/', Op::kDivide},
        {'%', Op::kModulus},
        {'.', Op::kMember},
        {'~', Op::kBitInverse},
        {'&', Op::kBitAnd},
        {'|', Op::kBitOr},
        {'^', Op::kBitXor},
        {'<', Op::kCmpLessThan},
        {'>', Op::kCmpGreaterThan},
        {'!', Op::kLogicNot},
        {'(', Op::kLeftBrace},
        {')', Op::kRightBrace},
        {'[', Op::kLeftBracket},
        {']', Op::kRightBracket},
        {'{', Op::kLeftBlock},
        {'}', Op::kRightBlock},
        {'?', Op::kTernary},
        {':', Op::kTernaryElse},
        {',', Op::kComma},
        {';', Op::kSemicolon},
    };

    static const inline std::map<std::tuple<char, char>, Op> lut2{
        {{'&', '&'}, Op::kLogicAnd},
        {{'|', '|'}, Op::kLogicOr},
        {{'=', '='}, Op::kCmpEqual},
        {{'!', '='}, Op::kCmpNotEqual},
        {{'<', '='}, Op::kCmpLessEqual},
        {{'>', '='}, Op::kCmpGreaterEqual},
        {{'<', '<'}, Op::kBitShl},
        {{'>', '>'}, Op::kBitShr},
        {{'+', '='}, Op::kPlusAssign},
        {{'-', '='}, Op::kMinusAssign},
        {{'*', '='}, Op::kMultiplyAssign},
        {{'/', '='}, Op::kDivideAssign},
        {{'%', '='}, Op::kModulusAssign},
        {{'&', '='}, Op::kBitAndAssign},
        {{'^', '='}, Op::kBitXorAssign},
        {{'|', '='}, Op::kBitOrAssign},
    };

    //static const inline std::map<std::tuple<char, char, char>, Op> lut3{
        //{{'<', '<', '='}, Op::kBitShlAssign},
        //{{'>', '>', '='}, Op::kBitShrAssign},
    //};

    static const inline std::map<std::string, Op> lutkwd{
        {"if", Op::kKeywordIf},
        {"else", Op::kKeywordElse},
        {"for", Op::kKeywordFor},
        {"while", Op::kKeywordWhile},
        {"return", Op::kKeywordReturn},
    };

    //为了解决二元运算符的左递归问题，我们需要引入一个运算符优先级map
    std::map<Op, int32_t> OpRec {
            {Op::kAssign, 2},//=
            {Op::kPlusAssign, 2},//+=
            {Op::kMinusAssign, 2}, // -=
            {Op::kDivideAssign, 2}, // /=
            {Op::kModulusAssign, 2}, //  %=
            {Op::kBitAndAssign, 2}, // &=
            {Op::kBitXorAssign, 2},  // ^=
            {Op::kBitOrAssign, 2}, // |=
            {Op::kLogicOr, 4},  //  ||
            {Op::kLogicAnd, 5}, // &&
            {Op::kBitOr, 6}, // |
            {Op::kBitOr, 7}, // ^
            {Op::kBitAnd, 8}, // &
            {Op::kCmpEqual, 9}, // ==
            {Op::kCmpNotEqual, 9}, // !=
            {Op::kCmpGreaterThan, 10}, // >
            {Op::kCmpGreaterEqual, 10}, // >=
            {Op::kCmpLessThan, 10}, // <
            {Op::kCmpLessEqual, 10}, // <=
            {Op::kPlus, 11}, // +
            {Op::kMinus, 11}, // -
            {Op::kMultiply,12}, // *
            {Op::kDivide,12}, // /
    };
    static bool isident(char c) noexcept {
        return std::isalnum(c) || c == '_' || c == '$' || c == '@';
    }

    std::optional<Token> take(std::string_view &ins) noexcept {
        if (ins.size() >= 1) {
            if (std::isdigit(ins[0]) || (ins[0] == '.' && ins.size() >= 2 && std::isdigit(ins[1]))) {
                auto ep = std::find_if_not(ins.cbegin() + 1, ins.cend(), [] (char c) {
                    return std::isdigit(c) || c == '.';
                });
                std::string id(ins.cbegin(), ep);
                ins.remove_prefix(ep - ins.cbegin());
                if (id.find_first_of('.') != std::string::npos) {
                    return std::stof(id);
                } else {
                    return std::stoi(id);
                }
            } else if (isident(ins[0])) {
                auto ep = std::find_if_not(ins.cbegin() + 1, ins.cend(), [] (char c) {
                    return isident(c);
                });
                std::string id(ins.cbegin(), ep);
                ins.remove_prefix(ep - ins.cbegin());
                if (auto it = lutkwd.find(id); it != lutkwd.end()) {
                    return it->second;
                } else {
                    return id;
                }
            }
        }
        if (ins.size() >= 2) {
            auto it = lut2.find({ins[0], ins[1]});
            if (it != lut2.end()) {
                ins.remove_prefix(2);
                return it->second;
            }
        }
        if (ins.size() >= 1) {
            auto it = lut1.find(ins[0]);
            if (it != lut1.end()) {
                ins.remove_prefix(1);
                return it->second;
            }
        }
        return std::nullopt;
    }

    void tokenize(std::string_view ins) noexcept {
        while (!ins.empty()) {
            if (auto t = take(ins)) {
                tokens.push_back(std::move(*t));
            } else {
                break;
            }
        }
    }
};


struct AST {
    Token token;
    std::vector<std::uniqueptr<AST>> chs;
};

//$
struct AstParm : public AST {
    std::string name;//同理如果是$F,那么string就是“F”，同理如果$有初始值的话那么也需要一个类型type和value
    //同里在parser的visit时，我们也是调用一个构造函数来构造这一个节点
    //一个Parm 和一个Sym ast节点有这么些内容 name 比如"clr" "pos" "F" 类型:vec3, int float, string 如果有初始值那么会加一个value
};


//@节点
struct AstSym : public AST {
    std::string name;//属性名字比如是clr的话就是“clr”,
    //我们需要添加两个类型 @pos = vec3(3, 2, 1)，那么需要 一个type:三维，浮点，整数，字符串 string，然后一个value,value代表一个初始值
    //最后我们在visit时候调用这个AST节点的构造函数
};

//整形字面量
struct IntegerLiteral : public AST {
    int value;//字面量数值
    //构造函数
};

struct FloatLiteral : public AST {
    float value;
};

struct StringLiteral : public AST {

};


struct ZFXParser {
    std::unique_ptr<AST> root;//Ast还是用shared_ptr吧
    AST *curr{};

    explicit ZFXParser(span<Token> a_tokens) noexcept : tokens{a_tokens} {
        root = std::make_unique<AST>();//可以看作是一个根节点
        curr = root.get();
    }

    span<Token> tokens;

    Token next_token() noexcept {
        auto token = std::move(tokens.front());
        tokens.remove_prefix(1);
        return token;
    }

    bool token_eof() const noexcept {
        return tokens.empty();
    }

    std::optional<Op> next_op(std::initializer_list<Op> const &ops) noexcept {
        if (token_eof())
            return std::nullopt;
        scope_restore rst{tokens};
        auto token = next_token();
        if (auto *p_op = std::get_if<Op>(&token)) {
            auto &op = *p_op;
            if (std::find(ops.begin(), ops.end(), op) != ops.end()) {
                rst.release();
                return op;
            }
        }
        return std::nullopt;
    }
//拿解析变量说明一下,解析结果是Sym, Param两个节点
    std::unique_ptr<AST> parseVariableDecl() {
            //简单的伪代码描述一下
            auto token = next_token();
            if (token == "$" || token == "@") {
                //在预读一个字符，把clr这种读出来赋值给name

                //再判断一下是否有等于好
                auto t = next_token();
                if (t == "=") {
                    //读出他的val 和type构造这一个节点
                    return std::make_shared<AST>();
                }
            }
    }
// 解析二元表达式采用运算符优先级算法， 比如 @pos + $c * 5,那么 + 运算符优先级小于 * 所以先计算 $c * 5
//原理就是 :@pos 后面跟了个 + 号所以肯定是一个加法表达式 继续往后遍历， 遇到 $c ，我们再往后看一看后面的运算符如果优先级大于前面的就
//解析赋值表达式
//解析一元运算符号，注意是前缀还是后缀

    std::shared_ptr<AST> parseUnary() {
    //一元表达式有以下三种情况，一个是前缀一元表达式就是 +@a这种， 另外一种是后缀++ -- a++ a--这种
    //递归解析
    //先解析前置一元情况，伪代码，前端要改一下
    //
    }
    std::unique_ptr<AST> expr_atom() noexcept {
        if (token_eof())
            return nullptr;
        scope_restore rst{tokens};
        auto token = next_token();
        return overloaded{
        [&] (Ident const &ident) {
            auto node = std::make_unique<ASTSym>();
            node->token = ident;
            node->name = ident;
            rst.release();
            return node;
        },
        [&] (float const &val) {
            auto node = std::make_unique<ASTFloat>();
            node->token = val;
            node->value = val;
            rst.release();
            return node;
        },
        [&] (int const &val) {
            auto node = std::make_unique<ASTInt>();
            node->token = val;
            node->val = val;
            rst.release();
            return node;
        },
        [&] (auto const &) {
            return nullptr;
        },
        }.match<std::unique_ptr<AST>>(token);
    }

    template <std::size_t N>
    std::unique_ptr<AST> expr_template(std::initializer_list<Op> *p_ops) noexcept {
        if constexpr (N == 0) {
            return expr_atom();
        } else {
            scope_restore rst{tokens};
            if (auto lhs = expr_template<N - 1>(p_ops + 1)) {
                while (1) if (auto p_op = next_op(*p_ops)) {
                    auto &op = *p_op;
                    if (auto rhs = expr_template<N - 1>(p_ops + 1)) {
                        auto node = std::make_unique<AST>();
                        node->token = op;
                        node->chs.push_back(std::move(lhs));
                        node->chs.push_back(std::move(rhs));
                        lhs = std::move(node);
                    }
                } else break;
                rst.release();
                return lhs;
            }
            return nullptr;
        }
    }

    std::unique_ptr<AST> expr_binary() noexcept {
        std::initializer_list<Op> lvs[] = {
            { Op::kLogicOr, },
            { Op::kLogicAnd, },
            { Op::kBitOr, },
            { Op::kBitXor, },
            { Op::kBitAnd, },
            { Op::kCmpEqual, Op::kCmpNotEqual, },
            { Op::kCmpLessThan, Op::kCmpLessEqual, Op::kCmpGreaterThan, Op::kCmpGreaterEqual, },
            { Op::kBitShl, Op::kBitShr, },
            { Op::kPlus, Op::kMinus, },
            { Op::kMultiply, Op::kDivide, Op::kModulus, },
            { Op::kAssign, Op::kPlusAssign, Op::kMinusAssign, Op::kMultiplyAssign, Op::kDivideAssign, Op::kModulusAssign, Op::kBitAndAssign, Op::kBitOrAssign, Op::kBitXorAssign, },
            { Op::kComma, },
        };
        return expr_template<std::size(lvs)>(std::data(lvs));
    }

    std::unique_ptr<AST> expr_top() noexcept {
        scope_restore rst{tokens};
        auto node = std::make_unique<AST>();
        node->token = Op::kSemicolon;
        while (1) if (auto p_stm = expr_binary()) {
            if (!next_op({Op::kSemicolon})) {
                break;
            }
            node->chs.push_back(std::move(p_stm));
        } else break;
        rst.release();
        return node;
    }
};

/*
enum class IRId : std::uint32_t {};

struct IREmpty {
};

struct IRBlock {
    std::vector<IRId> stmts;
};

struct IROp {
    Op op;
    std::vector<IRId> args;
};

struct IRSym {
    std::string name;
};

struct IRConstFloat {
    float val;
};

struct IRConstInt {
    int val;
};

using IRNode = std::variant
< IREmpty
, IRConstInt
, IRConstFloat
, IROp
, IRSym
>;
*/
struct ZFXLower {
    std::vector<IRNode> nodes;

    IRId visit(AST const *ast) noexcept {
        IRId currid{static_cast<std::uint32_t>(nodes.size())};
        auto node = overloaded{
            [&] (Ident const &name) {
                return IRSym{name};
            },
            [&] (Op const &op) {
                std::vector<IRId> chsid;
                chsid.reserve(ast->chs.size());
                for (auto const &chast: ast->chs) {
                    chsid.push_back(visit(chast.get()));
                }
                return IROp{op, std::move(chsid)};
            },
            [&] (float const &val) {
                return IRConstFloat{val};
            },
            [&] (int const &val) {
                return IRConstInt{val};
            },
            [&] (auto const &) {
                return IREmpty{};
            },
        }.match<IRNode>(ast->token);
        nodes.push_back(std::move(node));
        return currid;
    }
};


enum class RegId : std::uint32_t {};
enum class SymId : std::uint32_t {};

struct ZFXScanner {
    span<IRNode> nodes;
    std::vector<RegId> reglut;
    std::map<IRId, IRId> irdeps;

    explicit ZFXScanner(span<IRNode> a_nodes) noexcept : nodes{a_nodes} {}

    void scan() {
        std::uint32_t nodenr = 0;
        for (auto const &node: nodes) {
            IRId irid{nodenr};
            overloaded{
                [&] (IROp const &ir) {
                    for (auto const &arg: ir.args) {
                        irdeps.insert({irid, arg});
                    }
                },
                [&] (auto const &) {
                },
            }.match(node);
            reglut.push_back(RegId{to_underlying(irid)});
            ++nodenr;
        }
    }
};


struct ZFXEmitter {
    span<IRNode> nodes;
    span<RegId> reglut;
    std::vector<std::uint32_t> codes;

    static const inline std::map<Op, Bc> op2bc{
        {Op::kPlus, Bc::kPlus},
        {Op::kMinus, Bc::kMinus},
        {Op::kMultiply, Bc::kMultiply},
        {Op::kDivide, Bc::kDivide},
        {Op::kModulus, Bc::kModulus},
        {Op::kBitInverse, Bc::kBitInverse},
        {Op::kBitAnd, Bc::kBitAnd},
        {Op::kBitOr, Bc::kBitOr},
        {Op::kBitXor, Bc::kBitXor},
        {Op::kBitShl, Bc::kBitShl},
        {Op::kBitShr, Bc::kBitShr},
        {Op::kLogicNot, Bc::kLogicNot},
        {Op::kLogicAnd, Bc::kLogicAnd},
        {Op::kLogicOr, Bc::kLogicOr},
        {Op::kCmpEqual, Bc::kCmpEqual},
        {Op::kCmpNotEqual, Bc::kCmpNotEqual},
        {Op::kCmpLessThan, Bc::kCmpLessThan},
        {Op::kCmpLessEqual, Bc::kCmpLessEqual},
        {Op::kCmpGreaterThan, Bc::kCmpGreaterThan},
        {Op::kCmpGreaterEqual, Bc::kCmpGreaterEqual},
    };

    std::map<std::string, SymId> symlut;

    explicit ZFXEmitter(span<IRNode> a_nodes, span<RegId> a_reglut) noexcept
        : nodes{a_nodes}, reglut{a_reglut} {
    }

    void emit_bc(Bc bc) noexcept {
        codes.push_back(to_underlying(bc));
    }

    void emit_reg(RegId nr) noexcept {
        codes.push_back(to_underlying(nr));
    }

    void emit_sym(Ident const &id) noexcept {
        auto it = symlut.find(id);
        if (it != symlut.end()) {
            codes.push_back(to_underlying(it->second));
        } else {
            SymId symid{static_cast<std::uint32_t>(symlut.size())};
            symlut.insert({id, symid});
            codes.push_back(to_underlying(symid));
        }
    }

    void emit_int(int x) noexcept {
        codes.push_back(bit_cast<std::uint32_t>(x));
    }

    void emit_float(float x) noexcept {
        codes.push_back(bit_cast<std::uint32_t>(x));
    }

    void generate() noexcept {
        std::uint32_t nodenr = 0;
        for (IRNode const &node: nodes) {
            overloaded{
                [&] (IRConstInt const &ir) {
                    emit_bc(Bc::kLoadConstInt);
                    emit_reg(reglut.at(nodenr));
                    emit_int(ir.val);
                },
                [&] (IRConstFloat const &ir) {
                    emit_bc(Bc::kLoadConstFloat);
                    emit_reg(reglut.at(nodenr));
                    emit_float(ir.val);
                },
                [&] (IROp const &ir) {
                    if (auto it = op2bc.find(ir.op); it != op2bc.end()) {
                        emit_bc(it->second);
                        emit_reg(reglut.at(nodenr));
                        for (auto const &a: ir.args) {
                            emit_reg(reglut.at(to_underlying(a)));
                        }
                    } else if (ir.op == Op::kAssign) {
                    }
                },
                [&] (IRSym const &ir) {
                    emit_bc(Bc::kAddrSymbol);
                    emit_sym(ir.name);
                },
                [&] (auto const &) {
                },
            }.match(node);
            ++nodenr;
        }
    }
};


}


ZFXCode::ZFXCode(std::string_view ins) {
    ZFXTokenizer tok;
    tok.tokenize(ins);

    ZFXParser par{tok.tokens};
    auto ast = par.expr_top();
    if (!ast) throw std::runtime_error("failed to parse");

    ZFXLower low;
    auto irid = low.visit(ast.get());

    ZFXScanner sca{low.nodes};
    sca.scan();
    
    ZFXEmitter emi{low.nodes, sca.reglut};
    emi.generate();

    syms.resize(emi.symlut.size());
    for (auto &[k, v]: emi.symlut) {
        syms[to_underlying(v)] = std::move(k);
    }

    codes = std::move(emi.codes);
    nregs = sca.reglut.size();
}


#if 0
//实验性的字节码生成程序
//一个模板函数
template<typename T>
bool isType(const std::any& a) {
    return typeid(T) == a.type();
}
//实验性的字节码生成，目前只支持整数变量和浮点变量做加减, 如果遇到字符串和浮点数那就直接加入常量池

class BCModule {
public:
    std::vector<std::any> consts;
    //后期可以将一些内置函数加入进来
};
class BCGenerator {

    //需要一个Modules,代表一个可执行的计算模块
public:
    std::shared_ptr<BCModule> m;

    BCGenerator() {
        this->m = std::make_shared<Module>();
    }

    //有一个转换函数，将任何std::any 转换成uin8_t的opcode数组
    std::vector<uint8_t> anyToCode(const std::any& val) {
        if (val.has_value() && isType<std::vector<std::uint8_t>>(val)) {
            return std::any_cast<std::vector<uint8_t>>(val);
        }
        return {};
    }

    //拼接一下代码
    void concatCodeWithAny(std::vector<uint8_t>& code, const std::any& val) {
        if (val.has_value() && isType<std::vector<uint8_t>>(val)) {
            auto vec = std::any_cast<std::vector<uint8_t>>(val);
            code.insert(code.end(), vec.begin(), vec.end());
        }
        return;
    }

    //开始访问,目前先访问整形，浮点型，二元运算节点
    std::any visitInt() {
        std::vector<uint8_t> code;
        code.push_back(kLoad);
        code.push_back(value);
        return code;
    }

    std::any visitVariableDecl() {
        //为变量生成字节码，目前只有$ @两种变量
        std::vector<uint8_t> code;
//逻辑很简单，就是看一下变量是否有初始化，如果有就生成变量赋值的指令
        return code;
    }
    std::any visitFloat() {
//考虑到float没法转成uin8_t,就是把浮点数放到常量池中去,code中加入取值指令和下标
    //code.push_back();
    //code.push_back();//加载指令
    }

    std::any visitBinary(Binary& bi) {
        std::vector<uint8_t> code;
        //bi
//auto ret1;访问左子树代码
//auto ret2;访问右子树代码
//注意访问二元运算符的时候。我们不处理赋值运算
//拼接完ret1 和 ret2后再拼接opcode
        switch(OpCode) {
            case :
                if () {
            //如果是加运算符，那么需要盘点一些类型是否是string， string + 和number + 是两条指令
                }
            //接下来是各种二元指令运算，唯一要注意的是如果是<= < >= > != == 这种二元运算指令，我们需要引入一个临时变量

        }
        return code;
    }

     std::any visitUnary() {
//解析一元运算符也很简单
    }

    //生成获取本地变量值的指令
     std::vector<uint8_t> getVariableValue() {
        //逻辑很简单，就是生成一个load指令和一个变量数组的下标
    }

    std::vector<uint8_t> setVariableValue() {
        //和get相反我们生成一个store指令
    }

     //计算条件语句产生的偏移量
     void addOfferToJumpOp() {

    }

    std::vector<uint8_t> visitIfStatement() {

    }

    std::vector<uint8_t> visitForStatement() {

    }

    std::any visitString () {
        //直接将字符串加入到常量池中去
        this->m->consts.push_back();
    }


    std::any visitFunctionCall() {
        //这里所说的函数调用不包括sin,cos，tan这些内置函数，而是用户自定义函数，实验性质
        std::vector<uint8_t> code;
        //首先将函数参数压入栈中
        //查找本地变量的下标
        return code;
    }

    std::any visitReturnStatement() {
        //为了伺候函数返回语句
    }
/*
 * 虚拟机执行函数调用的时候，设置一个返回地址，当函数return时,会回到这一个位置，为被调用的函数创建一个新的栈帧，加入到调用栈中
 * 从操作数栈中取出函数的值， 赋值给新的栈帧，也就是说加入到了函数栈中的本地变量了， 将代码切换到被调用函数的代码中，并把代码计数器设置为0
 * 函数返回的操作是这样的，如果有返回值的话那么就直接从当前操作数栈中取出来，加载到上一级操作数栈中去，
 * 从调用栈中弹出当前当前函数栈
 * 将代码切换到调用者的代码， 并且把代码指针指向下一个
 * 一些sin,cos之类的函数，不需要生成在字节码文件里，把他当作内置函数直接调用即可
 * */

/*
 * 伺候if for等条件和循环语句
 * if的话，首先字节码需要支持一些if指令
 * 先生成条件指令 if块的代码， else块的代码，再计算偏移量
 * */


};

#endif
    class VMStackFrame {
        //对应的函数
        // std::shared_ptr<> function;
        //返回地址
        //uint32_t returnIndex = 0;
        //操作数栈
        //std::vector<std::any> oprandStack;
        //
    };

    class VM {
    public:
        std::vector<std::shared_ptr<VMStackFrame>> callStack;//调用栈

        //创建栈帧其实就是将当前函数的栈帧加入到调用栈帧中
        //auto frame = std::make_shared<VMStackFrame>();
        //this->callStack.push_back(frame);
        //判断一下当前函数栈帧是否有bytecode
        //std::vector<uint8_t> code;
        /*
        if() {
            this->code = frame->bytecode
        }
        */

        while (true) {
            switch(opcode) {

            }
        }
    };
}
