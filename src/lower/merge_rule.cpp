#include "merge_rule.h"

#include <vector>
#include <stack>

#include "expr.h"
#include "expr_visitor.h"
#include "expr_nodes.h"
#include "util/collections.h"

using namespace std;

namespace taco {
namespace lower {

// class MergeRuleNode
MergeRuleNode::~MergeRuleNode() {
}

std::ostream& operator<<(std::ostream& os, const MergeRuleNode& node) {
  class MergeRulePrinter : public MergeRuleVisitor {
  public:
    MergeRulePrinter(std::ostream& os) : os(os) {
    }
    std::ostream& os;
    void visit(const Step* rule) {
      os << rule->step.getPath().getTensor().getName();
    }
    void visit(const And* rule) {
      rule->a.accept(this);
      os << " \u2227 ";
      rule->b.accept(this);
    }
    void visit(const Or* rule) {
      rule->a.accept(this);
      os << " \u2228 ";
      rule->b.accept(this);
    }
  };
  MergeRulePrinter printer(os);
  node.accept(&printer);
  return os;
}


// class MergeRule
MergeRule::MergeRule(const MergeRuleNode* n)
    : util::IntrusivePtr<const MergeRuleNode>(n) {
}

MergeRule::MergeRule() : util::IntrusivePtr<const MergeRuleNode>() {
}

MergeRule MergeRule::make(const internal::Tensor& tensor, const Var& var,
                          const map<Expr,TensorPath>& tensorPaths,
                          const TensorPath& resultTensorPath) {

  struct ComputeMergeRule : public internal::ExprVisitor {
    using ExprVisitor::visit;

    ComputeMergeRule(Var var, const std::map<Expr,TensorPath>& tensorPaths)
        : var(var), tensorPaths(tensorPaths) {}

    Var var;
    const std::map<Expr,TensorPath>& tensorPaths;

    MergeRule mergeRule;
    MergeRule computeMergeRule(const Expr& expr) {
      expr.accept(this);
      return mergeRule;
    }

    void visit(const internal::Read* op) {
      size_t varLoc = util::locate(op->indexVars, var);
      mergeRule = Step::make(TensorPathStep(tensorPaths.at(op), (int)varLoc));
    }

    void createOrRule(const internal::BinaryExpr* node) {
      MergeRule a = computeMergeRule(node->a);
      MergeRule b = computeMergeRule(node->b);
      mergeRule = Or::make(a, b);
    }

    void createAndRule(const internal::BinaryExpr* node) {
      MergeRule a = computeMergeRule(node->a);
      MergeRule b = computeMergeRule(node->b);
      mergeRule = And::make(a, b);
    }

    void visit(const internal::Add* op) {
      createOrRule(op);
    }

    void visit(const internal::Sub* op) {
      createOrRule(op);
    }

    void visit(const internal::Mul* op) {
      createAndRule(op);
    }

    void visit(const internal::Div* op) {
      createAndRule(op);
    }
  };
  MergeRule mergeRule =
      ComputeMergeRule(var,tensorPaths).computeMergeRule(tensor.getExpr());
  size_t varLoc = util::locate(tensor.getIndexVars(), var);
  const_cast<MergeRuleNode*>(mergeRule.ptr)->resultStep =
      TensorPathStep(resultTensorPath, (int)varLoc);
  return mergeRule;
}

std::vector<TensorPathStep> MergeRule::getSteps() const {
  struct GetPathsVisitor : public lower::MergeRuleVisitor {
    using MergeRuleVisitor::visit;
    vector<lower::TensorPathStep> steps;
    void visit(const lower::Step* rule) {
      steps.push_back(rule->step);
    }
  };
  GetPathsVisitor getPathsVisitor;
  this->accept(&getPathsVisitor);
  return getPathsVisitor.steps;
}

TensorPathStep MergeRule::getResultStep() const {
  return ptr->resultStep;
}

void MergeRule::accept(MergeRuleVisitor* v) const {
  ptr->accept(v);
}

std::ostream& operator<<(std::ostream& os, const MergeRule& mergeRule) {
  return os << *(mergeRule.ptr);
}


// class Path
MergeRule Step::make(const TensorPathStep& step) {
  auto* node = new Step;
  node->step = step;
  return node;
}

void Step::accept(MergeRuleVisitor* v) const {
  v->visit(this);
}


// class And
MergeRule And::make(MergeRule a, MergeRule b) {
  auto node = new And;
  node->a = a;
  node->b = b;
  return node;
}

void And::accept(MergeRuleVisitor* v) const {
  v->visit(this);
}


// class Or
MergeRule Or::make(MergeRule a, MergeRule b) {
  auto* node = new Or;
  node->a = a;
  node->b = b;
  return node;
}

void Or::accept(MergeRuleVisitor* v) const {
  v->visit(this);
}


// class MergeRuleVisitor
MergeRuleVisitor::~MergeRuleVisitor() {
}

void MergeRuleVisitor::visit(const Step* rule) {
}

void MergeRuleVisitor::visit(const And* rule) {
  rule->a.accept(this);
  rule->b.accept(this);
}

void MergeRuleVisitor::visit(const Or* rule) {
  rule->a.accept(this);
  rule->b.accept(this);
}

}}