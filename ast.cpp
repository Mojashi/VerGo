#include "ast.h"

#include <iostream>


using std::cerr;
using std::endl;

extern Variable godVar;
extern CNF cnf;
shared_ptr<Variable> currentScope;

map<string, shared_ptr<Variable>> localVar;

struct InstanceFunction{
    shared_ptr<Variable> scope;
    shared_ptr<ASTNode> function;
    shared_ptr<Variable> call(){
        shared_ptr<Variable> cvScope(currentScope);
        currentScope = scope;
        shared_ptr<Variable> ret = function->eval();
        currentScope = cvScope;
        return ret;
    }
};

shared_ptr<Variabel> findVariable(list<string> ident){
    shared_ptr<Variable> cur(currentScope);
    while(cur){
        shared_ptr<Variable> fv = cur.getVariable(ident.front());
        if(fv){
            if(ident.size() == 1)
                return fv;
            else
                return fv.access(list<string>(++ident.begin(), ident.end()));
        }
        cur = cur->parent;
    }
    return shared_ptr<Variable>();
}

InstanceFunction findFunction(list<string> ident){
    if(ident.size() == 1){
        return currentScope.getFunction(ident.front());
    }
    else{
        shared_ptr<Variable> scope = findVariable(list<string>(ident.begin(), --ident.end()));
        if(!scope) return shared_ptr<ASTNode>();
        return {buf, buf->getFunction(ident.back())};
    }
}

shared_ptr<Variable> ASTNode::eval(){
    if(child[1])
        child[1]->eval();
    if(child[0])
        child[0]->eval();
    return shared_ptr<Variable>();
}

shared_ptr<Variable> ASTVariable::eval(){
    return findVariable(ident);
}

shared_ptr<Variable> ASTCallFunction::eval(){
    InstanceFunction func = findFunction(ident);
    if(!func.function){
        string conc;
        for(string str : ident) conc += str;
        cerr << "not found function " << conc << endl;
        exit(0);
    }

    return func.call();
}

shared_ptr<Variable> ASTIfStatement::eval(){
    //in construction
    child[1]->eval();
    return shared_ptr<Variable>();
}

shared_ptr<Variable> ASTDeclareVar::eval(){
    for(string name : names){
        if(vars.count(name)){
            cerr << "a variable named \"" << name << "\" has already declared" << endl;
            exit(0);
        }
        vars[name] = type->getInstance();
    }
    return shared_ptr<Variable>();

}

shared_ptr<Variable> ASTAssignment::eval(){
    shared_ptr<Variable> res = child[1]->eval();
    if(vars[varIdent]->structure != res->structure){
        cerr << "lvalue and rvalue types are different" << endl;
        exit(0);
    }
    vars[name] = res;
    return varIdent;
}

shared_ptr<Variable> ASTAddConst::eval(){
    vector<Literal> clause;
    for(shared_ptr<ASTNode> expr : exprs){
        clause.push_back(expr->eval()->litNum);
    }
    cnf.addClause(clause);
}