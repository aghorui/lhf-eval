//
// Created by ayush on 03/01/22.
//

#ifndef VASCO_CONTEXT_H
#define VASCO_CONTEXT_H

#include <iomanip>
#include <iostream>
#include <utility>
// #include "TransformIR.h"
#include "llvm/IR/Function.h"

// # define db(msg) {std::cout << std::fixed << "DEBUG [" << std::setw(4) <<
// std::right << __LINE__ << "]: " << msg << std::endl; std::cout.flush();}

template <typename F, typename B>
class Context {
private:
    int label;
    std::pair<F, B> Inflow, Outflow;
    llvm::Function *function;
    bool changeInFunc;
    int sourceContextLabel;

public:
    Context() {
        this->label = -1;
        this->function = nullptr;
        this->Inflow = {};
        this->Outflow = {};
        this->changeInFunc = false;
        this->sourceContextLabel = -1;
    }

    Context(int label, llvm::Function *function, std::pair<F, B> Inflow, std::pair<F, B> Outflow) {
        this->Inflow = Inflow;
        this->Outflow = Outflow;
        this->label = label;
        this->function = function;
        this->changeInFunc = false;
        this->sourceContextLabel = -1;
    }

    Context(
        int label, llvm::Function *function, std::pair<F, B> Inflow, std::pair<F, B> Outflow, int sourceContextLabel) {
        this->Inflow = Inflow;
        this->Outflow = Outflow;
        this->label = label;
        this->function = function;
        this->changeInFunc = false;
        this->sourceContextLabel = sourceContextLabel;
    }

    void setInflowvalue(std::pair<F, B>);

    void setOutflowValue(std::pair<F, B>);

    void setForwardInflow(F);

    void setForwardOutflow(F);

    void setBackwardInflow(B);

    void setBackwardOutflow(B);

    int getLabel() const;

    std::pair<F, B> getInflowValue() const;

    std::pair<F, B> getOutflowValue() const;

    llvm::Function *getFunction();

    bool isDFValueChanged();

    void setDFValueChange(bool);

    void setSourceContextLabel(int);

    int getSourceContextLabel();
};

template <class F, class B>
void Context<F, B>::setInflowvalue(std::pair<F, B> Inflow) {
    this->Inflow = Inflow;
}

template <class F, class B>
void Context<F, B>::setOutflowValue(std::pair<F, B> Outflow) {
    this->Outflow = Outflow;
}

template <class F, class B>
void Context<F, B>::setForwardInflow(F value) {
    this->Inflow.first = value;
}

template <class F, class B>
void Context<F, B>::setForwardOutflow(F value) {
    this->Outflow.first = value;
}

template <class F, class B>
void Context<F, B>::setBackwardInflow(B value) {
    this->Inflow.second = value;
}

template <class F, class B>
void Context<F, B>::setBackwardOutflow(B value) {
    this->Outflow.second = value;
}

template <class F, class B>
int Context<F, B>::getLabel() const {
    return this->label;
}

template <class F, class B>
std::pair<F, B> Context<F, B>::getInflowValue() const {
    return this->Inflow;
}

template <class F, class B>
std::pair<F, B> Context<F, B>::getOutflowValue() const {
    return this->Outflow;
}

template <class F, class B>
llvm::Function *Context<F, B>::getFunction() {
    return this->function;
}

template <class F, class B>
bool Context<F, B>::isDFValueChanged() {
    return this->changeInFunc;
}

template <class F, class B>
void Context<F, B>::setDFValueChange(bool value) {
    this->changeInFunc = value;
}

template <class F, class B>
void Context<F, B>::setSourceContextLabel(int sourceContextLabel) {
    this->sourceContextLabel = sourceContextLabel;
}

template <class F, class B>
int Context<F, B>::getSourceContextLabel() {
    return this->sourceContextLabel;
}

#endif // VASCO_CONTEXT_H
