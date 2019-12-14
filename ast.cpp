#include "ast.h"

#include <iostream>
#include <utility>

using std::cerr;
using std::endl;

extern Structure godStruct;
extern Variable godVar;
extern CNF cnf;
shared_ptr<Variable> currentScope;

map<string, shared_ptr<Variable>> localVar;

string concatIdent(list<string> ident){
    string ret;
    for(auto str : ident) ret += str+",";
    return ret;
}

shared_ptr<Variable> Structure::getInstance(shared_ptr<Variable> parent){
    auto ret = (new Variable(parent, getPtr()))->getPtr();
    list<variables> params;
    for(auto arg : constructArgs){
        params.push_back(arg.eval());
    }
    shared_ptr<Function> constructor = ret->getFunction(name, params);
    if(constructor){
        InsFunction ins{ret, constructor};
        ins.call(params);
    }
    return ret;
}

void Variable::assign(shared_ptr<Variable> var){
    if(var->getType() != getType()) return;
    if(getType() == boolStruct){
        reinterpret_cast<BoolVariable*>(this)->setlitNum(reinterpret_cast<BoolVariable*>(var.get())->getlitNum());
    }
    else{
        for(auto memb : variables){
            memb.second->assign(var->getVariable(memb.first));
        }
    }
}

void Variable::sameConst(shared_ptr<Variable> var){
    if(var->getType() != getType()) return;
    if(getType() == boolStruct){
        Literal a = ((BoolVariable*)this)->getlitNum(),b = ((BoolVariable*)(var.get()))->getlitNum();
        cnf.addClause({a,-b});
        cnf.addClause({-a,b});
    }
    else{
        for(auto memb : variables){
            memb.second->sameConst(var->getVariable(memb.first));
        }
    }
}
shared_ptr<Variable> BoolStructure::getInstance(shared_ptr<Variable> parent){
    BoolVariable* var = new BoolVariable(parent, getPtr());
    var->setlitNum(cnf.getNewVar());
    return var->getPtr();
}
shared_ptr<Variable> IntegerStructure::getInstance(shared_ptr<Variable> parent){
    BoolVariable* var = new BoolVariable(parent, getPtr());
    var->setlitNum(0);
    return var->getPtr();
}
shared_ptr<Variable> ArrayStructure::getInstance(shared_ptr<Variable> parent){
    ArrayVariable* var = new ArrayVariable(parent, getPtr());
    var->setElemStructure(elemStc);
    shared_ptr<Variable> sv = size->eval();
    if(sv->getType()->getBuiltInType() != INTEGERSTRUCT){
        cerr << "size must be Integer Variable." << endl;
        exit(0);
    }
    var->resize(((BoolVariable*)sv.get())->getlitNum());
    return var->getPtr();
}

shared_ptr<Variable> InsFunction::call(list<shared_ptr<Variable>> params){
    map<string, shared_ptr<Variable>> cvLocalVar(localVar);
    shared_ptr<Variable> cvScope(currentScope);
    localVar.clear();
    currentScope = scope;
    auto args = function->getArgs();
    if(args.size() != params.size()){
            cerr << "Not enough params." << endl;
            exit(0);
    }
    auto param = params.begin();
    auto arg = args.begin();
    for(;param != params.end(); param++, arg++){
            localVar[arg->second] = *param;
    }
    shared_ptr<Variable> ret = function->getNode()->eval();
    currentScope = cvScope;
    localVar = cvLocalVar;
    return ret;
}

shared_ptr<Variable> findVariable(list<string> ident){
    if(localVar.count(ident.front())){
        return localVar[ident.front()]->access(list<string>(++ident.begin(), ident.end()));
    }
    shared_ptr<Variable> cur(currentScope);
    while(cur){
        shared_ptr<Variable> fv = cur->getVariable(ident.front());
        if(fv){
            if(ident.size() == 1)
                return fv;
            else
                return fv->access(list<string>(++ident.begin(), ident.end()));
        }
        cur = cur->getParent();
    }
    return shared_ptr<Variable>();
}

bool argumentTypeCheck(list<pair<shared_ptr<Structure>, string>> args, list<shared_ptr<Variable>> params){
    if(args.size() != params.size()) return false;
    auto itr1 = args.begin();
    auto itr2 = params.begin();
    for(;itr1 != args.end();itr1++,itr2++){
        if(itr1->first != (*itr2)->getType())
            return false;
    }
    return true;
}

InsFunction findFunction(list<string> ident,list<shared_ptr<Variable>> params){
    if(ident.size() == 1){
        shared_ptr<Variable> cur(currentScope);
        while(cur){
            shared_ptr<Function> fv = cur->getFunction(ident.front(), params);
            if(fv)
                return {cur, fv};
            cur = cur->getParent();
        }

        if(ident.size() == 1 && ident.front().substr(0,8) == "operator"){
            for(shared_ptr<Variable> param : params){
                shared_ptr<Function> func = param->getFunction(ident.front(), params);
                if(func)
                    return {param, func};
                
            }
        }
    }
    else{
        shared_ptr<Variable> scope = findVariable(list<string>(ident.begin(), --ident.end()));
        if(scope){
            shared_ptr<Function> func = scope->getFunction(ident.back(), params);
            if(func)
                return {scope, func};
        }
    }

    return InsFunction();
}

shared_ptr<Variable> ASTNode::eval(){
#ifdef DEBUG
    cerr<<nodeTypeNames[getNodeType()]<<endl;
#endif
    shared_ptr<Variable> ret;
    for(auto ch : child){
        if(ch)
            ret = ch->eval();
    }
    return ret;
}

shared_ptr<Variable> ASTCallFunction::eval(){
#ifdef DEBUG
    cerr<<nodeTypeNames[getNodeType()]<<endl;
#endif
    list<shared_ptr<Variable>> params;
    for(auto param : child){
        params.push_back(param->eval());
    }
    InsFunction func;
    if(ident){
        shared_ptr<Variable> sc = ident->eval();
        func = {sc,sc->getFunction(name, params)};
    }
    else{
        func = findFunction({name}, params);
    }
    
    if(!func.function){
        cerr << "not found function named "<<name << endl;
        exit(0);
    }
    return func.call(params);
}

shared_ptr<Variable> ASTIfStatement::eval(){
#ifdef DEBUG
    cerr<<nodeTypeNames[getNodeType()]<<endl;
#endif
    //in construction
    return shared_ptr<Variable>();
}

shared_ptr<Variable> ASTDeclareVar::eval(){
#ifdef DEBUG
    cerr<<nodeTypeNames[getNodeType()]<<endl;
#endif
    auto name = names.begin();
    auto type = types.begin();
    for(;name != names.end();name++,type++){
        if(localVar.count(*name)){
            cerr << "a variable named \"" << *name << "\" has already declared" << endl;
            exit(0);
        }
        localVar[*name] = (*type)->getInstance(currentScope);
    }
    return shared_ptr<Variable>();
}

shared_ptr<Variable> ASTAssignment::eval(){
#ifdef DEBUG
    cerr<<nodeTypeNames[getNodeType()]<<endl;
#endif
    shared_ptr<Variable> res = child.front()->eval(), leftVal = lval->eval();
    if(leftVal->getType() != res->getType()){
        cerr << "lvalue and rvalue types are different" << endl;
        exit(0);
    }
    leftVal->assign(res);
    return res;
}


shared_ptr<Variable> ASTSame::eval(){
#ifdef DEBUG
    cerr<<nodeTypeNames[getNodeType()]<<endl;
#endif
    shared_ptr<Variable> l(child.front()->eval()),r((*(child.rbegin()))->eval()); 
    if(l->getType() != r->getType()){
        cerr << "lvalue and rvalue types are different" << endl;
        exit(0);
    }
    l->sameConst(r);
    return shared_ptr<Variable>();
}

shared_ptr<Variable> ASTAddConst::eval(){
#ifdef DEBUG
    cerr<<nodeTypeNames[getNodeType()]<<endl;
#endif
    vector<Literal> clause;
    for(shared_ptr<ASTNode> expr : child){
        shared_ptr<Variable> var = expr->eval();
        if(var->getType() != boolStruct){
            cerr << "invalid expr type" << endl;
            exit(0);
        }
        clause.push_back(((BoolVariable*)(var.get()))->getlitNum());
    }
    cnf.addClause(clause);
    return shared_ptr<Variable>();
}

shared_ptr<Variable> ASTVariable::eval(){
#ifdef DEBUG
    cerr<<nodeTypeNames[getNodeType()]<<endl;
#endif
    shared_ptr<Variable> ret;
    if(child.size() == 0){
        ret = findVariable({name});
    }
    else{
        for(shared_ptr<ASTNode> node : child){
            ret = node->eval();
        }
        if(name == ""){
            if(ret->getType()->getBuiltInType() != ARRAYSTRUCT){
                cerr << "this variable doesnt have index." << endl;
                exit(0);
            }
            shared_ptr<Variable> ridx = idx->eval();
            if(ridx->getType()->getBuiltInType() != INTEGERSTRUCT){
                cerr << "index must be Integer Variable." << endl;
                exit(0);
            }

            int iidx = ((BoolVariable*)ridx.get())->getlitNum();
            
            ret = ((ArrayVariable*)ret.get())->get(iidx);
        }
        else
            ret = ret->getVariable(name);
    }
    if(ret) return ret;
    cerr << "variable doesnt exist.";
#ifdef DEBUG
    cerr << name << "." << endl;
#endif
    exit(0);
}

shared_ptr<Variable> ASTInteger::eval(){
#ifdef DEBUG
    cerr<<nodeTypeNames[getNodeType()]<<endl;
#endif
    shared_ptr<Variable> ret = integerStruct->getInstance(currentScope);
    ((BoolVariable*)ret.get())->setlitNum(n);
    return ret;
}

shared_ptr<Variable> ASTLambda::eval(){
#ifdef DEBUG
    cerr<<nodeTypeNames[getNodeType()]<<endl;
#endif
    return proc(currentScope,localVar);
}

shared_ptr<Variable> ASTFor::eval(){
#ifdef DEBUG
    cerr<<nodeTypeNames[getNodeType()]<<endl;
#endif
    vector<shared_ptr<ASTNode>> cv(child.begin(), child.end());
    cv[0]->eval();
    shared_ptr<Variable> var = cv[1]->eval();
    if(var->getType()->getBuiltInType() != INTEGERSTRUCT){
        cerr << "this expression must be SysInt" << endl;
        exit(0);
    }
    int numOfLoop = ((BoolVariable*)(var.get()))->getlitNum();

    for(int i = 0; numOfLoop > i; i++){
        map<string, shared_ptr<Variable>> cvLocalVar(localVar);
        ((BoolVariable*)(localVar[ctname].get()))->setlitNum(i);
        cv[2]->eval();
        localVar = cvLocalVar;
    }
    localVar.erase(ctname);
    return shared_ptr<Variable>();
}