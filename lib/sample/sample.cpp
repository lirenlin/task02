#include "llvm/Analysis/MemoryDependenceAnalysis.h"
#include "llvm/LLVMContext.h"
#include "llvm/Analysis/Passes.h"
#include "llvm/Assembly/Writer.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ValueSymbolTable.h"
#include "llvm/Function.h"

using namespace llvm;

namespace {
    struct DrawMemDep : public FunctionPass {

        enum DepType {
            Clobber = 0,
            Def,
            NonFuncLocal,
            Unknown
        };

        typedef SmallSetVector<const Instruction *, 4> DepSet;
        typedef DenseMap<const Instruction *, DepSet> DepSetMap;
        DepSetMap Deps;

        static char ID; // Pass identifcation, replacement for typeid
        const Function *F;
        std::vector<const Instruction * > instVector ;
        DrawMemDep() : FunctionPass(ID) {
        }

        virtual bool runOnFunction(Function &F);

        //void print(raw_ostream &OS, const Module * = 0) const;
        //void addEdge(int, int, std::pair<int, bool> marker[]) const;


        virtual void releaseMemory() {
            Deps.clear();
            F = 0;
        }

    };
}

char DrawMemDep::ID = 0;


bool DrawMemDep::runOnFunction(Function &F) {
    this->F = &F;
    instVector.clear();

    errs() << "In function: " << F.getName() << '\n';

    const ValueSymbolTable &vTable = F.getValueSymbolTable();
    for (ValueSymbolTable::const_iterator I = vTable.begin(), E = vTable.end(); I != E; ++I)
    {
        const Value &tmp = *I->getValue();

        // skip basic block entry value
        if(tmp.getValueID() == Value::BasicBlockVal) continue;

        errs() << "Value name: " << tmp.getName() \
            << ", ID: " << tmp.getValueID() << ", Inst:";

        tmp.print(errs()); errs() << "\n";
        switch (tmp.getValueID() - Value::InstructionVal)
        {
            // I don't know why no this type of value
            case Instruction::Load:
                errs() << "Load" << "\n";
                break;
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
        for (Value::const_use_iterator I = tmp.use_begin(), E = tmp.use_end(); I != E; ++I)
        {
            const User *user = I.getUse().getUser();
            user->print(errs());
            errs() << '\n';
        }
        errs() << '\n';

    }
    //print(errs(), NULL);
    return false;
}

/*
void DrawMemDep::print(raw_ostream &OS, const Module *M) const {
    OS << "digraph tmp{\n";
    OS << "node [ \n shape = \"record\"\n]; ";

    // depth and terminator
    std::pair<int, bool> marker[20] ;
    int index = 0;

    for (std::vector<const Instruction *>::const_iterator I = instVector.begin(), E = instVector.end();\
            I != E; ++I, ++index) {
        const Instruction *Inst = *I;

        DepSetMap::const_iterator DI = Deps.find(Inst);
        if (DI == Deps.end())
            continue;

        OS << "\tNode"<< static_cast<const void *>(Inst) << " [label=\"";
        Inst->print(OS);
        OS << "\"];\n";

        const DepSet &InstDeps = DI->second;

        for (DepSet::const_iterator I = InstDeps.begin(), E = InstDeps.end();
                I != E; ++I) {
            const Instruction *DepInst = I->first.getPointer();
            DepType type = I->first.getInt();
            const BasicBlock *DepBB = I->second;

            if (DepInst) {
                std::vector<const Instruction *>::const_iterator tmp = std::find(instVector.begin(), instVector.end(), DepInst);
                //DepSetMap::const_iterator tmp = Deps.find(DepInst);
                if(tmp == instVector.end())
                {
                    marker[index].first = 1;
                    marker[index].second = true;

                    OS << "\tNode"<< static_cast<const void *>(DepInst) << " [label=\"";
                    DepInst->print(OS);
                    OS << "\"];\n";

                }
                else 
                {
                    int pre_index = std::distance(instVector.begin(), tmp);
                    marker[pre_index].second = false;

                    marker[index].first = marker[pre_index].first + 1;
                    marker[index].second = true;
                }

                // link
                OS << "\tNode"<< static_cast<const void *>(DepInst) << " -> Node" \
                    << static_cast<const void *>(Inst) << " ";
                OS << "[label=\""<< DepTypeStr[type] << "\"]"<< "; \n";

            }
            }

        }

           for(int i = 0; i < Deps.size(); ++i) 
           {
           if( marker[i].second ) 
           OS << "depth is " << marker[i].first \
           << ", terminator? " << marker[i].second << "\n";
           }
        addEdge(0, int(instVector.size()), marker);
        OS << "}\n";

    }

    void DrawMemDep::addEdge(int start, int end, std::pair<int, bool> marker[]) const
    {
        if(start == end) return;
        for(int i = start; i < end; ++i)
            if(marker[i].second)
            {
                start = i;
                break;
            }

        for(int i = start+1; i < end; ++i)
            if(marker[i].second)
            {
                errs() << "\tNode"<< static_cast<const void *>(instVector[start]) << " -> Node" \
                    << static_cast<const void *>(instVector[i]) << " ";
                errs() << "[style=dotted];\n";
            }
        addEdge(start+1, end, marker);
    }
*/

    static RegisterPass<DrawMemDep> X("samplePass", "print the memory dependency", true, true);

