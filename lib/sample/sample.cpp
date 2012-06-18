#include <vector>

#include "llvm/Pass.h"
#include "llvm/PassManager.h"
#include "llvm/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Instructions.h"
#include "llvm/Support/InstIterator.h"
#include "llvm/Analysis/MemoryDependenceAnalysis.h"


using namespace llvm;

namespace {
    /**
     * \brief Memory analysis pass
     * find the memory operation dependecy
     */
    class variablePass: public FunctionPass{
        public:
            variablePass(): FunctionPass(ID){}
            static char ID;

            virtual bool runOnFunction(Function &F);
            virtual void getAnalysisUsage(AnalysisUsage &AU) const;


    };
}

char variablePass::ID = 0;

bool variablePass::runOnFunction(Function &F)
{
    errs() << "In function: ";
    errs().write_escaped(F.getName()) << '\n';

    /// used to store memory related instructin(include potential ones, e.g. call)
    std::vector<Instruction *> instList;

    for(inst_iterator i = inst_begin(F), e = inst_end(F); i != e; ++i)
        switch (i->getOpcode())
        {

            case Instruction::Alloca:
                instList.push_back(&*i);
                break;
            case Instruction::Load:
                instList.push_back(&*i);
                break;
            case Instruction::Store:
                instList.push_back(&*i);
                break;
            case Instruction::Call:
                instList.push_back(&*i);
                break;
            default: break;
        }

    errs() << "total number of memory instruction " <<  instList.size() <<'\n';

    
    MemoryDependenceAnalysis &mda = getAnalysis<MemoryDependenceAnalysis>();
    for(int i = 0, size = instList.size(); i < size; i++)
        MemDepResult result = mda.getDependency(instList[i]);

    return false;
}

void variablePass::getAnalysisUsage(AnalysisUsage &AU) const 
{
      //AU.addRequiredTransitive<AliasAnalysis>();
      AU.addRequiredTransitive<MemoryDependenceAnalysis>();
      AU.setPreservesAll();
}

static RegisterPass<variablePass> X("variablePass", "variable scan Pass", false, true);
