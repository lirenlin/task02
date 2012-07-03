#include "llvm/Analysis/MemoryDependenceAnalysis.h"
#include "llvm/LLVMContext.h"
#include "llvm/Analysis/Passes.h"
#include "llvm/Assembly/Writer.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ValueSymbolTable.h"
#include "llvm/Function.h"
#include "llvm/Module.h"
#include "llvm/Support/InstIterator.h"


#include <stack>

using namespace llvm;

namespace {
    struct DrawMemDep : public ModulePass {

        typedef std::stack<const User *> DepSet;
        typedef DenseMap<const Value *, DepSet> DepSetMap;
        DepSetMap Deps;

        static char ID; // Pass identifcation, replacement for typeid
        DrawMemDep() : ModulePass(ID) {
        }

        virtual bool runOnModule(Module &F);

        void print(raw_ostream &OS, const Module * = 0) const;
        //void addEdge(int, int, std::pair<int, bool> marker[]) const;


        virtual void releaseMemory() {
            Deps.clear();
        }

    };
}

char DrawMemDep::ID = 0;


bool DrawMemDep::runOnModule(Module &M) {
    const Module::GlobalListType &gList = M.getGlobalList();
    for (Module::GlobalListType::const_iterator I = gList.begin(), E = gList.end(); I != E; ++I)
    {
        //errs() << I->getName() << '\n';
        for (GlobalValue::const_use_iterator II = I->use_begin(), E = I->use_end(); II != E; ++II) {
            const User *user = II.getUse().getUser();
            Deps[I].push(user);
            //user->print(errs());
            //errs() << '\n';
        }
    }

    const Module::FunctionListType &fList = M.getFunctionList();
    for (Module::FunctionListType::const_iterator I = fList.begin(), E = fList.end(); I != E; ++I)
    {
        const Function *F = &*I;

        for(Function::const_iterator J = F->begin(), E = F->end(); J != E; ++J)
            for(BasicBlock::const_iterator I = J->begin(), E = J->end(); I != E; ++I)
            {
                //if (I->mayReadFromMemory() || I->mayWriteToMemory())
                if (const LoadInst *LI = dyn_cast<LoadInst>(&*I))
                {
                    for (Value::const_use_iterator I = LI->use_begin(), E = LI->use_end(); I != E; ++I) {
                        const User *user = I.getUse().getUser();
                        Deps[LI].push(user);
                    }
                }
            }


        const ValueSymbolTable &vTable = F->getValueSymbolTable();
        for (ValueSymbolTable::const_iterator I = vTable.begin(), E = vTable.end(); I != E; ++I) {
            const Value &tmp = *I->getValue();

            // skip basic block entry value
            if(tmp.getValueID() == Value::BasicBlockVal) continue;

            //errs() << "Value name: " << tmp.getName() \
            //    << ", ID: " << tmp.getValueID() << ", Inst:";
            //tmp.print(errs()); errs() << '\n';
            switch (tmp.getValueID() - Value::InstructionVal)
            {
                case Instruction::Alloca:
                    //errs() << tmp.getName() << "\n";
                    break;
                case Instruction::Add:
                    //errs() << tmp.getName() << "\n";
                    break;
                case Instruction::Call:
                    //errs() << tmp.getName() << "\n";
                    break;
                default:
                    break;
            }
            for (Value::const_use_iterator I = tmp.use_begin(), E = tmp.use_end(); I != E; ++I) {
                const User *user = I.getUse().getUser();
                //user->print(errs());
                //errs() << '\n';
                Deps[&tmp].push(user);
            }

        }

    }

    print(errs(), &M);
    return false;
}

void DrawMemDep::print(raw_ostream &OS, const Module *M) const {
    OS << "digraph module{\n";
    OS << "node [ \n shape = \"record\"\n];\n ";

    // print the global variable first, because it will be ignored in the succedent loop
    OS << "subgraph " << "cluster0" << "{\n"\
        << "\tlabel=\"" << "global" << "\";\n"\
        << "\tcolor=lightgrey;\n"\
        << "\tstyle=filled;\n\n";
    const Module::GlobalListType &gList = M->getGlobalList();
    for (Module::GlobalListType::const_iterator I = gList.begin(), E = gList.end(); I != E; ++I)
    {
            OS << "\tNode"<< static_cast<const void *>(I) << " [label=\"";
            I->print(OS);
            OS << "\"];\n";
    }
    OS << "}\n";

    const Module::FunctionListType &fList = M->getFunctionList();
    int i = 1;
    for (Module::FunctionListType::const_iterator I = fList.begin(), E = fList.end(); I != E; ++I, ++i)
    {
        const Function *F = &*I;
        OS << "subgraph " << "cluster" << i << "{\n"\
            << "\tlabel=\"" << F->getName() << "\";\n"\
            << "\tcolor=lightgrey;\n"\
            << "\tstyle=filled;\n\n";

    for (DepSetMap::const_iterator DI = Deps.begin(), E = Deps.end(); DI != E; ++DI) {
        const Value *Inst = DI->first;
        const Value *root = Inst;

        //check if the node belongs to current function
        //errs() << Inst->getName() << '\n' ;
        if(Inst->getValueID() == Value::ArgumentVal)
        {
            OS << "\tNode"<< static_cast<const void *>(Inst) << " [label=\"";
            root->print(OS);
            OS << "\"];\n";
        }

        if(const Instruction * tmp = dyn_cast<Instruction>(Inst))
        if(tmp->getParent()->getParent() == F)
        {
            OS << "\tNode"<< static_cast<const void *>(Inst) << " [label=\"";
            root->print(OS);
            OS << "\"];\n";
        }

        DepSet InstDeps = DI->second;

        while (!InstDeps.empty())
        {
            const User *DepInst = InstDeps.top();
            InstDeps.pop();

            DepSetMap::const_iterator tmp = Deps.find(DepInst);
            if(tmp == Deps.end())
            {
                //check if the node belongs to current function
                if(dyn_cast<Instruction>(DepInst)->getParent()->getParent() == F)
                {
                    OS << "\tNode"<< static_cast<const void *>(DepInst) << " [label=\"";
                    DepInst->print(OS);
                    OS << "\"];\n";
                }

            }

            // reset the root node
            if(const Instruction *I = dyn_cast<Instruction>(DepInst))
            {
                // draw the link only once
                if(I->getParent()->getParent() != F) continue;
                
                if(I->getOpcode() == Instruction::Store)
                {
                    // link
                    OS << "\tNode"<< static_cast<const void *>(DepInst) << " -> Node" \
                        << static_cast<const void *>(Inst) << "; \n";
                    root = I;
                }
                else
                {
                    // link
                    OS << "\tNode"<< static_cast<const void *>(DepInst) << " -> Node" \
                        << static_cast<const void *>(root) << "; \n";
                }
            }

        }
    }
        OS << "}\n";
    }
    OS << "}\n";

}

static RegisterPass<DrawMemDep> X("samplePass", "print the memory dependency", true, true);



