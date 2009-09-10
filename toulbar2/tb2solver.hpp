/** \file tb2solver.hpp
 *  \brief Generic solver.
 * 
 */
 
#ifndef TB2SOLVER_HPP_
#define TB2SOLVER_HPP_

#include "toulbar2.hpp"
template <class T> struct DLink;
template <class T> class BTList;

class Solver
{
    static Solver *currentSolver;

    Store *store;
    Long nbNodes;
    Long nbBacktracks;
    WeightedCSP *wcsp;
    DLink<Value> *allVars;
    BTList<Value> *unassignedVars;
    int lastConflictVar;
        
    // Heuristics and search methods
	int getVarMinDomainDivMaxWeightedDegreeLastConflict();
	int getVarMinDomainDivMaxWeightedDegree();
    int getVarMinDomainDivMaxDegreeLastConflict();
    int getVarMinDomainDivMaxDegree();
    int getNextUnassignedVar();
    int getMostUrgent();
    void increase(int varIndex, Value value);
    void decrease(int varIndex, Value value);
    void assign(int varIndex, Value value);
    void remove(int varIndex, Value value);
   
    void singletonConsistency();
   
    void binaryChoicePoint(int xIndex, Value value);
    void binaryChoicePointLDS(int xIndex, Value value, int discrepancy);
    void narySortedChoicePoint(int xIndex);
    void narySortedChoicePointLDS(int xIndex, int discrepancy);
    void newSolution();
    void recursiveSolve();
    void recursiveSolveLDS(int discrepancy);
    Value postponeRule(int varIndex);
    void scheduleOrPostpone(int varIndex);

  int getNextUnassignedVar(Cluster *cluster);
  int getVarMinDomainDivMaxDegree(Cluster *cluster);
  int getVarMinDomainDivMaxDegreeLastConflict(Cluster *cluster);
  int getVarFreedom(Cluster *cluster);
  int getVarSup(Cluster *cluster);
  

  Cost binaryChoicePoint(Cluster *cluster, Cost lbgood, Cost cub, int varIndex, Value value);
  Cost recursiveSolve(Cluster *cluster, Cost lbgood, Cost cub);
  void russianDollSearch(Cluster *c, Cost cub);
    
  int nsolutions;

public:
    Solver(int storeSize, Cost initUpperBound);
    ~Solver();
    
    void read_wcsp(const char *fileName);
    void read_random(int n, int m, vector<int>& p, int seed, bool forceSubModular = false );
    
    bool solve();
    
    bool solve_symmax2sat(int n, int m, int *posx, int *posy, double *cost, int *sol);

    void dump_wcsp(const char *fileName);
    void read_solution(const char *fileName);
    
    friend void setvalue(int wcspId, int varIndex, Value value);

    WeightedCSP* getWCSP() { return wcsp; }
};

int solveSymMax2SAT(int n, int m, int *posx, int *posy, double *cost, int *sol);

#endif /*TB2SOLVER_HPP_*/
