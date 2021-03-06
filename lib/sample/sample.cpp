#include "llvm/Analysis/MemoryDependenceAnalysis.h"
#include "llvm/LLVMContext.h"
#include "llvm/Analysis/Passes.h"
#include "llvm/Assembly/Writer.h"
#include "llvm/Support/CallSite.h"
#include "llvm/Support/InstIterator.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ADT/SetVector.h"

#include "llvm/ADT/GraphTraits.h"
#include "llvm/Support/GraphWriter.h"


using namespace llvm;

namespace {
    /**
     * some data type
     */
    typedef SmallSetVector<const Instruction *, 4> DepSet;
    typedef DenseMap<const Instruction *, DepSet> DepSetMap;

    /**
     * \brief GraphTraits 
     * GraphTraits specialiyed to draw the graph
     */
    template<> struct GraphTraits<const DepSetMap::const_iterator*> :
        public GraphTraits<const Instruction*> {
            static Instruction *getEntryNode(const DepSetMap::const_iterator *ite) {
                return ite->first();
            }
            // nodes_iterator/begin/end - Allow iteration over all nodes in the set
            typedef DepSet::const_iterator nodes_iterator;
            static nodes_iterator nodes_begin(const DepSetMap::const_iterator *set) { return set->second()->begin(); }
            static nodes_iterator nodes_end  (const DepSetMap::const_iterator *set) { return set->second()->end(); }
        };

    template<> struct GraphTraits<const DepSetMap*> :
        public GraphTraits<const Instruction*> {
            typedef const DepSetMap NodeType;
            typedef DepSet::const_iterator ChildIteratorType;

            static Instruction *getEntryNode(const DepSetMap::const_iterator *ite) {
                return ite->first();
            }

            // nodes_iterator/begin/end - Allow iteration over all nodes in the set
            typedef DepSetMap::const_iterator nodes_iterator;
            static nodes_iterator nodes_begin(const DepSetMap::const_iterator *set) { return set->begin(); }
            static nodes_iterator nodes_end  (const DepSetMap::const_iterator *set) { return set->end(); }

            static ChildIteratorType child_begin(const NodeType *set) { return set->second()->begin(); }
            static ChildIteratorType child_end  (const NodeType *set) { return set->second()->end(); }
        };

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
            void print(raw_ostream &OS) const; 

        private:

            DepSetMap Deps;


    };
}

char variablePass::ID = 0;

bool variablePass::runOnFunction(Function &F)
{
    this->F = &F;
    errs() << "In function: ";
    errs().write_escaped(F.getName()) << '\n';

    MemoryDependenceAnalysis &MDA = getAnalysis<MemoryDependenceAnalysis>();
    AliasAnalysis &AA = getAnalysis<AliasAnalysis>();

    for(inst_iterator i = inst_begin(F), e = inst_end(F); i != e; ++i)
    {
        Instruction *inst = &*i;
        /// if the instruction may access the memory
        if(!inst->mayReadFromMemory() && !inst->mayWriteToMemory())
            continue;

        MemDepResult Res = MDA.getDependency(inst);
        /// local
        if (!Res.isNonLocal()) {
            /// add the result into the dependency vector
            Deps[inst].insert(Res.getInst());
            /// function call
        } else if (CallSite CS = cast<Value>(inst)) {
            const MemoryDependenceAnalysis::NonLocalDepInfo &NLDI =
                MDA.getNonLocalCallDependency(CS);

            DepSet &InstDeps = Deps[inst];
            for (MemoryDependenceAnalysis::NonLocalDepInfo::const_iterator
                    I = NLDI.begin(), E = NLDI.end(); I != E; ++I) {
                const MemDepResult &Res = I->getResult();
                InstDeps.insert(Res.getInst());
            }
        } else {
            SmallVector<NonLocalDepResult, 4> NLDI;
            if (LoadInst *LI = dyn_cast<LoadInst>(inst)) {
                if (!LI->isUnordered()) {
                    // FIXME: Handle atomic/volatile loads.
                    Deps[inst].insert(0)
                    continue;
                }
                AliasAnalysis::Location Loc = AA.getLocation(LI);
                MDA.getNonLocalPointerDependency(Loc, true, LI->getParent(), NLDI);
            } else if (StoreInst *SI = dyn_cast<StoreInst>(inst)) {
                if (!SI->isUnordered()) {
                    // FIXME: Handle atomic/volatile stores.
                    Deps[inst].insert(0);
                    continue;
                }
                AliasAnalysis::Location Loc = AA.getLocation(SI);
                MDA.getNonLocalPointerDependency(Loc, false, SI->getParent(), NLDI);
            } else if (VAArgInst *VI = dyn_cast<VAArgInst>(inst)) {
                AliasAnalysis::Location Loc = AA.getLocation(VI);
                MDA.getNonLocalPointerDependency(Loc, false, VI->getParent(), NLDI);
            } else {
                llvm_unreachable("Unknown memory instruction!");
            }

            DepSet &InstDeps = Deps[inst];
            for (SmallVectorImpl<NonLocalDepResult>::const_iterator
                    I = NLDI.begin(), E = NLDI.end(); I != E; ++I) {
                const MemDepResult &Res = I->getResult();
                InstDeps.insert(Res.getInst());
            }
        }
    }
    print(errs());
    return false;
}

void variablePass::getAnalysisUsage(AnalysisUsage &AU) const 
{
    AU.addRequiredTransitive<AliasAnalysis>();
    AU.addRequiredTransitive<MemoryDependenceAnalysis>();
    AU.setPreservesAll();
}

void variablePass::print(raw_ostream &OS) const {
   /* 
    for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I) {
        const Instruction *Inst = &*I;

        DepSetMap::const_iterator DI = Deps.find(Inst);
        if (DI == Deps.end())
            continue;

        const DepSet &InstDeps = DI->second;

        for (DepSet::const_iterator I = InstDeps.begin(), E = InstDeps.end();
                I != E; ++I) {
            const Instruction *DepInst = I->data();
            if (DepInst) {
                OS << " from: ";
                DepInst->print(OS);
            }
            OS << "\n";
        }
        OS << "***";
        Inst->print(OS);
        OS << "***" << '\n';
    }
    */
    //WriteGraph<DepSet>(errs(), InstDeps, false, "Memory Dependency");
    for (DepSetMap::const_iterator I = Deps.begin(), E = Deps.end();
            I != E; ++I) 
        ViewGraph<DepSet>(I, "graph.dot", false, "Memory Dependency", GraphProgram::DOT);
}



static RegisterPass<variablePass> X("variablePass", "variable scan Pass", false, true);
