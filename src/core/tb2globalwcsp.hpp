#ifndef TB2GLOBALWCSP_HPP_
#define TB2GLOBALWCSP_HPP_

#include "tb2abstractconstr.hpp"
#include "tb2enumvar.hpp"
#include "tb2wcsp.hpp"

extern void setvalue(int wcspId, int varIndex, Value value, void* solver);
extern void tb2setvalue(int wcspId, int varIndex, Value value, void* solver);
extern void tb2removevalue(int wcspId, int varIndex, Value value, void* solver);
extern void tb2setmin(int wcspId, int varIndex, Value value, void* solver);
extern void tb2setmax(int wcspId, int varIndex, Value value, void* solver);

class WeightedCSPConstraint : public AbstractNaryConstraint {
    bool isfinite; // true if any complete assignment of the input problem (or negproblem), before enforcing lb and ub, has a finite cost or if it is forbidden then there is another redundant constraint in the master problem which forbids the same assigment
    bool strongDuality; // if true then it assumes the propagation is complete when all channeling variables in the scope are assigned and the semantic of the constraint enforces that the optimum on the remaining variables is between lb and ub.
    Cost lb; // encapsulated slave problem lower bound hard constraint (must be greater or equal to this bound)
    Cost ub; // encapsulated slave problem upper bound hard constraint (must be strictly less than this bound)
    Cost negCost; // sum of cost shifts from slave problem and from its negative form
    WCSP *problem; // encapsulated slave problem
    WCSP *negproblem; // encapsulated slave problem in negative form (should be equivalent to -problem)
    StoreInt nonassigned; // number of non-assigned variables during search, must be backtrackable!
    vector<int> varIndexes; // copy of scope using integer identifiers inside slave problem (should be equal to [0, 1, 2, ..., arity-1])
    vector<Value> newValues; // used to convert Tuples into variable assignments
    vector<Long> conflictWeights; // used by weighted degree heuristics

public:
    static WCSP* MasterWeightedCSP; // Master problem used by value and variable ordering heuristics
    static map<int, WeightedCSPConstraint *> WeightedCSPConstraints;

    WeightedCSPConstraint(WCSP* wcsp, EnumeratedVariable** scope_in, int arity_in, WCSP *problem_in, WCSP *negproblem_in, Cost lb_in, Cost ub_in, bool duplicateHard = false, bool strongDuality_ = false)
        : AbstractNaryConstraint(wcsp, scope_in, arity_in)
        , isfinite(true)
        , strongDuality(strongDuality_)
        , lb(lb_in)
        , ub(ub_in)
        , negCost(MIN_COST)
        , problem(problem_in)
        , negproblem(negproblem_in)
        , nonassigned(arity_in)
    {
        assert(!problem || arity_ == (int)problem->numberOfVariables());
        assert(!negproblem || arity_ == (int)negproblem->numberOfVariables());
        if (lb >= ub) {
            cerr << "Wrong bounds in WeightedCSPConstraint: " << lb << " < " << ub << endl;
            throw WrongFileFormat();
        }
//        if (ToulBar2::preprocessFunctional > 0) {
//            cout << "Warning! Cannot perform functional elimination with WeightedCSPConstraint." << endl;
//            ToulBar2::preprocessFunctional = 0;
//        }
//        if (ToulBar2::elimDegree >= 0 || ToulBar2::elimDegree_preprocessing >= 0) {
//            cout << "Warning! Cannot perform variable elimination with WeightedCSPConstraint." << endl;
//            ToulBar2::elimDegree = -1;
//            ToulBar2::elimDegree_preprocessing = -1;
//            ToulBar2::elimDegree_ = -1;
//            ToulBar2::elimDegree_preprocessing_ = -1;
//        }
//        if (ToulBar2::DEE >= 1) {
//            cout << "Warning! Cannot perform dead-end elimination with WeightedCSPConstraint." << endl;
//            ToulBar2::DEE = 0;
//            ToulBar2::DEE_ = 0;
//        }
//        if (ToulBar2::FullEAC) {
//            cout << "Warning! Cannot perform VAC-integrality heuristic with WeightedCSPConstraint." << endl;
//            ToulBar2::FullEAC = false;
//        }
        for (int i = 0; i < arity_in; i++) {
            assert(!problem || scope_in[i]->getDomainInitSize() == ((EnumeratedVariable *)problem->getVar(i))->getDomainInitSize());
            assert(!negproblem || scope_in[i]->getDomainInitSize() == ((EnumeratedVariable *)negproblem->getVar(i))->getDomainInitSize());
            varIndexes.push_back(i);
            newValues.push_back(scope_in[i]->getInf());
            conflictWeights.push_back(0);
        }
        ToulBar2::setvalue = ::tb2setvalue;
        ToulBar2::removevalue = ::tb2removevalue;
        ToulBar2::setmin = ::tb2setmin;
        ToulBar2::setmax = ::tb2setmax;

        if (MasterWeightedCSP != NULL && MasterWeightedCSP != wcsp) {
            WeightedCSPConstraints.clear();
        }
        MasterWeightedCSP = wcsp; //FIXME: the slave problem should not contain a WeightedCSPConstraint inside!
        if (problem) {
            negCost += problem->getNegativeLb();
            WeightedCSPConstraints[problem->getIndex()] = this;
            problem->setSolver(wcsp->getSolver()); // force slave problems to use the same solver as the master
            if (!duplicateHard && !problem->isfinite()) isfinite = false;
            problem->updateUb(ub);
            problem->enforceUb();
        }
        if (negproblem) {
            negCost += negproblem->getNegativeLb();
            WeightedCSPConstraints[negproblem->getIndex()] = this;
            negproblem->setSolver(wcsp->getSolver());
            if (!duplicateHard && !negproblem->isfinite()) isfinite = false;
            negproblem->updateUb(-lb + negCost + UNIT_COST);
            negproblem->enforceUb();
//            negproblem->preprocessing();
        }
//        if (problem) {
//            problem->preprocessing();
//        }
    }

    virtual ~WeightedCSPConstraint() {}

    bool extension() const FINAL { return false; } // this is not a cost function represented by an exhaustive table of costs

    void reconnect() override
    {
        if (deconnected()) {
            nonassigned = arity_;
            AbstractNaryConstraint::reconnect();
        }
    }
    int getNonAssigned() const { return nonassigned; }

    Long getConflictWeight() const override { return Constraint::getConflictWeight(); }
    Long getConflictWeight(int varIndex) const override
    {
        assert(varIndex >= 0);
        assert(varIndex < arity_);
        return conflictWeights[varIndex] + Constraint::getConflictWeight();
    }
    void incConflictWeight(Constraint* from) override
    {
        assert(from != NULL);
        if (from == this) {
            if (deconnected() || nonassigned == arity_) {
                Constraint::incConflictWeight(1);
            } else {
                for (int i = 0; i < arity_; i++) {
                    if (connected(i)) {
                        conflictWeights[i]++;
                    }
                }
            }
        } else if (deconnected()) {
            for (int i = 0; i < from->arity(); i++) {
                int index = getIndex(from->getVar(i));
                if (index >= 0) { // the last conflict constraint may be derived from two binary constraints (boosting search), each one derived from an n-ary constraint with a scope which does not include parameter constraint from
                    assert(index < arity_);
                    conflictWeights[index]++;
                }
            }
        }
    }
    void resetConflictWeight() override
    {
        conflictWeights.assign(conflictWeights.size(), 0);
        Constraint::resetConflictWeight();
    }

    bool universal() override
    {
        if (isfinite && problem && negproblem && problem->getLb() >= lb && negproblem->getLb() > -ub + negCost) {
            return true;
        } else {
            return false;
        }
    }

    /// \brief returns true if all remaining (unassigned) variables are only connected to this global constraint
    bool canbeDeconnected() const
    {
        for (int i = 0; i < arity_; i++) {
            if (getVar(i)->unassigned() && getVar(i)->getDegree() > 1) {
                return false;
            }
        }
        return true;
    }

    static bool _protected_;
    static int preprocessFunctional;
    static int elimDegree;
    static int elimDegree_preprocessing;
    static int elimDegree_;
    static int elimDegree_preprocessing_;
    static int DEE;
    static int DEE_;
    static bool FullEAC;
    static bool RASPS;
    static int useRASPS;
    static void protect(bool master = true) ///< \brief deactivate some preprocessing/propagation features not compatible with our channeling mechanism
    {
        assert(!_protected_);
        if (master) {
            preprocessFunctional = ToulBar2::preprocessFunctional;
            elimDegree = ToulBar2::elimDegree;
            elimDegree_preprocessing = ToulBar2::elimDegree_preprocessing;
            elimDegree_ = ToulBar2::elimDegree_ ;
            elimDegree_preprocessing_ = ToulBar2::elimDegree_preprocessing_;
            DEE = ToulBar2::DEE;
            DEE_ = ToulBar2::DEE_;
            FullEAC = ToulBar2::FullEAC;
            RASPS = ToulBar2::RASPS;
            useRASPS = ToulBar2::useRASPS;
        }

        _protected_ = true;
        ToulBar2::preprocessFunctional = 0;
        ToulBar2::elimDegree = -1;
        ToulBar2::elimDegree_preprocessing = -1;
        ToulBar2::elimDegree_ = -1;
        ToulBar2::elimDegree_preprocessing_ = -1;
        ToulBar2::DEE = 0;
        ToulBar2::DEE_ = 0;
        ToulBar2::FullEAC = false;
        ToulBar2::RASPS = false;
        ToulBar2::useRASPS = 0;
    }
    static void unprotect() ///< \brief reactivate preprocessing/propagation features
    {
        if (_protected_) {
            _protected_ = false;
            ToulBar2::preprocessFunctional = preprocessFunctional;
            ToulBar2::elimDegree = elimDegree;
            ToulBar2::elimDegree_preprocessing = elimDegree_preprocessing;
            ToulBar2::elimDegree_ = elimDegree_;
            ToulBar2::elimDegree_preprocessing_ = elimDegree_preprocessing_;
            ToulBar2::DEE = DEE;
            ToulBar2::DEE_ = DEE_;
            ToulBar2::FullEAC = FullEAC;
            ToulBar2::RASPS = RASPS;
            ToulBar2::useRASPS = useRASPS;
        }
    }

    Cost eval(const Tuple& s) override
    {
        assert((int)s.size() == arity_);
        for (int i=0; i < arity_; i++) {
            newValues[i] = ((EnumeratedVariable *)getVar(i))->toValue(s[i]);
        }
        ToulBar2::setvalue = NULL;
        ToulBar2::removevalue = NULL;
        ToulBar2::setmin = NULL;
        ToulBar2::setmax = NULL;
        protect();
        int depth = Store::getDepth();
        bool unsat = false;
        bool problemActive = true;
        bool negproblemActive = true;
        try {
            Store::store();
            if (problem) {
                problem->enforceUb();
                problemActive = problem->isactivatePropagate();
                problem->reactivatePropagate();
                problem->assignLS(varIndexes, newValues); // throw a Contradiction if it violates ub
                if (problem->getLb() < lb) { // checks if the solution violates lb
                    unsat = true;
                }
            } else if (negproblem) {
                negproblem->enforceUb();
                negproblemActive = negproblem->isactivatePropagate();
                negproblem->reactivatePropagate();
                negproblem->assignLS(varIndexes, newValues); // throw a Contradiction if it violates lb
                if (negproblem->getLb() <= -ub + negCost) { // checks if the solution violates ub
                    unsat = true;
                }
            }
        } catch (const Contradiction&) {
            if (problem) problem->whenContradiction();
            if (negproblem) negproblem->whenContradiction();
            unsat = true;
        }
        Store::restore(depth);
        if (!problemActive) problem->deactivatePropagate();
        if (!negproblemActive) negproblem->deactivatePropagate();
        unprotect();
        ToulBar2::setvalue = ::tb2setvalue;
        ToulBar2::removevalue = ::tb2removevalue;
        ToulBar2::setmin = ::tb2setmin;
        ToulBar2::setmax = ::tb2setmax;
        if (unsat) {
            return MAX_COST;
        } else {
            return MIN_COST;
        }
    }

    Cost evalsubstr(const Tuple& s, Constraint* ctr) FINAL { return evalsubstrAny(s, ctr); }
    Cost evalsubstr(const Tuple& s, NaryConstraint* ctr) FINAL { return evalsubstrAny(s, ctr); }
    template <class T>
    Cost evalsubstrAny(const Tuple& s, T* ctr)
    {
        int count = 0;

        for (int i = 0; i < arity_; i++) {
            int ind = ctr->getIndex(getVar(i));
            if (ind >= 0) {
                evalTuple[i] = s[ind];
                count++;
            }
        }
        assert(count <= arity_);

        Cost cost;
        if (count == arity_)
            cost = eval(evalTuple);
        else
            cost = MIN_COST;

        return cost;
    }
    Cost getCost() FINAL
    {
        for (int i = 0; i < arity_; i++) {
            EnumeratedVariable* var = (EnumeratedVariable*)getVar(i);
            evalTuple[i] = var->toIndex(var->getValue());
        }
        return eval(evalTuple);
    }

    double computeTightness() override
    {
        double res = 0.; //FIXME: take into account elimBinConstr and elimTernConstr
        if (problem) {
            for (unsigned int c=0; c < problem->numberOfConstraints(); c++) {
                if (problem->getCtr(c)->connected()) {
                    res += problem->getCtr(c)->getTightness();
                }
            }
            return res / problem->numberOfConnectedConstraints();
        } else if (negproblem) {
            for (unsigned int c=0; c < negproblem->numberOfConstraints(); c++) {
                if (negproblem->getCtr(c)->connected()) {
                    res += negproblem->getCtr(c)->getTightness();
                }
            }
            return res / negproblem->numberOfConnectedConstraints();
        } else {
            return 1.;
        }
    }

    Cost getMaxFiniteCost() override
    {
        return MIN_COST;
    }

    void assign(int varIndex) override
    {
        if (connected(varIndex)) {
            deconnect(varIndex);
            nonassigned = nonassigned - 1;
            assert(nonassigned >= 0);

            if (universal()) {
                deconnect();
                return;
            }

            if (nonassigned <= NARYPROJECTIONSIZE && (!strongDuality || nonassigned == 0)) {
                deconnect();
                projectNary();
            } else {
                propagate();
            }
        }
    }
//    void increase(int varIndex) override {}
//    void decrease(int varIndex) override {}
//    void remove(int varIndex) override {}

    // propagates from scratch
    void propagate() override
    {
        //FIXME: synchronize current domains between master and slave problems at initialization?
        wcsp->revise(this);
        if (problem) problem->enforceUb();
        if (negproblem) negproblem->enforceUb();
        assigns();
        if (connected()) {
            try {
                if (problem && problem->isactivatePropagate()) {
                    protect();
                    problem->propagate();
                    unprotect();
                    if (strongDuality && connected() && canbeDeconnected()) {
                        if (problem->getLb() < lb) {
                            assert(!wcsp->vac || wcsp->vac->getThreshold() == UNIT_COST); //FIXME: we should check also variable.myThreshold and constraint.myThreshold are all zero
                            THROWCONTRADICTION;
                        } else {
                            deconnect();
                        }
                    }
                }
                if (connected()) {
                    if (negproblem && negproblem->isactivatePropagate()) {
                        protect();
                        negproblem->propagate();
                        unprotect();
                    }
                }
            } catch (const Contradiction&) {
                if (problem) problem->whenContradiction();
                if (negproblem) negproblem->whenContradiction();
                unprotect();
                THROWCONTRADICTION;
            }
        }
        assert(!problem || problem->getLb() < ub);
        assert(!negproblem || negproblem->getLb() < -lb + negCost + UNIT_COST);
    }

    void print(ostream& os) override
    {
        os << this << " WeightedWCSPConstraint(";
        int unassigned_ = 0;
        for (int i = 0; i < arity_; i++) {
            if (scope[i]->unassigned())
                unassigned_++;
            os << wcsp->getName(scope[i]->wcspIndex);
            if (i < arity_ - 1)
                os << ",";
        }
        os << ") in [" << lb << "," << ub << "[ ";
        if (ToulBar2::weightedDegree) {
            os << "/" << getConflictWeight();
            for (int i = 0; i < arity_; i++) {
                os << "," << conflictWeights[i];
            }
        }
        os << " isfinite: " << isfinite;
        os << " strongDuality: " << strongDuality;
        os << " arity: " << arity_;
        os << " unassigned: " << (int)nonassigned << "/" << unassigned_ << endl;
        if (problem) os << *problem << endl;
        if (negproblem) os << *negproblem << endl;
    }

    //TODO:
//    void dump(ostream& os, bool original = true) override
//    {
//    }

//    void dump_CFN(ostream& os, bool original = true) override
//    {
//    }

    friend void tb2setvalue(int wcspId, int varIndex, Value value, void* solver);
    friend void tb2removevalue(int wcspId, int varIndex, Value value, void* solver);
    friend void tb2setmin(int wcspId, int varIndex, Value value, void* solver);
    friend void tb2setmax(int wcspId, int varIndex, Value value, void* solver);
};

WCSP* WeightedCSPConstraint::MasterWeightedCSP = NULL;
map<int, WeightedCSPConstraint *> WeightedCSPConstraint::WeightedCSPConstraints;
bool WeightedCSPConstraint::_protected_ = false;
int WeightedCSPConstraint::preprocessFunctional = 0;
int WeightedCSPConstraint::elimDegree = -1;
int WeightedCSPConstraint::elimDegree_preprocessing = -1;
int WeightedCSPConstraint::elimDegree_ = -1;
int WeightedCSPConstraint::elimDegree_preprocessing_ = -1;
int WeightedCSPConstraint::DEE = 0;
int WeightedCSPConstraint::DEE_ = 0;
bool WeightedCSPConstraint::FullEAC = false;
bool WeightedCSPConstraint::RASPS = false;
int WeightedCSPConstraint::useRASPS = 0;

void tb2setvalue(int wcspId, int varIndex, Value value, void* solver)
{
    assert(WeightedCSPConstraint::MasterWeightedCSP);
    assert(wcspId == WeightedCSPConstraint::MasterWeightedCSP->getIndex() || WeightedCSPConstraint::WeightedCSPConstraints.find(wcspId) != WeightedCSPConstraint::WeightedCSPConstraints.end());
    WCSP *wcsp = NULL;
    Variable *masterVar = NULL;
    if (wcspId != WeightedCSPConstraint::MasterWeightedCSP->getIndex()) { // we came from a slave, wake up the master
        WeightedCSPConstraint *gc = WeightedCSPConstraint::WeightedCSPConstraints[wcspId];
        if (gc->problem && gc->problem->getIndex() == wcspId) {
            wcsp = gc->problem;
        } else {
            assert(gc->negproblem && gc->negproblem->getIndex() == wcspId);
            wcsp = gc->negproblem;
        }
        wcsp->deactivatePropagate();
        masterVar = WeightedCSPConstraint::WeightedCSPConstraints[wcspId]->getVar(varIndex);
        WeightedCSPConstraint::unprotect();
        masterVar->assign(value);
        WeightedCSPConstraint::protect(false);
    } else {
        // we came from the master
        wcsp = WeightedCSPConstraint::MasterWeightedCSP;
        wcsp->deactivatePropagate();
        masterVar = WeightedCSPConstraint::MasterWeightedCSP->getVar(varIndex);
        setvalue(WeightedCSPConstraint::MasterWeightedCSP->getIndex(), masterVar->wcspIndex, value, WeightedCSPConstraint::MasterWeightedCSP->getSolver());
        WeightedCSPConstraint::protect(true);
    }
    if (ToulBar2::verbose >= 2)
        cout << "EVENT: x" << varIndex << "_" << wcspId << " = " << value << endl;
    for (auto gc: WeightedCSPConstraint::WeightedCSPConstraints) if (gc.second->connected()) {
        int varCtrIndex = gc.second->getIndex(masterVar);
        if (varCtrIndex != -1) { // only for slave problems which are concerned by this variable
            if (gc.second->problem && wcspId != gc.second->problem->getIndex()) { // do not reenter inside the same problem as the one we came
                assert(WeightedCSPConstraint::_protected_);
                try {
                    gc.second->problem->enforceUb();
                    gc.second->problem->getVar(varCtrIndex)->assign(value);
                } catch (const Contradiction&) {
                    gc.second->problem->whenContradiction();
                    WeightedCSPConstraint::unprotect();
                    throw Contradiction();
                }
            }
            if (gc.second->connected() && gc.second->negproblem && wcspId != gc.second->negproblem->getIndex()) { // do not reenter inside the same problem as the one we came
                assert(WeightedCSPConstraint::_protected_);
                try {
                    gc.second->negproblem->enforceUb();
                    gc.second->negproblem->getVar(varCtrIndex)->assign(value);
                } catch (const Contradiction&) {
                    gc.second->negproblem->whenContradiction();
                    WeightedCSPConstraint::unprotect();
                    throw Contradiction();
                }
            }
        }
    }
    wcsp->reactivatePropagate();
    if (wcspId == WeightedCSPConstraint::MasterWeightedCSP->getIndex()) {
        WeightedCSPConstraint::unprotect();
    }
}

void tb2removevalue(int wcspId, int varIndex, Value value, void* solver)
{
    assert(WeightedCSPConstraint::MasterWeightedCSP);
    assert(wcspId == WeightedCSPConstraint::MasterWeightedCSP->getIndex() || WeightedCSPConstraint::WeightedCSPConstraints.find(wcspId) != WeightedCSPConstraint::WeightedCSPConstraints.end());
    WCSP *wcsp = NULL;
    Variable *masterVar = NULL;
    if (wcspId != WeightedCSPConstraint::MasterWeightedCSP->getIndex()) { // we came from a slave, wake up the master
        WeightedCSPConstraint *gc = WeightedCSPConstraint::WeightedCSPConstraints[wcspId];
        if (gc->problem && gc->problem->getIndex() == wcspId) {
            wcsp = gc->problem;
        } else {
            assert(gc->negproblem && gc->negproblem->getIndex() == wcspId);
            wcsp = gc->negproblem;
        }
        wcsp->deactivatePropagate();
        WeightedCSPConstraint::unprotect();
        masterVar = WeightedCSPConstraint::WeightedCSPConstraints[wcspId]->getVar(varIndex);
        masterVar->remove(value);
        WeightedCSPConstraint::protect(false);
    } else {
        // we came from the master
        wcsp = WeightedCSPConstraint::MasterWeightedCSP;
        wcsp->deactivatePropagate();
        masterVar = WeightedCSPConstraint::MasterWeightedCSP->getVar(varIndex);
        WeightedCSPConstraint::protect(true);
    }
    if (ToulBar2::verbose >= 2)
        cout << "EVENT: x" << varIndex << "_" << wcspId << " != " << value << endl;
    for (auto gc: WeightedCSPConstraint::WeightedCSPConstraints) if (gc.second->connected()) {
        int varCtrIndex = gc.second->getIndex(masterVar);
        if (varCtrIndex != -1) {
            if (gc.second->problem && wcspId != gc.second->problem->getIndex()) { // do not reenter inside the same problem as the one we came
                assert(WeightedCSPConstraint::_protected_);
                try {
                    gc.second->problem->enforceUb();
                    gc.second->problem->getVar(varCtrIndex)->remove(value);
                } catch (const Contradiction&) {
                    gc.second->problem->whenContradiction();
                    WeightedCSPConstraint::unprotect();
                    throw Contradiction();
                }
            }
            if (gc.second->connected() && gc.second->negproblem && wcspId != gc.second->negproblem->getIndex()) { // do not reenter inside the same problem as the one we came
                assert(WeightedCSPConstraint::_protected_);
                try {
                    gc.second->negproblem->enforceUb();
                    gc.second->negproblem->getVar(varCtrIndex)->remove(value);
                } catch (const Contradiction&) {
                    gc.second->negproblem->whenContradiction();
                    WeightedCSPConstraint::unprotect();
                    throw Contradiction();
                }
            }
        }
    }
    wcsp->reactivatePropagate();
    if (wcspId == WeightedCSPConstraint::MasterWeightedCSP->getIndex()) {
        WeightedCSPConstraint::unprotect();
    }
}

void tb2setmin(int wcspId, int varIndex, Value value, void* solver)
{
    assert(WeightedCSPConstraint::MasterWeightedCSP);
    assert(wcspId == WeightedCSPConstraint::MasterWeightedCSP->getIndex() || WeightedCSPConstraint::WeightedCSPConstraints.find(wcspId) != WeightedCSPConstraint::WeightedCSPConstraints.end());
    WCSP *wcsp = NULL;
    Variable *masterVar = NULL;
    if (wcspId != WeightedCSPConstraint::MasterWeightedCSP->getIndex()) { // we came from a slave, wake up the master
        WeightedCSPConstraint *gc = WeightedCSPConstraint::WeightedCSPConstraints[wcspId];
        if (gc->problem && gc->problem->getIndex() == wcspId) {
            wcsp = gc->problem;
        } else {
            assert(gc->negproblem && gc->negproblem->getIndex() == wcspId);
            wcsp = gc->negproblem;
        }
        wcsp->deactivatePropagate();
        WeightedCSPConstraint::unprotect();
        masterVar = WeightedCSPConstraint::WeightedCSPConstraints[wcspId]->getVar(varIndex);
        masterVar->increase(value);
        WeightedCSPConstraint::protect(false);
    } else {
        // we came from the master
        wcsp = WeightedCSPConstraint::MasterWeightedCSP;
        wcsp->deactivatePropagate();
        masterVar = WeightedCSPConstraint::MasterWeightedCSP->getVar(varIndex);
        WeightedCSPConstraint::protect(true);
    }
    if (ToulBar2::verbose >= 2)
        cout << "EVENT: x" << varIndex << "_" << wcspId << " >= " << value << endl;
    for (auto gc: WeightedCSPConstraint::WeightedCSPConstraints) if (gc.second->connected()) {
        int varCtrIndex = gc.second->getIndex(masterVar);
        if (varCtrIndex != -1) {
            if (gc.second->problem && wcspId != gc.second->problem->getIndex()) { // do not reenter inside the same problem as the one we came
                assert(WeightedCSPConstraint::_protected_);
                try {
                    gc.second->problem->enforceUb();
                    gc.second->problem->getVar(varCtrIndex)->increase(value);
                } catch (const Contradiction&) {
                    gc.second->problem->whenContradiction();
                    WeightedCSPConstraint::unprotect();
                    throw Contradiction();
                }
            }
            if (gc.second->connected() && gc.second->negproblem && wcspId != gc.second->negproblem->getIndex()) { // do not reenter inside the same problem as the one we came
                assert(WeightedCSPConstraint::_protected_);
                try {
                    gc.second->negproblem->enforceUb();
                    gc.second->negproblem->getVar(varCtrIndex)->increase(value);
                } catch (const Contradiction&) {
                    gc.second->negproblem->whenContradiction();
                    WeightedCSPConstraint::unprotect();
                    throw Contradiction();
                }
            }
        }
    }
    wcsp->reactivatePropagate();
    if (wcspId == WeightedCSPConstraint::MasterWeightedCSP->getIndex()) {
        WeightedCSPConstraint::unprotect();
    }
}

void tb2setmax(int wcspId, int varIndex, Value value, void* solver)
{
    assert(WeightedCSPConstraint::MasterWeightedCSP);
    WCSP *wcsp = NULL;
    assert(wcspId == WeightedCSPConstraint::MasterWeightedCSP->getIndex() || WeightedCSPConstraint::WeightedCSPConstraints.find(wcspId) != WeightedCSPConstraint::WeightedCSPConstraints.end());
    Variable *masterVar = NULL;
    if (wcspId != WeightedCSPConstraint::MasterWeightedCSP->getIndex()) { // we came from a slave, wake up the master
        WeightedCSPConstraint *gc = WeightedCSPConstraint::WeightedCSPConstraints[wcspId];
        if (gc->problem && gc->problem->getIndex() == wcspId) {
            wcsp = gc->problem;
        } else {
            assert(gc->negproblem && gc->negproblem->getIndex() == wcspId);
            wcsp = gc->negproblem;
        }
        wcsp->deactivatePropagate();
        WeightedCSPConstraint::unprotect();
        masterVar = WeightedCSPConstraint::WeightedCSPConstraints[wcspId]->getVar(varIndex);
        masterVar->decrease(value);
        WeightedCSPConstraint::protect(false);
    } else {
        // we came from the master
        wcsp = WeightedCSPConstraint::MasterWeightedCSP;
        wcsp->deactivatePropagate();
        masterVar = WeightedCSPConstraint::MasterWeightedCSP->getVar(varIndex);
        WeightedCSPConstraint::protect(true);
    }
    if (ToulBar2::verbose >= 2)
        cout << "EVENT: x" << varIndex << "_" << wcspId << " <= " << value << endl;
    for (auto gc: WeightedCSPConstraint::WeightedCSPConstraints) if (gc.second->connected()) {
        int varCtrIndex = gc.second->getIndex(masterVar);
        if (varCtrIndex != -1) {
            if (gc.second->problem && wcspId != gc.second->problem->getIndex()) { // do not reenter inside the same problem as the one we came
                assert(WeightedCSPConstraint::_protected_);
                try {
                    gc.second->problem->enforceUb();
                    gc.second->problem->getVar(varCtrIndex)->decrease(value);
                } catch (const Contradiction&) {
                    gc.second->problem->whenContradiction();
                    WeightedCSPConstraint::unprotect();
                    throw Contradiction();
                }
            }
            if (gc.second->connected() && gc.second->negproblem && wcspId != gc.second->negproblem->getIndex()) { // do not reenter inside the same problem as the one we came
                assert(WeightedCSPConstraint::_protected_);
                try {
                    gc.second->negproblem->enforceUb();
                    gc.second->negproblem->getVar(varCtrIndex)->decrease(value);
                } catch (const Contradiction&) {
                    gc.second->negproblem->whenContradiction();
                    WeightedCSPConstraint::unprotect();
                    throw Contradiction();
                }
            }
        }
    }
    wcsp->reactivatePropagate();
    if (wcspId == WeightedCSPConstraint::MasterWeightedCSP->getIndex()) {
        WeightedCSPConstraint::unprotect();
    }
}
#endif /*TB2GLOBALWCSP_HPP_*/

/* Local Variables: */
/* c-basic-offset: 4 */
/* tab-width: 4 */
/* indent-tabs-mode: nil */
/* c-default-style: "k&r" */
/* End: */
