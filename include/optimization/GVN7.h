#pragma once
#include "BasicBlock.h"
#include "Constant.h"
#include "DeadCode.h"
#include "FuncInfo.h"
#include "Function.h"
#include "Instruction.h"
#include "Module.h"
#include "PassManager.hpp"
#include "Value.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

struct CongruenceClass;

namespace GVNExpression {

// fold the constant value
class ConstFolder {
  public:
    ConstFolder(Module *m) : module_(m) {}
    Constant *compute(Instruction *instr, Constant *value1, Constant *value2);
    Constant *compute(Instruction *instr, Constant *value1);

  private:
    Module *module_;
};

/**
 * for constructor of class derived from `Expression`, we make it public
 * because `std::make_shared` needs the constructor to be publicly available,
 * but you should call the static factory method `create` instead the constructor itself to get the desired data
 */
class Expression {
  public:
    // TODO: you need to extend expression types according to testcases
    enum gvn_expr_t { e_constant, e_bin, e_phi, e_cmp, e_fcmp, e_alloca, e_load, e_call,
     e_gep, e_si2fp, e_fp2si, e_zext, e_argu, e_global, e_index };
    Expression(gvn_expr_t t) : expr_type(t) {}
    virtual ~Expression() = default;
    virtual std::string print() = 0;
    gvn_expr_t get_expr_type() const { return expr_type; }

  private:
    gvn_expr_t expr_type;
};

bool operator==(const std::shared_ptr<Expression> &lhs, const std::shared_ptr<Expression> &rhs);
bool operator==(const GVNExpression::Expression &lhs, const GVNExpression::Expression &rhs);

// arithmetic expression
class BinaryExpression : public Expression {
  public:
    static std::shared_ptr<BinaryExpression> create(Instruction::OpID op,
                                                    std::shared_ptr<Expression> lhs,
                                                    std::shared_ptr<Expression> rhs) {
        return std::make_shared<BinaryExpression>(op, lhs, rhs);
    }
    virtual std::string print() {
        return "(" + Instruction::get_instr_op_name(op_) + " " + lhs_->print() + " " + rhs_->print() + ")";
    }

    bool equiv(const BinaryExpression *other) const {
        if (op_ == other->op_ and *lhs_ == *(other->lhs_) and *rhs_ == *(other->rhs_)) {
            return true;
        }
        else
            return false;
    }

    BinaryExpression(Instruction::OpID op, std::shared_ptr<Expression> lhs, std::shared_ptr<Expression> rhs)
        : Expression(e_bin), op_(op), lhs_(lhs), rhs_(rhs) {}

    std::shared_ptr<Expression> get_lve() { return lhs_; }
    std::shared_ptr<Expression> get_rve() { return rhs_; }
    Instruction::OpID get_op() { return op_; }

  private:
    Instruction::OpID op_;
    std::shared_ptr<Expression> lhs_, rhs_;
};


class ConstantExpression : public Expression {
  public:
    static std::shared_ptr<ConstantExpression> create(Constant *c) { return std::make_shared<ConstantExpression>(c); }
    virtual std::string print() { return c_->print(); }
    // we leverage the fact that constants in lightIR have unique addresses
    bool equiv(const ConstantExpression *other) const { return c_ == other->c_; }
    ConstantExpression(Constant *c) : Expression(e_constant), c_(c) {}
    Constant * get_val() { return c_; }

  private:
    Constant *c_;
};

class ArguExpression : public Expression {
  public:
    static std::shared_ptr<ArguExpression> create(Argument * argu) { return std::make_shared<ArguExpression>(argu); }
    virtual std::string print() { std::string str = "arg"; return str; }
    // we leverage the fact that arguments in lightIR have unique addresses
    bool equiv(const ArguExpression *other) const { return argu == other->argu; }
    ArguExpression(Argument *argu) : Expression(e_argu), argu(argu) {}

  private:
    Argument *argu;
};

class GlobalExpression : public Expression {
  public:
    static std::shared_ptr<GlobalExpression> create(GlobalVariable* global_var) { return std::make_shared<GlobalExpression>(global_var); }
    virtual std::string print() { return global_var->print(); }
    // we leverage the fact that arguments in lightIR have unique addresses
    bool equiv(const GlobalExpression *other) const { return global_var == other->global_var; }
    GlobalExpression(GlobalVariable *global_var) : Expression(e_global), global_var(global_var) {}

  private:
    GlobalVariable *global_var;
};

class AllocaExpression : public Expression {
  public:
    static std::shared_ptr<AllocaExpression> create(Instruction *instr) { return std::make_shared<AllocaExpression>(instr); }
    virtual std::string print() { std::string str = "alloca"; return str; }
    // we leverage the fact that alloca instrs in lightIR have unique addresses (hopfully)
    bool equiv(const AllocaExpression *other) const { return AllocaInstr == other->AllocaInstr; }
    AllocaExpression(Instruction *instr) : Expression(e_alloca), AllocaInstr(instr) {}

  private:
    Instruction* AllocaInstr;
};

class LoadExpression : public Expression {
  public:
    static std::shared_ptr<LoadExpression> create(Instruction* LoadInstr) { return std::make_shared<LoadExpression>(LoadInstr); }
    virtual std::string print() { std::string str = "load"; return str; }
    // we leverage the fact that alloca instrs in lightIR have unique addresses (hopfully)
    bool equiv(const LoadExpression *other) const { return LoadInstr == other->LoadInstr; }
    LoadExpression(Instruction* LoadInstr) : Expression(e_load), LoadInstr(LoadInstr) {}

  private:
    Instruction* LoadInstr;
};



class IndexExpression : public Expression {
  public:
    static std::shared_ptr<IndexExpression> create(int index) { return std::make_shared<IndexExpression>(index); }
    virtual std::string print() { std::string str = "v" + std::to_string(index); return str; }
    // we leverage the fact that alloca instrs in lightIR have unique addresses (hopfully)
    bool equiv(const IndexExpression *other) const { return index == other->index; }
    IndexExpression(int index) : Expression(e_index), index(index) {}
    int get_index() { return index; }

  private:
    int index;
};

class Si2fpExpression : public Expression {
  public:
    static std::shared_ptr<Si2fpExpression> create(std::shared_ptr<Expression> orig, Type *dest_ty_) { return std::make_shared<Si2fpExpression>(orig, dest_ty_); }
    virtual std::string print() { std::string str = "si2fp"; return str; }
    // we leverage the fact that alloca instrs in lightIR have unique addresses (hopfully)
    bool equiv(const Si2fpExpression *other) const { return (*(orig) == *(other->orig) && dest_ty_ == other->dest_ty_); }
    Si2fpExpression(std::shared_ptr<Expression> orig, Type *dest_ty_) : Expression(e_si2fp), orig(orig), dest_ty_(dest_ty_) {}
    Type * get_dest_type() const { return dest_ty_; }

  private:
    std::shared_ptr<Expression> orig;
    Type *dest_ty_;
};

class ZextExpression : public Expression {
  public:
    static std::shared_ptr<ZextExpression> create(std::shared_ptr<Expression> orig, Type *dest_ty_) { return std::make_shared<ZextExpression>(orig, dest_ty_); }
    virtual std::string print() { std::string str = "zext"; return str; }
    // we leverage the fact that alloca instrs in lightIR have unique addresses (hopfully)
    bool equiv(const ZextExpression *other) const { 
      return (*(orig) == *(other->orig) && dest_ty_ == other->dest_ty_); 
    }
    ZextExpression(std::shared_ptr<Expression> orig, Type *dest_ty_) : Expression(e_zext), orig(orig), dest_ty_(dest_ty_) {}
    Type * get_dest_type() const { return dest_ty_; }

  private:
    std::shared_ptr<Expression> orig;
    Type *dest_ty_;
};

class Fp2siExpression : public Expression {
  public:
    static std::shared_ptr<Fp2siExpression> create(std::shared_ptr<Expression> orig, Type *dest_ty_) { return std::make_shared<Fp2siExpression>(orig, dest_ty_); }
    virtual std::string print() { std::string str = "fp2si"; return str; }
    // we leverage the fact that alloca instrs in lightIR have unique addresses (hopfully)
    bool equiv(const Fp2siExpression *other) const { return (*(orig) == *(other->orig) && dest_ty_ == other->dest_ty_); }
    Fp2siExpression(std::shared_ptr<Expression> orig, Type *dest_ty_) : Expression(e_fp2si), orig(orig), dest_ty_(dest_ty_) {}
    Type * get_dest_type() const { return dest_ty_; }

  private:
    std::shared_ptr<Expression> orig;
    Type *dest_ty_;
};


class PhiExpression : public Expression {
  public:
    static std::shared_ptr<PhiExpression> create(std::shared_ptr<Expression> lhs, std::shared_ptr<Expression> rhs) {
        return std::make_shared<PhiExpression>(lhs, rhs);
    }
    virtual std::string print() { return "(phi " + lhs_->print() + " " + rhs_->print() + ")"; }
    bool equiv(const PhiExpression *other) const {
        if (*lhs_ == *other->lhs_ and *rhs_ == *other->rhs_)
            return true;
        else
            return false;
    }
    PhiExpression(std::shared_ptr<Expression> lhs, std::shared_ptr<Expression> rhs)
        : Expression(e_phi), lhs_(lhs), rhs_(rhs) {}

    std::shared_ptr<Expression> get_lve() { return lhs_; }
    std::shared_ptr<Expression> get_rve() { return rhs_; }

  private:
    std::shared_ptr<Expression> lhs_, rhs_;
};

class CmpExpression : public Expression {
  public:
    static std::shared_ptr<CmpExpression> create(CmpInst::CmpOp op,
                                                    std::shared_ptr<Expression> lhs,
                                                    std::shared_ptr<Expression> rhs) {
        return std::make_shared<CmpExpression>(op, lhs, rhs);
    }
    virtual std::string print() {
        return "(cmp " + lhs_->print() + " " + rhs_->print() + ")";
    }

    bool equiv(const CmpExpression *other) const {
        if (op_ == other->op_ and *lhs_ == *other->lhs_ and *rhs_ == *other->rhs_)
            return true;
        else
            return false;
    }

    CmpExpression(CmpInst::CmpOp op, std::shared_ptr<Expression> lhs, std::shared_ptr<Expression> rhs)
        : Expression(e_cmp), op_(op), lhs_(lhs), rhs_(rhs) {}

    std::shared_ptr<Expression> get_lve() { return lhs_; }
    std::shared_ptr<Expression> get_rve() { return rhs_; }
    CmpInst::CmpOp get_op() { return op_; }

  private:
    CmpInst::CmpOp op_;
    std::shared_ptr<Expression> lhs_, rhs_;
};

class FCmpExpression : public Expression {
  public:
    static std::shared_ptr<FCmpExpression> create(FCmpInst::CmpOp op,
                                                    std::shared_ptr<Expression> lhs,
                                                    std::shared_ptr<Expression> rhs) {
        return std::make_shared<FCmpExpression>(op, lhs, rhs);
    }
    virtual std::string print() {
        return "(fcmp " + lhs_->print() + " " + rhs_->print() + ")";
    }

    bool equiv(const FCmpExpression *other) const {
        if (op_ == other->op_ and *lhs_ == *other->lhs_ and *rhs_ == *other->rhs_)
            return true;
        else
            return false;
    }

    FCmpExpression(FCmpInst::CmpOp op, std::shared_ptr<Expression> lhs, std::shared_ptr<Expression> rhs)
        : Expression(e_fcmp), op_(op), lhs_(lhs), rhs_(rhs) {}

    std::shared_ptr<Expression> get_lve() { return lhs_; }
    std::shared_ptr<Expression> get_rve() { return rhs_; }
    FCmpInst::CmpOp get_op() { return op_; }

  private:
    FCmpInst::CmpOp op_;
    std::shared_ptr<Expression> lhs_, rhs_;
};

class CallExpression : public Expression {
  public:
    static std::shared_ptr<CallExpression> create(Function* func,
                                                    std::vector<std::shared_ptr<Expression>> args,
                                                    int arg_num,
                                                    bool is_pure,
                                                    Instruction * instr) {
        return std::make_shared<CallExpression>(func, args, arg_num, is_pure, instr);
    }
    virtual std::string print() {
        return "(call func)";
    }

    bool equiv(const CallExpression *other) const {
        if (instr == other->instr) return true;
        if (is_pure == false) return false;
        if ((func == other->func) == false)
            return false;
        for (int i = 0; i < arg_num; i++) {
          if((*(args[i]) == *(other->args[i])) == false)
            return false;
        }
        return true;
    }

    CallExpression(Function* func, std::vector<std::shared_ptr<Expression>> args, int arg_num, bool is_pure, Instruction* instr)
        : Expression(e_call), func(func), args(args), arg_num(arg_num), is_pure(is_pure), instr(instr) {}

    std::vector<std::shared_ptr<Expression>> get_args() { return args; }
    Function* get_func() { return func; }
    int get_arg_num() { return arg_num; }

  private:
    Function* func;
    std::vector<std::shared_ptr<Expression>> args;
    int arg_num;
    bool is_pure;
    Instruction * instr;
};

class GepExpression : public Expression {
  public:
    static std::shared_ptr<GepExpression> create(std::vector<std::shared_ptr<Expression>> indxs,
                                                    int indx_num) {
        return std::make_shared<GepExpression>(indxs, indx_num);
    }
    virtual std::string print() {
        return "(call gep)";
    }

    bool equiv(const GepExpression *other) const {
        for (int i = 0; i < indx_num; i++) {
          if((*(indxs[i]) == *(other->indxs[i])) == false)
            return false;
        }
        return true;
    }

    GepExpression(std::vector<std::shared_ptr<Expression>> indxs, int indx_num)
        : Expression(e_gep), indxs(indxs), indx_num(indx_num) {}

    std::vector<std::shared_ptr<Expression>> get_indxs() { return indxs; }
    int get_indx_num() { return indx_num; }

  private:
    std::vector<std::shared_ptr<Expression>> indxs;
    int indx_num;
};
} // namespace GVNExpression

/**
 * Congruence class in each partitions
 * note: for constant propagation, you might need to add other fields
 * and for load/store redundancy detection, you most certainly need to modify the class
 */
struct CongruenceClass {
    size_t index_;
    // representative of the congruence class, used to replace all the members (except itself) when analysis is done
    Value *leader_;
    // value expression in congruence class
    std::shared_ptr<GVNExpression::Expression> value_expr_;
    // value φ-function is an annotation of the congruence class
    std::shared_ptr<GVNExpression::PhiExpression> value_phi_;
    // equivalent variables in one congruence class
    std::set<Value *> members_;

    CongruenceClass(size_t index) : index_(index), leader_{}, value_expr_{}, value_phi_{}, members_{} {}
    size_t get_indx() { return index_; }

    bool operator<(const CongruenceClass &other) const { return this->index_ < other.index_; }
    bool operator==(const CongruenceClass &other) const;
};

namespace std {
template <>
// overload std::less for std::shared_ptr<CongruenceClass>, i.e. how to sort the congruence classes
struct less<std::shared_ptr<CongruenceClass>> {
    bool operator()(const std::shared_ptr<CongruenceClass> &a, const std::shared_ptr<CongruenceClass> &b) const {
        // nullptrs should never appear in partitions, so we just dereference it
        return *a < *b;
    }
};
} // namespace std

class GVN : public Pass {
  public:
    using partitions = std::set<std::shared_ptr<CongruenceClass>>;
    GVN(Module *m, bool dump_json) : Pass(m), dump_json_(dump_json) {}
    // pass start
    void run() override;
    // init for pass metadata;
    void initPerFunction();

    // fill the following functions according to Pseudocode, **you might need to add more arguments**
    void detectEquivalences(std::unique_ptr<FuncInfo> &func_info_);
    partitions join(const partitions &P1, const partitions &P2);
    std::shared_ptr<CongruenceClass> intersect(std::shared_ptr<CongruenceClass>, std::shared_ptr<CongruenceClass>);
    partitions transferFunction(Instruction *x, Value *e, partitions& pin, std::unique_ptr<FuncInfo> &func_info_);
    std::shared_ptr<GVNExpression::Expression> valuePhiFunc(Instruction * instr, BasicBlock *bb_ptr, std::shared_ptr<GVNExpression::Expression>,
                                                               const partitions &);
    std::shared_ptr<GVNExpression::Expression> valueExpr(Value *instr, std::unique_ptr<FuncInfo> &func_info_, const partitions& p);
    std::shared_ptr<GVNExpression::Expression> getVN(const partitions &pout,
                                                     std::shared_ptr<GVNExpression::Expression> ve);

    std::shared_ptr<CongruenceClass> get_C(int index, const partitions & P);

    std::shared_ptr<GVNExpression::Expression> fold_constant(Instruction * instr, std::shared_ptr<GVNExpression::Expression> lhs, std::shared_ptr<GVNExpression::Expression> rhs, const partitions & Pl, const partitions & Pr, const partitions & P);

    // replace cc members with leader
    void replace_cc_members();

    // note: be careful when to use copy constructor or clone
    partitions clone(const partitions &p);

    // create congruence class helper
    std::shared_ptr<CongruenceClass> createCongruenceClass(size_t index = 0) {
        return std::make_shared<CongruenceClass>(index);
    }

    bool dfs_bb(BasicBlock * bb_ptr, bool is_first, std::unique_ptr<FuncInfo> &func_info_);

  private:
    bool dump_json_;
    std::uint64_t next_value_number_ = 1;
    Function *func_;
    std::map<BasicBlock *, partitions> pin_, pout_;
    std::unique_ptr<FuncInfo> func_info_;
    std::unique_ptr<GVNExpression::ConstFolder> folder_;
    std::unique_ptr<DeadCode> dce_;

};

bool operator==(const GVN::partitions &p1, const GVN::partitions &p2);
